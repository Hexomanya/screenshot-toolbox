class DAB_SkeletonInfo
{
	protected string m_sSkeletonKey;
	protected ref array<string> m_BoneNames = {};
	protected ref map<string, string> m_BoneParents = new map<string, string>();
	protected ref map<string, float> m_BoneToParentDistances = new map<string, float>();
	
	//-----------------------------------------------------------------------
	void DAB_SkeletonInfo(string skeletonKey, array<string> boneNames, map<string, string> boneParents, /*map<string, float> boneToParentDistances*/)
	{
		m_sSkeletonKey = skeletonKey;
		m_BoneNames = boneNames;
		m_BoneParents = boneParents;
		//m_BoneToParentDistances = boneToParentDistances;
	}

	//-----------------------------------------------------------------------
	void ~DAB_SkeletonInfo();
	
	
	string GetDirectParent(string childName)
	{
		string parent;
		if(!GetBoneParents().Find(childName, parent)) 
		{
			Print("Could not find child key in map!", LogLevel.ERROR);
		}
		
		return parent;
	}
	
	//--- GETTER METHODS ----------------------------------------------------
	string GetSkeletonKey(){ return m_sSkeletonKey; }
	array<string> GetBoneNames(){ return m_BoneNames; }
	map<string, string> GetBoneParents(){ return m_BoneParents; }
	map<string, float> GetBoneToParentDistances(){ return m_BoneToParentDistances; }
	

	//--- STATIC METHODS ----------------------------------------------------
	//-----------------------------------------------------------------------
	static string ComputeSkeletonKey(IEntity entity)
	{
		if (!entity) return "";

	    VObject vobj = entity.GetVObject();
	    if (vobj)
	    {
	        string path = vobj.GetResourceName();
	        if (!path.IsEmpty()) return path;
	    }
	
	    // Fallback: fingerprint from bone count + first + last bone name
	    Animation anim = entity.GetAnimation();
	    if (!anim) return "";
	
	    array<string> names = {};
	    anim.GetBoneNames(names);
	    if (names.IsEmpty()) return "";
	
	    return names.Count().ToString() + names[0] + names[names.Count() - 1];
	}
	
	
}