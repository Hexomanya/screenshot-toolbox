class DAB_ToolButtonInteractions
{
	// ── Public API ────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	static void CreateComponent(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api)
	{
		if (!parentTool.GetCurrentTargetEntitySource())
		{
			Print("CreateComponent: target entity source is null!", LogLevel.ERROR);
			return;
		}

		if (parentTool.GetTargetComponent())
		{
			Print("CreateComponent: entity already has a DAB_PoseModificationComponent!", LogLevel.WARNING);
			return;
		}

		api.BeginEntityAction();
		api.CreateComponent(parentTool.GetCurrentTargetEntitySource(), "DAB_PoseModificationComponent");
		api.EndEntityAction();

		parentTool.RefreshTargetEntity();
	}

	//-----------------------------------------------------------------------
	static void CreateNewConfig(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api, inout ResourceName workingConfigFolder)
	{
		if (!EnsureComponentExists(parentTool, api))
			return;

		if (!HandleExistingWorkingConfig(parentTool, api))
			return;

		string configName;
		string absolutePath;
		if (!PromptAndValidateNewConfig(workingConfigFolder, configName, absolutePath, parentTool))
			return;

		DAB_PoseModification emptyMod = WriteAndRegisterConfig(absolutePath);
		if (!emptyMod)
			return;

		string formattedResourceName = Workbench.GetResourceName(absolutePath);
		DAB_PoseModificationComponent workingComponent = SafeSetWorkingConfig(parentTool, api, formattedResourceName);
		if (workingComponent)
			workingComponent.SetWorkingModificationData(emptyMod, api);
		else
			Print("CreateNewConfig: could not get refreshed component to write initial mod data!", LogLevel.WARNING);

		Workbench.OpenModule(WorldEditor);
		Print("CreateNewConfig: created new config at " + absolutePath, LogLevel.NORMAL);
	}

	//-----------------------------------------------------------------------
	static void CopyToCinematicScene(ResourceName workingConfig, array<ResourceName> modificationStack, IEntity targetEntity, WorldEditorAPI api)
	{
		if (!targetEntity)
		{
			Workbench.Dialog("No Entity", "Select an entity first.");
			return;
		}

		if (workingConfig.IsEmpty())
		{
			Workbench.Dialog("No Config", "Select a working config first.");
			return;
		}

		map<string, CinematicEntity> currentScenes = DAB_CinematicsHelper.GetCurrentScenes(api);
		if (currentScenes.IsEmpty())
		{
			Workbench.Dialog("No Scenes Found", "No cinematic scenes exist in this world.");
			return;
		}

		array<string> sceneNames = {};
		for (MapIterator it = currentScenes.Begin(); it != currentScenes.End(); it = currentScenes.Next(it))
			sceneNames.Insert(currentScenes.GetIteratorKey(it));

		array<int> selectedIndices = {};
		int result = api.ShowItemListDialog(
			"Select Cinematic Scene",
			"Choose which scene to add '" + workingConfig + "' to:",
			400, 300,
			sceneNames,
			selectedIndices,
			0
		);

		if (result == -1 || selectedIndices.Count() == 0)
		{
			Print("CopyToCinematicScene: cancelled or no scene selected.");
			return;
		}

		if (!ShowConfirmDialog("Copy to Scene(s)", "You are about to copy the active manipulation to " + selectedIndices.Count() + " scene(s). Are you sure?"))
		{
			Print("CopyToCinematicScene: user cancelled.");
			return;
		}

		string currentEntityName = targetEntity.GetName();

		foreach (int sceneIndex : selectedIndices)
		{
			string key = sceneNames[sceneIndex];
			if (key.IsEmpty()) continue;

			CinematicEntity sceneEntity;
			if (!currentScenes.Find(key, sceneEntity) || !sceneEntity) continue;

			BaseContainer scene = DAB_CinematicsHelper.GetCinematicScene(sceneEntity, api);
			if (!scene) continue;

			if (DAB_CinematicsHelper.HasSlotBoneTrackForEntity(scene, currentEntityName))
			{
				if (!ShowConfirmDialog(
					"SlotBoneAnimationCinematicTrack Detected",
					"The entity '" + currentEntityName + "' has a SlotBoneAnimationCinematicTrack in scene '" + key + "'.\n\nThis may conflict with pose modifications. Do you want to continue anyway?"))
					continue;
			}

			int ownedPoseTrackCount = DAB_CinematicsHelper.CountOwnedPoseTracks(scene, currentEntityName);
			if (ownedPoseTrackCount > 1)
			{
				Workbench.Dialog(
					"Too Many Pose Tracks",
					"The entity '" + currentEntityName + "' has " + ownedPoseTrackCount + " DAB Pose Manipulation Tracks assigned.\n\nOnly one track per entity is supported. Please remove the extra " + (ownedPoseTrackCount - 1) + " track(s) before continuing."
				);
				return;
			}

			array<ResourceName> configs = {};
			foreach (ResourceName modification : modificationStack)
			{
				if (!modification.IsEmpty())
					configs.Insert(modification);
			}
			configs.Insert(workingConfig);

			IEntitySource sceneEntitySource = api.EntityToSource(sceneEntity);
			bool success;
			if (ownedPoseTrackCount == 0)
				success = DAB_CinematicsHelper.TryAddPoseTrackToScene(sceneEntitySource, currentEntityName, configs, api);
			else
				success = DAB_CinematicsHelper.TryUpdatePoseTrackConfigs(sceneEntitySource, currentEntityName, configs, api);

			if (!success)
				Workbench.Dialog("Failed", "Could not update the pose track in scene '" + key + "'.");
			else
				Print("CopyToCinematicScene: successfully updated scene '" + key + "'.", LogLevel.NORMAL);
		}
	}

	//-----------------------------------------------------------------------
	static void SaveEdits(DAB_BoneManipulatorTool tool, DAB_EditorController controller, bool askForSave = false)
	{
		if (askForSave && !ShowConfirmDialog("Confirm Save", "Do you want to save unsaved changes?"))
			return;

		int boneCount = controller.GetDirtyBoneCount();
		if (boneCount <= 0)
		{
			Workbench.Dialog("No Edits", "There are no edits to save!");
			return;
		}

		if (!controller.SaveDirtyBones())
		{
			Workbench.Dialog("Save Failed", "The system could not save the changes. Please check the console for errors and report this!");
			return;
		}

		PrintFormat("Saved %1 changed bones to config.", boneCount);
	}

	// ── Private: CreateNewConfig stages ───────────────────────────────────
	//-----------------------------------------------------------------------
	static protected bool EnsureComponentExists(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api)
	{
		if (parentTool.GetTargetComponent())
			return true;

		if (!ShowConfirmDialog(
			"No Component Found",
			"The selected entity does not have a DAB_PoseModificationComponent. Do you want to create one?"))
			return false;

		api.BeginEntityAction();
		api.CreateComponent(parentTool.GetCurrentTargetEntitySource(), "DAB_PoseModificationComponent");
		api.EndEntityAction();
		parentTool.RefreshTargetComponent();

		if (!parentTool.GetTargetComponent())
		{
			Print("EnsureComponentExists: component creation failed!", LogLevel.ERROR);
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------
	static protected bool HandleExistingWorkingConfig(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api)
	{
		DAB_PoseModificationComponent workingComponent = parentTool.GetTargetComponent();
		if (!workingComponent.GetWorkingModificationData())
			return true; // Nothing to handle — slot is already empty.

		// 0 = Cancel, 1 = Overwrite, 2 = Move to List
		int selectedChoice = ShowConfigConfrimOverwriteDialog(
			"Working Modification Already Set",
			"The working modification on the component is already set. Do you want to overwrite it or move it to the pose modification list?"
		);
		
		if (selectedChoice <= 0) return false;

		if (selectedChoice == 2 && !MoveWorkingConfigToList(parentTool, api, workingComponent))
			return false;

		if (!SafeSetWorkingConfig(parentTool, api, ""))
		{
			Print("HandleExistingWorkingConfig: failed to clear working config!", LogLevel.ERROR);
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------
	static protected bool MoveWorkingConfigToList(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api, DAB_PoseModificationComponent workingComponent)
	{
		ResourceName currentConfig = workingComponent.GetWorkingModificationConfig();
		if (currentConfig.IsEmpty())
		{
			Print("MoveWorkingConfigToList: working config is empty, nothing to move.", LogLevel.WARNING);
			return true;
		}

		IEntitySource entitySource = parentTool.GetCurrentTargetEntitySource();
		int componentIndex = FindPoseComponentIndex(entitySource);
		if (componentIndex == -1)
		{
			Print("MoveWorkingConfigToList: could not find component index!", LogLevel.ERROR);
			return false;
		}

		BaseContainer componentContainer = entitySource.GetObjectArray("components").Get(componentIndex);
		array<ResourceName> existingConfigs = {};
		componentContainer.Get("m_aPoseModifications", existingConfigs);

		PrintFormat("Retrieved list with %1 entries", existingConfigs.Count());

		// Build a comma-separated string for SetVariableValue, then append the new config.
		string newList;
		foreach (ResourceName rn : existingConfigs)
		{
			if (rn.IsEmpty()) continue;
			if (!newList.IsEmpty()) newList += ",";
			newList += rn;
		}
		if (!newList.IsEmpty()) newList += ",";
		newList += currentConfig;

		PrintFormat("newList is: %1", newList);
		
		parentTool.ClearTargetComponent();
		api.BeginEditSequence(entitySource);
		api.BeginEntityAction();
		api.SetVariableValue(
			entitySource,
			{ new ContainerIdPathEntry("components", componentIndex) },
			"m_aPoseModifications",
			newList
		);
		api.EndEntityAction();
		api.EndEditSequence(entitySource);

		parentTool.RefreshTargetEntity();
		parentTool.UpdatePropertyPanel();

		return true;
	}

	//-----------------------------------------------------------------------
	static protected bool PromptAndValidateNewConfig(inout ResourceName workingConfigFolder, out string configName, out string absolutePath, DAB_BoneManipulatorTool parentTool)
	{
		DAB_CreateNewManipulationDialog creationDialog = new DAB_CreateNewManipulationDialog(workingConfigFolder);
		if (!Workbench.ScriptDialog("Create New Config", "Please select a folder and name for the new config file.", creationDialog))
			return false;

		configName = creationDialog.GetNewConfigName();
		workingConfigFolder = creationDialog.GetWorkingConfigFolder();
		parentTool.UpdatePropertyPanel();

		if (configName.IsEmpty())
		{
			Workbench.Dialog("Missing Name", "Please enter a name for the new config.");
			return false;
		}

		if (workingConfigFolder.IsEmpty())
		{
			Workbench.Dialog("Missing Folder", "Please select a destination folder first.");
			return false;
		}

		string resourcePath = workingConfigFolder + "/" + configName + ".conf";
		if (FileIO.FileExists(resourcePath))
		{
			Workbench.Dialog("File Already Exists", "A config named '" + configName + ".conf' already exists in that folder.");
			return false;
		}

		// Strip any GUID prefix before resolving to an absolute path.
		string strippedPath = resourcePath;
		int guidEnd = resourcePath.IndexOf(DAB_BugWorkaround.CLOSED_BRACKET);
		if (guidEnd >= 0)
			strippedPath = resourcePath.Substring(guidEnd + 1, resourcePath.Length() - guidEnd - 1);

		if (!Workbench.GetAbsolutePath(strippedPath, absolutePath, false))
		{
			Workbench.Dialog("Path Error", "Could not resolve the folder to an absolute path.\nMake sure the folder is inside your mod.");
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------
	static protected DAB_PoseModification WriteAndRegisterConfig(string absolutePath)
	{
		DAB_PoseModification emptyMod = new DAB_PoseModification();
		Resource holder = BaseContainerTools.CreateContainerFromInstance(emptyMod);
		if (!holder)
		{
			Workbench.Dialog("Error", "Failed to create container from pose modification.");
			return null;
		}

		if (!BaseContainerTools.SaveContainer(holder.GetResource().ToBaseContainer(), "", absolutePath))
		{
			Workbench.Dialog("Save Failed", "Could not write to:\n" + absolutePath + "\n\nWorkbench may prompt for file system permission — check for a pending dialog.");
			return null;
		}

		ResourceManager rm = Workbench.GetModule(ResourceManager);
		if (!rm)
		{
			Workbench.Dialog("Error", "Could not get ResourceManager module.");
			return null;
		}

		rm.RegisterResourceFile(absolutePath, false);
		if (!rm.WaitForFile(absolutePath, 2000))
			Print("WriteAndRegisterConfig: file created but not ready in time. This may cause issues.", LogLevel.WARNING);

		return emptyMod;
	}

	// ── Private: shared utilities ─────────────────────────────────────────
	//-----------------------------------------------------------------------
	static protected bool ShowConfirmDialog(string title, string message)
	{
		DAB_ConfirmDialog dialog = new DAB_ConfirmDialog();
		return Workbench.ScriptDialog(title, message, dialog);
	}

	//-----------------------------------------------------------------------
	static protected int ShowConfigConfrimOverwriteDialog(string title, string message)
	{
		DAB_ConfigConfirmOverwriteDialog dialog = new DAB_ConfigConfirmOverwriteDialog();
		Workbench.ScriptDialog(title, message, dialog);
		return dialog.GetSelectedChoise();
	}

	//-----------------------------------------------------------------------
	static protected int FindPoseComponentIndex(IEntitySource entitySource)
	{
		BaseContainerList components = entitySource.GetObjectArray("components");
		if (!components) return -1;

		for (int i = 0; i < components.Count(); i++)
		{
			BaseContainer comp = components.Get(i);
			if (comp && comp.GetClassName() == "DAB_PoseModificationComponent")
				return i;
		}
		return -1;
	}

	//-----------------------------------------------------------------------
	static protected DAB_PoseModificationComponent SafeSetWorkingConfig(DAB_BoneManipulatorTool parentTool, WorldEditorAPI api, ResourceName configPath)
	{
		IEntitySource entitySource = parentTool.GetCurrentTargetEntitySource();
		if (!entitySource)
		{
			Print("SafeSetWorkingConfig: no entity source!", LogLevel.ERROR);
			return null;
		}

		int idx = FindPoseComponentIndex(entitySource);
		if (idx < 0)
		{
			Print("SafeSetWorkingConfig: component index not found!", LogLevel.ERROR);
			return null;
		}

		parentTool.ClearTargetComponent();

		api.BeginEditSequence(entitySource);
		api.BeginEntityAction();
		api.SetVariableValue(
			entitySource,
			{ new ContainerIdPathEntry("components", idx) },
			"m_sWorkingPoseModification",
			configPath
		);
		api.EndEntityAction();
		api.EndEditSequence(entitySource);

		parentTool.RefreshTargetEntity();
		parentTool.UpdatePropertyPanel();

		return parentTool.GetTargetComponent();
	}
}