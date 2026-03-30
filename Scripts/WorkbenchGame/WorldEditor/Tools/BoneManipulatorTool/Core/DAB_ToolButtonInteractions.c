class DAB_ToolButtonInteractions
{
	//-----------------------------------------------------------------------
	static void CreateNewConfig(DAB_BoneManipulatorTool parentTool, inout ResourceName workingConfig, inout ResourceName workingConfigFolder)
	{
		Print("Prev config is: " + workingConfig);
		
	    if (!workingConfig.IsEmpty())
	    {
	        Workbench.Dialog(
	            "Working Config Already Set",
	            "You already have a working config selected:\n" + workingConfig +
	            "\n\nClear the 'Current Pose Config' field first before creating a new one."
	        );
	        return;
	    }
		
		DAB_CreateNewManipulationDialog creationDialog = new DAB_CreateNewManipulationDialog(workingConfigFolder);
		bool didConfirmCreation = Workbench.ScriptDialog("Create new Config", "Please select a folder and name for the new config file.", creationDialog);
		
		if(!didConfirmCreation) return;
		
		string configName = creationDialog.GetNewConfigName();
		workingConfigFolder = creationDialog.GetWorkingConfigFolder();
		parentTool.UpdatePropertyPanel();
		
	    if (configName.IsEmpty())
	    {
	        Workbench.Dialog("Missing Name", "Please enter a name for the new config.");
	        return;
	    }
		
		if (workingConfigFolder.IsEmpty())
	    {
	        Workbench.Dialog("Missing Folder", "Please select a destination folder first.");
	        return;
	    }
	
	    // Build the resource-style path (used as ResourceName after saving)
	    string resourcePath = workingConfigFolder + "/" + configName + ".conf";
	
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
		
		string formattedResourceName = Workbench.GetResourceName(absolutePath);
		parentTool.SetWorkingConfig(formattedResourceName);
	    parentTool.UpdatePropertyPanel();
		
		Workbench.OpenModule(WorldEditor);
	    Print("DAB_BoneManipulatorTool: created new config at " + absolutePath, LogLevel.NORMAL);
	}
	
	//-----------------------------------------------------------------------
	static void CopyToCinematicScene(ResourceName workingConfig, array<ResourceName> modificationStack, IEntity targetEntity, WorldEditorAPI api)
	{
		if(!targetEntity)
		{
	        Workbench.Dialog("No Entity", "Select a entity first.");
	        return;
	    }
		
		if (workingConfig.IsEmpty())
	    {
	        Workbench.Dialog("No Config", "Select a working config first.");
	        return;
	    }

		map<string, CinematicEntity> currentScenes = GetCurrentScenes(api);
		
		if (currentScenes.IsEmpty())
	    {
	        Workbench.Dialog("No Scenes Found", "No cinematic scenes exist in this world.");
	        return;
	    }
	
	    array<string> sceneNames = {};
		for (MapIterator it = currentScenes.Begin(); it != currentScenes.End(); it = currentScenes.Next(it))
		{
			sceneNames.Insert(currentScenes.GetIteratorKey(it));
		}
		    
	
	    array<int> selectedIndices = {};
	    int result = api.ShowItemListDialog(
	        "Select Cinematic Scene",
	        "Choose which scene to add '" + workingConfig + "' to:",
	        400,
	        300,
	        sceneNames,
	        selectedIndices,
	        0
	    );
		
		if(result == -1 || selectedIndices.Count() == 0)
		{
			Print("Did not select a scene or canceled. Nothing was copied!");
			return;
		}
	
		bool didConfirm = ShowConfirmDialog("Copy to Scene(s)", "You are going to copy the ACTIVE manipilation to " + selectedIndices.Count() + " scenes. Are you sure?");
		
	    if (!didConfirm)
	    {
			Print("Did not confirm copy action! Nothing was copied!");
			return;
		}
		
		string currentEntityName = targetEntity.GetName();
	
		foreach (int sceneIndex : selectedIndices)
		{
		    string key = sceneNames[sceneIndex];
		    if (key.IsEmpty()) continue;
		
		    CinematicEntity sceneEntity;
		    if (!currentScenes.Find(key, sceneEntity)) continue;
		    if (!sceneEntity) continue;
		
		    BaseContainer scene = DAB_CinematicsHelper.GetCinematicScene(sceneEntity, api);
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
		    foreach (ResourceName modification : modificationStack)
		    {
		        if (!modification.IsEmpty())
		            configs.Insert(modification);
		    }
		    configs.Insert(workingConfig);
		
		    bool success;
		    IEntitySource sceneEntitySource = api.EntityToSource(sceneEntity);

			Print("ownedPoseTrackCount: " + ownedPoseTrackCount);
			if (ownedPoseTrackCount == 0)
			    success = DAB_CinematicsHelper.TryAddPoseTrackToScene(sceneEntitySource, currentEntityName, configs, api);
			else
			    success = DAB_CinematicsHelper.TryUpdatePoseTrackConfigs(sceneEntitySource, currentEntityName, configs, api);
		
		    if (!success)
		        Workbench.Dialog("Failed", "Could not update the pose track in scene '" + key + "'.");
		    else
		        Print("DAB_BoneManipulatorTool: successfully updated scene '" + key + "'.", LogLevel.NORMAL);
		}
	}
	
	static void SaveEdits(DAB_BoneManipulatorTool tool, DAB_EditorController controller, bool askForSave = false)
	{
		if(askForSave)
		{
			bool didConfirm = ShowConfirmDialog("Confirm Save", "Do you want to save unsafed changes?");
			if(!didConfirm) return;
		}
		
		ResourceName config = tool.GetWorkingConfig();
		if(config.IsEmpty())
		{
			Workbench.Dialog("No Config", "There is no config to safe to! Please create a new one!");
			return;
		}
		
		int boneCount = controller.GetDirtyBoneCount();
		if(boneCount)
		{
			Workbench.Dialog("No Edits", "There are no edits to safe!");
			return;
		}
		
		bool didSave = controller.SaveDirtyBones();
		
		if(!didSave)
		{
			Workbench.Dialog("No Save", "The system could not save the changes. Please check the console for errors and report this!");
			return;
		}
			
		PrintFormat("Saved %1 changed bones to config %2", boneCount, config);
	}
	
	//-----------------------------------------------------------------------
	static protected map<string, CinematicEntity> GetCurrentScenes(WorldEditorAPI api)
	{
		map<string, CinematicEntity> currentScenes = new map<string, CinematicEntity>();

		int entityCount = api.GetEditorEntityCount();
		for(int i = 0; i < entityCount; i++)
		{
			IEntitySource candidate = api.GetEditorEntity(i);
			if(!candidate) continue;
			
			IEntity entity = api.SourceToEntity(candidate);
			if(!entity) continue;

			CinematicEntity sceneEntity = CinematicEntity.Cast(entity);
			if(!sceneEntity) continue;
			
			string sceneName = entity.GetName();
			if (sceneName.IsEmpty())
			{
				Workbench.Dialog("Name Missing!", "Please make sure to name all your cinematic scenes!");
				sceneName = api.GetEntityNiceName(candidate);
			}
			    
			currentScenes.Set(sceneName, sceneEntity);
		}
		
		return currentScenes;
	}
	
	//-----------------------------------------------------------------------
	static protected bool ShowConfirmDialog(string title, string message)
	{
	    DAB_ConfirmDialog dialog = new DAB_ConfirmDialog();
	    return Workbench.ScriptDialog(title, message, dialog);
	}
}