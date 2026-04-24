class DAB_Helper
{
	private void DAB_Helper();
	private void ~DAB_Helper();

	//-----------------------------------------------------------------------
	//! Returns the current world-space camera position.
	static vector GetCameraPosition(WorldEditorAPI api)
	{
		vector camMat[4];
		api.GetWorld().GetCurrentCamera(camMat);
		return camMat[3];
	}

	//-----------------------------------------------------------------------
	//! Returns true when \p a and \p b differ by less than \p epsilon.
	static bool AreFloatsEqual(float a, float b, float epsilon = 0.001)
	{
		return Math.AbsFloat(a - b) < epsilon;
	}
	
	//-----------------------------------------------------------------------
	static string GetCombinedConfigString(array<ResourceName> configs)
	{
		string configsStr = "";
	    foreach (int i, ResourceName res : configs)
	    {
	        if (i > 0)
	        {
	            configsStr += ",";
	        }
	        configsStr += res.GetPath();
	    }
		
		return configsStr;
	}
	
	//-----------------------------------------------------------------------
	static string VectorToAttributeFormat(vector v)
	{
		return string.Format("%1 %2 %3", v[0], v[1], v[2]);
	}
	
	//-----------------------------------------------------------------------
	static Managed FindComponentExact(IEntity entity, typename exactType)
	{
		array<Managed> components = {};
		entity.FindComponents(exactType, components);
		
		foreach (Managed component : components)
		{
		    if (component.Type() == exactType) return component;
		}
	
		return null;
	} 
}