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

	protected ref DAB_BoneDisplaySettings m_LastBoneDisplaySettings;
	
	protected ResourceName m_sLastLoadedConfig = "";
	
	protected bool m_bEditAllowed;
	protected ref DebugTextScreenSpace m_InvalidSetupText;
	
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
		m_GizmoController = null;
		m_ParentTool = null;
		m_API = null;
	}
	
	// ── Lifecycle ──────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void OnActivate(WorldEditorAPI api)
	{
		// TODO: Find better way to handle api
		m_API = api; // Needed so we do not crash after reloading scripts. 
		
		CheckValidSetup();
		RedrawOverlay();
	}
	
	//-----------------------------------------------------------------------
	void OnDeActivate()
	{
		foreach(string compoundBoneName, DAB_BoneTransform transform : m_ModifiedBones)
		{
			ResetBone(compoundBoneName);
		}
		
		m_ModifiedBones.Clear();
		m_Renderer.Clear();
		m_DirtyBones.Clear();
		m_sSelectedBoneName = "";
		if(m_GizmoController) m_GizmoController.Clear(); // In rare cases this could be null already (e.g. reloading WB Scripts after error)
		m_bEditAllowed = false;
		m_InvalidSetupText = null;
		
		DAB_CinematicsHelper.RefreshAllScenes(m_API);
	}
	
	//-----------------------------------------------------------------------
	void OnAfterLoadWorld()
	{
		RedrawOverlay();
	}
	
	//-----------------------------------------------------------------------
	void OnBeforeUnloadWorld()
	{
		m_CachedSkeletons.Clear();
	}
	
	// ── Input ──────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void OnEnterEvent()
	{
		CheckValidSetup();
		RefreshDisplaySettings();
		LoadAndApplyWorkingConfig();
		
		if(! m_bEditAllowed) return;
		if (!m_sSelectedBoneName.IsEmpty())
		{
			SelectBone(m_sSelectedBoneName); // Reselect so gizmo updates correctly
		}
		else
		{
			RedrawOverlay();
		}
	}
	
	//-----------------------------------------------------------------------
	void OnMouseMoveEvent(float x, float y)
	{
		if(! m_bEditAllowed) return;
		m_GizmoController.OnMouseMove(x, y, m_API);
		CheckBoneHover(x, y);
	}
	
	//-----------------------------------------------------------------------
	void OnMousePressEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		if(! m_bEditAllowed) return;
		m_GizmoController.OnMousePress(x, y, buttons, m_API);

		if (!(buttons & WETMouseButtonFlag.LEFT)) return;
		if (!m_sSelectedBoneName.IsEmpty()) return; // gizmo handles input when a bone is active
		
		if(!m_ParentTool.GetCurrentTargetEntity())
		{
			Print("Target entity is null! Please close and reopen the tool. If it keeps happening report this!", LogLevel.ERROR);
			return;
		}

		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_API);
		if (!hitBoneName.IsEmpty())
			SelectBone(hitBoneName);
	}
	
	//-----------------------------------------------------------------------
	void OnMouseReleaseEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		if(! m_bEditAllowed) return;
		m_GizmoController.OnMouseRelease(x, y, buttons, m_API);
		
		if(m_ParentTool.GetShouldAutoSave()) SaveDirtyBones();
		
		bool isRightRelease = buttons & WETMouseButtonFlag.RIGHT;
		if (!isRightRelease) return;
			
		// TODO: Entangle this mess
		if (m_sSelectedBoneName.IsEmpty())
		{
			m_Renderer.RefreshSphereSizes(m_sHoveredBoneName, m_API);
		} else {
			m_Renderer.DrawSelectedBone(GetCurrentSkeletonInfo(), m_sSelectedBoneName, m_API);
		}

		m_GizmoController.OnCameraDistanceChange(m_API);
	}
	
	//-----------------------------------------------------------------------
	//! Key bindings: U = Rotation, I = Position, O = Scale, J = Reset bone, Escape = Deselect.
	void OnKeyPressEvent(KeyCode key, bool isAutoRepeat)
	{
		if (isAutoRepeat) return;
		if(! m_bEditAllowed) return;

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
	}
	
	// ── Public ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void OnTargetEntityChanged(IEntity changedEntity)
	{
		m_sCurrentSkeleton = "";
		
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
			Print("Creating new skeleton with key: " + skeletonKey);
			m_CachedSkeletons.Set(skeletonKey, new DAB_SkeletonInfo(skeletonKey, changedEntity));
		} 
		
		m_sCurrentSkeleton = skeletonKey;
		CheckValidSetup();
		
		//RefreshCameraTargetDistance();
		
		LoadAndApplyWorkingConfig(true);
	}
	
	// ── Public Getters ────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	int GetDirtyBoneCount(){ return m_DirtyBones.Count(); }
	
	// ── Private ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected void OnBoneTransformChanged(DAB_BoneTransform changedTransform)
	{
		string compoundBoneName = changedTransform.GetCompoundBoneName();
		m_ModifiedBones.Set(compoundBoneName, changedTransform);
		m_DirtyBones.Insert(compoundBoneName);
		
		RefreshBone(compoundBoneName);
	}

	//-----------------------------------------------------------------------
	// m_vPositionOffset is in world space. SetBone expects a position in the
	// bone's local frame, so the offset is projected onto the bone's ORIGINAL
	// world axes (entity × boneOrig — without accumRotation).
	// Using the current axes would silently shift the projected localOff on
	// every rotation call, making the bone orbit its rest location.
	protected void RefreshBone(string compoundBoneName)
	{
		//TODO: Check for allowed edit? m_bEditAllowed

		DAB_BoneTransform transform = m_ModifiedBones.Get(compoundBoneName);
		if (!transform)
		{
			Print("DAB_BoneManipulatorTool.RefreshBone: no stored transform for '" + compoundBoneName + "'.", LogLevel.ERROR);
			return;
		}
		
		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if(!skeletonInfo) return;
		
		IEntity slotEntity = skeletonInfo.GetSlotEntity(compoundBoneName);
		if(!slotEntity) return;
		
		string simpleBoneName = skeletonInfo.GetSimpleBoneName(compoundBoneName);
		if(simpleBoneName.IsEmpty()) return;

		Animation anim;
		TNodeId boneId = DAB_SkeletonInfo.GetBoneIndex(slotEntity, simpleBoneName, anim);
		if (!anim) return;

		vector rotRad = transform.m_vRotationOffset * Math.DEG2RAD;
		vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);

		vector entityWorld[4];
		slotEntity.GetTransform(entityWorld);

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

		anim.SetBone(slotEntity, boneId, rotRadCorrected, localOff, transform.m_fScale);
		slotEntity.Update();
		m_Renderer.DrawSelectedBone(GetCurrentSkeletonInfo(), compoundBoneName, m_API);
	}
	
	//-----------------------------------------------------------------------
	protected void SelectBone(string compoundBoneName)
	{
	    m_sSelectedBoneName = compoundBoneName;
	
	    DAB_BoneTransform transform = m_ModifiedBones.Get(compoundBoneName);
	    if (!transform)
	        transform = CreateNewTransform(compoundBoneName); // We do not safe it here. It gets saved once we actually modify it.
		
		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if(!skeletonInfo) return;
		
		IEntity slotEntity = skeletonInfo.GetSlotEntity(compoundBoneName);
		if(!slotEntity) return;
		    
	    m_GizmoController.Attach(slotEntity, transform);
	    RedrawOverlay();
	}
	
	//-----------------------------------------------------------------------
	protected DAB_BoneTransform CreateNewTransform(string compoundBoneName)
	{
		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if(!skeletonInfo) return null;
		
		DAB_BoneRecord record = skeletonInfo.GetBoneRecord(compoundBoneName);
		if(!record) return null;
		
		Print("New transfrom?");
		
		IEntity slotEntity = record.GetSlotEntity();
		if(!slotEntity) return null;
		
		string simpleBoneName = record.GetSimpleBoneName();
		if(simpleBoneName.IsEmpty()) return null;
		
		Animation anim;
		TNodeId boneId = DAB_SkeletonInfo.GetBoneIndex(slotEntity, simpleBoneName, anim);
		
		if (anim && boneId != -1)
		{
			anim.SetBone(slotEntity, boneId, vector.Zero, vector.Zero, 1.0);
			slotEntity.Update();
		}

		vector bonePosition;
		if (!DAB_BoneHelper.TryGetBonePosition(slotEntity, simpleBoneName, bonePosition))
		{
			Print("DAB_BoneManipulatorTool.CreateNewTransform: could not get position for '" + simpleBoneName + "'.", LogLevel.ERROR);
			return null;
		}

		vector boneRotation;
		if (!DAB_BoneHelper.TryGetBoneLocalRotation(slotEntity, boneId, boneRotation))
		{
			Print("DAB_BoneManipulatorTool.CreateNewTransform: could not get rotation for '" + simpleBoneName + "'.", LogLevel.ERROR);
			return null;
		}

		return new DAB_BoneTransform(compoundBoneName, bonePosition, boneRotation);
	}

	//-----------------------------------------------------------------------
	protected void DeselectBone()
	{
		m_sSelectedBoneName = "";
		m_GizmoController.Clear();
		IEntitySource targetEntitySource = m_ParentTool.GetCurrentTargetEntitySource();
		if (targetEntitySource)
			m_API.SetEntitySelection(targetEntitySource);
	}

	//-----------------------------------------------------------------------
	protected void ResetBone(string compoundBoneName)
	{
		DAB_BoneTransform transform = m_ModifiedBones.Get(compoundBoneName);
		if (!transform) return;
	
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!targetEntity) return;
	
		transform.m_vPositionOffset = vector.Zero;
		transform.m_vRotationOffset = vector.Zero;
		transform.m_fScale = 1.0;

		m_DirtyBones.Insert(compoundBoneName);
		RefreshBone(compoundBoneName);
		m_GizmoController.ResetAccumulatedTransform(m_API);
		m_Renderer.DrawSelectedBone(GetCurrentSkeletonInfo(), compoundBoneName, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RedrawOverlay()
	{
		if(! m_bEditAllowed) return;
		
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!targetEntity) return;

		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if (!skeletonInfo) return;

		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		m_LastBoneDisplaySettings = m_ParentTool.GetNewDisplaySettings();

		if (m_sSelectedBoneName.IsEmpty())
			m_Renderer.DrawAllBones(skeletonInfo, m_sHoveredBoneName, camPos, m_LastBoneDisplaySettings, m_API);
		else
			m_Renderer.DrawSelectedBone(skeletonInfo, m_sSelectedBoneName, m_API);
	}

	//-----------------------------------------------------------------------
	protected void CheckBoneHover(float x, float y)
	{
		if (!m_sSelectedBoneName.IsEmpty()) return;

		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_API);
		if (hitBoneName == m_sHoveredBoneName) return;

		m_sHoveredBoneName = hitBoneName;
		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		m_Renderer.RefreshSphereSizes(m_sHoveredBoneName, m_API);
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
	    
		DAB_PoseModificationComponent poseComponent = m_ParentTool.GetTargetComponent();
		if(!poseComponent)
		{
			Workbench.Dialog("No component", "System could not get a pose modification component from the target. Save was aported!");
			return false;
		}
		
	    ResourceName configPath = poseComponent.GetWorkingModificationConfig();
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
	
		// TODO: Is there not a better way for this?
	    // MANUAL ARRAY LOAD - Required because WriteToInstance is shallow for arrays
	    BaseContainerList modificationsArr = config.GetObjectArray("m_aBoneModifications");
	    if (modificationsArr)
	    {
	        poseInstance.m_aBoneModifications.Clear();
	        for (int i = 0; i < modificationsArr.Count(); i++)
	        {
	            BaseContainer entry = modificationsArr.Get(i);
	            DAB_BoneModification mod = new DAB_BoneModification();
	            entry.Get("m_sBoneName", mod.m_sBoneName);
				entry.Get("m_aSlotNames", mod.m_sBoneName);
	            entry.Get("m_vRotationOffset", mod.m_vRotationOffset);
	            entry.Get("m_vPositionOffset", mod.m_vPositionOffset);
	            entry.Get("m_fScale", mod.m_fScale);
	            poseInstance.m_aBoneModifications.Insert(mod);
	        }
	    }
	
	    map<string, ref DAB_BoneModification> modificationMap = poseInstance.GetBoneModificationsAsMap();
		
		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if(!skeletonInfo) return false;
		
	    foreach(string compoundBoneName : m_DirtyBones)
	    {
	        DAB_BoneTransform transform = m_ModifiedBones.Get(compoundBoneName);
	        if(!transform) continue;
	        
			string simpleBoneName = skeletonInfo.GetSimpleBoneName(compoundBoneName);
			
	        DAB_BoneModification mod = modificationMap.Get(simpleBoneName);
	        if(!mod)
	        {
	            mod = new DAB_BoneModification();
	            mod.m_sBoneName = simpleBoneName;
				mod.m_aSlotNames = skeletonInfo.GetSlotNames(compoundBoneName);
	            poseInstance.m_aBoneModifications.Insert(mod);
	        }
			
	        mod.m_vRotationOffset = transform.m_vRotationOffset;
	        mod.m_vPositionOffset = transform.m_vPositionOffset;
	        mod.m_fScale = transform.m_fScale;
	    }
	
	    BaseContainerTools.ReadFromInstance(poseInstance, config);
	    BaseContainerTools.SaveContainer(config, configPath);
		m_API.EndEntityAction();
	
	    m_DirtyBones.Clear();
		m_ParentTool.ReselectTargetEntity(); // We modify the config held by the component so we need to reselect
	    return true;
	}
	
	//-----------------------------------------------------------------------
	void LoadAndApplyWorkingConfig(bool forceApply = false)
	{		
		IEntity targetEntity = m_ParentTool.GetCurrentTargetEntity();
		if (!targetEntity)
		{
			Print("Target is null. Could not load and apply working config", LogLevel.WARNING);
			return;
		}
		
		DAB_PoseModificationComponent poseComponent = m_ParentTool.GetTargetComponent();
		if(!poseComponent)
		{
			Print("System could not get a pose modification component from the target. Loading was aported!", LogLevel.WARNING);
			return;
		}
	
		ResourceName currentConfig = poseComponent.GetWorkingModificationConfig();
	
		if (!forceApply && currentConfig == m_sLastLoadedConfig)
			return;
	
		m_sLastLoadedConfig = currentConfig;
		
		Resource configResource = BaseContainerTools.LoadContainer(currentConfig);
		if (!configResource)
		{
			Print("DAB_EditorController.LoadAndApplyWorkingConfig: Could not load config " + currentConfig, LogLevel.ERROR);
			RedrawOverlay();
			return;
		}
	
		BaseContainer config = configResource.GetResource().ToBaseContainer();
		BaseContainerList modificationsArr = config.GetObjectArray("m_aBoneModifications");
	
		if (!modificationsArr)
		{
			RedrawOverlay();
			return;
		}

		for (int i = 0; i < modificationsArr.Count(); i++)
		{
			BaseContainer entry = modificationsArr.Get(i);
			string simpleBoneName;
			array<string> slotNames;
			vector rotOffset, posOffset;
			float scale = 1.0;

			entry.Get("m_sBoneName", simpleBoneName);
			entry.Get("m_aSlotNames", slotNames);
			entry.Get("m_vRotationOffset", rotOffset);
			entry.Get("m_vPositionOffset", posOffset);
			entry.Get("m_fScale", scale);

			string compoundBoneName = DAB_SkeletonInfo.GetCompoundName(simpleBoneName, slotNames);
			
			// Create transform to capture the default base/original orientation
			DAB_BoneTransform newTransform = CreateNewTransform(compoundBoneName);
			if (!newTransform) continue;
			
			newTransform.m_vRotationOffset = rotOffset;
			newTransform.m_vPositionOffset = posOffset;
			newTransform.m_fScale = scale;

			m_ModifiedBones.Set(compoundBoneName, newTransform);
			RefreshBone(compoundBoneName);
		}

		RedrawOverlay();
	}
	
	protected void CheckValidSetup()
	{
		m_InvalidSetupText = null;
		m_bEditAllowed = false;
		
		if(!m_ParentTool || !m_API)
		{
			Print("DAB_EditorController.CheckValidSetup: Detected stale references. Please reopen the bone manipulator tool. If this keeps happening please report it!", LogLevel.ERROR);
			return;
		}

		IEntity currentTarget = m_ParentTool.GetCurrentTargetEntity();
		if(! currentTarget)
		{
			m_InvalidSetupText = DebugTextScreenSpace.Create(m_API.GetWorld(), "No Target Selected!", DebugTextFlags.FACE_CAMERA, 10, 10, 25, 0xFFFF0000);
			return;
		}
		
		Animation anim = currentTarget.GetAnimation();
		if(!anim)
		{
			m_InvalidSetupText = DebugTextScreenSpace.Create(m_API.GetWorld(), "Target has no animation on it!", DebugTextFlags.FACE_CAMERA, 10, 10, 25, 0xFFFF0000);
			return;
		}
		
		array<string> boneNames = {};
		anim.GetBoneNames(boneNames);
		if(boneNames.Count() <= 0)
		{
			m_InvalidSetupText = DebugTextScreenSpace.Create(m_API.GetWorld(), "Target has no bones to pose!", DebugTextFlags.FACE_CAMERA, 10, 10, 25, 0xFFFF0000);
			return;
		}
		
		DAB_PoseModificationComponent poseComponent = m_ParentTool.GetTargetComponent();
		if(! poseComponent)
		{
			m_InvalidSetupText = DebugTextScreenSpace.Create(m_API.GetWorld(), "Target has no DAB_PoseModificationComponent", DebugTextFlags.FACE_CAMERA, 10, 10, 25, 0xFFFF0000);
			return;
		}
		
		ResourceName poseModification = poseComponent.GetWorkingModificationConfig();
		if(poseModification.IsEmpty())
		{
			m_InvalidSetupText = DebugTextScreenSpace.Create(m_API.GetWorld(), "DAB_PoseModificationComponent has no working config", DebugTextFlags.FACE_CAMERA, 10, 10, 20, 0xFFFF0000);
			return;
		}
		
		m_bEditAllowed = true;
	}
}