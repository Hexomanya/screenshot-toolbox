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
	void DrawAllBones(IEntity entity, DAB_SkeletonInfo skeletonInfo, string hoveredBone, vector camPos, DAB_BoneDisplaySettings displaySettings, WorldEditorAPI api)
	{
		m_API = api;

		ClearShapes();
		ClearConnectionShapes();
		m_CachedWorldPositions.Clear();
		m_CachedSpatialMaxRadii.Clear();
		m_CachedFinalMaxRadii.Clear();

		if (!entity || !skeletonInfo)
		{
			Print("DAB_BoneOverlayRenderer.DrawAllBones: entity or skeletonInfo is null.", LogLevel.ERROR);
			return;
		}

		Animation anim = entity.GetAnimation();
		if (!anim)
		{
			Print("DAB_BoneOverlayRenderer.DrawAllBones: animation is null.", LogLevel.ERROR);
			return;
		}

		vector entityWorld[4];
		entity.GetTransform(entityWorld);

		CacheBoneWorldPositions(anim, entityWorld, skeletonInfo, displaySettings);
		ComputeSpatialConstraints(skeletonInfo);
		CombineConstraints(camPos);

		if (!displaySettings.GetHideBoneConnections())
			DrawBoneConnections(skeletonInfo);

		DrawBoneShapes(hoveredBone);
	}

	//-----------------------------------------------------------------------
	//! Re-scales all spheres for the new camera position without rebuilding caches.
	//! Call when the camera moves but the skeleton has not changed.
	void RefreshSphereSizes(string hoveredBone, vector camPos, WorldEditorAPI api)
	{
		if (m_CachedWorldPositions.IsEmpty()) return;
		m_API = api;
		CombineConstraints(camPos);
		ClearShapes();
		DrawBoneShapes(hoveredBone);
	}

	//-----------------------------------------------------------------------
	//! Draws a single highlighted sphere at the selected bone's current world position.
	void DrawSelectedBone(IEntity entity, string selectedBoneName, WorldEditorAPI api)
	{
		ClearShapes();
		ClearConnectionShapes();

		if (!entity)
		{
			Print("DAB_BoneOverlayRenderer.DrawSelectedBone: entity is null.", LogLevel.ERROR);
			return;
		}

		m_SelectedBoneText = DebugTextScreenSpace.Create(api.GetWorld(), selectedBoneName, DebugTextFlags.FACE_CAMERA, 10, 10);

		vector boneWorld[4];
		if (!TryGetBoneWorldMatrix(entity, selectedBoneName, boneWorld))
			return;

		float maxRadius;
		if (!m_CachedFinalMaxRadii.Find(selectedBoneName, maxRadius))
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
	//! Returns the name of the bone whose sphere is hit by a ray through (mouseX, mouseY),
	//! or an empty string on miss.
	string PickBoneAtScreenPos(int mouseX, int mouseY, IEntity entity, WorldEditorAPI api)
	{
		if (m_CachedWorldPositions.IsEmpty()) return "";

		vector traceStart, traceEnd, traceDir;
		api.TraceWorldPos(mouseX, mouseY, TraceFlags.ENTS, traceStart, traceEnd, traceDir);

		float  bestT    = float.MAX;
		string bestBone = "";

		for (MapIterator it = m_CachedWorldPositions.Begin(); it != m_CachedWorldPositions.End(); it = m_CachedWorldPositions.Next(it))
		{
			string boneName = m_CachedWorldPositions.GetIteratorKey(it);
			vector position = m_CachedWorldPositions.GetIteratorElement(it);
			if (position == vector.Zero) continue;

			float maxRadius;
			if (!m_CachedFinalMaxRadii.Find(boneName, maxRadius))
				maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

			float radius = Math.Min(DAB_VisConfig.ComputeBoneSphereRadius(api, position), maxRadius);

			float t;
			if (RaySphereIntersect(traceStart, traceDir, position, radius, t) && t < bestT)
			{
				bestT    = t;
				bestBone = boneName;
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
	protected void CacheBoneWorldPositions(Animation anim, vector entityWorld[4], DAB_SkeletonInfo skeletonInfo, DAB_BoneDisplaySettings displaySettings)
	{
		foreach (string boneName : skeletonInfo.GetBoneNames())
		{
			if (displaySettings.GetHideVolumeBones() && boneName.EndsWith("Volume")) continue;
			if (displaySettings.GetHideCameraBone() && boneName == "Camera") continue;
			if (displaySettings.GetHideFaceBones() && skeletonInfo.IsDescendantOf(boneName, "Head")) continue;
			
			TNodeId boneId = anim.GetBoneIndex(boneName);
			if (boneId == -1) continue;

			vector boneLocal[4];
			anim.GetBoneMatrix(boneId, boneLocal);

			vector boneWorld[4];
			Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);
			m_CachedWorldPositions.Set(boneName, boneWorld[3]);
		}
	}

	//-----------------------------------------------------------------------
	protected void ComputeSpatialConstraints(DAB_SkeletonInfo skeletonInfo)
	{
		map<string, string> boneParents = skeletonInfo.GetBoneParents();
		for (MapIterator it = m_CachedWorldPositions.Begin(); it != m_CachedWorldPositions.End(); it = m_CachedWorldPositions.Next(it))
		{
			string boneName = m_CachedWorldPositions.GetIteratorKey(it);
			vector bonePos  = m_CachedWorldPositions.GetIteratorElement(it);
			m_CachedSpatialMaxRadii.Set(boneName, ComputeSpatialConstraint(boneName, bonePos, boneParents));
		}
	}

	//-----------------------------------------------------------------------
	// Merges stable spatial constraints with a freshly computed angular constraint.
	// Reads from m_CachedSpatialMaxRadii so repeated calls never compound the angular cap.
	protected void CombineConstraints(vector camPos)
	{
		for (MapIterator it = m_CachedWorldPositions.Begin(); it != m_CachedWorldPositions.End(); it = m_CachedWorldPositions.Next(it))
		{
			string boneName = m_CachedWorldPositions.GetIteratorKey(it);
			vector bonePos  = m_CachedWorldPositions.GetIteratorElement(it);

			float spatial;
			m_CachedSpatialMaxRadii.Find(boneName, spatial);
			m_CachedFinalMaxRadii.Set(boneName, Math.Min(spatial, ComputeAngularConstraint(bonePos, camPos)));
		}
	}

	//-----------------------------------------------------------------------
	protected float ComputeSpatialConstraint(string boneName, vector bonePos, map<string, string> boneParents)
	{
		string myParent = "";
		boneParents.Find(boneName, myParent);

		float minConstraint = DAB_VisConfig.SPHERE_MAX_RADIUS;

		for (MapIterator itB = m_CachedWorldPositions.Begin(); itB != m_CachedWorldPositions.End(); itB = m_CachedWorldPositions.Next(itB))
		{
			string nameB = m_CachedWorldPositions.GetIteratorKey(itB);
			if (nameB == boneName) continue;

			string parentOfB = "";
			boneParents.Find(nameB, parentOfB);
			if (parentOfB == boneName) continue;

			float dist = vector.Distance(bonePos, m_CachedWorldPositions.GetIteratorElement(itB));
			if (dist < 0.0001) continue;

			float constraint = dist * (1.0 - DAB_VisConfig.SPHERE_GAP_FRACTION) * GetSpaceShare(nameB, myParent);
			if (constraint < minConstraint)
				minConstraint = constraint;
		}

		return minConstraint;
	}

	//-----------------------------------------------------------------------
	protected float ComputeAngularConstraint(vector bonePos, vector camPos)
	{
		vector dirA  = (bonePos - camPos).Normalized();
		float  distA = vector.Distance(camPos, bonePos);
		if (distA < 0.0001) return DAB_VisConfig.SPHERE_MAX_RADIUS;

		float minHalfAngle = float.MAX;

		for (MapIterator itB = m_CachedWorldPositions.Begin(); itB != m_CachedWorldPositions.End(); itB = m_CachedWorldPositions.Next(itB))
		{
			vector posB = m_CachedWorldPositions.GetIteratorElement(itB);
			if (posB == bonePos) continue;

			float theta     = Math.Acos(Math.Clamp(vector.Dot(dirA, (posB - camPos).Normalized()), -1.0, 1.0));
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

		for (MapIterator it = m_CachedWorldPositions.Begin(); it != m_CachedWorldPositions.End(); it = m_CachedWorldPositions.Next(it))
		{
			string childName = m_CachedWorldPositions.GetIteratorKey(it);
			vector childPos  = m_CachedWorldPositions.GetIteratorElement(it);

			string parentName = "";
			if (!boneParents.Find(childName, parentName) || parentName.IsEmpty()) continue;

			vector parentPos;
			if (!m_CachedWorldPositions.Find(parentName, parentPos)) continue;

			m_ConnectionShapes.Insert(Shape.Create(ShapeType.LINE, DAB_VisConfig.COLOR_BONE_CONNECTION, ShapeFlags.NOZBUFFER, childPos, parentPos));
		}
	}

	//-----------------------------------------------------------------------
	protected void DrawBoneShapes(string hoveredBone)
	{
		for (MapIterator it = m_CachedWorldPositions.Begin(); it != m_CachedWorldPositions.End(); it = m_CachedWorldPositions.Next(it))
		{
			string boneName = m_CachedWorldPositions.GetIteratorKey(it);
			vector pos      = m_CachedWorldPositions.GetIteratorElement(it);

			float maxRadius;
			if (!m_CachedFinalMaxRadii.Find(boneName, maxRadius))
				maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

			float sphereRadius = Math.Min(DAB_VisConfig.ComputeBoneSphereRadius(m_API, pos), maxRadius);
			int   color        = DAB_VisConfig.COLOR_DEFAULT;

			if (boneName == hoveredBone)
			{
				sphereRadius *= DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;
				color         = DAB_VisConfig.COLOR_HOVER;
			}

			m_ActiveShapes.Insert(Shape.CreateSphere(color, DAB_VisConfig.SPHERE_FLAGS, pos, sphereRadius));
		}
	}

	//-----------------------------------------------------------------------
	protected bool TryGetBoneWorldMatrix(IEntity entity, string boneName, out vector boneWorld[4])
	{
		Animation anim = entity.GetAnimation();
		if (!anim) return false;

		TNodeId boneId = anim.GetBoneIndex(boneName);
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