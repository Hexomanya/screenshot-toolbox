class DAB_BoneDisplaySettings
{
    protected bool m_bHideVolumeBones;
    protected bool m_bHideCameraBone;
    protected bool m_bHideBoneConnections;

    void DAB_BoneDisplaySettings(bool hideVolumeBones, bool hideCameraBone, bool hideBoneConnections)
    {
        m_bHideVolumeBones   = hideVolumeBones;
        m_bHideCameraBone    = hideCameraBone;
        m_bHideBoneConnections = hideBoneConnections;
    }
	
	//-----------------------------------------------------------------------
	void ~DAB_BoneDisplaySettings();
	
	//-----------------------------------------------------------------------
	bool IsSame(DAB_BoneDisplaySettings other)
	{
		if(!other) return false;
		
		return m_bHideVolumeBones 		== other.GetHideVolumeBones()
			&& m_bHideCameraBone 		== other.GetHideCameraBone()
			&& m_bHideBoneConnections 	== other.GetHideBoneConnections();
	}

    //--- GETTERS -----------------------------------------------------------
    bool GetHideVolumeBones()		{ return m_bHideVolumeBones; }
    bool GetHideCameraBone()     	{ return m_bHideCameraBone; }
    bool GetHideBoneConnections()	{ return m_bHideBoneConnections; }
}