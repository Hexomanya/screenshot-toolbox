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
	protected ref set<string> m_DirtyBones = new set<string>();

	protected float m_fCameraTargetDistance = 0;
	protected ref DAB_BoneDisplaySettings m_LastBoneDisplaySettings;
	
	protected SlotManagerComponent m_SlotManager;
	
	protected ResourceName m_sLastLoadedConfig = "";
	
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
		for (MapIterator it = m_ModifiedBones.Begin(); it != m_ModifiedBones.End(); it = m_ModifiedBones.Next(it))
		{
			string boneName = m_ModifiedBones.GetIteratorKey(it);
			ResetBone(boneName);
		}
		
		m_ModifiedBones.Clear();
		
		m_GizmoController.Clear(m_API);
		m_Renderer.Clear();
		m_sSelectedBoneName = "";
		
		DAB_CinematicsHelper.RefreshAllScenes(m_API);
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
		LoadAndApplyWorkingConfig();
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
		
		if(m_ParentTool.GetShouldAutoSave()) SaveDirtyBones();
		
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
		else {
			Print("Loaded saved skeleton");
		}
		
		m_sCurrentSkeleton = skeletonKey;
		m_SlotManager = SlotManagerComponent.Cast(changedEntity.FindComponent(SlotManagerComponent));
		
		RefreshCameraTargetDistance();
		
		LoadAndApplyWorkingConfig(true);
	}
	
	// ── Public Getters ────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	int GetDirtyBoneCount(){ return m_DirtyBones.Count(); }
	
	// ── Private ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected void OnBoneTransformChanged(DAB_BoneTransform changedTransform)
	{
		string boneName = changedTransform.GetBoneName();
		m_ModifiedBones.Set(boneName, changedTransform);
		m_DirtyBones.Insert(boneName);
		
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
	    {
	        transform = CreateNewTransform(boneName);
	    }
	    else
	    {
	        vector currentPos;
	        if (DAB_BoneHelper.TryGetBonePosition(m_ParentTool.GetCurrentTargetEntity(), boneName, currentPos))
	            transform.SetOriginalPosition(currentPos - transform.m_vPositionOffset);
	    }
	
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
		
		Print("Resetting current bone...");
		transform.m_vPositionOffset = vector.Zero;
		transform.m_vRotationOffset = vector.Zero;
		transform.m_fScale          = 1.0;

		m_DirtyBones.Insert(boneName);
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
		m_LastBoneDisplaySettings = m_ParentTool.GetNewDisplaySettings();

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
		DAB_BoneDisplaySettings newSettings = m_ParentTool.GetNewDisplaySettings();
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
	
	//-----------------------------------------------------------------------
	bool SaveDirtyBones()
	{
	    if(m_DirtyBones.Count() == 0) return false;
	    
	    ResourceName configPath = m_ParentTool.GetWorkingConfig();
	    if (configPath.IsEmpty()) 
		{
			Workbench.Dialog("No config", "There is no working config set to safe to! Save was aported!");
			return false;
		}
	
	    Resource configResource = BaseContainerTools.LoadContainer(configPath);
	    if (!configResource) return false;
	    BaseContainer config = configResource.GetResource().ToBaseContainer();
	    
	    DAB_PoseModification poseInstance = new DAB_PoseModification();
	    BaseContainerTools.WriteToInstance(poseInstance, config);
	
	    // [MANUAL ARRAY LOAD - Required because WriteToInstance is shallow for arrays]
	    BaseContainerList modificationsArr = config.GetObjectArray("m_aBoneModifications");
	    if (modificationsArr)
	    {
	        poseInstance.m_aBoneModifications.Clear();
	        for (int i = 0; i < modificationsArr.Count(); i++)
	        {
	            BaseContainer entry = modificationsArr.Get(i);
	            DAB_BoneModification mod = new DAB_BoneModification();
	            entry.Get("m_sBoneName", mod.m_sBoneName);
	            entry.Get("m_vRotationOffset", mod.m_vRotationOffset);
	            entry.Get("m_vPositionOffset", mod.m_vPositionOffset);
	            entry.Get("m_fScale", mod.m_fScale);
	            poseInstance.m_aBoneModifications.Insert(mod);
	        }
	    }
	
	    map<string, ref DAB_BoneModification> modificationMap = poseInstance.GetBoneModificationsAsMap();
	        
	    foreach(string boneName : m_DirtyBones)
	    {
	        DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
	        if(!transform) continue;
	        
	        DAB_BoneModification mod = modificationMap.Get(boneName);
	        if(!mod)
	        {
	            mod = new DAB_BoneModification();
	            mod.m_sBoneName = boneName;
	            poseInstance.m_aBoneModifications.Insert(mod);
	        }
			
	        mod.m_vRotationOffset = transform.m_vRotationOffset;
	        mod.m_vPositionOffset = transform.m_vPositionOffset;
	        mod.m_fScale = transform.m_fScale;
	    }
	
	    BaseContainerTools.ReadFromInstance(poseInstance, config);
	    bool success = BaseContainerTools.SaveContainer(config, configPath);
	    
	    if (success)
	    {
	        m_API.BeginEntityAction("Bulk Update");
	        array<ref ContainerIdPathEntry> path = {}; 
	        m_API.SetVariableValue(config, null, "m_sName", poseInstance.m_sName); //TODO: Remove name but make sure it still save
	        m_API.EndEntityAction();
	    }
	
	    m_DirtyBones.Clear();
	    return success;
	}
	
	//-----------------------------------------------------------------------
	void LoadAndApplyWorkingConfig(bool forceApply = false)
	{
		ResourceName currentConfig = m_ParentTool.GetWorkingConfig();

		// If nothing changed and we aren't forcing an update (like an entity swap), do nothing
		if (!forceApply && currentConfig == m_sLastLoadedConfig)
			return;

		m_sLastLoadedConfig = currentConfig;

		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!targetEntity) return;

		// 1. Reset currently modified bones back to default on the entity
		foreach (string boneName, DAB_BoneTransform transform : m_ModifiedBones)
		{
			transform.m_vPositionOffset = vector.Zero;
			transform.m_vRotationOffset = vector.Zero;
			transform.m_fScale          = 1.0;
			RefreshBone(boneName);
		}
		
		m_ModifiedBones.Clear();
		m_DirtyBones.Clear();
		m_GizmoController.Clear(m_API);

		// 2. Load new config if valid
		if (currentConfig.IsEmpty())
		{
			RedrawOverlay();
			return;
		}

		Resource configResource = BaseContainerTools.LoadContainer(currentConfig);
		if (!configResource)
		{
			Print("DAB_EditorController.LoadAndApplyWorkingConfig: Could not load config " + currentConfig, LogLevel.ERROR);
			RedrawOverlay();
			return;
		}

		// 3. Parse modifications and apply them
		BaseContainer config = configResource.GetResource().ToBaseContainer();
		BaseContainerList modificationsArr = config.GetObjectArray("m_aBoneModifications");

		if (modificationsArr)
		{
			for (int i = 0; i < modificationsArr.Count(); i++)
			{
				BaseContainer entry = modificationsArr.Get(i);
				string boneName;
				vector rotOffset, posOffset;
				float scale = 1.0;

				entry.Get("m_sBoneName", boneName);
				entry.Get("m_vRotationOffset", rotOffset);
				entry.Get("m_vPositionOffset", posOffset);
				entry.Get("m_fScale", scale);

				// Create transform to capture the default base/original orientation
				DAB_BoneTransform newTransform = CreateNewTransform(boneName);
				if (newTransform)
				{
					newTransform.m_vRotationOffset = rotOffset;
					newTransform.m_vPositionOffset = posOffset;
					newTransform.m_fScale = scale;

					m_ModifiedBones.Set(boneName, newTransform);
					RefreshBone(boneName);
				}
			}
		}

		RedrawOverlay();
	}
}