[BaseContainerProps(configRoot: true)]
class DAB_PoseModification
{
    [Attribute(desc: "Name for this modification (e.g. HoldGrenade, LookLeft)")]
    string m_sName;

    [Attribute(desc: "Bone overrides that make up this modification")]
    ref array<ref DAB_BoneModification> m_aBoneModifications = {}; // Would be better as map, but maps do not work here
	
	map<string, ref DAB_BoneModification> GetBoneModificationsAsMap()
	{
		map<string, ref DAB_BoneModification> boneModificactions = new map<string, ref DAB_BoneModification>();
		foreach(ref DAB_BoneModification boneMod : m_aBoneModifications)
		{
			if(boneModificactions.Contains(boneMod.m_sBoneName))
			{
				Print("Found two modifications for the same bone! This should not happen. Please report!", LogLevel.WARNING);
				continue;
			}
			
			boneModificactions.Set(boneMod.m_sBoneName, boneMod);
		}
		
		return boneModificactions;
	}
}