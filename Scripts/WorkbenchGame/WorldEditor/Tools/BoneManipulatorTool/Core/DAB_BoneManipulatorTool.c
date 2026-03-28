[WorkbenchToolAttribute(
	name: "Bone Manipulator",
	description: "Direct skeleton posing via viewport gizmos",
	awesomeFontCode: 0xf5d7
)]
//! Workbench tool for posing individual bones via viewport gizmos.
//! Select a bone by clicking its sphere; manipulate with rotation (U),
//! position (I) or scale (O) gizmos. Press J to reset the selected bone.
class DAB_BoneManipulatorTool : WorldEditorTool
{

	// ── Tool attributes ────────────────────────────────────────────────────
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide volume bones from the overlay", category: "Display")]
	protected bool m_bHideVolumeBones;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide the camera bone from the overlay", category: "Display")]
	protected bool m_bHideCameraBone;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Hide lines connecting bones to their parents", category: "Display")]
	protected bool m_bHideBoneConnections;
	
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select an existing pose modification to load and edit",
	    params: "conf DAB_PoseModification",
	    category: "Current Pose Config"
	)]
	protected ResourceName m_sWorkingConfig;
	
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select existing pose modifications to preview while editing. Current config overwrites these, which is not guranteed in the timeline.",
	    params: "conf DAB_PoseModification",
	    category: "Current Pose Config"
	)]
	protected ref array<ResourceName> m_aPreviewConfigs;
	
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select the folder you are currently working in. New configs will be created there!",
	    params: "FileNameFormat=absolute folders",
	    category: "New Config Creation"
	)]
	protected ResourceName m_sWorkingConfigFolder;
	
	// ── Runtime state ──────────────────────────────────────────────────────
	protected IEntity       m_TargetEntity;
	protected IEntitySource m_TargetEntitySource;

	protected ref DAB_BoneOverlayRenderer m_Renderer;
	protected ref DAB_GizmoController     m_gizmoController;

	protected string m_sSelectedBoneName = "";
	protected string m_sHoveredBoneName  = "";

	protected ref map<string, ref DAB_SkeletonInfo> m_CachedSkeletons = new map<string, ref DAB_SkeletonInfo>();
	protected string m_sCurrentSkeleton;
	protected ref map<string, ref DAB_BoneTransform> m_ModifiedBones = new map<string, ref DAB_BoneTransform>();

	protected float m_fCameraTargetDistance = 0;
	protected ref DAB_BoneDisplaySettings m_LastBoneDisplaySettings;

	protected SlotManagerComponent m_SlotManager;
	
	// ── Lifecycle ──────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	override void OnActivate()
	{
		m_Renderer        = new DAB_BoneOverlayRenderer();
		m_gizmoController = new DAB_GizmoController(m_API);
		m_gizmoController.GetOnTransformChanged().Insert(this.OnBoneTransformChanged);

		RefreshTargetEntity();
		RedrawOverlay();
	}

	//-----------------------------------------------------------------------
	override void OnDeActivate()
	{
		m_gizmoController.Clear(m_API);
		m_Renderer.Clear();
		m_sSelectedBoneName = "";
	}

	//-----------------------------------------------------------------------
	override void OnAfterLoadWorld()
	{
		RefreshTargetEntity();
		RedrawOverlay();
	}

	//-----------------------------------------------------------------------
	protected override void OnEnterEvent()
	{
		RefreshDisplaySettings();
	}
	/**/
	// ── Tool Interaction ───────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	[ButtonAttribute("Create New Config")]
	void CreateNewConfig()
	{
	    if (!m_sWorkingConfig.IsEmpty())
	    {
	        Workbench.Dialog(
	            "Working Config Already Set",
	            "You already have a working config selected:\n" + m_sWorkingConfig +
	            "\n\nClear the 'Current Pose Config' field first before creating a new one."
	        );
	        return;
	    }
		
		DAB_CreateNewManipulationDialog creationDialog = new DAB_CreateNewManipulationDialog(m_sWorkingConfigFolder);
		bool didConfirmCreation = Workbench.ScriptDialog("Create new Config", "Please select a folder and name for the new config file.", creationDialog);
		
		if(!didConfirmCreation) return;
		
		string configName = creationDialog.GetNewConfigName();
		m_sWorkingConfigFolder = creationDialog.GetWorkingConfigFolder();
		UpdatePropertyPanel();
		
	    if (configName.IsEmpty())
	    {
	        Workbench.Dialog("Missing Name", "Please enter a name for the new config.");
	        return;
	    }
		
		if (m_sWorkingConfigFolder.IsEmpty())
	    {
	        Workbench.Dialog("Missing Folder", "Please select a destination folder first.");
	        return;
	    }
	
	    // Build the resource-style path (used as ResourceName after saving)
	    string resourcePath = m_sWorkingConfigFolder + "/" + configName + ".conf";
	
	    // Check if it already exists as a resource
	    if (FileIO.FileExists(resourcePath))
	    {
	        Workbench.Dialog(
	            "File Already Exists",
	            "A config named '" + configName + ".conf' already exists in that folder."
	        );
	        return;
	    }
	
	    string strippedPath = resourcePath;
		int guidEnd = resourcePath.IndexOf(DAB_BugWorkaround.CLOSED_BRACKET);
		
		if (guidEnd >= 0)
		    strippedPath = resourcePath.Substring(guidEnd + 1, resourcePath.Length() - guidEnd - 1);
		
		string absolutePath;
		if (!Workbench.GetAbsolutePath(strippedPath, absolutePath, false))
		{
		    Workbench.Dialog(
		        "Path Error",
		        "Could not resolve the folder to an absolute path.\nMake sure the folder is inside your mod."
		    );
		    return;
		}
	
	    // Write the empty conf to disk
	    DAB_PoseModification emptyMod = new DAB_PoseModification();
	    emptyMod.m_sName = configName;
	
	    Resource holder = BaseContainerTools.CreateContainerFromInstance(emptyMod);
	    if (!holder)
	    {
	        Workbench.Dialog("Error", "Failed to create container from pose modification.");
	        return;
	    }
	
	    bool saved = BaseContainerTools.SaveContainer(holder.GetResource().ToBaseContainer(), "", absolutePath);
	    if (!saved)
	    {
	        Workbench.Dialog(
	            "Save Failed",
	            "Could not write to:\n" + absolutePath +
	            "\n\nWorkbench may prompt for file system permission — check for a pending dialog."
	        );
	        return;
	    }
		
		ResourceManager rm = Workbench.GetModule(ResourceManager);
		if (!rm)
		{
		    Workbench.Dialog("Error", "Could not get ResourceManager module.");
		    return;
		}
		
		rm.RegisterResourceFile(absolutePath, false);
		bool fileReady = rm.WaitForFile(absolutePath, 2000);
		if(!fileReady) Print("File was created, but was not ready in time. This MIGHT cause issues", LogLevel.WARNING);
		
	    m_sWorkingConfig = Workbench.GetResourceName(absolutePath);
	    UpdatePropertyPanel();
		
		Workbench.OpenModule(WorldEditor);
	    Print("DAB_BoneManipulatorTool: created new config at " + absolutePath, LogLevel.NORMAL);
	}
	
	[ButtonAttribute("Copy To Scene")]
	void CopyToCinematicScene()
	{
		if(!m_TargetEntity)
		{
	        Workbench.Dialog("No Entity", "Select a entity first.");
	        return;
	    }
		
		if (m_sWorkingConfig.IsEmpty())
	    {
	        Workbench.Dialog("No Config", "Select a working config first.");
	        return;
	    }

		map<string, CinematicEntity> currentScenes = RefreshCurrentScenes();
		
		if (currentScenes.IsEmpty())
	    {
	        Workbench.Dialog("No Scenes Found", "No cinematic scenes exist in this world.");
	        return;
	    }
	
	    array<string> sceneNames = {};
		for (MapIterator it = currentScenes.Begin(); it != currentScenes.End(); it = currentScenes.Next(it))
		    sceneNames.Insert(currentScenes.GetIteratorKey(it));
	
	    array<int> selectedIndices = {};
	    int result = m_API.ShowItemListDialog(
	        "Select Cinematic Scene",
	        "Choose which scene to add '" + m_sWorkingConfig + "' to:",
	        400,
	        300,
	        sceneNames,
	        selectedIndices,
	        0
	    );
	
		bool didConfirm = ShowConfirmDialog("Copy to Scene(s)", "You are going to copy the ACTIVE manipilation to " + selectedIndices.Count() + " scenes. Are you sure?");
		Print("result: " + result);
		Print("didConfirm: " + didConfirm);
		
	    if (selectedIndices.IsEmpty())
	    {
			Print("Did not select anything! Nothing was copied!");
			return;
		}
		
		string currentEntityName = m_TargetEntity.GetName();
	
		foreach (int sceneIndex : selectedIndices)
		{
		    string key = sceneNames[sceneIndex];
		    if (key.IsEmpty()) continue;
		
		    CinematicEntity sceneEntity;
		    if (!currentScenes.Find(key, sceneEntity)) continue;
		    if (!sceneEntity) continue;
		
		    BaseContainer scene = DAB_CinematicsHelper.GetCinematicScene(sceneEntity, m_API);
		    if (!scene) continue;
			
		    if (DAB_CinematicsHelper.HasSlotBoneTrackForEntity(scene, currentEntityName))
		    {
		        bool shouldContinue = ShowConfirmDialog(
		            "SlotBoneAnimationCinematicTrack Detected",
		            "The entity '" + currentEntityName + "' has a SlotBoneAnimationCinematicTrack in scene '" + key + "'.\n\n" +
		            "This may conflict with pose modifications. Do you want to continue anyway?"
		        );
		        if (!shouldContinue) continue;
		    }
		
		    int ownedPoseTrackCount = DAB_CinematicsHelper.CountOwnedPoseTracks(scene, currentEntityName);
		    if (ownedPoseTrackCount > 1)
		    {
		        Workbench.Dialog(
		            "Too Many Pose Tracks",
		            "The entity '" + currentEntityName + "' has " + ownedPoseTrackCount + " DAB Pose Manipulation Tracks assigned.\n\n" +
		            "Only one track per entity is supported. Please remove the extra " + (ownedPoseTrackCount - 1) + " track(s) from the cinematic scene before continuing."
		        );
		        return;
		    }
		
		    array<ResourceName> configs = {};
		    foreach (ResourceName previewConfig : m_aPreviewConfigs)
		    {
		        if (!previewConfig.IsEmpty())
		            configs.Insert(previewConfig);
		    }
		    configs.Insert(m_sWorkingConfig);
		
		    bool success;
		    IEntitySource sceneEntitySource = m_API.EntityToSource(sceneEntity);

			Print("ownedPoseTrackCount: " + ownedPoseTrackCount);
			if (ownedPoseTrackCount == 0)
			    success = DAB_CinematicsHelper.TryAddPoseTrackToScene(sceneEntitySource, currentEntityName, configs, m_API);
			else
			    success = DAB_CinematicsHelper.TryUpdatePoseTrackConfigs(sceneEntitySource, currentEntityName, configs, m_API);
		
		    if (!success)
		        Workbench.Dialog("Failed", "Could not update the pose track in scene '" + key + "'.");
		    else
		        Print("DAB_BoneManipulatorTool: successfully updated scene '" + key + "'.", LogLevel.NORMAL);
		}
	}
	
	
	private void TestData()
	{
		string resourcePathString = string.Empty;
		resourcePathString = m_sWorkingConfig;
		Print("As string: " + resourcePathString);
		
		Resource holder = BaseContainerTools.LoadContainer(resourcePathString);
		if (!holder || !holder.IsValid())
		{
		    Print("Failed to load config: " + m_sWorkingConfig);
		    return;
		}
		
		DAB_PoseModification poseData = DAB_PoseModification.Cast(
		    BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer())
		);
		
		if (!poseData)
		{
		    Print("Failed to cast to DAB_PoseModification");
		    return;
		}
		
		Print("Loaded modification name: " + poseData.m_sName);
	}

	// ── Input ──────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	override void OnMouseMoveEvent(float x, float y)
	{
		RefreshCameraTargetDistance();
		m_gizmoController.OnMouseMove(x, y, m_API);
		CheckBoneHover(x, y);
	}

	//-----------------------------------------------------------------------
	//! Also selects a bone on left-click when none is currently active.
	override void OnMousePressEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		m_gizmoController.OnMousePress(x, y, buttons, m_API);

		if (!(buttons & WETMouseButtonFlag.LEFT)) return;
		if (!m_sSelectedBoneName.IsEmpty()) return; // gizmo handles input when a bone is active

		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_TargetEntity, m_API);
		if (!hitBoneName.IsEmpty())
			SelectBone(hitBoneName);
	}

	//-----------------------------------------------------------------------
	//! Also refreshes the camera target distance on right-click release.
	override void OnMouseReleaseEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		m_gizmoController.OnMouseRelease(x, y, buttons, m_API);
		if (buttons & WETMouseButtonFlag.RIGHT)
			RefreshCameraTargetDistance();
	}

	//-----------------------------------------------------------------------
	//! Key bindings: U = Rotation, I = Position, O = Scale, J = Reset bone, Escape = Deselect.
	override void OnKeyPressEvent(KeyCode key, bool isAutoRepeat)
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
					m_gizmoController.SwitchMode(DAB_GizmoMode.Rotation, m_API);
				break;

			case KeyCode.KC_I:
				if (!m_sSelectedBoneName.IsEmpty())
					m_gizmoController.SwitchMode(DAB_GizmoMode.Position, m_API);
				break;

			case KeyCode.KC_O:
				if (!m_sSelectedBoneName.IsEmpty())
					m_gizmoController.SwitchMode(DAB_GizmoMode.Scale, m_API);
				break;

			case KeyCode.KC_J:
				if (!m_sSelectedBoneName.IsEmpty())
					ResetBone(m_sSelectedBoneName);
				break;
		}

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
		if (!m_TargetEntity)
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

		TNodeId boneId = DAB_BoneHelper.GetBoneId(m_TargetEntity, boneName);
		vector rotRad = transform.m_vRotationOffset * Math.DEG2RAD;
		vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);

		Animation anim = GetSlotDependentAnim(boneName);

		vector entityWorld[4];
		m_TargetEntity.GetTransform(entityWorld);

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

		anim.SetBone(m_TargetEntity, boneId, rotRadCorrected, localOff, transform.m_fScale);
		m_TargetEntity.Update();
		m_Renderer.DrawSelectedBone(m_TargetEntity, boneName, m_API);
	}

	//-----------------------------------------------------------------------
	// Returns the Animation component that owns boneName, preferring slot entities
	// so that bones on attached equipment are manipulated correctly.
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

	//-----------------------------------------------------------------------
	protected void SelectBone(string boneName)
	{
		m_sSelectedBoneName = boneName;

		DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
		if (!transform)
			transform = CreateNewTransform(boneName);

		m_gizmoController.Attach(m_TargetEntity, transform, m_API, m_fCameraTargetDistance);
		RedrawOverlay();
	}

	//-----------------------------------------------------------------------
	protected DAB_BoneTransform CreateNewTransform(string boneName)
	{
		vector bonePosition;
		if (!DAB_BoneHelper.TryGetBonePosition(m_TargetEntity, boneName, bonePosition))
		{
			Print("DAB_BoneManipulatorTool.CreateNewTransform: could not get position for '" + boneName + "'.", LogLevel.ERROR);
			return null;
		}

		TNodeId boneId = DAB_BoneHelper.GetBoneId(m_TargetEntity, boneName);
		vector  boneRotation;
		if (!DAB_BoneHelper.TryGetBoneLocalRotation(m_TargetEntity, boneId, boneRotation))
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
		m_gizmoController.Clear(m_API);
		if (m_TargetEntitySource)
			m_API.SetEntitySelection(m_TargetEntitySource);
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
		m_gizmoController.ResetAccumulatedTransform(m_API);
		m_Renderer.DrawSelectedBone(m_TargetEntity, boneName, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RefreshCameraTargetDistance()
	{
		if (!m_TargetEntity) return;

		vector camPos   = DAB_Helper.GetCameraPosition(m_API);
		float  distance = vector.Distance(camPos, m_TargetEntity.GetOrigin());

		if (DAB_Helper.AreFloatsEqual(distance, m_fCameraTargetDistance, DAB_VisConfig.CAMERA_POLLING_DISTANCE_EPSILON))
			return;

		m_fCameraTargetDistance = distance;

		if (m_sSelectedBoneName.IsEmpty())
			m_Renderer.RefreshSphereSizes(m_sHoveredBoneName, camPos, m_API);
		else
			m_Renderer.DrawSelectedBone(m_TargetEntity, m_sSelectedBoneName, m_API);

		m_gizmoController.OnCameraDistanceChange(m_fCameraTargetDistance, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RefreshTargetEntity()
	{
		m_sCurrentSkeleton = "";

		m_TargetEntitySource = m_API.GetSelectedEntity(0);
		if (m_TargetEntitySource)
			m_TargetEntity = m_API.SourceToEntity(m_TargetEntitySource);

		if (!m_TargetEntity)
		{
			Print("DAB_BoneManipulatorTool.RefreshTargetEntity: no entity selected.", LogLevel.NORMAL);
			return;
		}

		Animation anim = m_TargetEntity.GetAnimation();
		if (!anim)
		{
			Print("DAB_BoneManipulatorTool.RefreshTargetEntity: entity has no Animation component.", LogLevel.ERROR);
			return;
		}

		string skeletonKey = DAB_SkeletonInfo.ComputeSkeletonKey(m_TargetEntity);
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
			map<string, float>  boneParentDistances = DAB_BoneHelper.ComputeBoneParentDistances(m_TargetEntity, boneParents);
			m_CachedSkeletons.Set(skeletonKey, new DAB_SkeletonInfo(skeletonKey, boneNames, boneParents, boneParentDistances));
		}
		m_sCurrentSkeleton = skeletonKey;
		
		m_SlotManager = SlotManagerComponent.Cast(m_TargetEntity.FindComponent(SlotManagerComponent));
		
		RefreshCameraTargetDistance();
	}

	//-----------------------------------------------------------------------
	protected void RedrawOverlay()
	{
		if (!m_TargetEntity) return;

		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if (!skeletonInfo) return;

		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		m_LastBoneDisplaySettings = new DAB_BoneDisplaySettings(m_bHideVolumeBones, m_bHideCameraBone, m_bHideBoneConnections);

		if (m_sSelectedBoneName.IsEmpty())
			m_Renderer.DrawAllBones(m_TargetEntity, skeletonInfo, m_sHoveredBoneName, camPos, m_LastBoneDisplaySettings, m_API);
		else
			m_Renderer.DrawSelectedBone(m_TargetEntity, m_sSelectedBoneName, m_API);
	}

	//-----------------------------------------------------------------------
	protected void CheckBoneHover(float x, float y)
	{
		if (!m_sSelectedBoneName.IsEmpty()) return;

		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_TargetEntity, m_API);
		if (hitBoneName == m_sHoveredBoneName) return;

		m_sHoveredBoneName = hitBoneName;
		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		m_Renderer.RefreshSphereSizes(m_sHoveredBoneName, camPos, m_API);
	}

	//-----------------------------------------------------------------------
	protected void RefreshDisplaySettings()
	{
		DAB_BoneDisplaySettings newSettings = new DAB_BoneDisplaySettings(m_bHideVolumeBones, m_bHideCameraBone, m_bHideBoneConnections);
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
	protected map<string, CinematicEntity> RefreshCurrentScenes()
	{
		map<string, CinematicEntity> currentScenes = new map<string, CinematicEntity>();

		int entityCount = m_API.GetEditorEntityCount();
		for(int i = 0; i < entityCount; i++)
		{
			IEntitySource candidate = m_API.GetEditorEntity(i);
			if(!candidate) continue;
			
			IEntity entity = m_API.SourceToEntity(candidate);
			if(!entity) continue;

			CinematicEntity sceneEntity = CinematicEntity.Cast(entity);
			if(!sceneEntity) continue;
			
			string sceneName = entity.GetName();
			if (sceneName.IsEmpty())
			{
				Workbench.Dialog("Name Missing!", "Please make sure to name all your cinematic scenes!");
				sceneName = m_API.GetEntityNiceName(candidate);
			}
			    
			currentScenes.Set(sceneName, sceneEntity);
		}
		
		return currentScenes;
	}
	
	//-----------------------------------------------------------------------
	protected bool ShowConfirmDialog(string title, string message)
	{
	    DAB_ConfirmDialog dialog = new DAB_ConfirmDialog();
	    return Workbench.ScriptDialog(title, message, dialog);
	}
}