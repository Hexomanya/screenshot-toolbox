enum DAB_Axis
{
	X_Axis,
	Y_Axis,
	Z_Axis,
	NONE,
}

class DAB_AxisHelper
{
	private void DAB_AxisHelper();
	private void ~DAB_AxisHelper();

	//-----------------------------------------------------------------------
	//! Returns a zero vector with \p delta applied on the component matching \p axis.
	static vector GetVectorFromAxis(DAB_Axis axis, float delta)
	{
		vector v = vector.Zero;
		switch (axis)
		{
			case DAB_Axis.X_Axis: v[0] = delta; break;
			case DAB_Axis.Y_Axis: v[1] = delta; break;
			case DAB_Axis.Z_Axis: v[2] = delta; break;
		}
		return v;
	}
}