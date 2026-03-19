class DAB_Shape
{
	private void DAB_Shape();
	private void ~DAB_Shape();

	//-----------------------------------------------------------------------
	//! Creates an arc/ring as a triangle mesh.
	//! \p arcFraction controls what fraction of the full circle is drawn (0–1).
	//! \p startAngle is the arc's starting angle in radians.
	//! \p ax1, \p ax2 are the in-plane basis vectors; computed from \p normal if both are zero.
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
	//! Creates a solid 3-D arrow (cylinder shaft + cone head) as a single triangle mesh.
	//! \p base is the arrow tail, \p tip is the arrowhead point.
	//! \p coneLength is measured back from \p tip; shaftRadius < coneRadius expected.
	//!
	//! Geometry (8 segments): cylinder 16 tris, annular cap 16 tris, cone 8 tris = 40 tris total.
	static Shape CreateArrow(vector base, vector tip, float shaftRadius, float coneRadius, float coneLength, int color, ShapeFlags flags)
	{
		vector totalVec = tip - base;
		float  totalLen = totalVec.Length();
		if (totalLen < 0.0001) return null;
		vector axisDir = totalVec.Normalized();

		vector coneBase = tip - axisDir * coneLength;

		vector up = vector.Up;
		if (Math.AbsFloat(vector.Dot(axisDir, up)) > 0.99)
			up = vector.Right;
		vector ax1 = SCR_Math3D.Cross(axisDir, up).Normalized();
		vector ax2 = SCR_Math3D.Cross(axisDir, ax1).Normalized();

		const int SEGMENTS = 8;
		vector verts[120];
		int vi       = 0;
		int triCount = 0;

		// Cylinder sides
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			verts[vi++] = base     + r0 * shaftRadius;
			verts[vi++] = coneBase + r0 * shaftRadius;
			verts[vi++] = base     + r1 * shaftRadius;
			verts[vi++] = base     + r1 * shaftRadius;
			verts[vi++] = coneBase + r0 * shaftRadius;
			verts[vi++] = coneBase + r1 * shaftRadius;
			triCount += 2;
		}

		// Annular cap — closes the ring gap between shaft rim and cone base rim
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			vector inner0 = coneBase + r0 * shaftRadius;
			vector inner1 = coneBase + r1 * shaftRadius;
			vector outer0 = coneBase + r0 * coneRadius;
			vector outer1 = coneBase + r1 * coneRadius;

			verts[vi++] = inner0; verts[vi++] = outer1; verts[vi++] = outer0;
			verts[vi++] = inner0; verts[vi++] = inner1; verts[vi++] = outer1;
			triCount += 2;
		}

		// Cone sides
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			verts[vi++] = coneBase + r0 * coneRadius;
			verts[vi++] = tip;
			verts[vi++] = coneBase + r1 * coneRadius;
			triCount++;
		}

		return Shape.CreateTris(color, flags, verts, triCount);
	}

	//-----------------------------------------------------------------------
	//! Creates a scale-handle shape: a cylinder shaft topped with a solid cube.
	//! \p base is the shaft start, \p tip is the centre of the cube head.
	//! \p boxHalfSize is the half-extent of the cube on every axis.
	//!
	//! Geometry: cylinder 16 tris + cube 12 tris = 28 tris total.
	static Shape CreateScaleHandle(vector base, vector tip, float shaftRadius, float boxHalfSize, int color, ShapeFlags flags)
	{
		vector totalVec = tip - base;
		float  totalLen = totalVec.Length();
		if (totalLen < 0.0001) return null;
		vector axisDir = totalVec.Normalized();

		vector boxBottom = tip - axisDir * boxHalfSize;

		vector up = vector.Up;
		if (Math.AbsFloat(vector.Dot(axisDir, up)) > 0.99)
			up = vector.Right;
		vector ax1 = SCR_Math3D.Cross(axisDir, up).Normalized();
		vector ax2 = SCR_Math3D.Cross(axisDir, ax1).Normalized();

		const int SEGMENTS = 8;
		vector verts[84];
		int vi       = 0;
		int triCount = 0;

		// Cylinder shaft (base → boxBottom)
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = (float)i       / SEGMENTS * Math.PI2;
			float a1 = (float)(i + 1) / SEGMENTS * Math.PI2;

			vector r0 = ax1 * Math.Cos(a0) + ax2 * Math.Sin(a0);
			vector r1 = ax1 * Math.Cos(a1) + ax2 * Math.Sin(a1);

			verts[vi++] = base      + r0 * shaftRadius;
			verts[vi++] = boxBottom + r0 * shaftRadius;
			verts[vi++] = base      + r1 * shaftRadius;
			verts[vi++] = base      + r1 * shaftRadius;
			verts[vi++] = boxBottom + r0 * shaftRadius;
			verts[vi++] = boxBottom + r1 * shaftRadius;
			triCount += 2;
		}

		// Cube head
		// Corners: top (+axisDir) = 0-3, bottom (-axisDir) = 4-7
		float s = boxHalfSize;
		vector c0 = tip + ax1 * s + ax2 * s + axisDir * s;
		vector c1 = tip - ax1 * s + ax2 * s + axisDir * s;
		vector c2 = tip - ax1 * s - ax2 * s + axisDir * s;
		vector c3 = tip + ax1 * s - ax2 * s + axisDir * s;
		vector c4 = tip + ax1 * s + ax2 * s - axisDir * s;
		vector c5 = tip - ax1 * s + ax2 * s - axisDir * s;
		vector c6 = tip - ax1 * s - ax2 * s - axisDir * s;
		vector c7 = tip + ax1 * s - ax2 * s - axisDir * s;

		// Top
		verts[vi++] = c0; verts[vi++] = c1; verts[vi++] = c2;
		verts[vi++] = c0; verts[vi++] = c2; verts[vi++] = c3;
		triCount += 2;
		// Bottom
		verts[vi++] = c4; verts[vi++] = c6; verts[vi++] = c5;
		verts[vi++] = c4; verts[vi++] = c7; verts[vi++] = c6;
		triCount += 2;
		// +ax1 side
		verts[vi++] = c0; verts[vi++] = c3; verts[vi++] = c7;
		verts[vi++] = c0; verts[vi++] = c7; verts[vi++] = c4;
		triCount += 2;
		// -ax1 side
		verts[vi++] = c1; verts[vi++] = c5; verts[vi++] = c6;
		verts[vi++] = c1; verts[vi++] = c6; verts[vi++] = c2;
		triCount += 2;
		// +ax2 side
		verts[vi++] = c0; verts[vi++] = c4; verts[vi++] = c5;
		verts[vi++] = c0; verts[vi++] = c5; verts[vi++] = c1;
		triCount += 2;
		// -ax2 side
		verts[vi++] = c3; verts[vi++] = c2; verts[vi++] = c6;
		verts[vi++] = c3; verts[vi++] = c6; verts[vi++] = c7;
		triCount += 2;

		return Shape.CreateTris(color, flags, verts, triCount);
	}
}