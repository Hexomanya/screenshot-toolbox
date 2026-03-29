class DAB_Debug
{
	static string GetContainerAsJson(BaseContainer container, string indent = "")
	{
	    if (!container) return "null";
	
	    string json = "{\n";
	    string nextIndent = indent + "  ";
	    
	    json += nextIndent + "\"_ClassName\": \"" + container.GetClassName() + "\",\n";
	
	    int varCount = container.GetNumVars();
	    for (int i = 0; i < varCount; i++)
	    {
	        string varName = container.GetVarName(i);
	        json += nextIndent + "\"" + varName + "\": ";
	
	        // 1. Objects / Arrays
	        BaseContainerList list = container.GetObjectArray(varName);
	        BaseContainer subObj = container.GetObject(varName);
	
	        if (list)
	        {
	            json += "[\n";
	            for (int j = 0; j < list.Count(); j++)
	            {
	                json += nextIndent + "  " + GetContainerAsJson(list.Get(j), nextIndent + "  ");
	                if (j < list.Count() - 1) json += ",";
	                json += "\n";
	            }
	            json += nextIndent + "]";
	        }
	        else if (subObj)
	        {
	            json += GetContainerAsJson(subObj, nextIndent);
	        }
	        // 2. Primitives (Fixed with implicit string conversion)
	        else
	        {
	            string sVal;
	            int iVal;
	            float fVal;
	            bool bVal;
	
	            if (container.Get(varName, sVal))
	            {
	                json += "\"" + sVal + "\"";
	            }
	            else if (container.Get(varName, iVal))
	            {
	                json += iVal.ToString(); // Enfusion converts int to string automatically here
	            }
	            else if (container.Get(varName, fVal))
	            {
	                json += fVal.ToString(); // Enfusion converts float to string automatically here
	            }
	            else if (container.Get(varName, bVal))
	            {
	                if (bVal) json += "true";
	                else json += "false";
	            }
	            else
	            {
	                json += "null";
	            }
	        }
	
	        if (i < varCount - 1) json += ",\n";
	        else json += "\n";
	    }
	
	    return json + indent + "}";
	}
	
	static string GetExistingTracksAsString(BaseContainer sceneContainer)
	{
	    BaseContainerList trackList = sceneContainer.GetObjectArray("Tracks");
	    if (!trackList) return "";
	
	    string result = "";
	    for (int i = 0; i < trackList.Count(); i++)
	    {
	        BaseContainer track = trackList.Get(i);
	        if (!track) continue;
	
	        string trackName;
	        track.Get("TrackName", trackName);
	
	        result += track.GetClassName() + " {\n";
	        for (int j = 0; j < track.GetNumVars(); j++)
	        {
	            string key = track.GetVarName(j);
	            string value;
	            track.Get(key, value);
	            if (!value.IsEmpty())
	                result += " " + key + " \"" + value + "\"\n";
	        }
	        result += "}\n";
	    }
	
	    return result;
	}
	
	static string GetDebugVariablePath(array<ref ContainerIdPathEntry> path)
	{
		string combinedPath = "";
		foreach(ContainerIdPathEntry pathEntry : path)
		{
			combinedPath = combinedPath + " -> " + pathEntry.PropertyName;
		}
		
		return combinedPath;
	}
}