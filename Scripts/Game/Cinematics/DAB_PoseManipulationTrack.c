[CinematicTrackAttribute(
	name: "DAB Pose Manipulation Track",
	description: "Applies pose modifications sourced from the entity's DAB_PoseModificationComponent"
)]
class DAB_PoseManipulationTrack : CinematicTrackBase
{
	[Attribute("1", desc: "If unchecked, pose modifications will not be applied to the target entity.")]
	bool m_bApplyModifications;

	private IEntity m_TargetEntity;
	protected SlotManagerComponent m_SlotManager;
	protected World m_World;

	protected ref map<ResourceName, ref DAB_PoseModification> m_mCachedModifications = new map<ResourceName, ref DAB_PoseModification>();
	protected ref map<string, vector> m_mBaseRotationCache = new map<string, vector>();

	// ── Overrides ─────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	//! Applies active pose modifications from the target entity's DAB_PoseModificationComponent.
	override void OnApply(float time)
	{
		if (!m_bApplyModifications) return;

		m_mCachedModifications.Clear();
		if (!m_World) return;

		string currentTargetName = GetEntityName();
		if (!m_TargetEntity || m_TargetEntity.GetName() != currentTargetName)
			RefreshTargetEntity(m_World);

		if (!m_TargetEntity) return;

		DAB_PoseModificationComponent poseComp = DAB_PoseModificationComponent.Cast(m_TargetEntity.FindComponent(DAB_PoseModificationComponent));
		if (!poseComp)
		{
			Print("DAB_PoseManipulationTrack: target entity has no DAB_PoseModificationComponent!", LogLevel.WARNING);
			return;
		}

		array<ResourceName> poseList = poseComp.GetPoseModifications();
		foreach (ResourceName modName : poseList)
		{
			ApplyModificationFromResource(modName);
		}

		ResourceName workingConfig = poseComp.GetWorkingModificationConfig();
		if (!workingConfig.IsEmpty() && !poseList.Contains(workingConfig))
			ApplyModificationFromResource(workingConfig);
	}

	//-----------------------------------------------------------------------
	//! Initializes the track, clears caches, and resolves the target entity in the world.
	override void OnInit(World world)
	{
		m_mCachedModifications.Clear();
		m_mBaseRotationCache.Clear();

		m_World = world;

		RefreshTargetEntity(m_World);
		ResetBonesToAnimation(); // If we add/remove tracks this object will reinit, but the bones won't. So on the next apply we would cache the modified rotation
	}

	// ── Public ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	//! Derives the target entity name from the track name (prefix before the first '_').
	string GetEntityName()
	{
		TStringArray strs = new TStringArray;
		GetTrackName().Split("_", strs, true);
		if (strs.Count() == 0) return "";
		return strs.Get(0);
	}

	// ── Protected ─────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected bool FindSlotEntity(DAB_BoneModification boneModification, out IEntity outEntity)
	{
		outEntity = m_TargetEntity;		
		
		int count = boneModification.m_aSlotNames.Count();
		if(count <= 0) return true; // Older config versions had no array, so we just wave them through
		
		for (int i = 0; i < boneModification.m_aSlotNames.Count(); i++)
		{
		    string slotName = boneModification.m_aSlotNames[i];
			if(slotName == DAB_Constants.SLOT_ROOT_ID) continue;
			
			SlotManagerComponent slotManager = SlotManagerComponent.Cast(DAB_Helper.FindComponentExact(outEntity, SlotManagerComponent));
			if(!slotManager)
			{
				PrintFormat("Could not find a slot manager for the slot with the name: %1", slotName, LogLevel.ERROR);
				return false;
			}
			
			EntitySlotInfo slotInfo = slotManager.GetSlotByName(slotName);
			if(!slotInfo)
			{
				PrintFormat("Could not resolve slotName! slotName: %1; index: %2", slotName, i, LogLevel.ERROR);
				return false;
			}

			outEntity = slotInfo.GetAttachedEntity();
			if(!outEntity)
			{
				PrintFormat("Encountered null attached entity while resolving slotNames array! slotName: %1; index: %2", slotName, i, LogLevel.ERROR);
				return false;
			}
		}
		
		return true;
	}
	
	//-----------------------------------------------------------------------
	protected void ApplyBoneModification(DAB_BoneModification boneModification)
	{
		IEntity slotEntity;
		if(!FindSlotEntity(boneModification, slotEntity))
		{
			PrintFormat("Could find slotEntity for bone: %1", boneModification.m_sBoneName, LogLevel.ERROR);
			return;
		}
		
		Animation anim;
		TNodeId boneId = DAB_BoneHelper.GetBoneIndex(slotEntity, boneModification.m_sBoneName, anim);
		if(boneId == -1)
		{
			PrintFormat("Could not find boneid for bone with name: %1", boneModification.m_sBoneName);
			return;
		}

		vector originalRotation;
		if (!m_mBaseRotationCache.Find(boneModification.m_sBoneName, originalRotation))
		{
			if (!DAB_BoneHelper.TryGetBoneLocalRotation(slotEntity, boneId, originalRotation)) return;
			m_mBaseRotationCache.Insert(boneModification.m_sBoneName, originalRotation);
		}

		vector rotRad = boneModification.m_vRotationOffset * Math.DEG2RAD;
		vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);

		vector entityWorld[4];
		slotEntity.GetWorldTransform(entityWorld);

		vector entityRot3[3];
		entityRot3[0] = entityWorld[0];
		entityRot3[1] = entityWorld[1];
		entityRot3[2] = entityWorld[2];

		vector boneOrigLocal3[3];
		Math3D.AnglesToMatrix(Vector(originalRotation[1], originalRotation[0], originalRotation[2]), boneOrigLocal3);

		vector boneOrigWorldRot[3];
		Math3D.MatrixMultiply3(entityRot3, boneOrigLocal3, boneOrigWorldRot);

		vector worldOff = boneModification.m_vPositionOffset;
		vector localOff;
		localOff[0] = vector.Dot(worldOff, boneOrigWorldRot[0]);
		localOff[1] = vector.Dot(worldOff, boneOrigWorldRot[1]);
		localOff[2] = vector.Dot(worldOff, boneOrigWorldRot[2]);
		
		anim.SetBone(slotEntity, boneId, rotRadCorrected, localOff, boneModification.m_fScale);
		slotEntity.Update();
	}

	//-----------------------------------------------------------------------
	protected void ApplyModificationFromResource(ResourceName modificationName)
	{
		DAB_PoseModification poseModification = GetModificationFromFile(modificationName);
		if (!poseModification || poseModification.m_aBoneModifications.IsEmpty()) return;

		ApplyPoseModification(poseModification);
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
	protected DAB_PoseModification GetModificationFromFile(ResourceName modificationName)
	{
		DAB_PoseModification cached;
		if (m_mCachedModifications.Find(modificationName, cached)) return cached;

		Resource configResource = BaseContainerTools.LoadContainer(modificationName);
		if (!configResource || !configResource.IsValid()) return null;

		BaseContainer container = configResource.GetResource().ToBaseContainer();
		if (!container) return null;

		DAB_PoseModification poseData = new DAB_PoseModification();
		BaseContainerTools.WriteToInstance(poseData, container);

		//if (poseData) m_mCachedModifications.Insert(modificationName, poseData);

		return poseData;
	}

	//-----------------------------------------------------------------------
	protected void RefreshTargetEntity(World world)
	{
		if (!world) return;
		
		string entityName = GetEntityName();
		if (entityName.IsEmpty()) return;

		IEntity found = world.FindEntityByName(entityName);

		if (found != m_TargetEntity)
			m_mBaseRotationCache.Clear();

		m_TargetEntity = found;	
	}

	//-----------------------------------------------------------------------
	protected void ResetBonesToAnimation()
	{
		if (!m_TargetEntity) return;

		Animation anim = m_TargetEntity.GetAnimation();
		if (!anim) return;

		array<string> boneNames = {};
		anim.GetBoneNames(boneNames);

		foreach (string boneName : boneNames)
		{
			TNodeId boneId = anim.GetBoneIndex(boneName);
			anim.SetBone(m_TargetEntity, boneId, vector.Zero, vector.Zero, 1.0);
		}

		m_TargetEntity.Update();
	}
}