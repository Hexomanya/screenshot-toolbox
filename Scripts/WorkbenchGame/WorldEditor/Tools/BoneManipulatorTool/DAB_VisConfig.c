class DAB_VisConfig
{
	// Camera
	static const float CAMERA_POLLING_INTERVAL_SEC = 0.5;
	static const float CAMERA_POLLING_DISTANCE_EPSILON = 0.001;
	
	// Sphere
	static const float SPHERE_DRAW_RADIUS_MIN = 0.01;
	static const float SPHERE_MAX_RADIUS = 0.05;
	static const float SPHERE_DISTANCE_SLOPE = 0.1;
	static const float SPHERE_START_SLOPE_DISTANCE = 1;
	static const float SPHERE_HOVER_MULTIPLIER = 1.1;
	
	//Colors
    static const int COLOR_DEFAULT = ARGB(168, 240, 240, 240);
	static const int COLOR_HOVER = ARGB(255, 255, 220,  60);
    static const int COLOR_SELECTED = ARGB(255, 180, 255, 48);

	static const int COLOR_AXIS_X = ARGB(200, 255, 0, 0);
	static const int COLOR_AXIS_Y = ARGB(200, 0, 255, 0);
	static const int COLOR_AXIS_Z = ARGB(200, 0, 0, 255);
	
	//ShapeFlags
	static const ShapeFlags SPHERE_FLAGS = ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE;
	static const ShapeFlags GIZMO_FLAGS = ShapeFlags.NOZBUFFER  | ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE | ShapeFlags.DOUBLESIDE;


	//g\left(x\right)=\left\{x\le b:a,\ d-\frac{d-a}{1+c\left(x-b\right)^{2}}\right\}
	static float ComputeSphereSize(float currentDistance)
	{
		float a = SPHERE_DRAW_RADIUS_MIN;
		float b = SPHERE_START_SLOPE_DISTANCE;
		float c = SPHERE_DISTANCE_SLOPE;
		float d = SPHERE_MAX_RADIUS;
		float x = currentDistance;
		
		if(x <= b) return a;
		
		return d - ((d - a) / (1 + c * Math.Pow((x - b), 2)));
	}
}