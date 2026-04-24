//! Draws all bone-sphere overlays and connection lines in the viewport.
class DAB_BoneOverlayRenderer
{
	// ── Member Variables ───────────────────────────────────────────────────
	protected ref map<string, vector> m_mCachedWorldPositions = new map<string, vector>();
	protected ref map<string, float> m_mCachedSpatialMaxRadii = new map<string, float>();
	protected ref map<string, float> m_mCachedFinalMaxRadii = new map<string, float>();
	
	// ── Fast Iterator Caches ───────────────────────────────────────────────
	protected ref array<string> m_aActiveBoneNames = new array<string>();
	protected ref array<vector> m_aActiveBonePositions = new array<vector>();
	protected ref array<string> m_aActiveBoneParents = new array<string>();
	protected ref array<float> m_aCachedSpatialRadii = new array<float>();

	// ── Live shapes ────────────────────────────────────────────────────────
	protected ref array<ref Shape> m_aActiveShapes = new array<ref Shape>();
	protected ref array<ref Shape> m_aConnectionShapes = new array<ref Shape>();
	protected DebugTextScreenSpace m_SelectedBoneText;

	protected WorldEditorAPI m_API;

	// ── Public ─────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	//! Redraws all bone spheres and connection lines.
	//! Rebuilds all caches from scratch; call when the skeleton changes or the tool activates.
	void DrawAllBones(DAB_SkeletonInfo skeletonInfo, string hoveredBone, vector camPos, DAB_BoneDisplaySettings displaySettings, WorldEditorAPI api)
	{
		m_API = api;

		Clear();

		if (!skeletonInfo)
		{
			Print("DAB_BoneOverlayRenderer.DrawAllBones: skeletonInfo is null.", LogLevel.ERROR);
			return;
		}
		
		CacheBoneWorldPositions(skeletonInfo, displaySettings);
		ComputeSpatialConstraints(skeletonInfo);
		CombineConstraints(camPos);

		if (!displaySettings.GetHideBoneConnections())
			DrawBoneConnections();

		DrawBoneShapes(hoveredBone);
	}

	//-----------------------------------------------------------------------
	//! Re-scales all spheres for the new camera position without rebuilding caches.
	//! Call when the camera moves but the skeleton has not changed.
	void RefreshSphereSizes(string hoveredBone, WorldEditorAPI api)
	{
		if (m_mCachedWorldPositions.IsEmpty()) return;
		
		m_API = api; // TODO: Remember why exactly we are reassigning here!
		
		CombineConstraints(DAB_Helper.GetCameraPosition(m_API));
		
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
		if (!m_mCachedFinalMaxRadii.Find(selectedCompoundBoneName, maxRadius))
			maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

		float sphereRadius = Math.Min(
			DAB_VisConfig.ComputeBoneSphereRadius(api, boneWorld[3]),
			maxRadius
		) * DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;

		m_aActiveShapes.Insert(Shape.CreateSphere(
			DAB_VisConfig.COLOR_SELECTED,
			ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP,
			boneWorld[3],
			sphereRadius
		));
	}

	//-----------------------------------------------------------------------
	//! Returns the compound name of the bone whose sphere is hit by a ray through (mouseX, mouseY), or an empty string on miss.
	string PickBoneAtScreenPos(int mouseX, int mouseY, WorldEditorAPI api)
	{
		if (m_mCachedWorldPositions.IsEmpty()) return "";

		vector traceStart, traceEnd, traceDir;
		api.TraceWorldPos(mouseX, mouseY, TraceFlags.ENTS, traceStart, traceEnd, traceDir);

		float  bestT    = float.MAX;
		string bestBone = "";

		foreach(string compoundBoneName, vector bonePosition : m_mCachedWorldPositions)
		{
			if (bonePosition == vector.Zero) continue;

			float maxRadius;
			if (!m_mCachedFinalMaxRadii.Find(compoundBoneName, maxRadius))
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
		m_mCachedWorldPositions.Clear();
		m_mCachedSpatialMaxRadii.Clear();
		m_mCachedFinalMaxRadii.Clear();	
		m_aActiveBoneNames.Clear();
		m_aActiveBonePositions.Clear();
		m_aActiveBoneParents.Clear();
		m_aCachedSpatialRadii.Clear();
	}

	// ── Protected / Private ────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected void CacheBoneWorldPositions(DAB_SkeletonInfo skeletonInfo, DAB_BoneDisplaySettings displaySettings)
	{
		map<string, string> boneParents = skeletonInfo.GetBoneParents();
		
		foreach (string compoundBoneName, DAB_BoneRecord record : skeletonInfo.GetBoneRecords())
		{
			string boneName = DAB_SkeletonInfo.ExtractBoneNameFromCompundName(compoundBoneName);
			if (!ShouldDisplayBone(compoundBoneName, boneName, displaySettings, skeletonInfo))
			    continue;
			
			IEntity slotEntity = record.GetSlotEntity();
			if(!slotEntity)
			{
				PrintFormat("DAB_BoneOverlayRenderer.CacheBoneWorldPositions: Bone '%1' slotEntity is null!", compoundBoneName, LogLevel.ERROR);
				continue;
			}
			
			Animation anim = slotEntity.GetAnimation();
			if(!anim)
			{
				PrintFormat("DAB_BoneOverlayRenderer.CacheBoneWorldPositions: Bone '%1' slotEntity has no animation!", compoundBoneName, LogLevel.ERROR);
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
			m_mCachedWorldPositions.Set(compoundBoneName, boneWorld[3]);
			
			m_aActiveBoneNames.Insert(compoundBoneName);
			m_aActiveBonePositions.Insert(boneWorld[3]);
			string parent = "";
			boneParents.Find(compoundBoneName, parent);
			m_aActiveBoneParents.Insert(parent);
		}
	}

	//-----------------------------------------------------------------------
	protected void ComputeSpatialConstraints(DAB_SkeletonInfo skeletonInfo)
	{
		int n = m_aActiveBoneNames.Count();
		if (n == 0) return;
		
		float gapFactor = 1.0 - DAB_VisConfig.SPHERE_GAP_FRACTION;
		float maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;
		
		for (int i = 0; i < n; i++)
		{
			string compoundBoneNameA = m_aActiveBoneNames[i];
			vector bonePositionA = m_aActiveBonePositions[i];
			string boneParentA = m_aActiveBoneParents[i];
			
			float minConstraint = maxRadius;
			
			for (int j = 0; j < n; j++)
			{
				if (j == i) continue;
				if (m_aActiveBoneParents[j] == compoundBoneNameA) continue; // skip direct children
				
				vector bonePositionB = m_aActiveBonePositions[j];
				float dist = vector.Distance(bonePositionA, bonePositionB);
				
				if (dist < 0.0001 || dist > maxRadius) continue;
				
				float share = 0.5;
				if(m_aActiveBoneNames[j] == boneParentA) share = DAB_VisConfig.SPHERE_CHILD_SHARE;

				float constraint = dist * gapFactor * share;
				if (constraint < minConstraint)
					minConstraint = constraint;
			}
			
			m_mCachedSpatialMaxRadii.Set(compoundBoneNameA, minConstraint);
		}
	}

	//-----------------------------------------------------------------------
	protected void PrepareCameraConstraints(vector camPosition, out array<vector> camDirections, out array<float> distFromCam)
	{
		int n = m_aActiveBoneNames.Count();
		if (n == 0) return;
		
		float maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;
		
		camDirections.Clear();
		distFromCam.Clear();
		m_aCachedSpatialRadii.Clear();
		
		camDirections.Reserve(n);
		distFromCam.Reserve(n);
		m_aCachedSpatialRadii.Reserve(n);
		
		for (int i = 0; i < n; i++)
		{
			vector toBone = m_aActiveBonePositions[i] - camPosition;
			float dist = toBone.Length();
			
			distFromCam.Insert(dist);
			
			if (dist > 0.0001)
				camDirections.Insert(toBone * (1.0 / dist));
			else
				camDirections.Insert(vector.Forward);
			
			// Spatial radii are stable -> cache them once
			float sp = maxRadius;
			m_mCachedSpatialMaxRadii.Find(m_aActiveBoneNames[i], sp);
			m_aCachedSpatialRadii.Insert(sp);
		}
	}
	
	//-----------------------------------------------------------------------
	protected void ComputeFinalRadii(array<vector> camDirections, array<float> distFromCam)
	{
		int n = m_aActiveBoneNames.Count();
		if (n == 0) return;
		
		float gapFactor = 1.0 - DAB_VisConfig.SPHERE_GAP_FRACTION;
		float minRadius = DAB_VisConfig.SPHERE_MIN_RADIUS;
		float maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;
		
		for (int i = 0; i < n; i++)
		{
			float spatial     = m_aCachedSpatialRadii[i];
			float distA       = distFromCam[i];
			vector dirA       = camDirections[i];
			
			float angularConstraint = maxRadius;
			if (distA >= 0.0001)
			{
				float maxDot = -1.0;
				for (int j = 0; j < n; j++)
				{
					if (j == i) continue;
					float d = vector.Dot(dirA, camDirections[j]);
					if (d > maxDot)
						maxDot = d;
				}
				
				float theta     = Math.Acos(Math.Clamp(maxDot, -1.0, 1.0));
				float halfAngle = theta * 0.5 * gapFactor;
				angularConstraint = Math.Min(halfAngle * distA, maxRadius);
			}
			
			float finalRadius = Math.Min(spatial, angularConstraint);
			m_mCachedFinalMaxRadii.Set(m_aActiveBoneNames[i], Math.Max(finalRadius, minRadius));
		}
	}
	
	//-----------------------------------------------------------------------
	protected void CombineConstraints(vector camPosition)
	{
		array<vector> camDirections = new array<vector>();
		array<float>  distFromCam   = new array<float>();
		
		PrepareCameraConstraints(camPosition, camDirections, distFromCam);
		ComputeFinalRadii(camDirections, distFromCam);
	}

	//-----------------------------------------------------------------------
	protected void DrawBoneConnections()
	{
	    int n = m_aActiveBoneNames.Count();
	    if (n == 0)
	        return;
	
	    for (int i = 0; i < n; i++)
	    {
	        string parentCompoundName = m_aActiveBoneParents[i];
	        if (parentCompoundName.IsEmpty())
	            continue;
	
	        vector childPosition = m_aActiveBonePositions[i];
	
	        vector parentPosition;
	        if (!m_mCachedWorldPositions.Find(parentCompoundName, parentPosition))
	        {
	            PrintFormat("DAB_BoneOverlayRenderer.DrawBoneConnections: Could not find a parent position for: %1", parentCompoundName, LogLevel.WARNING);
	            continue;
	        }
	
	        m_aConnectionShapes.Insert(Shape.Create(
	          	ShapeType.LINE,
	            DAB_VisConfig.COLOR_BONE_CONNECTION,
	            ShapeFlags.NOZBUFFER,
	            childPosition,
	            parentPosition
	        ));
	    }
	}

	//-----------------------------------------------------------------------
	protected void DrawBoneShapes(string hoveredBone)
	{
		foreach(string boneCompoundName, vector position : m_mCachedWorldPositions)
		{
			float maxRadius;
			if (!m_mCachedFinalMaxRadii.Find(boneCompoundName, maxRadius))
				maxRadius = DAB_VisConfig.SPHERE_MAX_RADIUS;

			float sphereRadius = Math.Min(DAB_VisConfig.ComputeBoneSphereRadius(m_API, position), maxRadius);
			int color = DAB_VisConfig.COLOR_DEFAULT;

			if (boneCompoundName == hoveredBone)
			{
				sphereRadius *= DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;
				color = DAB_VisConfig.COLOR_HOVER;
			}

			m_aActiveShapes.Insert(Shape.CreateSphere(color, DAB_VisConfig.SPHERE_FLAGS, position, sphereRadius));
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
		m_aActiveShapes.Clear();
		m_SelectedBoneText = null;
	}

	//-----------------------------------------------------------------------
	protected void ClearConnectionShapes()
	{
		m_aConnectionShapes.Clear();
	}

	//-----------------------------------------------------------------------
	protected bool ShouldDisplayBone(string compoundBoneName, string boneName, DAB_BoneDisplaySettings displaySettings, DAB_SkeletonInfo skeletonInfo)
	{
	    string lowercaseBoneName = boneName;
	    lowercaseBoneName.ToLower();
	    
	    if (displaySettings.GetHideIKTargetBones() && lowercaseBoneName.Contains("_ik")) return false;
	    if (displaySettings.GetHideVolumeBones() && (lowercaseBoneName.EndsWith("prop") || lowercaseBoneName.EndsWith("volume"))) return false;
	    if (displaySettings.GetHideCameraBone() && lowercaseBoneName == "camera") return false;
	    if (displaySettings.GetHideFaceBones() && skeletonInfo.IsAncestorOf(boneName, "Head")) return false;
	    
	    if (!displaySettings.GetFilterBoneName().IsEmpty())
	    {
	        string lowercaseFilter = displaySettings.GetFilterBoneName().Trim();
	        lowercaseFilter.ToLower();
	        if (!lowercaseBoneName.Contains(lowercaseFilter)) return false;
	    }
	    
	    return true;
	}	
}