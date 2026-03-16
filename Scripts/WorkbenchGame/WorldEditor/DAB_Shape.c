class DAB_Shape
{
	static Shape CreateRing(vector center, vector normal, float radius, float thickness, int color, int segments, ShapeFlags flags, float arcFraction = 1.0, float startAngle = 0.0, vector ax1 = vector.Zero, vector ax2 = vector.Zero)
	{
	    float innerRadius = radius - thickness * 0.5;
	    float outerRadius = radius + thickness * 0.5;
	
	    if (ax1 == vector.Zero || ax2 == vector.Zero)
	    {
	        vector up = vector.Up;
	        if (Math.AbsFloat(vector.Dot(normal, up)) > 0.99)
	            up = vector.Right;
	        ax1 = SCR_Math3D.Cross(normal, up).Normalized();
	        ax2 = SCR_Math3D.Cross(normal, ax1).Normalized();
	    }
	
	    int arcSegments = (int)(segments * arcFraction);
	    vector verts[192];
	
	    for (int i = 0; i < arcSegments; i++)
	    {
	        float a0 = startAngle + (float)i       / arcSegments * Math.PI2 * arcFraction;
	        float a1 = startAngle + (float)(i + 1) / arcSegments * Math.PI2 * arcFraction;
	
	        vector outerA0 = center + (ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0)) * outerRadius;
	        vector innerA0 = center + (ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0)) * innerRadius;
	        vector outerA1 = center + (ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1)) * outerRadius;
	        vector innerA1 = center + (ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1)) * innerRadius;
	
	        verts[i * 6 + 0] = outerA0;
	        verts[i * 6 + 1] = innerA0;
	        verts[i * 6 + 2] = outerA1;
	
	        verts[i * 6 + 3] = innerA0;
	        verts[i * 6 + 4] = innerA1;
	        verts[i * 6 + 5] = outerA1;
	    }
	
	    return Shape.CreateTris(color, flags, verts, arcSegments * 2);
	}
}