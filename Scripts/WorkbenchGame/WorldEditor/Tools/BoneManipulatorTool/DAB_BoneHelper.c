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

		boneRotation = Math3D.MatrixToAngles(rotMat);
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

	    boneRotation = Math3D.MatrixToAngles(rotMat);
	    return true;
	}
	
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
}