class DAB_BoneTransform 
{
	protected string m_sBoneName;
	protected vector m_vOriginalPosition;
	protected vector m_vOriginalRotation;
	
	vector m_vPositionOffset = vector.Zero;
	vector m_vRotationOffset = vector.Zero;
	float  m_fScale          = 1.0;
	
	//-----------------------------------------------------------------------
	void DAB_BoneTransform(string boneName, vector originalPosition, vector originalRotation)
	{
		m_sBoneName = boneName;
		m_vOriginalPosition = originalPosition;
		m_vOriginalRotation = originalRotation;
	}
	
	//-----------------------------------------------------------------------
	string GetBoneName()         { return m_sBoneName; }
	vector GetOriginalPosition() { return m_vOriginalPosition; }
	vector GetOriginalRotation() { return m_vOriginalRotation; }
	vector GetAdjustedPosition() { return m_vOriginalPosition + m_vPositionOffset; }
	vector GetAdjustedRotation() { return m_vOriginalRotation + m_vRotationOffset; }
}