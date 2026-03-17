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
	
	    // MatrixToAngles returns Vector(yaw, pitch, roll).
	    // Store as Vector(pitch, yaw, roll) to match the convention
	    // used by m_vRotationOffset and expected by Attach/UpdateGizmo.
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
	
	    // MatrixToAngles returns Vector(yaw, pitch, roll).
	    // Store as Vector(pitch, yaw, roll) to match the convention
	    // used by m_vRotationOffset and expected by Attach/UpdateGizmo.
	    vector ypr = Math3D.MatrixToAngles(rotMat);
	    boneRotation = Vector(ypr[1], ypr[0], ypr[2]);
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
	
	//-----------------------------------------------------------------------
	static string FindParentBoneName(Animation anim, TNodeId boneId, array<string> allBoneNames)
	{
	    vector boneLocal[4];
	    anim.GetBoneLocalMatrix(boneId, boneLocal);
	
	    if (boneLocal[3].LengthSq() < 0.0001) return "";
	
	    vector boneWorld[4];
	    anim.GetBoneMatrix(boneId, boneWorld);
	
	    // parentWorld = boneWorld * inverse(boneLocal)
	    // For orthonormal matrix: inverse translation = -R^T * t = (-dot(ax0,t), -dot(ax1,t), -dot(ax2,t))
	    float itx = -vector.Dot(boneLocal[0], boneLocal[3]);
	    float ity = -vector.Dot(boneLocal[1], boneLocal[3]);
	    float itz = -vector.Dot(boneLocal[2], boneLocal[3]);
	
	    vector parentWorldPos = boneWorld[3]
	        + boneWorld[0] * itx
	        + boneWorld[1] * ity
	        + boneWorld[2] * itz;
	
	    float bestDist = 0.001;
	    string bestName = "";
	
	    foreach (string name : allBoneNames)
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
}