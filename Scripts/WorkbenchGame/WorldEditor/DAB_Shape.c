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

	//-----------------------------------------------------------------------
	// Builds a solid 3-D arrow (cylinder shaft + cone head) as a single
	// triangle mesh returned in one Shape.CreateTris call.
	//
	// Geometry (8 segments fixed):
	//   Cylinder sides  — 8 × 2 tris  = 16 tris  (48 verts)
	//   Annular cap     — 8 × 2 tris  = 16 tris  (48 verts)
	//     Seals the ring-shaped gap between shaft rim (shaftRadius)
	//     and cone base rim (coneRadius) at the junction point.
	//   Cone sides      — 8 × 1 tri   =  8 tris  (24 verts)
	//   Total           —              = 40 tris (120 verts)
	//
	// 'base' is the arrow tail, 'tip' is the arrowhead point.
	// coneLength is measured back from tip; shaftRadius < coneRadius expected.
	static Shape CreateArrow(vector base, vector tip, float shaftRadius, float coneRadius, float coneLength, int color, ShapeFlags flags)
	{
		vector totalVec   = tip - base;
		float  totalLen   = totalVec.Length();
		if (totalLen < 0.0001) return null;
		vector axisDir = totalVec.Normalized();

		vector coneBase = tip - axisDir * coneLength;

		// Stable perpendicular basis, matching the approach used in CreateRing
		vector up = vector.Up;
		if (Math.AbsFloat(vector.Dot(axisDir, up)) > 0.99)
			up = vector.Right;
		vector ax1 = SCR_Math3D.Cross(axisDir, up).Normalized();
		vector ax2 = SCR_Math3D.Cross(axisDir, ax1).Normalized();

		const int SEGMENTS = 8;
		vector verts[120]; // 40 tris × 3 verts
		int vi       = 0;
		int triCount = 0;

		// ── Cylinder sides ────────────────────────────────────────────────
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			vector p00 = base     + r0 * shaftRadius;
			vector p01 = base     + r1 * shaftRadius;
			vector p10 = coneBase + r0 * shaftRadius;
			vector p11 = coneBase + r1 * shaftRadius;

			verts[vi++] = p00; verts[vi++] = p10; verts[vi++] = p01;
			verts[vi++] = p01; verts[vi++] = p10; verts[vi++] = p11;
			triCount += 2;
		}

		// ── Annular cap (shaft–cone junction) ─────────────────────────────
		// Covers the ring-shaped gap between the shaft outer rim (shaftRadius)
		// and the cone base rim (coneRadius), both at the coneBase plane.
		// Normal faces backward (−axisDir), closing the arrow from the rear.
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			// Inner edge = shaft outer rim; outer edge = cone base rim
			vector inner0 = coneBase + r0 * shaftRadius;
			vector inner1 = coneBase + r1 * shaftRadius;
			vector outer0 = coneBase + r0 * coneRadius;
			vector outer1 = coneBase + r1 * coneRadius;

			verts[vi++] = inner0; verts[vi++] = outer1; verts[vi++] = outer0;
			verts[vi++] = inner0; verts[vi++] = inner1; verts[vi++] = outer1;
			triCount += 2;
		}

		// ── Cone sides ────────────────────────────────────────────────────
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			vector c0 = coneBase + r0 * coneRadius;
			vector c1 = coneBase + r1 * coneRadius;

			verts[vi++] = c0; verts[vi++] = tip; verts[vi++] = c1;
			triCount++;
		}

		return Shape.CreateTris(color, flags, verts, triCount);
	}
}