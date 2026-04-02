void DAB_MoveGizmo_OnMove(DAB_Axis axis, float delta);
typedef func DAB_MoveGizmo_OnMove;

//! Three-axis translation gizmo rendered as arrows along bone-local axes.
//! Fires OnMove with (axis, worldSpaceDeltaMetres) every drag frame.
class DAB_MoveGizmo
{
	// ── State ──────────────────────────────────────────────────────────────
	protected vector m_vCenter;
	protected float  m_fRadius;
	protected vector m_mRotation[4];

	protected DAB_Axis m_eActiveAxis = DAB_Axis.NONE;
	protected bool     m_bIsDragging;

	// Re-entrancy guard: TraceWorldPos can flush Workbench events and fire a
	// second OnMouseMove before the first Invoke() returns.
	protected bool m_bHandlingMove;

	// Drag reference frozen at press time so SetPosition() calls mid-drag
	// do not shift the reference point and cause a feedback loop.
	protected vector m_vDragStartCenter;
	protected float  m_fLastAxisT;

	// Camera-relative axis signs (+1 toward camera, -1 away).
	// Updated each Render() so arrows always face the viewer.
	protected float m_fXSign = 1;
	protected float m_fYSign = 1;
	protected float m_fZSign = -1;

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
	//! ScriptInvoker fired on every translation drag frame.
	ScriptInvokerBase<DAB_MoveGizmo_OnMove> GetOnMove()
	{
		if (!m_OnMove)
			m_OnMove = new ScriptInvokerBase<DAB_MoveGizmo_OnMove>();
		return m_OnMove;
	}

	//-----------------------------------------------------------------------
	//! Moves the gizmo to a new world position.
	void SetPosition(vector pos) { m_vCenter = pos; }

	//-----------------------------------------------------------------------
	//! Updates the screen-space radius.
	void SetRadius(float radius) { m_fRadius = radius; }

	//-----------------------------------------------------------------------
	//! Updates the gizmo orientation from (pitch, yaw, roll) in degrees.
	void SetRotation(vector rotation)
	{
		Math3D.AnglesToMatrix(Vector(rotation[1], rotation[0], rotation[2]), m_mRotation);
	}

	//-----------------------------------------------------------------------
	//! Redraws the arrows. Must be called whenever position, rotation or radius changes.
	void Render(WorldEditorAPI api)
	{
		m_aShapes.Clear();

		vector camPos, rayEnd, rayDir;
		api.TraceWorldPos(api.GetScreenWidth() / 2, api.GetScreenHeight() / 2, TraceFlags.WORLD, camPos, rayEnd, rayDir);

		if (!m_bIsDragging)
	    {
	        vector camDir = (camPos - m_vCenter).Normalized();
	        if (vector.Dot(m_mRotation[0], camDir) >= 0) m_fXSign =  1.0; else m_fXSign = -1.0;
	        if (vector.Dot(m_mRotation[1], camDir) >= 0) m_fYSign =  1.0; else m_fYSign = -1.0;
	        if (vector.Dot(m_mRotation[2], camDir) >= 0) m_fZSign =  1.0; else m_fZSign = -1.0;
	    }

		float shaftRadius = m_fRadius * 0.035;
		float coneRadius  = m_fRadius * 0.10;
		float coneLength  = m_fRadius * 0.28;

		// Sort furthest-first; active axis always drawn last (on top)
		DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
		array<DAB_Axis> sortedAxes  = {};
		array<float>    sortedDists = {};

		foreach (DAB_Axis axis : axes)
		{
			if (axis == m_eActiveAxis) continue;
			float dist = vector.DistanceSq(camPos, m_vCenter + GetAxisVector(axis) * m_fRadius * 0.5);
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
				default: color = ARGB(200, 200, 200, 200); break;
			}

			vector arrowTip = m_vCenter + GetAxisVector(axis) * m_fRadius;
			Shape arrow = DAB_Shape.CreateArrow(m_vCenter, arrowTip, shaftRadius, coneRadius, coneLength, color, DAB_VisConfig.GIZMO_FLAGS);
			if (arrow) m_aShapes.Insert(arrow);
		}
	}

	//-----------------------------------------------------------------------
	//! Handles mouse press; returns true when an arrow was hit and dragging starts.
	bool OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!(buttons & WETMouseButtonFlag.LEFT)) return false;

		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		m_eActiveAxis = CheckPicking(rayOrigin, rayDir);
		if (m_eActiveAxis != DAB_Axis.NONE)
		{
			m_bIsDragging      = true;
			m_vDragStartCenter = m_vCenter;
			m_fLastAxisT       = ComputeAxisT(rayOrigin, rayDir, m_eActiveAxis);
			Render(api);
			return true;
		}

		m_bIsDragging = false;
		return false;
	}

	//-----------------------------------------------------------------------
	//! Handles mouse move: fires OnMove while dragging, highlights on hover.
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		if (m_bIsDragging)
		{
			// Guard before TraceWorldPos — that call can flush Workbench events
			// and trigger a reentrant OnMouseMove before Invoke() returns.
			if (m_bHandlingMove) return;
			m_bHandlingMove = true;

			vector rayOrigin, rayEnd, rayDir;
			api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

			float currentT = ComputeAxisT(rayOrigin, rayDir, m_eActiveAxis);
			float delta = currentT - m_fLastAxisT;

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
			vector rayOrigin, rayEnd, rayDir;
			api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

			DAB_Axis hovered = CheckPicking(rayOrigin, rayDir);
			if (hovered != m_eActiveAxis)
			{
				m_eActiveAxis = hovered;
				Render(api);
			}
		}
	}

	//-----------------------------------------------------------------------
	//! Handles mouse release.
	void OnMouseRelease(WETMouseButtonFlag buttons)
	{
		if (buttons & WETMouseButtonFlag.LEFT)
			m_bIsDragging = false;
	}

	//-----------------------------------------------------------------------
	//! Camera-relative sign for the X arrow (+1 or -1).
	float GetXSign() { return m_fXSign; }
	//! Camera-relative sign for the Y arrow (+1 or -1).
	float GetYSign() { return m_fYSign; }
	//! Camera-relative sign for the Z arrow (+1 or -1).
	float GetZSign() { return m_fZSign; }

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	// Returns the camera-facing axis direction (sign already applied).
	protected vector GetAxisVector(DAB_Axis axis)
	{
		switch (axis)
		{
			case DAB_Axis.X_Axis: return m_mRotation[0] * m_fXSign;
			case DAB_Axis.Y_Axis: return m_mRotation[1] * m_fYSign;
			case DAB_Axis.Z_Axis: return m_mRotation[2] * m_fZSign;
		}
		return vector.Zero;
	}

	//-----------------------------------------------------------------------
	protected DAB_Axis CheckPicking(vector rayOrigin, vector rayDir)
	{
		float    pickRadius = m_fRadius * 0.13;
		DAB_Axis bestAxis   = DAB_Axis.NONE;
		float    bestT      = float.MAX;

		DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
		foreach (DAB_Axis axis : axes)
		{
			float t;
			if (DAB_Math3D.IntersectRayCylinder(rayOrigin, rayDir, m_vCenter, GetAxisVector(axis), pickRadius, m_fRadius, t))
			{
				if (t < bestT) { bestT = t; bestAxis = axis; }
			}
		}

		return bestAxis;
	}

	//-----------------------------------------------------------------------
	// T along the axis from m_vDragStartCenter closest to the mouse ray.
	protected float ComputeAxisT(vector rayOrigin, vector rayDir, DAB_Axis axis)
	{
		return DAB_Math3D.RayClosestPointOnAxis(rayOrigin, rayDir, m_vDragStartCenter, GetAxisVector(axis));
	}
}