[BaseContainerProps()]
class DAB_PoseConfigEntry
{
    [Attribute(
        uiwidget: UIWidgets.ResourceNamePicker,
        desc: "Pose modification conf",
        params: "conf DAB_PoseModification"
    )]
    ResourceName m_sConfig;
}


[CinematicTrackAttribute(
    name: "DAB Pose Manipulation Track",
    description: "Applies one or more stacked pose modifications from .conf files"
)]
class DAB_PoseManipulationTrack : CinematicTrackBase
{
    // The names of these attributes MUST match the 'TrackName' of the ChildTracks 
    // created in your Workbench helper script.
    [Attribute(desc: "Entity to apply this pose to.")]
    protected string m_sEntityName;
    
    [Attribute(desc: "Comma-separated list of ResourceNames for pose configs")]
    protected string m_sSerializedConfigs;

    private GenericEntity m_TargetEntity;
    protected SlotManagerComponent m_SlotManager;

    //-----------------------------------------------------------------------
    // Helper to parse the serialized string back into an array for processing
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
    override void OnInit(World world)
    {
        if (!world) return;
        RefreshTargetEntity(world);
    }

    //-----------------------------------------------------------------------
    override void OnApply(float time)
    {
        // Re-verify the target entity. This is important if m_sEntityName 
        // changes over the timeline via keyframes.
        string currentTarget = GetEntityName();
        if (!m_TargetEntity || m_TargetEntity.GetName() != currentTarget)
        {
            //RefreshTargetEntity(GetWorld());
        }

        // Parse the configs for the current frame
        array<ResourceName> configs = GetConfigs();
        
        if (configs.IsEmpty() || !m_TargetEntity)
            return;
        
        foreach (ResourceName modificationName : configs)
        {
            DAB_PoseModification poseModification = GetModificationFromFile(modificationName);
            if (!poseModification)
                continue;
            
            ApplyPoseModification(poseModification);
        }
    }
    
    //-----------------------------------------------------------------------
    protected DAB_PoseModification GetModificationFromFile(ResourceName modificationName)
    {
        Resource resource = Resource.Load(modificationName);
        if (!resource || !resource.IsValid()) return null;
        
        DAB_PoseModification poseData = DAB_PoseModification.Cast(
            BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer())
        );
        
        return poseData;
    }
    
    //-----------------------------------------------------------------------
    protected void ApplyPoseModification(DAB_PoseModification poseModification)
    {
        if (poseModification.m_aBoneModifications.IsEmpty())
            return;
        
        foreach (DAB_BoneModification boneModification : poseModification.m_aBoneModifications)
        {
            ApplyBoneModification(boneModification);
        }
    }

    //-----------------------------------------------------------------------
  	protected void ApplyBoneModification(DAB_BoneModification boneModification)
	{	
		TNodeId boneId = DAB_BoneHelper.GetBoneId(m_TargetEntity, boneModification.m_sBoneName);	
		vector originalBonePosition;
		DAB_BoneHelper.TryGetBonePosition(m_TargetEntity, boneModification.m_sBoneName, originalBonePosition);
		
		vector rotRad = boneModification.m_vRotationOffset * Math.DEG2RAD;
		vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);
	
		Animation anim = GetSlotDependentAnim(boneModification.m_sBoneName);
	
		vector entityWorld[4];
		m_TargetEntity.GetTransform(entityWorld);
	
		vector entityRot3[3];
		entityRot3[0] = entityWorld[0];
		entityRot3[1] = entityWorld[1];
		entityRot3[2] = entityWorld[2];
	
		vector boneOrigLocal3[3];
		Math3D.AnglesToMatrix(Vector(originalBonePosition[1], originalBonePosition[0], originalBonePosition[2]), boneOrigLocal3);
	
		vector boneOrigWorldRot[3];
		Math3D.MatrixMultiply3(entityRot3, boneOrigLocal3, boneOrigWorldRot);
	
		vector worldOff = boneModification.m_vPositionOffset;
		vector localOff;
		localOff[0] = vector.Dot(worldOff, boneOrigWorldRot[0]);
		localOff[1] = vector.Dot(worldOff, boneOrigWorldRot[1]);
		localOff[2] = vector.Dot(worldOff, boneOrigWorldRot[2]);
	
		anim.SetBone(m_TargetEntity, boneId, rotRadCorrected, localOff, boneModification.m_fScale);
		m_TargetEntity.Update();
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
        if (!m_TargetEntity) return;

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
            if (!anim) continue;

            if (anim.GetBoneIndex(boneName) != -1)
                return anim;
        }

        return m_TargetEntity.GetAnimation();
    }
}