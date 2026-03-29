//! Immutable snapshot of the bone-overlay display toggles used to detect setting changes.
class DAB_BoneDisplaySettings
{
	protected bool m_bHideVolumeBones;
	protected bool m_bHideCameraBone;
	protected bool m_bHideFaceBones;
	protected bool m_bHideBoneConnections;

	//-----------------------------------------------------------------------
	void DAB_BoneDisplaySettings(bool hideVolumeBones, bool hideCameraBone, bool hideFaceBones, bool hideBoneConnections)
	{
		m_bHideVolumeBones     = hideVolumeBones;
		m_bHideCameraBone      = hideCameraBone;
		m_bHideFaceBones	   = hideFaceBones;
		m_bHideBoneConnections = hideBoneConnections;
	}

	//-----------------------------------------------------------------------
	void ~DAB_BoneDisplaySettings();

	//-----------------------------------------------------------------------
	//! Returns true when \p other has the same settings as this instance.
	bool IsSame(DAB_BoneDisplaySettings other)
	{
		if (!other) return false;
		return m_bHideVolumeBones     == other.GetHideVolumeBones()
			&& m_bHideCameraBone      == other.GetHideCameraBone()
			&& m_bHideFaceBones       == other.GetHideFaceBones()
			&& m_bHideBoneConnections == other.GetHideBoneConnections();
	}

	// ── Getters ────────────────────────────────────────────────────────────
	bool GetHideVolumeBones()    { return m_bHideVolumeBones; }
	bool GetHideCameraBone()     { return m_bHideCameraBone; }
	bool GetHideFaceBones()      { return m_bHideFaceBones; }
	bool GetHideBoneConnections(){ return m_bHideBoneConnections; }
}