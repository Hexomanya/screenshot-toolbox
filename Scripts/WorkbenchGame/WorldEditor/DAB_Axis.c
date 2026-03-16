class DAB_AxisHelper
{
	static vector GetVectorFromAxis(DAB_Axis axis, float delta){
		vector rotation = vector.Zero;

	    switch (axis)
	    {
	        case DAB_Axis.X_Axis: rotation[0] = delta; break;
	        case DAB_Axis.Y_Axis: rotation[1] = delta; break;
	        case DAB_Axis.Z_Axis: rotation[2] = delta; break;
	    }
		
		return rotation;
	}
}