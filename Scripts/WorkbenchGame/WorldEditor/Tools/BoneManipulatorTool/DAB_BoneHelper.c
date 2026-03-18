class DAB_BoneHelper
{
	private void DAB_BoneHelper();
	private void ~DAB_BoneHelper();
	
	//-----------------------------------------------------------------------
	static bool TryGetBonePosition(IEntity entity, TNodeId boneId, out vector bonePosition)
	{
		bonePosition = vector.Zero;
		if (!entity) return false;
		
        Animation anim = entity.GetAnimation();
		if (!anim)
		{
			Print("TryGetBonePosition: Animation is null.", LogLevel.ERROR);
			return false;
		}
		
        vector entityWorld[4];
        entity.GetTransform(entityWorld);
		
        vector boneLocal[4];
        anim.GetBoneMatrix(boneId, boneLocal);

        vector boneWorld[4];
        Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);

        bonePosition = boneWorld[3];
		return true;
	}
	
	//-----------------------------------------------------------------------
	static bool TryGetBoneLocalRotation(IEntity entity, TNodeId boneId, out vector boneRotation)
	{
	    boneRotation = vector.Zero;
	    if (!entity) return false;
	
	    Animation anim = entity.GetAnimation();
	    if (!anim) return false;
	
	    vector boneLocal[4];
	    anim.GetBoneMatrix(boneId, boneLocal);
	
	    vector rotMat[3];
	    rotMat[0] = boneLocal[0];
	    rotMat[1] = boneLocal[1];
	    rotMat[2] = boneLocal[2];

	    vector ypr = Math3D.MatrixToAngles(rotMat);
	    boneRotation = Vector(ypr[1], ypr[0], ypr[2]);
	    return true;
	}
	
	//-----------------------------------------------------------------------
	static bool TryGetBoneWorldRotation(IEntity entity, TNodeId boneId, out vector boneRotation)
	{
	    boneRotation = vector.Zero;
	    if (!entity) return false;
	
	    Animation anim = entity.GetAnimation();
	    if (!anim) return false;
	
	    vector boneLocal[4];
	    anim.GetBoneMatrix(boneId, boneLocal);
	
	    vector entityWorld[4];
	    entity.GetWorldTransform(entityWorld);
	
	    vector boneWorld[4];
	    Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);
	
	    vector rotMat[3];
	    rotMat[0] = boneWorld[0];
	    rotMat[1] = boneWorld[1];
	    rotMat[2] = boneWorld[2];
	
	    vector ypr = Math3D.MatrixToAngles(rotMat);
	    boneRotation = Vector(ypr[1], ypr[0], ypr[2]);
	    return true;
	}
	
	// TODO: Can we do an overload that accepts anim?
	//-----------------------------------------------------------------------
	static TNodeId GetBoneId(IEntity entity, string boneName)
	{
		if (!entity)
		{
			Print("GetBoneId: no entity provided.", LogLevel.ERROR);
			return int.INVALID_INDEX;
		}
		
		Animation anim = entity.GetAnimation();
		if (!anim)
		{
			Print("GetBoneId: Animation is null.", LogLevel.ERROR);
			return int.INVALID_INDEX;
		}
		
		return anim.GetBoneIndex(boneName);	
	}
	
	//-----------------------------------------------------------------------
	static string FindParentBoneName(Animation anim, string boneName, array<string> boneNames)
	{
		TNodeId boneId = anim.GetBoneIndex(boneName);
		
	    vector boneLocal[4];
	    anim.GetBoneLocalMatrix(boneId, boneLocal);
	
	    if (boneLocal[3].LengthSq() < 0.0001) return "";
	
	    vector boneWorld[4];
	    anim.GetBoneMatrix(boneId, boneWorld);

	    float itx = -vector.Dot(boneLocal[0], boneLocal[3]);
	    float ity = -vector.Dot(boneLocal[1], boneLocal[3]);
	    float itz = -vector.Dot(boneLocal[2], boneLocal[3]);
	
	    vector parentWorldPos = boneWorld[3]
	        + boneWorld[0] * itx
	        + boneWorld[1] * ity
	        + boneWorld[2] * itz;
	
	    float bestDist = 0.001;
	    string bestName = "";
	
	    foreach (string name : boneNames)
	    {
	        TNodeId candidateId = anim.GetBoneIndex(name);
	        if (candidateId == boneId) continue;
	
	        vector candidateMat[4];
	        anim.GetBoneMatrix(candidateId, candidateMat);
	
	        float dist = vector.Distance(candidateMat[3], parentWorldPos);
	        if (dist < bestDist)
	        {
	            bestDist = dist;
	            bestName = name;
	        }
	    }
	    return bestName;
	}
	
	//-----------------------------------------------------------------------
	static map<string, string> ComputeBoneParents(Animation anim, array<string> boneNames)
	{
		map<string, string> boneParents = new map<string, string>();
		
		foreach(string boneName : boneNames)
		{
			string parentName = FindParentBoneName(anim, boneName, boneNames);
			boneParents.Set(boneName, parentName); // We add "" value for bones without parent, so we know they were processed.
		}
		
		return boneParents;
	}
}