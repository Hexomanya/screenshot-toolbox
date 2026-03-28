class DAB_EditorController
{
	protected DAB_BoneManipulatorTool m_ParentTool;
	protected WorldEditorAPI m_API;
	
	protected ref DAB_BoneOverlayRenderer m_Renderer;
	protected ref DAB_GizmoController m_GizmoController;
	
	protected string m_sSelectedBoneName = "";
	protected string m_sHoveredBoneName  = "";

	protected ref map<string, ref DAB_SkeletonInfo> m_CachedSkeletons = new map<string, ref DAB_SkeletonInfo>();
	protected string m_sCurrentSkeleton;
	protected ref map<string, ref DAB_BoneTransform> m_ModifiedBones = new map<string, ref DAB_BoneTransform>();

	protected float m_fCameraTargetDistance = 0;
	protected ref DAB_BoneDisplaySettings m_LastBoneDisplaySettings;

	protected SlotManagerComponent m_SlotManager;
	
	// ── Constructors ──────────────────────────────────────────────────────────
	void DAB_EditorController(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api)
	{
		m_ParentTool = parentTool;
		m_API = api;
		
		m_Renderer = new DAB_BoneOverlayRenderer();
		m_GizmoController = new DAB_GizmoController(m_API);
		m_GizmoController.GetOnTransformChanged().Insert(this.OnBoneTransformChanged);
	}
	
	void ~DAB_EditorController()
	{
		m_ParentTool = null;
		m_API = null;
	}
	
	// ── Lifecycle ──────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void OnActivate()
	{
		RedrawOverlay();
	}
	
	//-----------------------------------------------------------------------
	void OnDeActivate()
	{
		m_GizmoController.Clear(m_API);
		m_Renderer.Clear();
		m_sSelectedBoneName = "";
	}
	
	//-----------------------------------------------------------------------
	void OnAfterLoadWorld()
	{
		RedrawOverlay();
	}
	
	// ── Input ──────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void OnEnterEvent()
	{
		RefreshDisplaySettings();
	}
	
	//-----------------------------------------------------------------------
	void OnMouseMoveEvent(float x, float y)
	{
		RefreshCameraTargetDistance();
		m_GizmoController.OnMouseMove(x, y, m_API);
		CheckBoneHover(x, y);
	}
	
	//-----------------------------------------------------------------------
	void OnMousePressEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		m_GizmoController.OnMousePress(x, y, buttons, m_API);

		if (!(buttons & WETMouseButtonFlag.LEFT)) return;
		if (!m_sSelectedBoneName.IsEmpty()) return; // gizmo handles input when a bone is active

		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_ParentTool.GetCurrentTargetEntity(), m_API);
		if (!hitBoneName.IsEmpty())
			SelectBone(hitBoneName);
	}
	
	//-----------------------------------------------------------------------
	void OnMouseReleaseEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		m_GizmoController.OnMouseRelease(x, y, buttons, m_API);
		if (buttons & WETMouseButtonFlag.RIGHT)
			RefreshCameraTargetDistance();
	}
	
	//-----------------------------------------------------------------------
	//! Key bindings: U = Rotation, I = Position, O = Scale, J = Reset bone, Escape = Deselect.
	void OnKeyPressEvent(KeyCode key, bool isAutoRepeat)
	{
		if (isAutoRepeat) return;

		switch (key)
		{
			case KeyCode.KC_ESCAPE:
				DeselectBone();
				RedrawOverlay();
				break;

			case KeyCode.KC_U:
				if (!m_sSelectedBoneName.IsEmpty())
					m_GizmoController.SwitchMode(DAB_GizmoMode.Rotation, m_API);
				break;

			case KeyCode.KC_I:
				if (!m_sSelectedBoneName.IsEmpty())
					m_GizmoController.SwitchMode(DAB_GizmoMode.Position, m_API);
				break;

			case KeyCode.KC_O:
				if (!m_sSelectedBoneName.IsEmpty())
					m_GizmoController.SwitchMode(DAB_GizmoMode.Scale, m_API);
				break;

			case KeyCode.KC_J:
				if (!m_sSelectedBoneName.IsEmpty())
					ResetBone(m_sSelectedBoneName);
				break;
		}

		RefreshCameraTargetDistance();
	}
	
	// ── Public ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void OnTargetEntityChanged(IEntity changedEntity)
	{
		m_sCurrentSkeleton = "";
		m_SlotManager = null;
		
		if (!changedEntity)
		{
			Print("DAB_BoneManipulatorTool.RefreshTargetEntity: no entity selected.", LogLevel.NORMAL);
			return;
		}

		Animation anim = changedEntity.GetAnimation();
		if (!anim)
		{
			Print("DAB_BoneManipulatorTool.RefreshTargetEntity: entity has no Animation component.", LogLevel.ERROR);
			return;
		}

		string skeletonKey = DAB_SkeletonInfo.ComputeSkeletonKey(changedEntity);
		if (skeletonKey.IsEmpty())
		{
			Print("DAB_BoneManipulatorTool.RefreshTargetEntity: could not compute skeleton key.", LogLevel.ERROR);
			return;
		}

		if (!m_CachedSkeletons.Contains(skeletonKey))
		{
			array<string> boneNames = {};
			anim.GetBoneNames(boneNames);
			map<string, string> boneParents = DAB_BoneHelper.ComputeBoneParents(anim, boneNames);
			map<string, float>  boneParentDistances = DAB_BoneHelper.ComputeBoneParentDistances(changedEntity, boneParents);
			m_CachedSkeletons.Set(skeletonKey, new DAB_SkeletonInfo(skeletonKey, boneNames, boneParents, boneParentDistances));
		}
		m_sCurrentSkeleton = skeletonKey;
		
		m_SlotManager = SlotManagerComponent.Cast(changedEntity.FindComponent(SlotManagerComponent));
		
		RefreshCameraTargetDistance();
	}
	
	// ── Private ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected void OnBoneTransformChanged(DAB_BoneTransform changedTransform)
	{
		string boneName = changedTransform.GetBoneName();
		m_ModifiedBones.Set(boneName, changedTransform);
		RefreshBone(boneName);
	}

	//-----------------------------------------------------------------------
	// m_vPositionOffset is in world space. SetBone expects a position in the
	// bone's local frame, so the offset is projected onto the bone's ORIGINAL
	// world axes (entity × boneOrig — without accumRotation).
	// Using the current axes would silently shift the projected localOff on
	// every rotation call, making the bone orbit its rest location.
	protected void RefreshBone(string boneName)
	{
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		
		if (!targetEntity)
		{
			Print("DAB_BoneManipulatorTool.RefreshBone: no valid entity selected.", LogLevel.ERROR);
			return;
		}

		DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
		if (!transform)
		{
			Print("DAB_BoneManipulatorTool.RefreshBone: no stored transform for '" + boneName + "'.", LogLevel.ERROR);
			return;
		}

		TNodeId boneId = DAB_BoneHelper.GetBoneId(targetEntity, boneName);
		vector rotRad = transform.m_vRotationOffset * Math.DEG2RAD;
		vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);

		Animation anim = GetSlotDependentAnim(boneName);

		vector entityWorld[4];
		targetEntity.GetTransform(entityWorld);

		vector entityRot3[3];
		entityRot3[0] = entityWorld[0];
		entityRot3[1] = entityWorld[1];
		entityRot3[2] = entityWorld[2];

		vector boneOrigLocal3[3];
		vector orig = transform.GetOriginalRotation();
		Math3D.AnglesToMatrix(Vector(orig[1], orig[0], orig[2]), boneOrigLocal3);

		vector boneOrigWorldRot[3];
		Math3D.MatrixMultiply3(entityRot3, boneOrigLocal3, boneOrigWorldRot);

		vector worldOff = transform.m_vPositionOffset;
		vector localOff;
		localOff[0] = vector.Dot(worldOff, boneOrigWorldRot[0]);
		localOff[1] = vector.Dot(worldOff, boneOrigWorldRot[1]);
		localOff[2] = vector.Dot(worldOff, boneOrigWorldRot[2]);

		anim.SetBone(targetEntity, boneId, rotRadCorrected, localOff, transform.m_fScale);
		targetEntity.Update();
		m_Renderer.DrawSelectedBone(targetEntity, boneName, m_API);
	}

	//-----------------------------------------------------------------------
	// Returns the Animation component that owns boneName, preferring slot entities
	// so that bones on attached equipment are manipulated correctly.
	protected Animation GetSlotDependentAnim(string boneName)
	{
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!m_SlotManager) return targetEntity.GetAnimation();

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

		return targetEntity.GetAnimation();
	}
	
	//-----------------------------------------------------------------------
	protected void SelectBone(string boneName)
	{
		m_sSelectedBoneName = boneName;

		DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
		if (!transform)
			transform = CreateNewTransform(boneName);

		m_GizmoController.Attach(m_ParentTool.GetCurrentTargetEntity(), transform, m_API, m_fCameraTargetDistance);
		RedrawOverlay();
	}

	//-----------------------------------------------------------------------
	protected DAB_BoneTransform CreateNewTransform(string boneName)
	{
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		vector bonePosition;
		if (!DAB_BoneHelper.TryGetBonePosition(targetEntity, boneName, bonePosition))
		{
			Print("DAB_BoneManipulatorTool.CreateNewTransform: could not get position for '" + boneName + "'.", LogLevel.ERROR);
			return null;
		}

		TNodeId boneId = DAB_BoneHelper.GetBoneId(targetEntity, boneName);
		vector  boneRotation;
		if (!DAB_BoneHelper.TryGetBoneLocalRotation(targetEntity, boneId, boneRotation))
		{
			Print("DAB_BoneManipulatorTool.CreateNewTransform: could not get rotation for '" + boneName + "'.", LogLevel.ERROR);
			return null;
		}

		return new DAB_BoneTransform(boneName, bonePosition, boneRotation);
	}

	//-----------------------------------------------------------------------
	protected void DeselectBone()
	{
		m_sSelectedBoneName = "";
		m_GizmoController.Clear(m_API);
		IEntitySource targetEntitySource = m_ParentTool.GetCurrentTargetEntitySource();
		if (targetEntitySource)
			m_API.SetEntitySelection(targetEntitySource);
	}

	//-----------------------------------------------------------------------
	protected void ResetBone(string boneName)
	{
		DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
		if (!transform) return;

		transform.m_vPositionOffset = vector.Zero;
		transform.m_vRotationOffset = vector.Zero;
		transform.m_fScale          = 1.0;

		RefreshBone(boneName);
		m_GizmoController.ResetAccumulatedTransform(m_API);
		m_Renderer.DrawSelectedBone(m_ParentTool.GetCurrentTargetEntity(), boneName, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RefreshCameraTargetDistance()
	{
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!targetEntity) return;

		vector camPos   = DAB_Helper.GetCameraPosition(m_API);
		float  distance = vector.Distance(camPos, targetEntity.GetOrigin());

		if (DAB_Helper.AreFloatsEqual(distance, m_fCameraTargetDistance, DAB_VisConfig.CAMERA_POLLING_DISTANCE_EPSILON))
			return;

		m_fCameraTargetDistance = distance;

		if (m_sSelectedBoneName.IsEmpty())
			m_Renderer.RefreshSphereSizes(m_sHoveredBoneName, camPos, m_API);
		else
			m_Renderer.DrawSelectedBone(targetEntity, m_sSelectedBoneName, m_API);

		m_GizmoController.OnCameraDistanceChange(m_fCameraTargetDistance, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RedrawOverlay()
	{
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!targetEntity) return;

		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if (!skeletonInfo) return;

		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		m_LastBoneDisplaySettings = m_ParentTool.GetNewDisplaySttings();

		if (m_sSelectedBoneName.IsEmpty())
			m_Renderer.DrawAllBones(targetEntity, skeletonInfo, m_sHoveredBoneName, camPos, m_LastBoneDisplaySettings, m_API);
		else
			m_Renderer.DrawSelectedBone(targetEntity, m_sSelectedBoneName, m_API);
	}

	//-----------------------------------------------------------------------
	protected void CheckBoneHover(float x, float y)
	{
		if (!m_sSelectedBoneName.IsEmpty()) return;

		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_ParentTool.GetCurrentTargetEntity(), m_API);
		if (hitBoneName == m_sHoveredBoneName) return;

		m_sHoveredBoneName = hitBoneName;
		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		m_Renderer.RefreshSphereSizes(m_sHoveredBoneName, camPos, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RefreshDisplaySettings()
	{
		//TODO: We set settings here but create them new again in RedrawOverlay
		DAB_BoneDisplaySettings newSettings = m_ParentTool.GetNewDisplaySttings();
		if (newSettings.IsSame(m_LastBoneDisplaySettings)) return;
		RedrawOverlay();
	}

	//-----------------------------------------------------------------------
	protected DAB_SkeletonInfo GetCurrentSkeletonInfo()
	{
		if (m_sCurrentSkeleton.IsEmpty())
		{
			Print("DAB_BoneManipulatorTool.GetCurrentSkeletonInfo: skeleton key is empty.", LogLevel.ERROR);
			return null;
		}

		DAB_SkeletonInfo info;
		if (!m_CachedSkeletons.Find(m_sCurrentSkeleton, info))
		{
			Print("DAB_BoneManipulatorTool.GetCurrentSkeletonInfo: no entry for key '" + m_sCurrentSkeleton + "'.", LogLevel.ERROR);
			return null;
		}

		return info;
	}
}