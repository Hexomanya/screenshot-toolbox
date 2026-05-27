class DAB_BoneHelper
{
	private void DAB_BoneHelper();
	private void ~DAB_BoneHelper();

	//-----------------------------------------------------------------------
	//! Gets the world-space position of \p simpleBoneName on \p entity.
	//! Returns false if the entity or its Animation component is missing.
	static bool TryGetBonePosition(IEntity entity, string simpleBoneName, out vector bonePosition)
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
		bonePosition = GetBonePosition(anim, entityWorld, simpleBoneName);
		return true;
	}
	
	//-----------------------------------------------------------------------
	//! Gets the full world-space 4×4 matrix of \p simpleBoneName on \p entity.
	//! Returns false (and logs an error) if the entity or animation is missing.
	static bool TryGetBoneWorldMatrix(IEntity entity, string simpleBoneName, out vector outWorldMat[4])
	{
	    Math3D.MatrixIdentity4(outWorldMat);
	    if (!entity)
	    {
	        Print("TryGetBoneWorldMatrix: entity is null.", LogLevel.ERROR);
	        return false;
	    }
	
	    Animation anim = entity.GetAnimation();
	    if (!anim)
	    {
	        Print("TryGetBoneWorldMatrix: Animation is null.", LogLevel.ERROR);
	        return false;
	    }
	
	    TNodeId boneId = anim.GetBoneIndex(simpleBoneName);
	
	    vector boneLocal[4];
	    anim.GetBoneMatrix(boneId, boneLocal);
	
	    vector entityWorld[4];
	    entity.GetWorldTransform(entityWorld);
	
	    Math3D.MatrixMultiply4(entityWorld, boneLocal, outWorldMat);
	    return true;
	}

	//-----------------------------------------------------------------------
	//! Returns the world-space position of \p simpleBoneName using a pre-fetched entity transform.
	static vector GetBonePosition(Animation anim, vector entityWorld[4], string simpleBoneName)
	{
		TNodeId boneId = anim.GetBoneIndex(simpleBoneName);

		vector boneLocal[4];
		anim.GetBoneMatrix(boneId, boneLocal);

		vector boneWorld[4];
		Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);
		return boneWorld[3];
	}

	//-----------------------------------------------------------------------
	//! Gets the local rotation of bone \p boneId as (pitch, yaw, roll) in degrees.
	//! Returns false if the entity or animation is missing.
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
	//! Gets the world-space rotation of bone \p boneId as (pitch, yaw, roll) in degrees.
	//! Returns false if the entity or animation is missing.
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
	
	//-----------------------------------------------------------------------
	static TNodeId GetBoneIndex(IEntity entity, string simpleBoneName, out Animation outAnim = null)
	{	
	    if (!entity)
	    {
	        Print("GetBoneIndex: no entity provided.", LogLevel.ERROR);
	        return int.INVALID_INDEX;
	    }
	
	    Animation anim = entity.GetAnimation();
	    if (!anim)
	    {
	        Print("GetBoneIndex: Animation is null.", LogLevel.ERROR);
	        return int.INVALID_INDEX;
	    }
	
	    outAnim = anim;
	    return anim.GetBoneIndex(simpleBoneName);
	}
	
	//-----------------------------------------------------------------------
    static string GetCompoundName(string simpleBoneName, array<string> slotNames)
    {
		if(simpleBoneName.IsEmpty()) return "";
		
		// This adds to the array in the main function too, since its reference based. But in this case ok to not copy.
		if(slotNames.IsEmpty()) slotNames.Insert(DAB_Constants.SLOT_ROOT_ID); //TODO: Could generated null exception
		
		string combinedSlotString;
		for(int i = 0; i < slotNames.Count(); i++)
		{
			combinedSlotString += slotNames[i];
			if(i != (slotNames.Count() - 1)) combinedSlotString += DAB_Constants.SLOT_SLOT_DELIMITER;
		}
		
        return string.Format("%1%2%3", combinedSlotString, DAB_Constants.SLOT_BONE_DELIMITER, simpleBoneName);
    }

	//-----------------------------------------------------------------------
	//Parent bones are always on the same entity as the entity, so we can just use normal simpleBoneNames
	static string FindParentBoneName(Animation anim, string childSimpleBoneName, array<string> simpleBoneNames)
	{
		TNodeId boneId = anim.GetBoneIndex(childSimpleBoneName);
	
		vector boneLocal[4];
		anim.GetBoneLocalMatrix(boneId, boneLocal);
		if (boneLocal[3].LengthSq() < 0.0001) return "";
	
		vector boneWorld[4];
		anim.GetBoneMatrix(boneId, boneWorld);
	
		float bestDist = 0.001;
		string bestName = "";
	
		foreach (string name : simpleBoneNames)
		{
			TNodeId candidateId = anim.GetBoneIndex(name);
			if (candidateId == boneId) continue;
	
			vector candidateMat[4];
			anim.GetBoneMatrix(candidateId, candidateMat);
	
			vector relativeMat[4];
			Math3D.MatrixInvMultiply4(candidateMat, boneWorld, relativeMat);
	
			float dist = vector.Distance(relativeMat[3], boneLocal[3]);
			if (dist < bestDist)
			{
				bestDist = dist;
				bestName = name;
			}
		}
		
		if(childSimpleBoneName == bestName)
			PrintFormat("Found parent with the same boneName for %1!", childSimpleBoneName, LogLevel.WARNING);
		
	
		return bestName;
	}
}