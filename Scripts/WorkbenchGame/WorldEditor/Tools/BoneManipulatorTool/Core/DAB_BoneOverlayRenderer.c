//! Draws all bone-sphere overlays and connection lines in the viewport.
//! Caches world positions and pre-computed size constraints to avoid
//! redundant per-frame recalculation.
class DAB_BoneOverlayRenderer
{
	// ── Caches ─────────────────────────────────────────────────────────────
	protected ref map<string, vector> m_CachedWorldPositions  = new map<string, vector>();
	protected ref map<string, float>  m_CachedSpatialMaxRadii = new map<string, float>();
	protected ref map<string, float>  m_CachedFinalMaxRadii   = new map<string, float>();

	// ── Live shapes ────────────────────────────────────────────────────────
	protected ref array<ref Shape> m_ActiveShapes     = new array<ref Shape>();
	protected ref array<ref Shape> m_ConnectionShapes = new array<ref Shape>();
	protected ref DebugTextScreenSpace m_SelectedBoneText;

	protected WorldEditorAPI m_API;

	//-----------------------------------------------------------------------
	//! Redraws all bone spheres and connection lines.
	//! Rebuilds all caches from scratch; call when the skeleton changes or the tool activates.
	void DrawAllBones(DAB_SkeletonInfo skeletonInfo, string hoveredBone, vector camPos, DAB_BoneDisplaySettings displaySettings, WorldEditorAPI api)
	{
		m_API = api;

		ClearShapes();
		ClearConnectionShapes();
		m_CachedWorldPositions.Clear();
		m_CachedSpatialMaxRadii.Clear();
		m_CachedFinalMaxRadii.Clear();

		if (!skeletonInfo)
		{
			Print("DAB_BoneOverlayRenderer.DrawAllBones: skeletonInfo is null.", LogLevel.ERROR);
			return;
		}
		
		Print("---------------------------------------------");
		Print("Stated to draw all bones");
		int start = System.GetTickCount();
		
		CacheBoneWorldPositions(skeletonInfo, displaySettings);
		int elapsedCacheMs = System.GetTickCount() - start;
		
		ComputeSpatialConstraints(skeletonInfo);
		int elapsedSpatialMs = System.GetTickCount() - start- elapsedCacheMs;
		
		CombineConstraints(camPos);
		int elapsedConstraintsMs = System.GetTickCount() - start - elapsedCacheMs - elapsedSpatialMs;
		
		int elapsedPrecomputeMs = System.GetTickCount() - start;

		if (!displaySettings.GetHideBoneConnections())
			DrawBoneConnections(skeletonInfo);

		DrawBoneShapes(hoveredBone);
		
		int elapsedTotalMs = System.GetTickCount() - start;
		PrintFormat("DrawAllBones cache took %1ms", elapsedCacheMs);
		PrintFormat("DrawAllBones spatial took %1ms", elapsedSpatialMs);
		PrintFormat("DrawAllBones constraint took %1ms", elapsedConstraintsMs);
		PrintFormat("DrawAllBones precompute took %1ms", elapsedPrecomputeMs);
		PrintFormat("DrawAllBones total took %1ms", elapsedTotalMs);
	}

	//-----------------------------------------------------------------------
	//! Re-scales all spheres for the new camera position without rebuilding caches.
	//! Call when the camera moves but the skeleton has not changed.
	void RefreshSphereSizes(string hoveredBone, WorldEditorAPI api)
	{
		if (m_CachedWorldPositions.IsEmpty()) return;
		
		m_API = api; // TODO: Why do we need to cache here again? Past Benni did not tell me
		
		CombineConstraints(DAB_Helper.GetCameraPosition(api));
		ClearShapes();
		DrawBoneShapes(hoveredBone);
	}

	//-----------------------------------------------------------------------
	//! Draws a single highlighted sphere at the selected bone's current world position.
	void DrawSelectedBone(DAB_SkeletonInfo skeletonInfo, string selectedCompoundBoneName, WorldEditorAPI api)
	{
		ClearShapes();
		ClearConnectionShapes();

		IEntity slotEntity = skeletonInfo.GetSlotEntity(selectedCompoundBoneName);
		if (!slotEntity)
		{
			Print("DAB_BoneOverlayRenderer.DrawSelectedBone: slotEntity is null.", LogLevel.ERROR);
			return;
		}
		
		string niceCompoundName = skeletonInfo.GetNiceCompoundName(selectedCompoundBoneName);
		string simpleBoneName = skeletonInfo.GetSimpleBoneName(selectedCompoundBoneName);

		m_SelectedBoneText = DebugTextScreenSpace.Create(api.GetWorld(), niceCompoundName, DebugTextFlags.FACE_CAMERA, 10, 10);
		
		vector boneWorld[4];
		if (!TryGetBoneWorldMatrix(slotEntity, simpleBoneName, boneWorld))
			return;

		float maxRadius;
		if (!m_CachedFinalMaxRadii.Find(selectedCompoundBoneName, maxRadius))
			maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

		float sphereRadius = Math.Min(
			DAB_VisConfig.ComputeBoneSphereRadius(api, boneWorld[3]),
			maxRadius
		) * DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;

		m_ActiveShapes.Insert(Shape.CreateSphere(
			DAB_VisConfig.COLOR_SELECTED,
			ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP,
			boneWorld[3],
			sphereRadius
		));
	}

	//-----------------------------------------------------------------------
	//! Returns the compound name of the bone whose sphere is hit by a ray through (mouseX, mouseY),
	//! or an empty string on miss.
	string PickBoneAtScreenPos(int mouseX, int mouseY, WorldEditorAPI api)
	{
		if (m_CachedWorldPositions.IsEmpty()) return "";

		vector traceStart, traceEnd, traceDir;
		api.TraceWorldPos(mouseX, mouseY, TraceFlags.ENTS, traceStart, traceEnd, traceDir);

		float  bestT    = float.MAX;
		string bestBone = "";

		
		foreach(string compoundBoneName, vector bonePosition : m_CachedWorldPositions)
		{
			if (bonePosition == vector.Zero) continue;

			float maxRadius;
			if (!m_CachedFinalMaxRadii.Find(compoundBoneName, maxRadius))
				maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

			float radius = Math.Min(DAB_VisConfig.ComputeBoneSphereRadius(api, bonePosition), maxRadius);

			float t;
			if (RaySphereIntersect(traceStart, traceDir, bonePosition, radius, t) && t < bestT)
			{
				bestT    = t;
				bestBone = compoundBoneName;
			}
		}

		return bestBone;
	}

	//-----------------------------------------------------------------------
	//! Clears all shapes and cached data.
	void Clear()
	{
		ClearShapes();
		ClearConnectionShapes();
		m_CachedWorldPositions.Clear();
		m_CachedSpatialMaxRadii.Clear();
		m_CachedFinalMaxRadii.Clear();
	}

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	protected void CacheBoneWorldPositions(DAB_SkeletonInfo skeletonInfo, DAB_BoneDisplaySettings displaySettings)
	{
		foreach (string compoundBoneName, DAB_BoneRecord record : skeletonInfo.GetBoneRecords())
		{
			string boneName = DAB_SkeletonInfo.ExtractBoneNameFromCompundName(compoundBoneName);
			string lowercaseBoneName = boneName;
			lowercaseBoneName.ToLower();
			
			if (displaySettings.GetHideIKTargetBones() && lowercaseBoneName.Contains("_ik")) continue;
			if (displaySettings.GetHideVolumeBones() && lowercaseBoneName.EndsWith("prop")) continue;
			if (displaySettings.GetHideVolumeBones() && lowercaseBoneName.EndsWith("volume")) continue;
			if (displaySettings.GetHideCameraBone() && lowercaseBoneName == "camera") continue;
			if (displaySettings.GetHideFaceBones() && skeletonInfo.IsAncestorOf(boneName, "Head")) continue;
			
			if(!displaySettings.GetFilterBoneName().IsEmpty())
			{
				string lowercaseFilterName = displaySettings.GetFilterBoneName().Trim();
				lowercaseFilterName.ToLower();
				if (!lowercaseBoneName.Contains(lowercaseFilterName)) continue;
			}
			
			IEntity slotEntity = record.GetSlotEntity();
			if(!slotEntity)
			{
				PrintFormat("Bone '%1' slotEntity is null!", compoundBoneName, LogLevel.ERROR);
				continue;
			}
			
			Animation anim = slotEntity.GetAnimation();
			if(!anim)
			{
				PrintFormat("Bone '%1' slotEntity has no animation!", compoundBoneName, LogLevel.ERROR);
				continue;
			}
			
			TNodeId boneId = anim.GetBoneIndex(boneName);
			if (boneId == -1) continue;

			vector boneLocal[4];
			anim.GetBoneMatrix(boneId, boneLocal);

			vector entityWorld[4];
			slotEntity.GetTransform(entityWorld);
			
			vector boneWorld[4];
			Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);
			m_CachedWorldPositions.Set(compoundBoneName, boneWorld[3]);
		}
	}

	//-----------------------------------------------------------------------
	protected void ComputeSpatialConstraints(DAB_SkeletonInfo skeletonInfo)
	{
		map<string, string> boneParents = skeletonInfo.GetBoneParents();
		foreach(string compoundBoneName, vector bonePositon : m_CachedWorldPositions)
		{
			m_CachedSpatialMaxRadii.Set(compoundBoneName, ComputeSpatialConstraint(compoundBoneName, bonePositon, boneParents));
		}
	}

	//-----------------------------------------------------------------------
	// Merges stable spatial constraints with a freshly computed angular constraint.
	// Reads from m_CachedSpatialMaxRadii so repeated calls never compound the angular cap.
	protected void CombineConstraints(vector camPosition)
	{
		foreach(string compoundBoneName, vector bonePositon : m_CachedWorldPositions)
		{
			float spatial, finalRadius;
			m_CachedSpatialMaxRadii.Find(compoundBoneName, spatial);
			
			finalRadius = Math.Min(spatial, ComputeAngularConstraint(bonePositon, camPosition));
			m_CachedFinalMaxRadii.Set(compoundBoneName, Math.Max(finalRadius, DAB_VisConfig.SPHERE_MIN_RADIUS));
		}
	}

	//-----------------------------------------------------------------------
	protected float ComputeSpatialConstraint(string compoundBoneNameA, vector bonePositionA, map<string, string> boneParents)
	{
		string boneParent = "";
		boneParents.Find(compoundBoneNameA, boneParent);

		float minConstraint = DAB_VisConfig.SPHERE_MAX_RADIUS;

		
		foreach(string compoundBoneNameB, vector bonePositonB : m_CachedWorldPositions)
		{
			if (compoundBoneNameB == compoundBoneNameA) continue;

			string parentOfB = "";
			boneParents.Find(compoundBoneNameB, parentOfB);
			if (parentOfB == compoundBoneNameA) continue;

			float dist = vector.Distance(bonePositionA, bonePositonB);
			if (dist < 0.0001) continue;

			float constraint = dist * (1.0 - DAB_VisConfig.SPHERE_GAP_FRACTION) * GetSpaceShare(compoundBoneNameB, boneParent);
			if (constraint < minConstraint)
				minConstraint = constraint;
		}

		return minConstraint;
	}

	//-----------------------------------------------------------------------
	protected float ComputeAngularConstraint(vector bonePosition, vector cameraPosition)
	{
		vector dirA  = (bonePosition - cameraPosition).Normalized();
		float  distA = vector.Distance(cameraPosition, bonePosition);
		if (distA < 0.0001) return DAB_VisConfig.SPHERE_MAX_RADIUS;

		float minHalfAngle = float.MAX;

		foreach(string compoundBoneNameB, vector bonePositonB : m_CachedWorldPositions)
		{
			if (bonePositonB == bonePosition) continue;

			float theta     = Math.Acos(Math.Clamp(vector.Dot(dirA, (bonePositonB - cameraPosition).Normalized()), -1.0, 1.0));
			float halfAngle = theta * 0.5 * (1.0 - DAB_VisConfig.SPHERE_GAP_FRACTION);
			if (halfAngle < minHalfAngle)
				minHalfAngle = halfAngle;
		}

		if (minHalfAngle == float.MAX)
			return DAB_VisConfig.SPHERE_MAX_RADIUS;

		return Math.Min(minHalfAngle * distA, DAB_VisConfig.SPHERE_MAX_RADIUS);
	}

	//-----------------------------------------------------------------------
	protected float GetSpaceShare(string nameB, string parentOfA)
	{
		if (nameB == parentOfA)
			return DAB_VisConfig.SPHERE_CHILD_SHARE;
		return 0.5;
	}

	//-----------------------------------------------------------------------
	protected void DrawBoneConnections(DAB_SkeletonInfo skeletonInfo)
	{		
		map<string, string> boneParents = skeletonInfo.GetBoneParents();
		
		foreach(string childCompoundName, vector childPosition : m_CachedWorldPositions)
		{
			string parentCompoundName = "";
			if (!boneParents.Find(childCompoundName, parentCompoundName) || parentCompoundName.IsEmpty()) 
			{
				//PrintFormat("Could no find a parent for: %1", childCompoundName);
				continue;
			}

			vector parentPosition;
			if (!m_CachedWorldPositions.Find(parentCompoundName, parentPosition))
			{
				PrintFormat("Could no find a parent position for: %1", parentCompoundName, LogLevel.WARNING);
				continue;
			}

			m_ConnectionShapes.Insert(Shape.Create(ShapeType.LINE, DAB_VisConfig.COLOR_BONE_CONNECTION, ShapeFlags.NOZBUFFER, childPosition, parentPosition));
		}
	}

	//-----------------------------------------------------------------------
	protected void DrawBoneShapes(string hoveredBone)
	{
		foreach(string boneCompoundName, vector position : m_CachedWorldPositions)
		{
			float maxRadius;
			if (!m_CachedFinalMaxRadii.Find(boneCompoundName, maxRadius))
				maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

			float sphereRadius = Math.Min(DAB_VisConfig.ComputeBoneSphereRadius(m_API, position), maxRadius);
			int color = DAB_VisConfig.COLOR_DEFAULT;

			if (boneCompoundName == hoveredBone)
			{
				sphereRadius *= DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;
				color = DAB_VisConfig.COLOR_HOVER;
			}

			m_ActiveShapes.Insert(Shape.CreateSphere(color, DAB_VisConfig.SPHERE_FLAGS, position, sphereRadius));
		}
	}

	//-----------------------------------------------------------------------
	protected bool TryGetBoneWorldMatrix(IEntity entity, string simpleBoneName, out vector boneWorld[4])
	{
		Animation anim;
		TNodeId boneId = DAB_SkeletonInfo.GetBoneIndex(entity, simpleBoneName, anim);
		if (boneId == -1) return false;

		vector boneLocal[4];
		anim.GetBoneMatrix(boneId, boneLocal);

		vector entityWorld[4];
		entity.GetTransform(entityWorld);

		Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);
		return true;
	}

	//-----------------------------------------------------------------------
	protected bool RaySphereIntersect(vector ro, vector rd, vector sc, float r, out float t)
	{
		vector oc   = ro - sc;
		float  b    = vector.Dot(oc, rd);
		float  c    = vector.Dot(oc, oc) - r * r;
		float  disc = b * b - c;
		if (disc < 0) return false;
		t = -b - Math.Sqrt(disc);
		return t >= 0;
	}

	//-----------------------------------------------------------------------
	protected void ClearShapes()
	{
		m_ActiveShapes.Clear();
		m_SelectedBoneText = null;
	}

	//-----------------------------------------------------------------------
	protected void ClearConnectionShapes()
	{
		m_ConnectionShapes.Clear();
	}
}