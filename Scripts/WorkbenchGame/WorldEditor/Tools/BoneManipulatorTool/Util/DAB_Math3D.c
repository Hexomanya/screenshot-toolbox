class DAB_Math3D
{
	private void DAB_Math3D();
	private void ~DAB_Math3D();

	//-----------------------------------------------------------------------
	//! Ray vs disc intersection. Sets \p hitPoint and returns true on a hit.
	static bool IntersectionRayCircle(vector rayStart, vector rayDir, vector circleCenter, vector circleNormal, float circleRadius, inout vector hitPoint)
	{
		float denom = vector.Dot(circleNormal, rayDir);
		if (Math.AbsFloat(denom) < 1e-6) return false;

		float t = vector.Dot(circleNormal, circleCenter - rayStart) / denom;
		if (t < 0) return false;

		hitPoint = rayStart + t * rayDir;
		return vector.Distance(hitPoint, circleCenter) <= circleRadius;
	}

	//-----------------------------------------------------------------------
	//! Ray vs infinite plane intersection. Sets \p intersection and returns true on a hit.
	static bool IntersectRayPlane(vector rayOrigin, vector rayDir, vector planePos, vector planeNormal, out vector intersection)
	{
		float denom = vector.Dot(planeNormal, rayDir);
		if (Math.AbsFloat(denom) <= 0.0001) return false;

		float t = vector.Dot(planePos - rayOrigin, planeNormal) / denom;
		if (t < 0) return false;

		intersection = rayOrigin + rayDir * t;
		return true;
	}

	//-----------------------------------------------------------------------
	//! Ray vs finite cylinder (axis-aligned, capped at [0, maxLength]). Sets \p hitT and returns true on a hit.
	static bool IntersectRayCylinder(vector rayOrigin, vector rayDir, vector axisOrigin, vector axisDir, float radius, float maxLength, out float hitT)
	{
		vector oc = rayOrigin - axisOrigin;

		float projRay = vector.Dot(rayDir, axisDir);
		float projOC  = vector.Dot(oc,     axisDir);

		vector dPerp = rayDir - axisDir * projRay;
		vector fPerp = oc     - axisDir * projOC;

		float a = vector.Dot(dPerp, dPerp);
		if (a < 0.0001) return false;

		float b    = 2.0 * vector.Dot(fPerp, dPerp);
		float c    = vector.Dot(fPerp, fPerp) - radius * radius;
		float disc = b * b - 4.0 * a * c;
		if (disc < 0.0) return false;

		float sqrtDisc = Math.Sqrt(disc);
		float t = (-b - sqrtDisc) / (2.0 * a);
		if (t < 0.0) t = (-b + sqrtDisc) / (2.0 * a);
		if (t < 0.0) return false;

		float axisT = vector.Dot(rayOrigin + rayDir * t - axisOrigin, axisDir);
		if (axisT < 0.0 || axisT > maxLength) return false;

		hitT = t;
		return true;
	}

	//-----------------------------------------------------------------------
	//! Returns the scalar T along \p axisDir (from \p axisOrigin) at the point on that axis
	//! closest to the given mouse ray. Used by gizmos to convert mouse movement into displacement.
	static float RayClosestPointOnAxis(vector rayOrigin, vector rayDir, vector axisOrigin, vector axisDir)
	{
		vector w = rayOrigin - axisOrigin;
		float b = vector.Dot(axisDir, rayDir);
		float d = vector.Dot(axisDir, w);
		float e = vector.Dot(rayDir,  w);
		float denom = 1.0 - b * b;

		// Ray nearly parallel to axis — project origin onto axis
		if (Math.AbsFloat(denom) < 0.1)
			return vector.Dot(w, axisDir);

		return (d - b * e) / denom;
	}
}