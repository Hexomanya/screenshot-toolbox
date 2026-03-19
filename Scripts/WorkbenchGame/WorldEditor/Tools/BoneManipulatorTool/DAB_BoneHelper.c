class DAB_BoneHelper
{
	private void DAB_BoneHelper();
	private void ~DAB_BoneHelper();
	
	//-----------------------------------------------------------------------
	static bool TryGetBonePosition(IEntity entity, string boneName, out vector bonePosition)
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
		
        bonePosition = DAB_BoneHelper.GetBonePosition(anim, entityWorld, boneName);
		return true;
	}
	
	// Would it be better to make this Try... as well and check anim?
	//-----------------------------------------------------------------------
	static vector GetBonePosition(Animation anim, vector entityWorld[4], string boneName)
	{
		TNodeId boneId = anim.GetBoneIndex(boneName);
			
        vector boneLocal[4];
        anim.GetBoneMatrix(boneId, boneLocal);

        vector boneWorld[4];
        Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);

        return boneWorld[3];
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
	
	//-----------------------------------------------------------------------
	static map<string, float> ComputeBoneParentDistances(IEntity entity, map<string, string> boneParents)
	{
		if(!entity)
		{
			Print("Entity can not be null!", LogLevel.ERROR);
			return null;
		}
		
		map<string, float> boneParentDistances = new map<string, float>();
		map<string, vector> bonePositionCache = new map<string, vector>();
		
		Animation anim = entity.GetAnimation(); // TODO: Can this be null?
		vector entityWorld[4];
	    entity.GetTransform(entityWorld);
		
		for (MapIterator it = boneParents.Begin(); it != boneParents.End(); it = boneParents.Next(it))
		{
		    string childBoneName = boneParents.GetIteratorKey(it);
		    string parentBoneName = boneParents.GetIteratorElement(it);
		
			if(childBoneName.IsEmpty())
			{
				Print("Child bonename should never be empty!", LogLevel.ERROR);
				continue;
			}
			
		    if(parentBoneName.IsEmpty()) // Does not have a parent
			{
				boneParentDistances.Set(childBoneName, -1);
				continue;
			}
			
			vector childPos, parentPos;
			if(! bonePositionCache.Find(childBoneName, childPos)) 
			{
				childPos = DAB_BoneHelper.GetBonePosition(anim, entityWorld, childBoneName);
				bonePositionCache.Set(childBoneName, childPos);
			}
			
			if(! bonePositionCache.Find(parentBoneName, parentPos)) 
			{
				parentPos = DAB_BoneHelper.GetBonePosition(anim, entityWorld, parentBoneName);
				bonePositionCache.Set(parentBoneName, parentPos);
			}
			
			float distance = vector.Distance(childPos, parentPos);
			boneParentDistances.Set(childBoneName, distance);
		}
		
		return boneParentDistances;
	}
}