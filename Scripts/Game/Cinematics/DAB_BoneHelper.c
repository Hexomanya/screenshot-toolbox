class DAB_BoneHelper
{
	private void DAB_BoneHelper();
	private void ~DAB_BoneHelper();

	//-----------------------------------------------------------------------
	//! Gets the world-space position of \p boneName on \p entity.
	//! Returns false if the entity or its Animation component is missing.
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
		bonePosition = GetBonePosition(anim, entityWorld, boneName);
		return true;
	}

	//-----------------------------------------------------------------------
	//! Returns the world-space position of \p boneName using a pre-fetched entity transform.
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
	//! Returns the bone index for \p boneName on \p entity, or int.INVALID_INDEX on failure.
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
	//! Builds a bone→parent name map for every bone in \p boneNames.
	//! Root bones map to an empty string.
	static map<string, string> ComputeBoneParents(Animation anim, array<string> boneNames)
	{
		map<string, string> boneParents = new map<string, string>();
		foreach (string boneName : boneNames)
			boneParents.Set(boneName, FindParentBoneName(anim, boneName, boneNames));
		return boneParents;
	}

	//-----------------------------------------------------------------------
	//! Builds a bone→parent-distance map (world-space metres). Bones without a parent store -1.
	static map<string, float> ComputeBoneParentDistances(IEntity entity, map<string, string> boneParents)
	{
		if (!entity)
		{
			Print("ComputeBoneParentDistances: entity cannot be null!", LogLevel.ERROR);
			return null;
		}

		map<string, float>  boneParentDistances = new map<string, float>();
		map<string, vector> positionCache        = new map<string, vector>();

		Animation anim = entity.GetAnimation();
		vector entityWorld[4];
		entity.GetTransform(entityWorld);

		for (MapIterator it = boneParents.Begin(); it != boneParents.End(); it = boneParents.Next(it))
		{
			string childName  = boneParents.GetIteratorKey(it);
			string parentName = boneParents.GetIteratorElement(it);

			if (parentName.IsEmpty())
			{
				boneParentDistances.Set(childName, -1);
				continue;
			}

			vector childPos, parentPos;
			if (!positionCache.Find(childName, childPos))
			{
				childPos = GetBonePosition(anim, entityWorld, childName);
				positionCache.Set(childName, childPos);
			}
			if (!positionCache.Find(parentName, parentPos))
			{
				parentPos = GetBonePosition(anim, entityWorld, parentName);
				positionCache.Set(parentName, parentPos);
			}

			boneParentDistances.Set(childName, vector.Distance(childPos, parentPos));
		}

		return boneParentDistances;
	}

	// ── Internal ───────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	// Finds the parent bone by back-projecting the bone's local offset to the
	// parent world position, then finding which other bone sits closest to it.
	// Returns an empty string for root bones (zero local offset).
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

		float  bestDist = 0.001;
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
}