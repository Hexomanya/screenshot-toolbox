class DAB_Helper 
{
	static vector GetCameraPosition(WorldEditorAPI api)
	{
		vector camMat[4];
		api.GetWorld().GetCurrentCamera(camMat);
		return camMat[3];
	}
	
	static bool AreFloatsEqual(float a, float b, float epsilon = 0.001)
	{
	    return Math.AbsFloat(a - b) < epsilon;
	}
}