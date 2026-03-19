class DAB_VisConfig
{
	// Camera
	static const float CAMERA_POLLING_INTERVAL_SEC = 0.5;
	static const float CAMERA_POLLING_DISTANCE_EPSILON = 0.001;
	
	// Sphere
	/*
	static const float SPHERE_DRAW_RADIUS_MIN = 0.025;  
	static const float SPHERE_START_SLOPE_DISTANCE = 1;
	static const float SPHERE_DISTANCE_SLOPE  = 0.05;*/
	
	static const float SPHERE_SCREEN_FRACTION = 0.012;
	static const float SPHERE_MAX_RADIUS      = 0.10;
	static const float SPHERE_HOVER_MULTIPLIER = 1.1;
	static const float SPHERE_GAP_FRACTION = 0.25;
	static const float SPHERE_CHILD_SHARE = 0.3;
	
	
	// Gizmo
	// Gizmo radius expressed as a fraction of the viewport height.
	// 0.15 means the gizmo ring spans 15% of the screen height regardless of camera distance.
	static const float GIZMO_SCREEN_FRACTION = 0.2;
	static const float GIZMO_RING_THICKNESS_RATIO = 0.25;
	static const float GIZMO_ARC_FRACTION = 0.25;
	
	//Colors
    static const int COLOR_DEFAULT = ARGB(168, 240, 240, 240);
	static const int COLOR_HOVER = ARGB(255, 255, 220,  60);
    static const int COLOR_SELECTED = ARGB(255, 180, 255, 48);
	static const int COLOR_BONE_CONNECTION = ARGB(120, 200, 200, 200);
	
	static const int COLOR_AXIS_X = ARGB(200, 255, 0, 0);
	static const int COLOR_AXIS_Y = ARGB(200, 0, 255, 0);
	static const int COLOR_AXIS_Z = ARGB(200, 0, 0, 255);
	
	// Scale gizmo — a mint green distinct from the Y-axis pure green
	static const int COLOR_SCALE         = ARGB(200,  80, 220,  80);
	static const int COLOR_SCALE_HOVER   = ARGB(255, 120, 255, 120);
	static const int COLOR_SCALE_ACTIVE  = ARGB(255, 160, 255, 160);
	
	// World-space units of scale change per metre of drag along world up.
	// At the default gizmo size (~0.3 m radius) a full-length drag adds ~0.6 scale.
	static const float SCALE_SENSITIVITY = 2.0;
	
	//ShapeFlags
	static const ShapeFlags SPHERE_FLAGS = ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE;
	static const ShapeFlags GIZMO_FLAGS = ShapeFlags.NOZBUFFER  | ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE | ShapeFlags.DOUBLESIDE;


	//Dezmos demo: g\left(x\right)=\left\{x\le b:a,\ d-\frac{d-a}{1+c\left(x-b\right)^{2}}\right\}
	/*static float ComputeSphereSize(float currentDistance)
	{
		float a = SPHERE_DRAW_RADIUS_MIN;
		float b = SPHERE_START_SLOPE_DISTANCE;
		float c = SPHERE_DISTANCE_SLOPE;
		float d = SPHERE_MAX_RADIUS;
		float x = currentDistance;
		
		if(x <= b) return a;
		
		return d - ((d - a) / (1 + c * Math.Pow((x - b), 2)));
	}*/
	
	
	// Computes the gizmo radius in world space such that it always appears as
	// GIZMO_SCREEN_FRACTION of the viewport height, regardless of camera distance.
	//-----------------------------------------------------------------------
	static float ComputeGizmoRadius(WorldEditorAPI api, vector gizmoPos)
	{
		int cx = api.GetScreenWidth()  / 2;
		int cy = api.GetScreenHeight() / 2;
		int pixelOffset = (int)(api.GetScreenHeight() * GIZMO_SCREEN_FRACTION);

		vector camPos, rayEnd, rayDir1, rayDir2;
		api.TraceWorldPos(cx, cy,               TraceFlags.WORLD, camPos, rayEnd, rayDir1);
		api.TraceWorldPos(cx, cy + pixelOffset,  TraceFlags.WORLD, camPos, rayEnd, rayDir2);

		// Use the primary ray direction as the plane normal so the intersection
		// is always at the gizmo depth and not skewed by oblique view angles.
		vector planeNormal = rayDir1;

		float denom1 = vector.Dot(rayDir1, planeNormal); // always 1.0
		float denom2 = vector.Dot(rayDir2, planeNormal);

		if (Math.AbsFloat(denom2) < 0.0001)
			return 0.1; // degenerate view — safe fallback

		vector toGizmo = gizmoPos - camPos;
		float t1 = vector.Dot(toGizmo, planeNormal) / denom1;
		float t2 = vector.Dot(toGizmo, planeNormal) / denom2;

		vector p1 = camPos + rayDir1 * t1;
		vector p2 = camPos + rayDir2 * t2;

		return vector.Distance(p1, p2);
	}
	
	
	// Computes the world-space radius for a bone sphere such that it always
	// appears as SPHERE_SCREEN_FRACTION of the viewport height.
	//-----------------------------------------------------------------------
	static float ComputeBoneSphereRadius(WorldEditorAPI api, vector bonePos)
	{
	    int cx = api.GetScreenWidth()  / 2;
	    int cy = api.GetScreenHeight() / 2;
	    int pixelOffset = (int)(api.GetScreenHeight() * SPHERE_SCREEN_FRACTION);
	
	    vector camPos, rayEnd, rayDir1, rayDir2;
	    api.TraceWorldPos(cx, cy,              TraceFlags.WORLD, camPos, rayEnd, rayDir1);
	    api.TraceWorldPos(cx, cy + pixelOffset, TraceFlags.WORLD, camPos, rayEnd, rayDir2);
	
	    vector planeNormal = rayDir1;
	    float denom2 = vector.Dot(rayDir2, planeNormal);
	    if (Math.AbsFloat(denom2) < 0.0001) return 0.005;
	
	    vector toGizmo = bonePos - camPos;
	    float t1 = vector.Dot(toGizmo, planeNormal); // denom1 == 1.0
	    float t2 = vector.Dot(toGizmo, planeNormal) / denom2;
	
	    vector p1 = camPos + rayDir1 * t1;
	    vector p2 = camPos + rayDir2 * t2;
	
	    return Math.Min(vector.Distance(p1, p2), SPHERE_MAX_RADIUS);
	}
}