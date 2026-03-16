class DAB_Math3D
{
	private void DAB_Math3D();
	private void ~DAB_Math3D();

	//-----------------------------------------------------------------------
	static bool IntersectionRayCircle(vector rayStart, vector rayDir, vector circleCenter, vector circleNormal, float circleRadius, inout vector hitPoint)
	{
		float denominator = vector.Dot(circleNormal, rayDir);
		
		bool isParallel = Math.AbsFloat(denominator) < 1e-6;
		if(isParallel) return false;
		
		float t = vector.Dot(circleNormal, circleCenter - rayStart) / denominator;
		
		if(t < 0) return false;
		
		hitPoint = rayStart + t * rayDir;
		
		return vector.Distance(hitPoint, circleCenter) <= circleRadius;
	}
}