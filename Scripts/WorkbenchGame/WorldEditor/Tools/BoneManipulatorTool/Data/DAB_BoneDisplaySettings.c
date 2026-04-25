//! Immutable snapshot of the bone-overlay display toggles used to detect setting changes.
class DAB_BoneDisplaySettings
{
	protected bool   m_bHideIKTargetBones;
	protected bool   m_bHidePropBones;
	protected bool   m_bHideVolumeBones;
	protected bool   m_bHideCameraBone;
	protected bool   m_bHideFaceBones;
	protected bool   m_bHideBoneConnections;
	protected bool   m_bHideDuplicateNamedSlotBones; 
	protected string m_sFilterBoneName;

	//-----------------------------------------------------------------------
	void DAB_BoneDisplaySettings(bool hideIKTargetBones, bool hidePropBones, bool hideVolumeBones, bool hideCameraBone, bool hideFaceBones, bool hideBoneConnections, bool hideDuplicateNamedSlotBones, string filterBoneName = "")
	{
		m_bHideIKTargetBones           = hideIKTargetBones;
		m_bHidePropBones               = hidePropBones;
		m_bHideVolumeBones             = hideVolumeBones;
		m_bHideCameraBone              = hideCameraBone;
		m_bHideFaceBones               = hideFaceBones;
		m_bHideBoneConnections         = hideBoneConnections;
		m_bHideDuplicateNamedSlotBones = hideDuplicateNamedSlotBones; 
		m_sFilterBoneName              = filterBoneName;
	}

	//-----------------------------------------------------------------------
	void ~DAB_BoneDisplaySettings();

	//-----------------------------------------------------------------------
	bool IsSame(DAB_BoneDisplaySettings other)
	{
		if (!other) return false;
		return m_bHideIKTargetBones           == other.GetHideIKTargetBones()
			&& m_bHidePropBones               == other.GetHidePropBones()
			&& m_bHideVolumeBones             == other.GetHideVolumeBones()
			&& m_bHideCameraBone              == other.GetHideCameraBone()
			&& m_bHideFaceBones               == other.GetHideFaceBones()
			&& m_bHideBoneConnections         == other.GetHideBoneConnections()
			&& m_bHideDuplicateNamedSlotBones == other.GetHideDuplicateNamedSlotBones() 
			&& m_sFilterBoneName              == other.GetFilterBoneName();
	}

	// ── Getters ────────────────────────────────────────────────────────────
	bool   GetHideIKTargetBones()         { return m_bHideIKTargetBones; }
	bool   GetHidePropBones()             { return m_bHidePropBones; }
	bool   GetHideVolumeBones()           { return m_bHideVolumeBones; }
	bool   GetHideCameraBone()            { return m_bHideCameraBone; }
	bool   GetHideFaceBones()             { return m_bHideFaceBones; }
	bool   GetHideBoneConnections()       { return m_bHideBoneConnections; }
	bool   GetHideDuplicateNamedSlotBones(){ return m_bHideDuplicateNamedSlotBones; } 
	string GetFilterBoneName()            { return m_sFilterBoneName; }
}