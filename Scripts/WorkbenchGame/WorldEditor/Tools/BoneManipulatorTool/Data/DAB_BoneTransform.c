//! Stores the original bone transform snapshot and all accumulated manipulation offsets
//! applied to a single bone since it was selected.
class DAB_BoneTransform
{
	// ── Identity (captured at selection time) ──────────────────────────────
	protected string m_sBoneName;
	protected vector m_vOriginalPosition;
	protected vector m_vOriginalRotation;

	// ── Accumulated offsets (mutated by gizmo dragging) ────────────────────
	vector m_vPositionOffset = vector.Zero;
	vector m_vRotationOffset = vector.Zero;
	float  m_fScale          = 1.0;

	//-----------------------------------------------------------------------
	void DAB_BoneTransform(string boneName, vector originalPosition, vector originalRotation)
	{
		m_sBoneName         = boneName;
		m_vOriginalPosition = originalPosition;
		m_vOriginalRotation = originalRotation;
	}

	// ── Getters ────────────────────────────────────────────────────────────
	//! Name of the bone this transform belongs to.
	string GetBoneName()         { return m_sBoneName; }
	//! World-space bone position at selection time.
	vector GetOriginalPosition() { return m_vOriginalPosition; }
	//! Local bone rotation (pitch, yaw, roll) at selection time.
	vector GetOriginalRotation() { return m_vOriginalRotation; }
	//! Original world position plus the accumulated position offset.
	vector GetAdjustedPosition() { return m_vOriginalPosition + m_vPositionOffset; }
	//! Original local rotation plus the accumulated rotation offset.
	vector GetAdjustedRotation() { return m_vOriginalRotation + m_vRotationOffset; }
}