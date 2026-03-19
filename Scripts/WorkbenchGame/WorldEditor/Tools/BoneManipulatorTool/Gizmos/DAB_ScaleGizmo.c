void DAB_ScaleGizmo_OnScale(float delta);
typedef func DAB_ScaleGizmo_OnScale;

//! Uniform-scale gizmo rendered as a vertical shaft capped with a cube.
//! Fires OnScale with a signed world-space delta (metres) every drag frame.
class DAB_ScaleGizmo
{
	// ── State ──────────────────────────────────────────────────────────────
	protected vector m_vCenter;
	protected float  m_fRadius;

	protected bool m_bIsHovered;
	protected bool m_bIsDragging;

	// Re-entrancy guard — same risk as DAB_MoveGizmo.
	protected bool m_bHandlingScale;

	// Drag reference frozen at press time.
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
	//! ScriptInvoker fired on every scale drag frame.
	ScriptInvokerBase<DAB_ScaleGizmo_OnScale> GetOnScale()
	{
		if (!m_OnScale)
			m_OnScale = new ScriptInvokerBase<DAB_ScaleGizmo_OnScale>();
		return m_OnScale;
	}

	//-----------------------------------------------------------------------
	//! Moves the gizmo to a new world position.
	void SetPosition(vector pos) { m_vCenter = pos; }

	//-----------------------------------------------------------------------
	//! Updates the screen-space radius.
	void SetRadius(float radius) { m_fRadius = radius; }

	//-----------------------------------------------------------------------
	//! Redraws the handle. Must be called whenever position or radius changes.
	void Render(WorldEditorAPI api)
	{
		m_aShapes.Clear();

		float shaftRadius = m_fRadius * 0.035;
		float boxHalfSize = m_fRadius * 0.12;
		vector tip = m_vCenter + vector.Up * m_fRadius;

		int color;
		if (m_bIsDragging)     color = DAB_VisConfig.COLOR_SCALE_ACTIVE;
		else if (m_bIsHovered) color = DAB_VisConfig.COLOR_SCALE_HOVER;
		else                   color = DAB_VisConfig.COLOR_SCALE;

		Shape handle = DAB_Shape.CreateScaleHandle(m_vCenter, tip, shaftRadius, boxHalfSize, color, DAB_VisConfig.GIZMO_FLAGS);
		if (handle) m_aShapes.Insert(handle);
	}

	//-----------------------------------------------------------------------
	//! Handles mouse press; returns true when the handle was hit and dragging starts.
	bool OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!(buttons & WETMouseButtonFlag.LEFT)) return false;

		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		if (CheckPicking(rayOrigin, rayDir))
		{
			m_bIsDragging      = true;
			m_bIsHovered       = false;
			m_vDragStartCenter = m_vCenter;
			m_fLastAxisT       = ComputeAxisT(rayOrigin, rayDir);
			Render(api);
			return true;
		}

		m_bIsDragging = false;
		return false;
	}

	//-----------------------------------------------------------------------
	//! Handles mouse move: fires OnScale while dragging, highlights on hover.
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		if (m_bIsDragging)
		{
			// Guard before TraceWorldPos — same reentrancy risk as DAB_MoveGizmo.
			if (m_bHandlingScale) return;
			m_bHandlingScale = true;

			vector rayOrigin, rayEnd, rayDir;
			api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

			float currentT = ComputeAxisT(rayOrigin, rayDir);
			float delta = currentT - m_fLastAxisT;

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
	//! Handles mouse release.
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
	protected bool CheckPicking(vector rayOrigin, vector rayDir)
	{
		float boxHalfSize = m_fRadius * 0.12;
		float pickRadius  = m_fRadius * 0.15;
		float pickLength  = m_fRadius + boxHalfSize;
		float t;
		return DAB_Math3D.IntersectRayCylinder(rayOrigin, rayDir, m_vCenter, vector.Up, pickRadius, pickLength, t);
	}

	//-----------------------------------------------------------------------
	// T along world up from m_vDragStartCenter closest to the mouse ray.
	protected float ComputeAxisT(vector rayOrigin, vector rayDir)
	{
		return DAB_Math3D.RayClosestPointOnAxis(rayOrigin, rayDir, m_vDragStartCenter, vector.Up);
	}
}