class DAB_CinematicsHelper
{
	//-----------------------------------------------------------------------
	static map<string, CinematicEntity> GetCurrentScenes(WorldEditorAPI api)
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
	static BaseContainer GetCinematicScene(CinematicEntity cinematicEntity, WorldEditorAPI api)
	{
	    if (!cinematicEntity)
	    {
	        Print("GetCinematicScene: cinematicEntity is null.", LogLevel.ERROR);
	        return null;
	    }
	
	    IEntitySource entitySource = api.EntityToSource(cinematicEntity);
	    if (!entitySource)
	    {
	        Print("GetCinematicScene: could not get entity source for '" + cinematicEntity.GetName() + "'.", LogLevel.ERROR);
	        return null;
	    }
	
	    BaseContainer container = entitySource.ToBaseContainer();
	    if (!container)
	    {
	        Print("GetCinematicScene: could not get base container for '" + cinematicEntity.GetName() + "'.", LogLevel.ERROR);
	        return null;
	    }
	
	    BaseContainer sceneContainer = container.GetObject("Scene");
	    if (!sceneContainer)
	    {
	        Print("GetCinematicScene: no 'Scene' object found on '" + cinematicEntity.GetName() + "'.", LogLevel.ERROR);
	        return null;
	    }
	
	    return sceneContainer;
	}
	
	//-----------------------------------------------------------------------
	static array<CinematicTrackBase> GetTracksFromScene(BaseContainer sceneContainer)
	{
	    if (!sceneContainer)
	    {
	        Print("GetTracksFromScene: sceneContainer is null.", LogLevel.ERROR);
	        return null;
	    }
	
	    BaseContainerList trackList = sceneContainer.GetObjectArray("Tracks");
	    if (!trackList || trackList.Count() == 0)
	    {
	        Print("GetTracksFromScene: no tracks found in scene.", LogLevel.WARNING);
	        return null;
	    }
	
	    array<CinematicTrackBase> tracks = {};
	
	    for (int i = 0; i < trackList.Count(); i++)
	    {
	        BaseContainer trackContainer = trackList.Get(i);
	        if (!trackContainer) continue;
	
	        CinematicTrackBase track = CinematicTrackBase.Cast(BaseContainerTools.CreateInstanceFromContainer(trackContainer)
	        );
	
	        if (!track)
	        {
	            Print("GetTracksFromScene: could not cast track at index " + i + " [" + trackContainer.GetClassName() + "] to CinematicTrackBase.", LogLevel.WARNING);
	            continue;
	        }
	
	        tracks.Insert(track);
	    }
	
	    return tracks;
	}
	
	//-----------------------------------------------------------------------
	static bool ContainsTracksOfClass(array<CinematicTrackBase> tracks, typename classToCheck, out array<int> indices)
	{
	    indices = {};
	
	    for (int i = 0; i < tracks.Count(); i++)
	    {
	        CinematicTrackBase track = tracks[i];
	        if (track.IsInherited(classToCheck))
	            indices.Insert(i);
	    }
	
	    return !indices.IsEmpty();
	}
	
	//-----------------------------------------------------------------------
	static string GetTrackEntityName(CinematicTrackBase track)
	{
	    DAB_PoseManipulationTrack poseTrack = DAB_PoseManipulationTrack.Cast(track);
	    if (poseTrack) return poseTrack.GetEntityName();
	
	    TStringArray strs = new TStringArray;
	    track.GetTrackName().Split("_", strs, true);
	    return strs.Get(0);
	}
	
	//-----------------------------------------------------------------------
	static bool HasSlotBoneTrackForEntity(BaseContainer sceneContainer, string entityName)
	{
	    BaseContainerList trackList = sceneContainer.GetObjectArray("Tracks");
	    if (!trackList) return false;
	
	    for (int i = 0; i < trackList.Count(); i++)
	    {
	        BaseContainer trackContainer = trackList.Get(i);
	        if (!trackContainer) continue;
	        if (trackContainer.GetClassName() != "CustomCinematicTrack") continue;
	
	        string slotClassName;
	        trackContainer.Get("ClassName", slotClassName);
	        if (slotClassName != "SlotBoneAnimationCinematicTrack") continue;
	
	        string trackName;
	        trackContainer.Get("TrackName", trackName);
	        if (trackName == entityName) return true;
	    }
	
	    return false;
	}

	//-----------------------------------------------------------------------
	static int CountOwnedPoseTracks(BaseContainer sceneContainer, string entityName)
	{
	    BaseContainerList trackList = sceneContainer.GetObjectArray("Tracks");
	    if (!trackList)
	        return 0;
	    
	
	    int count = 0;
	    for (int i = 0; i < trackList.Count(); i++)
	    {
	        BaseContainer trackContainer = trackList.Get(i);
	        if (!trackContainer)
	            continue;

	        string classNameStr;
	        trackContainer.Get("ClassName", classNameStr);
	        if (classNameStr != "DAB_PoseManipulationTrack")
	            continue;
	        
	
	        BaseContainerList childTracks = trackContainer.GetObjectArray("ChildTracks");
	        if (!childTracks)
	            continue;
	        
	        for (int j = 0; j < childTracks.Count(); j++)
	        {
	            BaseContainer childTrack = childTracks.Get(j);
	            if (!childTrack)
	                continue;
	            
	
	            string trackName;
	            childTrack.Get("TrackName", trackName);
	            if (trackName != "m_sEntityName") continue;
	            
				BaseContainerList keyframes = childTrack.GetObjectArray("Keyframes");
	            if (keyframes && keyframes.Count() <= 0) continue;
	                
	            BaseContainer keyframe = keyframes.Get(0);
	            if (!keyframe) continue;
	            
                string storedEntityName;
                keyframe.Get("Value", storedEntityName);
                
                if (storedEntityName == entityName)
                    count++;

                // We found the target track, no need to check other child tracks
                break;
	        }
	    }
	
	    return count;
	}
	
	//-----------------------------------------------------------------------
	// Returns number of scenes refreshed
	static int RefreshAllScenes(WorldEditorAPI api)
	{
		map<string, CinematicEntity> currentScenes = GetCurrentScenes(api);
		
		for (MapIterator it = currentScenes.Begin(); it != currentScenes.End(); it = currentScenes.Next(it))
		{
			string name = currentScenes.GetIteratorKey(it);
			CinematicEntity entity = currentScenes.GetIteratorElement(it);
			
			entity.Play();
			entity.Stop();
		}
		
		return currentScenes.Count();
	}
	
	//-----------------------------------------------------------------------
	static bool TryAddPoseTrack(IEntitySource sceneSource, string trackName, WorldEditorAPI api)
	{
	    if (!sceneSource) return false;
	
	    BaseContainer entityContainer = sceneSource.ToBaseContainer();
	    if (!entityContainer) return false;
	
	    array<ref ContainerIdPathEntry> path = { new ContainerIdPathEntry("Scene") };
	
	    api.BeginEntityAction();
	
	    if (!api.CreateObjectArrayVariableMember(entityContainer, path, "Tracks", "CustomCinematicTrack", -1))
	    {
	        api.EndEntityAction();
	        return false;
	    }
	    
	    BaseContainer sceneContainer = entityContainer.GetObject("Scene");
	    BaseContainerList tracks = sceneContainer.GetObjectArray("Tracks");
	    int newIdx = tracks.Count() - 1;
	
	    path.Insert(new ContainerIdPathEntry("Tracks", newIdx));
	
	    api.SetVariableValue(entityContainer, path, "ClassName", "DAB_PoseManipulationTrack");
	    api.SetVariableValue(entityContainer, path, "TrackName", trackName);
	
	    api.EndEntityAction();
	    return true;
	}
}