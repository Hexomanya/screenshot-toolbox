//! Cached skeleton data for one model: bone names, parent relationships and inter-bone distances.
//! Shared across all entities that use the same skeleton (keyed by ComputeSkeletonKey).
class DAB_SkeletonInfo
{
    protected string m_sSkeletonKey;
	protected ref map<string, ref DAB_BoneRecord> m_mBoneRecords;
	protected ref map<string, string> m_mBoneParents;
	 
    // ── Constructors ──────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    void DAB_SkeletonInfo(string skeletonKey, IEntity entity)
    {
		PrintFormat("Creating new skeleton information with key: %1", skeletonKey);

		m_sSkeletonKey = skeletonKey;
		m_mBoneRecords = DAB_SkeletonInfo.CreateBoneRecords(entity);
		m_mBoneParents = DAB_SkeletonInfo.ExtractBoneRecords(m_mBoneRecords);
    }
 
    //-----------------------------------------------------------------------
    void ~DAB_SkeletonInfo() {}
 
    // ── Public ────────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    //! Returns the name of the direct parent of \p childName, or an empty string for root bones.
    string GetDirectParent(string childName)
    {
        string parent = "";
        if (!m_mBoneParents.Find(childName, parent))
            Print("GetDirectParent: bone '" + childName + "' not found in parent map.", LogLevel.ERROR);
        return parent;
    }
 
    //-----------------------------------------------------------------------
    //! Returns true if \p ancestorSimpleName is a parent (or parent-of-parent) of \p childCompoundName. Only works if ancestorSimpleName is not in slot!
    bool IsAncestorOf(string childCompoundName, string ancestorSimpleName)
    {
		string ancestorCompoundName = DAB_SkeletonInfo.GetCompoundName(ancestorSimpleName, {});
		
        string parentCompoundName;
        string current = childCompoundName;
		
        while (m_mBoneParents.Find(current, parentCompoundName))
        {			
            if (parentCompoundName == ancestorCompoundName) return true;
 
            current = parentCompoundName;
            if (current.IsEmpty()) break;
        }
 
        return false;
    }
	
	//-----------------------------------------------------------------------
	IEntity GetSlotEntity(string compoundBoneName)
	{
		DAB_BoneRecord record = m_mBoneRecords.Get(compoundBoneName);
		if(!record) return null;
		return record.GetSlotEntity();
	}
	
	//-----------------------------------------------------------------------
	string GetSimpleBoneName(string compoundBoneName)
	{
		DAB_BoneRecord record;
		if(m_mBoneRecords.Find(compoundBoneName, record)) return record.GetSimpleBoneName();
		
		PrintFormat("Needed to manually convert to simple bone name for '%1' on skeleton '%2'", compoundBoneName, m_sSkeletonKey, LogLevel.WARNING);
		
		return ExtractBoneNameFromCompundName(compoundBoneName);
	}
	
	bool GetHasDuplicateName(string compoundBoneName)
	{
		DAB_BoneRecord record;
		if(m_mBoneRecords.Find(compoundBoneName, record)) return record.GetHasDuplicateName();
		PrintFormat("Could not find bone record for bone with name '%1' on skeleton '%2'", compoundBoneName, m_sSkeletonKey, LogLevel.WARNING);
		return true;
	}
	
	//-----------------------------------------------------------------------
	array<string> GetSlotNames(string compoundBoneName)
	{
		DAB_BoneRecord record;
		if(m_mBoneRecords.Find(compoundBoneName, record)) return record.GetSlotNames();
		
		PrintFormat("Could not retrieve record for '%1' returning empty slots names", compoundBoneName, LogLevel.ERROR);
		
		return {};
	}
	
	//-----------------------------------------------------------------------
	//! Returns a display-friendly compound name by stripping the root slot prefix.
	//! e.g. "DAB_ROOT::v_door_L01_handl"       => "v_door_L01_handl"
	//!      "DAB_ROOT/Turret::v_door_L01_handl" => "Turret::v_door_L01_handl"
	string GetNiceCompoundName(string compoundBoneName)
	{
	    int delimiterIndex = compoundBoneName.IndexOf(DAB_Constants.SLOT_SLOT_DELIMITER);
	
	    if (delimiterIndex == -1)
	        return GetSimpleBoneName(compoundBoneName);
	
	    if (!compoundBoneName.Contains(DAB_Constants.SLOT_ROOT_ID))
	    {
	        PrintFormat("GetNiceCompoundName: Malformed compound name '%1' — has slot delimiter but no root!", compoundBoneName, LogLevel.ERROR);
	        return "MALFORMED:" + compoundBoneName;
	    }
	
	    int start = delimiterIndex + DAB_Constants.SLOT_SLOT_DELIMITER.Length();
	    return compoundBoneName.Substring(start, compoundBoneName.Length() - start);
	}
	
	DAB_BoneRecord GetBoneRecord(string compoundBoneName)
	{
		DAB_BoneRecord record;
		if(!m_mBoneRecords.Find(compoundBoneName, record))
			PrintFormat("Could not find record for '%1'", compoundBoneName, LogLevel.ERROR);
		
		return record;
	}
	
 
    // ── Getters ────────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    string GetSkeletonKey() { return m_sSkeletonKey; }
    map<string, ref DAB_BoneRecord> GetBoneRecords() { return m_mBoneRecords; }
    map<string, string> GetBoneParents() { return m_mBoneParents; }
 
    // ── Static Helpers ─────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    //! Returns a stable cache key for the skeleton used by \p entity.
	//TODO: Rethink this
    static string ComputeSkeletonKey(IEntity entity)
    {
        if (!entity) return "";
 
		string entityName = entity.GetName();
		if(!entityName.IsEmpty()) return entityName;
		
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
        return string.Format("Skeleton_Bones:%1", names.Count());
    }
 
	
	//-----------------------------------------------------------------------
	static string ExtractBoneNameFromCompundName(string compoundName)
	{
		int delimiterIndex = compoundName.IndexOf(DAB_Constants.SLOT_BONE_DELIMITER);
		if (delimiterIndex == -1)
			return compoundName;
		
		int boneStart = delimiterIndex + DAB_Constants.SLOT_BONE_DELIMITER.Length();
		return compoundName.Substring(boneStart, compoundName.Length() - boneStart);
	}
	
	//-----------------------------------------------------------------------
    static map<string, ref DAB_BoneRecord> CreateBoneRecords(IEntity entity)
    {
        map<string, ref DAB_BoneRecord> boneRecords = new map<string, ref DAB_BoneRecord>();
        CollectBoneRecords(entity, {DAB_Constants.SLOT_ROOT_ID}, boneRecords);
        return boneRecords;
    }
	
	//-----------------------------------------------------------------------
	//! Returns the bone index for \p boneName on \p entity, or int.INVALID_INDEX on failure.
	static TNodeId GetBoneIndex(IEntity entity, string simpleBoneName, out Animation outAnim = null)
	{
	    if (simpleBoneName.Contains(DAB_Constants.SLOT_BONE_DELIMITER))
	    {
	        PrintFormat("GetBoneIndex was provided compound bone name! Name was '%1'. Extracting name...", simpleBoneName, LogLevel.WARNING);
	        simpleBoneName = ExtractBoneNameFromCompundName(simpleBoneName);
	    }
	
	    if (!entity)
	    {
	        Print("GetBoneIndex: no entity provided.", LogLevel.ERROR);
	        return int.INVALID_INDEX;
	    }
	
	    Animation anim = entity.GetAnimation();
	    if (!anim)
	    {
	        Print("GetBoneIndex: Animation is null.", LogLevel.ERROR);
	        return int.INVALID_INDEX;
	    }
	
	    outAnim = anim;
	    return anim.GetBoneIndex(simpleBoneName);
	}
 
    // ── Internal ──────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
	protected static void CollectBoneRecords(IEntity entity, array<string> slotNames, inout map<string, ref DAB_BoneRecord> outBoneRecords)
	{
		if (!entity) return;
	
		Animation anim = entity.GetAnimation();
		if (!anim) return;
		
		array<string> localBoneNames = {};
		anim.GetBoneNames(localBoneNames);
		
		// Reset bones to ensure parent calculation works
		foreach(string boneName : localBoneNames)
		{
			TNodeId id = anim.GetBoneIndex(boneName);
			if(id == -1) 
			{
				PrintFormat("Could not find id for boneName: %1", boneName);
				continue;
			}

			anim.SetBone(entity, id, vector.Zero, vector.Zero, 1);
		}
		entity.Update();
		
		foreach (string boneName : localBoneNames)
		{
			if(boneName.IsEmpty()) continue;
			
			string compoundName = DAB_SkeletonInfo.GetCompoundName(boneName, slotNames); //Should never happen since boneName can not be empty, but to better stay safe for future modifications
			if(compoundName.IsEmpty())
			{
				Print("Something went wrong when generating compoundName!", LogLevel.ERROR);
				continue;
			}
			
			string parentSimpleName = DAB_BoneHelper.FindParentBoneName(anim, boneName, localBoneNames);
			string parentCompoundName = DAB_SkeletonInfo.GetCompoundName(parentSimpleName, slotNames);
			bool isDuplicateName = DAB_SkeletonInfo.DuplicateNameExistAlready(boneName, outBoneRecords);
				
			DAB_BoneRecord record = new DAB_BoneRecord(boneName, slotNames, entity, parentCompoundName, isDuplicateName);
			
			outBoneRecords.Insert(compoundName, record);
		}
		
		SlotManagerComponent slotManager = SlotManagerComponent.Cast(DAB_Helper.FindComponentExact(entity, SlotManagerComponent));
		if (!slotManager) return;
	
		array<EntitySlotInfo> slotInfos = {};
		slotManager.GetSlotInfos(slotInfos);
	
		foreach (EntitySlotInfo slotInfo : slotInfos)
		{
			IEntity childEntity = slotInfo.GetAttachedEntity();
			if (!childEntity) continue;
			
			array<string> newSlotNames = {};
			newSlotNames.Copy(slotNames);
			newSlotNames.Insert(slotInfo.GetSourceName());
 
			CollectBoneRecords(childEntity, newSlotNames, outBoneRecords);
		}
	}
	
	//TODO: There might be a more performant way of handling this
	protected static bool DuplicateNameExistAlready(string simpleBoneName, map<string, ref DAB_BoneRecord> outBoneRecords)
	{
		foreach(string compundBoneName, ref DAB_BoneRecord record : outBoneRecords)
		{
			if(record.GetSimpleBoneName() == simpleBoneName) return true;
		}
		
		return false;
	}
	
	//-----------------------------------------------------------------------
    static string GetCompoundName(string simpleBoneName, array<string> slotNames)
    {
		if(simpleBoneName.IsEmpty()) return "";
		
		// This adds to the array in the main function too, since its reference based. But in this case ok to not copy.
		if(slotNames.IsEmpty()) slotNames.Insert(DAB_Constants.SLOT_ROOT_ID); //TODO: Could generated null exception
		
		string combinedSlotString;
		for(int i = 0; i < slotNames.Count(); i++)
		{
			combinedSlotString += slotNames[i];
			if(i != (slotNames.Count() - 1)) combinedSlotString += DAB_Constants.SLOT_SLOT_DELIMITER;
		}
		
        return string.Format("%1%2%3", combinedSlotString, DAB_Constants.SLOT_BONE_DELIMITER, simpleBoneName);
    }
	
	//-----------------------------------------------------------------------
	protected static map<string, string> ExtractBoneRecords(map<string, ref DAB_BoneRecord> records)
	{
		map<string, string> parentNames = new map<string, string>();
		
		foreach(string key, DAB_BoneRecord record : records)
		{
			parentNames.Insert(key, record.GetParentCompoundName());
		}
		
		return parentNames;
	}
}