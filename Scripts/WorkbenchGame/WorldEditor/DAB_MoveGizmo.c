void DAB_MoveGizmo_OnMove(DAB_Axis axis, float delta);
typedef func DAB_MoveGizmo_OnMove;

class DAB_MoveGizmo
{
	protected vector m_vCenter;
	protected float  m_fRadius;
	protected vector m_mRotation[4]; // bone world orientation (entity × boneOrig × accumRot)

	protected DAB_Axis m_eActiveAxis = DAB_Axis.NONE;
	protected bool     m_bIsDragging;

	// Re-entrancy guard: Render() calls TraceWorldPos() which can flush
	// Workbench events and fire a second OnMouseMove before the first
	// Invoke() has returned, causing a recursive-invoke crash.
	protected bool     m_bHandlingMove;

	// Drag-start reference: the axis T values are always measured from the
	// center that existed when the drag began, so SetPosition() calls during
	// the drag do not shift the reference point and cause a feedback loop.
	protected vector m_vDragStartCenter;
	protected float  m_fLastAxisT;

	protected ref ScriptInvokerBase<DAB_MoveGizmo_OnMove> m_OnMove;
	protected ref array<ref Shape> m_aShapes = {};

	//-----------------------------------------------------------------------
	void DAB_MoveGizmo(vector center, vector rotation, float radius)
	{
		m_vCenter = center;
		m_fRadius = radius;
		SetRotation(rotation);
	}

	//-----------------------------------------------------------------------
	void ~DAB_MoveGizmo() {}

	//-----------------------------------------------------------------------
	ScriptInvokerBase<DAB_MoveGizmo_OnMove> GetOnMove()
	{
		if (!m_OnMove)
			m_OnMove = new ScriptInvokerBase<DAB_MoveGizmo_OnMove>();
		return m_OnMove;
	}

	//-----------------------------------------------------------------------
	void SetPosition(vector pos) { m_vCenter = pos; }

	//-----------------------------------------------------------------------
	void SetRadius(float radius) { m_fRadius = radius; }

	//-----------------------------------------------------------------------
	// rotation is (pitch, yaw, roll) in the game's (P,Y,Z) convention —
	// same format passed around by the rotation gizmo.
	void SetRotation(vector rotation)
	{
		Math3D.AnglesToMatrix(Vector(rotation[1], rotation[0], rotation[2]), m_mRotation);
	}

	//-----------------------------------------------------------------------
	void Render(WorldEditorAPI api)
	{
		m_aShapes.Clear();

		// Camera position for back-to-front depth sorting
		vector camPos, rayEnd, rayDir;
		api.TraceWorldPos(api.GetScreenWidth() / 2, api.GetScreenHeight() / 2, TraceFlags.WORLD, camPos, rayEnd, rayDir);

		float shaftRadius = m_fRadius * 0.035;
		float coneRadius  = m_fRadius * 0.10;
		float coneLength  = m_fRadius * 0.28;

		// Sort axes furthest-first; active axis always drawn last (renders on top)
		DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
		array<DAB_Axis> sortedAxes = {};
		array<float>    sortedDists = {};

		foreach (DAB_Axis axis : axes)
		{
			if (axis == m_eActiveAxis) continue;

			// Sort by distance to the arrow midpoint
			vector arrowMid = m_vCenter + GetAxisVector(axis) * m_fRadius * 0.5;
			float dist = vector.DistanceSq(camPos, arrowMid);

			int insertAt = sortedAxes.Count();
			for (int j = 0; j < sortedAxes.Count(); j++)
			{
				if (dist > sortedDists[j]) { insertAt = j; break; }
			}
			sortedAxes.InsertAt(axis, insertAt);
			sortedDists.InsertAt(dist, insertAt);
		}

		if (m_eActiveAxis != DAB_Axis.NONE)
			sortedAxes.Insert(m_eActiveAxis); // active axis always on top

		foreach (DAB_Axis axis : sortedAxes)
		{
			int color;
			switch (axis)
			{
				case DAB_Axis.X_Axis:
					if (m_eActiveAxis == DAB_Axis.X_Axis) color = ARGB(255, 255, 0, 0);
					else color = DAB_VisConfig.COLOR_AXIS_X;
					break;
				case DAB_Axis.Y_Axis:
					if (m_eActiveAxis == DAB_Axis.Y_Axis) color = ARGB(255, 0, 255, 0);
					else color = DAB_VisConfig.COLOR_AXIS_Y;
					break;
				case DAB_Axis.Z_Axis:
					if (m_eActiveAxis == DAB_Axis.Z_Axis) color = ARGB(255, 0, 0, 255);
					else color = DAB_VisConfig.COLOR_AXIS_Z;
					break;
				default: color = ARGB(200, 200, 200, 200); break;
			}

			vector arrowTip = m_vCenter + GetAxisVector(axis) * m_fRadius;
			Shape arrow = DAB_Shape.CreateArrow(m_vCenter, arrowTip, shaftRadius, coneRadius, coneLength, color, DAB_VisConfig.GIZMO_FLAGS);
			if (arrow)
				m_aShapes.Insert(arrow);
		}
	}

	//-----------------------------------------------------------------------
	bool OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!(buttons & WETMouseButtonFlag.LEFT)) return false;

		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		m_eActiveAxis = CheckPicking(rayOrigin, rayDir);
		if (m_eActiveAxis != DAB_Axis.NONE)
		{
			m_bIsDragging = true;
			// Snapshot the center NOW so all T values this drag are measured from
			// a fixed reference point. Without this, every SetPosition() call during
			// the drag shifts m_vCenter, making the next frame's T differ even with
			// a stationary mouse — causing a runaway feedback loop.
			m_vDragStartCenter = m_vCenter;
			m_fLastAxisT = ComputeAxisT(rayOrigin, rayDir, m_eActiveAxis);
			Render(api);
			return true;
		}

		m_bIsDragging = false;
		return false;
	}

	//-----------------------------------------------------------------------
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		if (m_bIsDragging)
		{
			// Guard against re-entrant Invoke: Render() -> TraceWorldPos() can
			// flush Workbench events and call OnMouseMove again before the
			// current Invoke() has returned, which Enfusion's ScriptInvoker
			// treats as a recursive call and throws an exception.
			if (m_bHandlingMove) return;
			m_bHandlingMove = true;

			// T is a scalar along the axis from the FIXED drag-start center.
			// m_vCenter may shift each frame as the bone moves, but
			// m_vDragStartCenter never changes, so delta is purely mouse-driven.
			float currentT = ComputeAxisT(rayOrigin, rayDir, m_eActiveAxis);
			float delta = currentT - m_fLastAxisT;

			// Clamp per-frame movement as a backstop against degenerate rays
			float maxDelta = m_fRadius * 0.5;
			if (delta >  maxDelta) delta =  maxDelta;
			if (delta < -maxDelta) delta = -maxDelta;

			m_fLastAxisT = currentT;

			if (m_OnMove)
				m_OnMove.Invoke(m_eActiveAxis, delta);

			m_bHandlingMove = false;
		}
		else
		{
			// Hover highlight
			DAB_Axis hovered = CheckPicking(rayOrigin, rayDir);
			if (hovered != m_eActiveAxis)
			{
				m_eActiveAxis = hovered;
				Render(api);
			}
		}
	}

	//-----------------------------------------------------------------------
	void OnMouseRelease(WETMouseButtonFlag buttons)
	{
		if (buttons & WETMouseButtonFlag.LEFT)
			m_bIsDragging = false;
	}

	//-----------------------------------------------------------------------
	// Note: Z is negated so the arrow faces toward the viewer, matching the
	// Enfusion convention where local +Z would otherwise point into the scene.
	// OnGizmoMove negates Z's worldAxisDir to keep drag direction consistent.
	protected vector GetAxisVector(DAB_Axis axis)
	{
		switch (axis)
		{
			case DAB_Axis.X_Axis: return m_mRotation[0];
			case DAB_Axis.Y_Axis: return m_mRotation[1];
			case DAB_Axis.Z_Axis: return m_mRotation[2] * -1;
		}
		return vector.Zero;
	}

	//-----------------------------------------------------------------------
	// Ray-vs-cylinder pick test against each arrow.
	// The pick cylinder is intentionally wider than the visual shaft so thin
	// shafts remain easy to click.
	protected DAB_Axis CheckPicking(vector rayOrigin, vector rayDir)
	{
		// Pick volume: radius covers the cone generously
		float pickRadius = m_fRadius * 0.13;

		DAB_Axis bestAxis = DAB_Axis.NONE;
		float    bestT    = float.MAX;

		DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
		foreach (DAB_Axis axis : axes)
		{
			float t;
			if (IntersectRayCylinder(rayOrigin, rayDir, m_vCenter, GetAxisVector(axis), pickRadius, m_fRadius, t))
			{
				if (t < bestT) { bestT = t; bestAxis = axis; }
			}
		}

		return bestAxis;
	}

	//-----------------------------------------------------------------------
	// Returns the scalar T along the axis (measured from m_vDragStartCenter)
	// at the point on the axis closest to the mouse ray.
	// Using a fixed center means SetPosition() calls mid-drag never affect T.
	//
	// Two-line closest-approach:
	//   Axis: A(t) = m_vDragStartCenter + t * axisDir
	//   Ray:  R(s) = rayOrigin + s * rayDir
	//   w = rayOrigin - m_vDragStartCenter
	//   t = (b*e - d) / (1 - b²)
	protected float ComputeAxisT(vector rayOrigin, vector rayDir, DAB_Axis axis)
	{
		vector axisDir = GetAxisVector(axis);

		vector w = rayOrigin - m_vDragStartCenter;
		float b = vector.Dot(axisDir, rayDir);
		float d = vector.Dot(axisDir, w);
		float e = vector.Dot(rayDir,  w);
		float denom = 1.0 - b * b;

		if (Math.AbsFloat(denom) < 0.1)
		{
			// Ray nearly parallel to axis — project ray origin onto axis
			return vector.Dot(w, axisDir);
		}

		// Correct two-line closest-approach for t on the axis:
		// t = (d - b*e) / (1 - b²)
		// The sign was previously inverted here, causing every drag delta
		// to be negated — making every axis move in the wrong direction.
		return (d - b * e) / denom;
	}

	//-----------------------------------------------------------------------
	// Intersects a ray against a finite cylinder aligned to axisDir,
	// starting at axisOrigin, capped at [0, maxLength].
	// Returns false on miss or if both intersections are behind the ray.
	protected bool IntersectRayCylinder(vector rayOrigin, vector rayDir, vector axisOrigin, vector axisDir, float radius, float maxLength, out float hitT)
	{
		vector oc = rayOrigin - axisOrigin;

		// Strip the component along the axis to work in the perpendicular plane
		float projRay = vector.Dot(rayDir, axisDir);
		float projOC  = vector.Dot(oc,     axisDir);

		vector dPerp = rayDir - axisDir * projRay;
		vector fPerp = oc     - axisDir * projOC;

		float a = vector.Dot(dPerp, dPerp);
		if (a < 0.0001) return false; // ray parallel to axis

		float b    =  2.0 * vector.Dot(fPerp, dPerp);
		float c    = vector.Dot(fPerp, fPerp) - radius * radius;
		float disc = b * b - 4.0 * a * c;

		if (disc < 0.0) return false;

		float sqrtDisc = Math.Sqrt(disc);
		float t1 = (-b - sqrtDisc) / (2.0 * a);
		float t2 = (-b + sqrtDisc) / (2.0 * a);

		// Use nearest positive intersection
		float t = t1;
		if (t < 0.0) t = t2;
		if (t < 0.0) return false;

		// Confirm hit lands within the arrow's length
		vector hitPoint = rayOrigin + rayDir * t;
		float axisT = vector.Dot(hitPoint - axisOrigin, axisDir);
		if (axisT < 0.0 || axisT > maxLength) return false;

		hitT = t;
		return true;
	}
}