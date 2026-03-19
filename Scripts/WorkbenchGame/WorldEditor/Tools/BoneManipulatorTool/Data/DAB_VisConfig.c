class DAB_VisConfig
{
	private void DAB_VisConfig();
	private void ~DAB_VisConfig();

	// ── Camera ─────────────────────────────────────────────────────────────
	static const float CAMERA_POLLING_DISTANCE_EPSILON = 0.001;

	// ── Bone spheres ───────────────────────────────────────────────────────
	/// Sphere radius as a fraction of viewport height (screen-space constant size).
	static const float SPHERE_SCREEN_FRACTION  = 0.012;
	static const float SPHERE_MAX_RADIUS       = 0.10;
	static const float SPHERE_HOVER_MULTIPLIER = 1.1;
	/// Fraction of the gap between two bone spheres that is left empty.
	static const float SPHERE_GAP_FRACTION     = 0.25;
	/// Space fraction given to a sphere when its neighbour is its parent.
	static const float SPHERE_CHILD_SHARE      = 0.3;

	// ── Gizmos ─────────────────────────────────────────────────────────────
	/// Gizmo ring radius as a fraction of viewport height (screen-space constant size).
	static const float GIZMO_SCREEN_FRACTION      = 0.2;
	static const float GIZMO_RING_THICKNESS_RATIO = 0.25;
	/// Fraction of a full circle rendered for each rotation arc (0–1).
	static const float GIZMO_ARC_FRACTION         = 0.25;

	// ── Scale gizmo ────────────────────────────────────────────────────────
	/// Scale units added per metre of upward drag on the scale handle.
	static const float SCALE_SENSITIVITY = 2.0;

	// ── Colors ─────────────────────────────────────────────────────────────
	static const int COLOR_DEFAULT         = ARGB(168, 240, 240, 240);
	static const int COLOR_HOVER           = ARGB(255, 255, 220,  60);
	static const int COLOR_SELECTED        = ARGB(255, 180, 255,  48);
	static const int COLOR_BONE_CONNECTION = ARGB(120, 200, 200, 200);

	static const int COLOR_AXIS_X = ARGB(200, 255,   0,   0);
	static const int COLOR_AXIS_Y = ARGB(200,   0, 255,   0);
	static const int COLOR_AXIS_Z = ARGB(200,   0,   0, 255);

	/// Mint green — distinct from the Y-axis pure green.
	static const int COLOR_SCALE        = ARGB(200,  80, 220,  80);
	static const int COLOR_SCALE_HOVER  = ARGB(255, 120, 255, 120);
	static const int COLOR_SCALE_ACTIVE = ARGB(255, 160, 255, 160);

	// ── Shape flags ────────────────────────────────────────────────────────
	static const ShapeFlags SPHERE_FLAGS = ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE;
	static const ShapeFlags GIZMO_FLAGS  = ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE | ShapeFlags.DOUBLESIDE;

	//-----------------------------------------------------------------------
	//! Returns the world-space gizmo radius so the gizmo always appears as
	//! GIZMO_SCREEN_FRACTION of the viewport height regardless of camera distance.
	static float ComputeGizmoRadius(WorldEditorAPI api, vector gizmoPos)
	{
		return ComputeScreenSpaceWorldRadius(api, gizmoPos, GIZMO_SCREEN_FRACTION);
	}

	//-----------------------------------------------------------------------
	//! Returns the world-space sphere radius so a bone sphere always appears as
	//! SPHERE_SCREEN_FRACTION of the viewport height, capped at SPHERE_MAX_RADIUS.
	static float ComputeBoneSphereRadius(WorldEditorAPI api, vector bonePos)
	{
		return Math.Min(ComputeScreenSpaceWorldRadius(api, bonePos, SPHERE_SCREEN_FRACTION), SPHERE_MAX_RADIUS);
	}

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	// Projects two rays through the centre pixel and a pixel screenFraction below it
	// onto the plane at worldPos depth, then returns the world-space distance between
	// those two points — giving a radius that is a fixed fraction of the viewport
	// height regardless of camera distance or FOV.
	protected static float ComputeScreenSpaceWorldRadius(WorldEditorAPI api, vector worldPos, float screenFraction)
	{
		int cx          = api.GetScreenWidth()  / 2;
		int cy          = api.GetScreenHeight() / 2;
		int pixelOffset = (int)(api.GetScreenHeight() * screenFraction);

		vector camPos, rayEnd, rayDir1, rayDir2;
		api.TraceWorldPos(cx, cy,               TraceFlags.WORLD, camPos, rayEnd, rayDir1);
		api.TraceWorldPos(cx, cy + pixelOffset,  TraceFlags.WORLD, camPos, rayEnd, rayDir2);

		// dot(rayDir1, rayDir1) == 1.0, so t1 needs no division.
		float denom2 = vector.Dot(rayDir2, rayDir1);
		if (Math.AbsFloat(denom2) < 0.0001) return 0.01;

		vector toPos = worldPos - camPos;
		float  t1    = vector.Dot(toPos, rayDir1);
		float  t2    = vector.Dot(toPos, rayDir1) / denom2;

		return vector.Distance(camPos + rayDir1 * t1, camPos + rayDir2 * t2);
	}
}