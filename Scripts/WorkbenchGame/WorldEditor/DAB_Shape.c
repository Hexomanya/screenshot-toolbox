class DAB_Shape
{
	static Shape CreateRing(vector center, vector normal, float radius, float thickness, int color, int segments, ShapeFlags flags)
	{
	    float innerRadius = radius - thickness * 0.5;
	    float outerRadius = radius + thickness * 0.5;
		
	    vector up = vector.Up;
		if (Math.AbsFloat(vector.Dot(normal, up)) > 0.99)
		    up = vector.Right;
	
	    vector ax1 = SCR_Math3D.Cross(normal, up).Normalized();
	    vector ax2 = SCR_Math3D.Cross(normal, ax1).Normalized();
	
	    // 32 segments * 2 tris * 3 verts = 192
	    vector verts[192];
	
	    for (int i = 0; i < segments; i++)
	    {
	        float a0 = (float)i       / segments * Math.PI2;
	        float a1 = (float)(i + 1) / segments * Math.PI2;
	
	        vector outerA0 = center + (ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0)) * outerRadius;
	        vector innerA0 = center + (ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0)) * innerRadius;
	        vector outerA1 = center + (ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1)) * outerRadius;
	        vector innerA1 = center + (ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1)) * innerRadius;
	
	        // Triangle 1
	        verts[i * 6 + 0] = outerA0;
	        verts[i * 6 + 1] = innerA0;
	        verts[i * 6 + 2] = outerA1;
	
	        // Triangle 2
	        verts[i * 6 + 3] = innerA0;
	        verts[i * 6 + 4] = innerA1;
	        verts[i * 6 + 5] = outerA1;
	    }
	
	    return Shape.CreateTris(color, flags, verts, segments * 2);
	}
}