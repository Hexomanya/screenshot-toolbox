void DAB_ScaleGizmo_OnScale(float delta);
typedef func DAB_ScaleGizmo_OnScale;

class DAB_ScaleGizmo
{
	protected vector m_vCenter;
	protected float  m_fRadius;

	protected bool m_bIsHovered;
	protected bool m_bIsDragging;

	// Re-entrancy guard: same risk as DAB_MoveGizmo — TraceWorldPos inside
	// OnMouseMove can flush Workbench events and fire a second OnMouseMove
	// before the first Invoke() returns.
	protected bool m_bHandlingScale;

	// Drag-start reference: T is always measured from the centre that existed
	// when the drag began, so SetPosition() calls during the drag do not shift
	// the reference and cause a feedback loop.
	protected vector m_vDragStartCenter;
	protected float  m_fLastAxisT;

	protected ref ScriptInvokerBase<DAB_ScaleGizmo_OnScale> m_OnScale;
	protected ref array<ref Shape> m_aShapes = {};

	//-----------------------------------------------------------------------
	void DAB_ScaleGizmo(vector center, float radius)
	{
		m_vCenter = center;
		m_fRadius = radius;
	}

	//-----------------------------------------------------------------------
	void ~DAB_ScaleGizmo() {}

	//-----------------------------------------------------------------------
	ScriptInvokerBase<DAB_ScaleGizmo_OnScale> GetOnScale()
	{
		if (!m_OnScale)
			m_OnScale = new ScriptInvokerBase<DAB_ScaleGizmo_OnScale>();
		return m_OnScale;
	}

	//-----------------------------------------------------------------------
	void SetPosition(vector pos) { m_vCenter = pos; }

	//-----------------------------------------------------------------------
	void SetRadius(float radius) { m_fRadius = radius; }

	//-----------------------------------------------------------------------
	void Render(WorldEditorAPI api)
	{
		m_aShapes.Clear();

		float shaftRadius = m_fRadius * 0.035;
		float boxHalfSize = m_fRadius * 0.12;

		// Tip is the centre of the cube head, one radius above the bone
		vector tip = m_vCenter + vector.Up * m_fRadius;

		int color;
		if (m_bIsDragging)
			color = DAB_VisConfig.COLOR_SCALE_ACTIVE;
		else if (m_bIsHovered)
			color = DAB_VisConfig.COLOR_SCALE_HOVER;
		else
			color = DAB_VisConfig.COLOR_SCALE;

		Shape handle = DAB_Shape.CreateScaleHandle(m_vCenter, tip, shaftRadius, boxHalfSize, color, DAB_VisConfig.GIZMO_FLAGS);
		if (handle)
			m_aShapes.Insert(handle);
	}

	//-----------------------------------------------------------------------
	bool OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!(buttons & WETMouseButtonFlag.LEFT)) return false;

		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		if (CheckPicking(rayOrigin, rayDir))
		{
			m_bIsDragging = true;
			m_bIsHovered  = false;
			m_vDragStartCenter = m_vCenter;
			m_fLastAxisT = ComputeAxisT(rayOrigin, rayDir);
			Render(api);
			return true;
		}

		m_bIsDragging = false;
		return false;
	}

	//-----------------------------------------------------------------------
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		if (m_bIsDragging)
		{
			// Guard must come before TraceWorldPos for the same reentrancy
			// reason documented in DAB_MoveGizmo.OnMouseMove.
			if (m_bHandlingScale) return;
			m_bHandlingScale = true;

			vector rayOrigin, rayEnd, rayDir;
			api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

			float currentT = ComputeAxisT(rayOrigin, rayDir);
			float delta = currentT - m_fLastAxisT;

			// Clamp per-frame movement as a backstop against degenerate rays
			float maxDelta = m_fRadius * 0.5;
			if (delta >  maxDelta) delta =  maxDelta;
			if (delta < -maxDelta) delta = -maxDelta;

			m_fLastAxisT = currentT;

			if (m_OnScale)
				m_OnScale.Invoke(delta);

			m_bHandlingScale = false;
		}
		else
		{
			vector rayOrigin, rayEnd, rayDir;
			api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

			bool hovered = CheckPicking(rayOrigin, rayDir);
			if (hovered != m_bIsHovered)
			{
				m_bIsHovered = hovered;
				Render(api);
			}
		}
	}

	//-----------------------------------------------------------------------
	void OnMouseRelease(WETMouseButtonFlag buttons)
	{
		if (buttons & WETMouseButtonFlag.LEFT)
		{
			m_bIsDragging = false;
			m_bIsHovered  = false;
		}
	}

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	// The handle always points world up, so no rotation matrix is needed.
	// Pick cylinder covers the full shaft + cube head length.
	protected bool CheckPicking(vector rayOrigin, vector rayDir)
	{
		float boxHalfSize = m_fRadius * 0.12;
		float pickRadius  = m_fRadius * 0.15; // wider than the box half-size for easy clicking
		float pickLength  = m_fRadius + boxHalfSize; // shaft + top half of cube

		float t;
		return IntersectRayCylinder(rayOrigin, rayDir, m_vCenter, vector.Up, pickRadius, pickLength, t);
	}

	//-----------------------------------------------------------------------
	// Returns the scalar T along world up from m_vDragStartCenter at the
	// point on the axis closest to the mouse ray.
	// Identical maths to DAB_MoveGizmo.ComputeAxisT but axis is fixed to Up.
	protected float ComputeAxisT(vector rayOrigin, vector rayDir)
	{
		vector axisDir = vector.Up;

		vector w = rayOrigin - m_vDragStartCenter;
		float b = vector.Dot(axisDir, rayDir);
		float d = vector.Dot(axisDir, w);
		float e = vector.Dot(rayDir,  w);
		float denom = 1.0 - b * b;

		if (Math.AbsFloat(denom) < 0.1)
		{
			// Ray nearly parallel to world up — project ray origin onto axis
			return vector.Dot(w, axisDir);
		}

		return (d - b * e) / denom;
	}

	//-----------------------------------------------------------------------
	// Identical to the version in DAB_MoveGizmo — copied here so the gizmo
	// has no external dependency on that class.
	protected bool IntersectRayCylinder(vector rayOrigin, vector rayDir, vector axisOrigin, vector axisDir, float radius, float maxLength, out float hitT)
	{
		vector oc = rayOrigin - axisOrigin;

		float projRay = vector.Dot(rayDir, axisDir);
		float projOC  = vector.Dot(oc,     axisDir);

		vector dPerp = rayDir - axisDir * projRay;
		vector fPerp = oc     - axisDir * projOC;

		float a = vector.Dot(dPerp, dPerp);
		if (a < 0.0001) return false;

		float b    =  2.0 * vector.Dot(fPerp, dPerp);
		float c    = vector.Dot(fPerp, fPerp) - radius * radius;
		float disc = b * b - 4.0 * a * c;

		if (disc < 0.0) return false;

		float sqrtDisc = Math.Sqrt(disc);
		float t1 = (-b - sqrtDisc) / (2.0 * a);
		float t2 = (-b + sqrtDisc) / (2.0 * a);

		float t = t1;
		if (t < 0.0) t = t2;
		if (t < 0.0) return false;

		vector hitPoint = rayOrigin + rayDir * t;
		float axisT = vector.Dot(hitPoint - axisOrigin, axisDir);
		if (axisT < 0.0 || axisT > maxLength) return false;

		hitT = t;
		return true;
	}
}