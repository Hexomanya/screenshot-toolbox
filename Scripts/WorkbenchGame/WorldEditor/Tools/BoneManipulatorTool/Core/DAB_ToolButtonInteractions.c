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
	static void CreateCinematicTrack(IEntity targetEntity, WorldEditorAPI api)
	{
	    if (!targetEntity)
	    {
	        Workbench.Dialog("No Entity", "Please select an entity in the world first.");
	        Print("CreateCinematicTrack: No target entity provided.", LogLevel.WARNING);
	        return;
	    }
	
	    string entityName = targetEntity.GetName();
	    if (entityName.IsEmpty())
	    {
	        Workbench.Dialog("Unnamed Entity", "The entity must have a unique Name (not just a label) to link to a track.");
	        Print("CreateCinematicTrack: Entity has no name.", LogLevel.ERROR);
	        return;
	    }
	
	    map<string, CinematicEntity> scenes = DAB_CinematicsHelper.GetCurrentScenes(api);
	    if (scenes.IsEmpty())
	    {
	        Workbench.Dialog("No Scenes", "No Cinematic Scenes found in the current world.");
	        Print("CreateCinematicTrack: No scenes found.", LogLevel.WARNING);
	        return;
	    }
	
	    array<string> sceneNames = {};
	    for (int i = 0; i < scenes.Count(); i++)
	    {
	        sceneNames.Insert(scenes.GetKey(i));
	    }
		
	    array<int> selectedIndices = {};
	    int result = api.ShowItemListDialog("Select Scenes", "Add pose track to:", 400, 300, sceneNames, selectedIndices, 0);
	
	    if (result == -1 || selectedIndices.Count() == 0)
	    {
	        Print("CreateCinematicTrack: User cancelled selection.", LogLevel.NORMAL);
	        return;
	    }
	
	    // Define the track name. The track uses everything before the '_' as the target entity name.
	    string trackName = entityName + "_PoseTrack";
	
	    foreach (int index : selectedIndices)
	    {
	        string key = sceneNames[index];
	        CinematicEntity sceneEntity = scenes.Get(key);
	        
	        if (!sceneEntity)
	        {
	            PrintFormat("CreateCinematicTrack: Scene '%1' is invalid.", key, LogLevel.ERROR);
	            continue;
	        }
	
	        BaseContainer container = DAB_CinematicsHelper.GetCinematicScene(sceneEntity, api);
	        if (!container)
	        {
	            PrintFormat("CreateCinematicTrack: Could not get container for scene '%1'.", key, LogLevel.ERROR);
	            continue;
	        }
	
	        // Prevent duplicate tracks for the same entity in the same scene
	        if (DAB_CinematicsHelper.CountOwnedPoseTracks(container, entityName) > 0)
	        {
	            Workbench.Dialog("Track Exists", "A pose track for '" + entityName + "' already exists in '" + key + "'.");
	            continue;
	        }
	
	        IEntitySource sceneSource = api.EntityToSource(sceneEntity);
	        if (!DAB_CinematicsHelper.TryAddPoseTrack(sceneSource, trackName, api))
	        {
	            Workbench.Dialog("Error", "Failed to create track in scene: " + key);
	            PrintFormat("CreateCinematicTrack: TryAddPoseTrack failed for %1", key, LogLevel.ERROR);
	        }
	        else
	        {
	            PrintFormat("CreateCinematicTrack: Added track '%1' to scene '%2'", trackName, key);
	        }
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