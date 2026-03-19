void DAB_RotationGizmo_OnRotate(DAB_Axis axis, float delta);
typedef func DAB_RotationGizmo_OnRotate;

//! Three-axis rotation gizmo rendered as arc rings.
//! Fires OnRotate with (axis, angleDeltaDegrees) every drag frame.
class DAB_RotationGizmo
{
	// ── State ──────────────────────────────────────────────────────────────
	protected vector m_vCenter;
	protected float  m_fRadius;
	protected vector m_mRotation[4];

	protected DAB_Axis m_eActiveAxis = DAB_Axis.NONE;
	protected vector   m_mDragBasis[4];
	protected bool     m_bIsDragging;
	protected float    m_fLastAngle;

	// Arc start angles cached at drag-start so rings do not snap every frame.
	// Indexed by DAB_Axis enum value.
	protected float m_fCachedArcStartAngles[3];

	protected ref ScriptInvokerBase<DAB_RotationGizmo_OnRotate> m_OnRotate;
	protected ref array<ref Shape> m_aShapes = {};

	//-----------------------------------------------------------------------
	void DAB_RotationGizmo(vector center, vector rotation, float radius = 1.5)
	{
		m_vCenter = center;
		m_fRadius = radius;
		SetRotation(rotation);
	}

	//-----------------------------------------------------------------------
	void ~DAB_RotationGizmo();

	//-----------------------------------------------------------------------
	//! ScriptInvoker fired on every rotation drag frame.
	ScriptInvokerBase<DAB_RotationGizmo_OnRotate> GetOnRotate()
	{
		if (!m_OnRotate)
			m_OnRotate = new ScriptInvokerBase<DAB_RotationGizmo_OnRotate>();
		return m_OnRotate;
	}

	//-----------------------------------------------------------------------
	//! Moves the gizmo to a new world position.
	void SetPosition(vector pos) { m_vCenter = pos; }

	//-----------------------------------------------------------------------
	//! Updates the gizmo orientation from (pitch, yaw, roll) in degrees.
	void SetRotation(vector rotation)
	{
		Math3D.AnglesToMatrix(Vector(rotation[1], rotation[0], rotation[2]), m_mRotation);
	}

	//-----------------------------------------------------------------------
	//! Updates the screen-space radius.
	void SetRadius(float radius) { m_fRadius = radius; }

	//-----------------------------------------------------------------------
	//! Redraws the rings. Must be called whenever position, rotation or radius changes.
	void Render(WorldEditorAPI api)
	{
		m_aShapes.Clear();

		vector camPos, rayEnd, rayDir;
		api.TraceWorldPos(api.GetScreenWidth() / 2, api.GetScreenHeight() / 2, TraceFlags.WORLD, camPos, rayEnd, rayDir);

		// Only recompute arc orientations when not dragging — prevents rings from
		// snapping to face the camera on every rotation tick during a drag.
		if (!m_bIsDragging)
		{
			vector camOffset = camPos - m_vCenter;
			m_fCachedArcStartAngles[DAB_Axis.X_Axis] = ComputeArcStartAngle(DAB_Axis.X_Axis, camOffset);
			m_fCachedArcStartAngles[DAB_Axis.Y_Axis] = ComputeArcStartAngle(DAB_Axis.Y_Axis, camOffset);
			m_fCachedArcStartAngles[DAB_Axis.Z_Axis] = ComputeArcStartAngle(DAB_Axis.Z_Axis, camOffset);
		}

		DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };

		// Insertion sort furthest-first; active axis always rendered last (on top)
		array<DAB_Axis> sortedAxes  = {};
		array<float>    sortedDists = {};

		foreach (DAB_Axis axis : axes)
		{
			if (axis == m_eActiveAxis) continue;
			float dist = vector.DistanceSq(camPos, m_vCenter + GetAxis(axis, m_mRotation) * m_fRadius);
			int insertAt = sortedAxes.Count();
			for (int j = 0; j < sortedAxes.Count(); j++)
			{
				if (dist > sortedDists[j]) { insertAt = j; break; }
			}
			sortedAxes.InsertAt(axis, insertAt);
			sortedDists.InsertAt(dist, insertAt);
		}
		if (m_eActiveAxis != DAB_Axis.NONE)
			sortedAxes.Insert(m_eActiveAxis);

		foreach (DAB_Axis axis : sortedAxes)
		{
			int color;
			switch (axis)
			{
				case DAB_Axis.X_Axis:
					if (m_eActiveAxis == axis) color = ARGB(255, 255, 0, 0); else color = DAB_VisConfig.COLOR_AXIS_X;
					break;
				case DAB_Axis.Y_Axis:
					if (m_eActiveAxis == axis) color = ARGB(255, 0, 255, 0); else color = DAB_VisConfig.COLOR_AXIS_Y;
					break;
				case DAB_Axis.Z_Axis:
					if (m_eActiveAxis == axis) color = ARGB(255, 0, 0, 255); else color = DAB_VisConfig.COLOR_AXIS_Z;
					break;
			}

			vector ax1, ax2;
			GetRingBasis(axis, ax1, ax2);

			m_aShapes.Insert(
				DAB_Shape.CreateRing(
					m_vCenter,
					GetAxis(axis, m_mRotation),
					m_fRadius,
					GetRingThickness(),
					color,
					32,
					DAB_VisConfig.GIZMO_FLAGS,
					DAB_VisConfig.GIZMO_ARC_FRACTION,
					m_fCachedArcStartAngles[axis],
					ax1,
					ax2
				)
			);
		}
	}

	//-----------------------------------------------------------------------
	//! Handles mouse press; returns true when a ring was hit and dragging starts.
	bool OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!(buttons & WETMouseButtonFlag.LEFT)) return false;

		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		m_eActiveAxis = CheckPicking(rayOrigin, rayDir);
		if (m_eActiveAxis != DAB_Axis.NONE)
		{
			m_bIsDragging = true;
			Math3D.MatrixCopy(m_mRotation, m_mDragBasis);
			m_fLastAngle = GetMouseAngle(rayOrigin, rayDir);
			return true;
		}

		m_bIsDragging = false;
		return false;
	}

	//-----------------------------------------------------------------------
	//! Handles mouse move: fires OnRotate while dragging, highlights on hover.
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		if (m_bIsDragging)
		{
			float currentAngle = GetMouseAngle(rayOrigin, rayDir);
			float delta = currentAngle - m_fLastAngle;

			if (delta >  180) delta -= 360;
			if (delta < -180) delta += 360;

			if (m_OnRotate)
				m_OnRotate.Invoke(m_eActiveAxis, delta);

			// Sync drag basis so angle measurement stays accurate
			Math3D.MatrixCopy(m_mRotation, m_mDragBasis);
			m_fLastAngle = GetMouseAngle(rayOrigin, rayDir);
		}
		else
		{
			DAB_Axis hovered = CheckPicking(rayOrigin, rayDir);
			if (hovered != m_eActiveAxis)
			{
				m_eActiveAxis = hovered;
				Render(api);
			}
		}
	}

	//-----------------------------------------------------------------------
	//! Handles mouse release; re-renders so arc orientations snap to face the camera.
	void OnMouseRelease(WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!(buttons & WETMouseButtonFlag.LEFT)) return;
		m_bIsDragging = false;
		Render(api);
	}

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	protected float GetRingThickness()
	{
		return m_fRadius * DAB_VisConfig.GIZMO_RING_THICKNESS_RATIO;
	}

	//-----------------------------------------------------------------------
	protected vector GetAxis(DAB_Axis axis, vector rotationMatrix[4])
	{
		switch (axis)
		{
			case DAB_Axis.X_Axis: return rotationMatrix[0];
			case DAB_Axis.Y_Axis: return rotationMatrix[1];
			case DAB_Axis.Z_Axis: return rotationMatrix[2];
		}
		return vector.Zero;
	}

	//-----------------------------------------------------------------------
	protected void GetRingBasis(DAB_Axis axis, out vector ax1, out vector ax2)
	{
		switch (axis)
		{
			case DAB_Axis.X_Axis: ax1 = m_mRotation[1]; ax2 = m_mRotation[2]; break;
			case DAB_Axis.Y_Axis: ax1 = m_mRotation[2]; ax2 = m_mRotation[0]; break;
			case DAB_Axis.Z_Axis: ax1 = m_mRotation[0]; ax2 = m_mRotation[1]; break;
		}
	}

	//-----------------------------------------------------------------------
	protected DAB_Axis CheckPicking(vector rayOrigin, vector rayDir)
	{
		float    threshold = GetRingThickness() * 0.5;
		DAB_Axis bestAxis  = DAB_Axis.NONE;
		float    bestDiff  = float.MAX;

		DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
		foreach (DAB_Axis axis : axes)
		{
			vector hitPos;
			if (!DAB_Math3D.IntersectRayPlane(rayOrigin, rayDir, m_vCenter, GetAxis(axis, m_mRotation), hitPos))
				continue;

			float diff = Math.AbsFloat(vector.Distance(m_vCenter, hitPos) - m_fRadius);
			if (diff >= threshold || diff >= bestDiff)
				continue;

			// Reject hits in the missing arc gap
			vector ax1, ax2;
			GetRingBasis(axis, ax1, ax2);
			vector dir = hitPos - m_vCenter;
			float hitAngle = Math.Atan2(vector.Dot(dir, ax2), vector.Dot(dir, ax1));

			float relAngle = hitAngle - m_fCachedArcStartAngles[axis];
			if (relAngle < 0)         relAngle += Math.PI2;
			if (relAngle >= Math.PI2) relAngle -= Math.PI2;
			if (relAngle > DAB_VisConfig.GIZMO_ARC_FRACTION * Math.PI2) continue;

			bestDiff = diff;
			bestAxis = axis;
		}

		return bestAxis;
	}

	//-----------------------------------------------------------------------
	protected float GetMouseAngle(vector rayOrigin, vector rayDir)
	{
		vector hitPos;
		DAB_Math3D.IntersectRayPlane(rayOrigin, rayDir, m_vCenter, GetAxis(m_eActiveAxis, m_mDragBasis), hitPos);

		vector dir = hitPos - m_vCenter;
		int iAxis1 = (m_eActiveAxis + 1) % 3;
		int iAxis2 = (m_eActiveAxis + 2) % 3;

		return Math.Atan2(
			vector.Dot(dir, GetAxis(iAxis2, m_mDragBasis)),
			vector.Dot(dir, GetAxis(iAxis1, m_mDragBasis))
		) * Math.RAD2DEG;
	}

	//-----------------------------------------------------------------------
	// Computes the arc start angle so the visible arc faces the camera.
	// The arc centre is aligned to the camera projection onto the ring plane,
	// and the start is offset by half the arc span so the visible portion is centred.
	protected float ComputeArcStartAngle(DAB_Axis axis, vector camOffset)
	{
		vector ax1, ax2;
		GetRingBasis(axis, ax1, ax2);

		float uSnap, vSnap;
		if (vector.Dot(camOffset, ax1) >= 0) uSnap =  1.0; else uSnap = -1.0;
		if (vector.Dot(camOffset, ax2) >= 0) vSnap =  1.0; else vSnap = -1.0;

		return Math.Atan2(vSnap, uSnap) - (DAB_VisConfig.GIZMO_ARC_FRACTION * Math.PI2 * 0.5);
	}
}