//! Cached skeleton data for one model: bone names, parent relationships and inter-bone distances.
//! Shared across all entities that use the same skeleton (keyed by ComputeSkeletonKey).
class DAB_SkeletonInfo
{
	protected string                   m_sSkeletonKey;
	protected ref array<string>        m_BoneNames             = {};
	protected ref map<string, string>  m_BoneParents           = new map<string, string>();
	protected ref map<string, float>   m_BoneToParentDistances = new map<string, float>();

	//-----------------------------------------------------------------------
	void DAB_SkeletonInfo(string skeletonKey, array<string> boneNames, map<string, string> boneParents, map<string, float> boneToParentDistances)
	{
		m_sSkeletonKey          = skeletonKey;
		m_BoneNames             = boneNames;
		m_BoneParents           = boneParents;
		m_BoneToParentDistances = boneToParentDistances;
	}

	//-----------------------------------------------------------------------
	void ~DAB_SkeletonInfo();

	//-----------------------------------------------------------------------
	//! Returns the name of the direct parent of \p childName, or an empty string for root bones.
	string GetDirectParent(string childName)
	{
		string parent;
		if (!m_BoneParents.Find(childName, parent))
			Print("GetDirectParent: bone '" + childName + "' not found in parent map.", LogLevel.ERROR);
		return parent;
	}

	// ── Getters ────────────────────────────────────────────────────────────
	string              GetSkeletonKey()           { return m_sSkeletonKey; }
	array<string>       GetBoneNames()             { return m_BoneNames; }
	map<string, string> GetBoneParents()           { return m_BoneParents; }
	map<string, float>  GetBoneToParentDistances() { return m_BoneToParentDistances; }

	// ── Static helpers ─────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	//! Returns a stable cache key for the skeleton used by \p entity.
	//! Prefers the mesh resource path; falls back to a bone-count fingerprint.
	static string ComputeSkeletonKey(IEntity entity)
	{
		if (!entity) return "";

		VObject vobj = entity.GetVObject();
		if (vobj)
		{
			string path = vobj.GetResourceName();
			if (!path.IsEmpty()) return path;
		}

		Animation anim = entity.GetAnimation();
		if (!anim) return "";

		array<string> names = {};
		anim.GetBoneNames(names);
		if (names.IsEmpty()) return "";

		return names.Count().ToString() + names[0] + names[names.Count() - 1];
	}
}