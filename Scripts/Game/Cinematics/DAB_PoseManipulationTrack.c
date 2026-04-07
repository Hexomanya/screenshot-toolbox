[CinematicTrackAttribute(
	name: "DAB Pose Manipulation Track",
	description: "Applies pose modifications sourced from the entity's DAB_PoseModificationComponent"
)]
class DAB_PoseManipulationTrack : CinematicTrackBase
{
	[Attribute("1", desc: "If unchecked, pose modifications will not be applied to the target entity.")]
	bool m_bApplyModifications;

	private GenericEntity m_TargetEntity;
	protected SlotManagerComponent m_SlotManager;
	protected World m_World;

	protected ref map<ResourceName, ref DAB_PoseModification> m_mCachedModifications = new map<ResourceName, ref DAB_PoseModification>();
	protected ref map<string, vector> m_mBaseRotationCache = new map<string, vector>();

	// ── Lifecycle & Overrides ─────────────────────────────────────────────
	//-----------------------------------------------------------------------
	override void OnInit(World world)
	{
		m_mCachedModifications.Clear();
		m_mBaseRotationCache.Clear();

		m_World = world;
		if (!m_World) return;

		RefreshTargetEntity(m_World);
	}

	//-----------------------------------------------------------------------
	override void OnApply(float time)
	{
		if (!m_bApplyModifications) return;

		m_mCachedModifications.Clear();
		if (!m_World) return;

		string currentTarget = GetEntityName();
		if (!m_TargetEntity || m_TargetEntity.GetName() != currentTarget)
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
	protected void ApplyModificationFromResource(ResourceName modificationName)
	{
		DAB_PoseModification poseModification = GetModificationFromFile(modificationName);
		if (!poseModification || poseModification.m_aBoneModifications.IsEmpty()) return;

		ApplyPoseModification(poseModification);
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

		if (poseData) m_mCachedModifications.Insert(modificationName, poseData);

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

		vector originalRotation;
		if (!m_mBaseRotationCache.Find(boneModification.m_sBoneName, originalRotation))
		{
			if (!DAB_BoneHelper.TryGetBoneLocalRotation(m_TargetEntity, boneId, originalRotation)) return;

			m_mBaseRotationCache.Insert(boneModification.m_sBoneName, originalRotation);
		}

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
		Math3D.AnglesToMatrix(Vector(originalRotation[1], originalRotation[0], originalRotation[2]), boneOrigLocal3);

		vector boneOrigWorldRot[3];
		Math3D.MatrixMultiply3(entityRot3, boneOrigLocal3, boneOrigWorldRot);

		vector worldOff = boneModification.m_vPositionOffset;
		vector localOff;
		localOff[0] = vector.Dot(worldOff, boneOrigWorldRot[0]);
		localOff[1] = vector.Dot(worldOff, boneOrigWorldRot[1]);
		localOff[2] = vector.Dot(worldOff, boneOrigWorldRot[2]);

		if (boneModification.m_sBoneName == "Hips")
			PrintFormat("Track: Setting hips to pos: %1, rot: %2", localOff, rotRadCorrected);

		anim.SetBone(m_TargetEntity, boneId, rotRadCorrected, localOff, boneModification.m_fScale);
		m_TargetEntity.Update();
	}

	//-----------------------------------------------------------------------
	protected void RefreshTargetEntity(World world)
	{
		if (!world) return;

		string entityName = GetEntityName();
		if (entityName.IsEmpty()) return;

		IEntity found = world.FindEntityByName(entityName);
		if (!found) return;

		m_TargetEntity = GenericEntity.Cast(found);
		if (m_TargetEntity)
			m_SlotManager = SlotManagerComponent.Cast(m_TargetEntity.FindComponent(SlotManagerComponent));
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
			if (anim && anim.GetBoneIndex(boneName) != -1) return anim;
		}

		return m_TargetEntity.GetAnimation();
	}
}