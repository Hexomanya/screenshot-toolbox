[CinematicTrackAttribute(
    name: "DAB Pose Manipulation Track",
    description: "Applies one or more stacked pose modifications from .conf files"
)]
class DAB_PoseManipulationTrack : CinematicTrackBase
{
    [Attribute(desc: "Entity to apply this pose to.")]
    protected string m_sEntityName;
    
    [Attribute(desc: "Comma-separated list of ResourceNames for pose configs")]
    protected string m_sSerializedConfigs;

    private GenericEntity m_TargetEntity;
    protected SlotManagerComponent m_SlotManager;
    protected World m_World;
	
    // Cache to prevent re-parsing .conf files every frame
    protected ref map<ResourceName, ref DAB_PoseModification> m_CachedModifications = new map<ResourceName, ref DAB_PoseModification>();

    //-----------------------------------------------------------------------
    override void OnInit(World world)
    {
        m_World = world;
        if (!m_World) return;
        
        RefreshTargetEntity(m_World);
    }

    //-----------------------------------------------------------------------
    override void OnApply(float time)
    {
        if (!m_World) return;

        // Ensure we are targeting the right entity
        string currentTarget = GetEntityName();
        if (!m_TargetEntity || m_TargetEntity.GetName() != currentTarget)
        {
            RefreshTargetEntity(m_World);
        }

        if (!m_TargetEntity)
            return;

        array<ResourceName> configs = GetConfigs();
        if (configs.IsEmpty())
            return;
        
        foreach (ResourceName modificationName : configs)
        {
            DAB_PoseModification poseModification = GetModificationFromFile(modificationName);
            if (!poseModification || poseModification.m_aBoneModifications.IsEmpty())
                continue;
            
            ApplyPoseModification(poseModification);
        }
    }
    
    //-----------------------------------------------------------------------
    protected DAB_PoseModification GetModificationFromFile(ResourceName modificationName)
    {
        DAB_PoseModification cached;
        if (m_CachedModifications.Find(modificationName, cached))
            return cached;

        Resource configResource = BaseContainerTools.LoadContainer(modificationName);
		Print("modificationName that errors is: " + modificationName);
        if (!configResource || !configResource.IsValid()) return null;
        
        BaseContainer container = configResource.GetResource().ToBaseContainer();
        if (!container) return null;

        DAB_PoseModification poseData = new DAB_PoseModification();
        
        // FIX: Using WriteToInstance to map Container -> Script Object
        BaseContainerTools.WriteToInstance(poseData, container);
        
        if (poseData)
            m_CachedModifications.Insert(modificationName, poseData);
        
        return poseData;
    }
    
    //-----------------------------------------------------------------------
    protected void ApplyPoseModification(DAB_PoseModification poseModification)
    {
        foreach (DAB_BoneModification boneModification : poseModification.m_aBoneModifications)
        {
            ApplyBoneModification(boneModification);
        }
    }

    //-----------------------------------------------------------------------
    protected void ApplyBoneModification(DAB_BoneModification boneModification)
    {	
        TNodeId boneId = DAB_BoneHelper.GetBoneId(m_TargetEntity, boneModification.m_sBoneName);	
        
        // Removed guards as -1 is considered a valid bone index in this context
        vector originalRotation;
        if (!DAB_BoneHelper.TryGetBoneLocalRotation(m_TargetEntity, boneId, originalRotation))
            return;

        // Alignment with DAB_EditorController.c swizzling (Pitch, Yaw, Roll)
        vector rotRad = boneModification.m_vRotationOffset * Math.DEG2RAD;
        vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);

        Animation anim = GetSlotDependentAnim(boneModification.m_sBoneName);

        vector entityWorld[4];
        m_TargetEntity.GetTransform(entityWorld);

        vector entityRot3[3];
        entityRot3[0] = entityWorld[0];
        entityRot3[1] = entityWorld[1];
        entityRot3[2] = entityWorld[2];

        // Create the base orientation matrix
        vector boneOrigLocal3[3];
        Math3D.AnglesToMatrix(Vector(originalRotation[1], originalRotation[0], originalRotation[2]), boneOrigLocal3);

        vector boneOrigWorldRot[3];
        Math3D.MatrixMultiply3(entityRot3, boneOrigLocal3, boneOrigWorldRot);

        // Project the world-space position offset into bone-local space
        vector worldOff = boneModification.m_vPositionOffset;
        vector localOff;
        localOff[0] = vector.Dot(worldOff, boneOrigWorldRot[0]);
        localOff[1] = vector.Dot(worldOff, boneOrigWorldRot[1]);
        localOff[2] = vector.Dot(worldOff, boneOrigWorldRot[2]);

        anim.SetBone(m_TargetEntity, boneId, rotRadCorrected, localOff, boneModification.m_fScale);
        m_TargetEntity.Update();
    }

    //-----------------------------------------------------------------------
    protected array<ResourceName> GetConfigs()
    {
        array<ResourceName> configs = {};
        if (m_sSerializedConfigs.IsEmpty())
            return configs;

        TStringArray paths = new TStringArray();
        m_sSerializedConfigs.Split(",", paths, true);
        
        foreach (string path : paths)
        {
            configs.Insert(path);
        }
        return configs;
    }

    //-----------------------------------------------------------------------
    protected void RefreshTargetEntity(World world)
    {
        if(!world) return;
		
        string entityName = GetEntityName();
        if (entityName.IsEmpty()) return;

        IEntity found = world.FindEntityByName(entityName);
        if (!found) return;

        m_TargetEntity = GenericEntity.Cast(found);
        if (m_TargetEntity)
            m_SlotManager = SlotManagerComponent.Cast(m_TargetEntity.FindComponent(SlotManagerComponent));
    }
    
    //-----------------------------------------------------------------------
    string GetEntityName()
    {
        if (!m_sEntityName.IsEmpty()) return m_sEntityName;
        
        TStringArray strs = new TStringArray;
        GetTrackName().Split("_", strs, true);
        if(strs.Count() == 0) return "";
        return strs.Get(0);
    }

    //-----------------------------------------------------------------------
    protected Animation GetSlotDependentAnim(string boneName)
    {
        if (!m_SlotManager) return m_TargetEntity.GetAnimation();

        array<EntitySlotInfo> slotInfos = {};
        m_SlotManager.GetSlotInfos(slotInfos);

        foreach (EntitySlotInfo slotInfo : slotInfos)
        {
            IEntity slotEntity = slotInfo.GetAttachedEntity();
            if (!slotEntity) continue;

            Animation anim = slotEntity.GetAnimation();
            if (anim && anim.GetBoneIndex(boneName) != -1)
                return anim;
        }

        return m_TargetEntity.GetAnimation();
    }
}