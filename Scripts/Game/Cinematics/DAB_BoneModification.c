[BaseContainerProps()]
class DAB_BoneModification
{
    [Attribute(desc: "Simple name of the bone to override")]
    string m_sBoneName;

    [Attribute(desc: "Slot names under where to find the bone entity")]
    ref array<string> m_aSlotNames;

    [Attribute("0 0 0", desc: "Rotation delta in degrees (pitch, yaw, roll)")]
    vector m_vRotationOffset;

    [Attribute("0 0 0", desc: "Position offset in world space")]
    vector m_vPositionOffset;

    [Attribute("1", desc: "Scale multiplier")]
    float m_fScale;
}