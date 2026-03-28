[BaseContainerProps(configRoot: true)]
class DAB_PoseModification
{
    [Attribute(desc: "Name for this modification (e.g. HoldGrenade, LookLeft)")]
    string m_sName;

    [Attribute(desc: "Bone overrides that make up this modification")]
    ref array<ref DAB_BoneModification> m_aBoneModifications = {};
}