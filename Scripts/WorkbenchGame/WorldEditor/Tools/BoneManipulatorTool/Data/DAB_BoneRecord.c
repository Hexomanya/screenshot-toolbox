class DAB_BoneRecord
{
    protected string m_sSimpleBoneName;
	protected ref array<string> m_aSlotNames;
    protected IEntity m_SlotEntity;
	protected string m_sParentCompoundName;

    // ── Constructors ──────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    void DAB_BoneRecord(string simpleBoneName, array<string> slotNames, IEntity slotEntity,  string parentCompoundName)
    {
        if (simpleBoneName.IsEmpty()) Print("DAB_BoneRecord: Created BoneRecord with empty simpleBoneName!", LogLevel.ERROR);
		if (slotNames.IsEmpty()) PrintFormat("DAB_BoneRecord: Created BoneRecord with no slotNames! (Simple Name: %1)", simpleBoneName, LogLevel.ERROR);
        if (!slotEntity) PrintFormat("DAB_BoneRecord: Created BoneRecord with no slotEntity! (Simple Name: %1)", simpleBoneName, LogLevel.ERROR);

        m_sSimpleBoneName = simpleBoneName;
		m_aSlotNames = slotNames;
        m_SlotEntity = slotEntity;
        m_sParentCompoundName = parentCompoundName;
    }

    //-----------------------------------------------------------------------
    void ~DAB_BoneRecord() {}

    // ── Public ────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
    TNodeId GetBoneIndex()
    {
        if (!m_SlotEntity || m_sSimpleBoneName.IsEmpty())
        {
            PrintFormat("DAB_BoneRecord.GetBoneIndex: BoneRecord is not fully configured! (SimpleName: %1, ParentCompound: %2, Entity: %3)", m_sSimpleBoneName, m_sParentCompoundName, m_SlotEntity, LogLevel.ERROR);
            return -1;
        }

        Animation anim = m_SlotEntity.GetAnimation();
        if (!anim)
        {
            PrintFormat("DAB_BoneRecord.GetBoneIndex: Entity has no animation! (SimpleName: %1, ParentCompound: %2)", m_sSimpleBoneName, m_sParentCompoundName, LogLevel.ERROR);
            return -1;
        }

        return anim.GetBoneIndex(m_sSimpleBoneName);
    }
	
    //-----------------------------------------------------------------------
    string GetSimpleBoneName()
    {
        if (m_sSimpleBoneName.IsEmpty()) Print("DAB_BoneRecord.GetSimpleBoneName: Returning empty bone name!", LogLevel.ERROR);
        return m_sSimpleBoneName;
    }
	
	//-----------------------------------------------------------------------
    array<string> GetSlotNames()
    {
        if (m_aSlotNames.IsEmpty()) Print("DAB_BoneRecord.GetSimpleBoneName: Returning empty slotNames!", LogLevel.ERROR);
        return m_aSlotNames;
    }

    //-----------------------------------------------------------------------
    IEntity GetSlotEntity()
    {
        if (!m_SlotEntity) Print("DAB_BoneRecord.GetSlotEntity: Returning null entity!", LogLevel.ERROR);
        return m_SlotEntity;
    }
	
	//-----------------------------------------------------------------------
    string GetParentCompoundName() { return m_sParentCompoundName; }
}