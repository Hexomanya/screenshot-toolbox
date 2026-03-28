[BaseContainerProps()]
class DAB_BoneModification
{
    [Attribute(desc: "Name of the bone to override")]
    string m_sBoneName;

    [Attribute(desc: "Slot name on the entity")]
    string m_sSlotName;

    [Attribute("0 0 0", desc: "Rotation delta in degrees (pitch, yaw, roll)")]
    vector m_vRotationOffset;

    [Attribute("0 0 0", desc: "Position offset in world space")]
    vector m_vPositionOffset;

    [Attribute("1", desc: "Scale multiplier")]
    float m_fScale;
}