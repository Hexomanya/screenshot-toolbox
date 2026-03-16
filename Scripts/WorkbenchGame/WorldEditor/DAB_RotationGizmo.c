void DAB_RotationGizmo_OnRotate(DAB_Axis axis, float delta);
typedef func DAB_RotationGizmo_OnRotate;

enum DAB_Axis
{
	X_Axis,
	Y_Axis,
	Z_Axis,
	NONE,
}

class DAB_RotationGizmo
{
	static float ringThickness = 0.05;
	
	protected vector m_vCenter;
	protected float m_fRadius;
	
	protected vector m_mRotation[4];

	protected DAB_Axis m_eActiveAxis = DAB_Axis.NONE;
	protected vector m_mDragBasis[4];
	protected bool m_bIsDragging;
	protected float m_fLastAngle;

	protected ref ScriptInvokerBase<DAB_RotationGizmo_OnRotate> m_OnRotate;
	protected ref array<ref Shape> m_aShapes = {};

	//-----------------------------------------------------------------------
	void DAB_RotationGizmo(vector center, vector rotation, float radius = 1.5)
	{
		m_vCenter = center;
		m_fRadius = radius;
		SetRotation(rotation);
	}

	//-----------------------------------------------------------------------
	void ~DAB_RotationGizmo();

	//-----------------------------------------------------------------------
	ScriptInvokerBase<DAB_RotationGizmo_OnRotate> GetOnRotate()
	{
		if (!m_OnRotate)
			m_OnRotate = new ScriptInvokerBase<DAB_RotationGizmo_OnRotate>();

		return m_OnRotate;
	}

	//-----------------------------------------------------------------------
	void SetPosition(vector pos)
	{
		m_vCenter = pos;
	}

	//-----------------------------------------------------------------------
	void SetRotation(vector rotation)
	{
		rotation = Vector(rotation[1], rotation[0], rotation[2]);
		Math3D.AnglesToMatrix(rotation, m_mRotation);
	}
	
	//-----------------------------------------------------------------------
	void SetRadius(float radius)
	{
	    m_fRadius = radius;
	}
	
	//-----------------------------------------------------------------------
	protected float GetRingThickness()
	{
	    return m_fRadius * DAB_VisConfig.GIZMO_RING_THICKNESS_RATIO;
	}

	//-----------------------------------------------------------------------
	protected vector GetAxis(DAB_Axis axis, vector rotationMatrix[4])
	{
	    switch (axis)
	    {
	        case DAB_Axis.X_Axis: return rotationMatrix[0]; 
	        case DAB_Axis.Y_Axis: return rotationMatrix[1];
	        case DAB_Axis.Z_Axis: return rotationMatrix[2];
	    }
	    return vector.Zero;
	}
	
	//-----------------------------------------------------------------------
	void Render(WorldEditorAPI api)
	{
	    m_aShapes.Clear();
		
	    // Build axis list with distances from camera
	    vector camPos, rayEnd, rayDir;
	    int cx = api.GetScreenWidth() / 2;
	    int cy = api.GetScreenHeight() / 2;
	    api.TraceWorldPos(cx, cy, TraceFlags.WORLD, camPos, rayEnd, rayDir);
	
	    DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
	
	    // Insertion sort — furthest first, active axis always last (renders on top)
	    array<DAB_Axis> sortedAxes = {};
	    array<float> sortedDists = {};
	
	    foreach (int i, DAB_Axis axis : axes)
	    {
	        if (axis == m_eActiveAxis) continue;
	
	        float dist = vector.DistanceSq(camPos, m_vCenter + GetAxis(axis, m_mRotation) * m_fRadius);
	        int insertAt = sortedAxes.Count();
	
	        for (int j = 0; j < sortedAxes.Count(); j++)
	        {
	            if (dist > sortedDists[j])
	            {
	                insertAt = j;
	                break;
	            }
	        }
	
	        sortedAxes.InsertAt(axis, insertAt);
	        sortedDists.InsertAt(dist, insertAt);
	    }

	    if (m_eActiveAxis != DAB_Axis.NONE)
	        sortedAxes.Insert(m_eActiveAxis);
	
	    foreach (DAB_Axis axis : sortedAxes)
		{
		    int color;
		    switch (axis)
		    {
		        case DAB_Axis.X_Axis:
		            if (m_eActiveAxis == DAB_Axis.X_Axis) color = ARGB(255, 255, 0, 0);
		            else color = DAB_VisConfig.COLOR_AXIS_X;
		            break;
		        case DAB_Axis.Y_Axis:
		            if (m_eActiveAxis == DAB_Axis.Y_Axis) color = ARGB(255, 0, 255, 0);
		            else color = DAB_VisConfig.COLOR_AXIS_Y;
		            break;
		        case DAB_Axis.Z_Axis:
		            if (m_eActiveAxis == DAB_Axis.Z_Axis) color = ARGB(255, 0, 0, 255);
		            else color = DAB_VisConfig.COLOR_AXIS_Z;
		            break;
		    }
			
			vector ax1, ax2;
			GetRingBasis(axis, ax1, ax2);
			
			vector camOffset = camPos - m_vCenter;
			float startAngle = ComputeArcStartAngle(axis, camOffset);
		
		   	m_aShapes.Insert(
				DAB_Shape.CreateRing(
					m_vCenter, 
					GetAxis(axis, m_mRotation), 
					m_fRadius, 
					GetRingThickness(), 
					color, 
					32, 
					DAB_VisConfig.GIZMO_FLAGS, 
					DAB_VisConfig.GIZMO_ARC_FRACTION,
					startAngle,
					ax1,
					ax2
				)
			);
		}
	}

	//-----------------------------------------------------------------------
	bool OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
	    if (!(buttons & WETMouseButtonFlag.LEFT)) return false;
	
	    vector rayOrigin, rayEnd, rayDir;
	    api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);
	
	    m_eActiveAxis = CheckPicking(rayOrigin, rayDir);
	    if (m_eActiveAxis != DAB_Axis.NONE)
	    {
	        m_bIsDragging = true;
	        Math3D.MatrixCopy(m_mRotation, m_mDragBasis);
	        m_fLastAngle = GetMouseAngle(rayOrigin, rayDir);
	        return true;
	    }
	
	    m_bIsDragging = false;
	    return false;
	}

	//-----------------------------------------------------------------------
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		vector rayOrigin, rayEnd, rayDir;
		api.TraceWorldPos((int)x, (int)y, TraceFlags.WORLD, rayOrigin, rayEnd, rayDir);

		if (m_bIsDragging)
		{
			float currentAngle = GetMouseAngle(rayOrigin, rayDir);
			float delta = currentAngle - m_fLastAngle;
			
			if (delta > 180) delta -= 360;
			if (delta < -180) delta += 360;
			
			if (m_OnRotate)
				m_OnRotate.Invoke(m_eActiveAxis, delta);

			m_fLastAngle = currentAngle;
		}
		else
		{
			DAB_Axis hovered = CheckPicking(rayOrigin, rayDir);
			if (hovered != m_eActiveAxis)
			{
				m_eActiveAxis = hovered;
				Render(api);
			}
		}
	}

	//-----------------------------------------------------------------------
	void OnMouseRelease(WETMouseButtonFlag buttons)
	{
		if (buttons & WETMouseButtonFlag.LEFT)
			m_bIsDragging = false;
	}
	
	//-----------------------------------------------------------------------
	protected DAB_Axis CheckPicking(vector rayOrigin, vector rayDir)
	{
	    float threshold = GetRingThickness() * 0.5;
	    DAB_Axis bestAxis = DAB_Axis.NONE;
	    float bestDiff = float.MAX;
	
	    vector camOffset = rayOrigin - m_vCenter;
	
	    DAB_Axis axes[3] = { DAB_Axis.X_Axis, DAB_Axis.Y_Axis, DAB_Axis.Z_Axis };
	    foreach (DAB_Axis axis : axes)
	    {
	        vector hitPos;
	        if (!IntersectRayPlane(rayOrigin, rayDir, m_vCenter, GetAxis(axis, m_mRotation), hitPos))
	            continue;
	
	        float dist = vector.Distance(m_vCenter, hitPos);
	        float diff = Math.AbsFloat(dist - m_fRadius);
	        if (diff >= threshold || diff >= bestDiff)
	            continue;
	
	        // Reject hits that fall in the missing 3/4
	        vector ax1, ax2;
	        GetRingBasis(axis, ax1, ax2);
	        vector dir = hitPos - m_vCenter;
	        float hitAngle = Math.Atan2(vector.Dot(dir, ax2), vector.Dot(dir, ax1));
	
	        float startAngle = ComputeArcStartAngle(axis, camOffset);
	        float relAngle = hitAngle - startAngle;
			if (relAngle < 0)         relAngle += Math.PI2;
			if (relAngle >= Math.PI2) relAngle -= Math.PI2;
	
	        if (relAngle > DAB_VisConfig.GIZMO_ARC_FRACTION * Math.PI2)
	            continue;
	
	        bestDiff = diff;
	        bestAxis = axis;
	    }
	
	    return bestAxis;
	}
	
	//-----------------------------------------------------------------------
	protected void GetRingBasis(DAB_Axis axis, out vector ax1, out vector ax2)
	{
	    switch (axis)
	    {
	        case DAB_Axis.X_Axis: ax1 = m_mRotation[1]; ax2 = m_mRotation[2]; break;
	        case DAB_Axis.Y_Axis: ax1 = m_mRotation[2]; ax2 = m_mRotation[0]; break;
	        case DAB_Axis.Z_Axis: ax1 = m_mRotation[0]; ax2 = m_mRotation[1]; break;
	    }
	}

	//-----------------------------------------------------------------------
	protected float GetMouseAngle(vector rayOrigin, vector rayDir)
	{
	    vector hitPos;
	    IntersectRayPlane(rayOrigin, rayDir, m_vCenter, GetAxis(m_eActiveAxis, m_mDragBasis), hitPos);
	
	    vector dir = hitPos - m_vCenter;
	
	    int iAxis1 = (m_eActiveAxis + 1) % 3;
	    int iAxis2 = (m_eActiveAxis + 2) % 3;
	    
	    float u = vector.Dot(dir, GetAxis(iAxis1, m_mDragBasis));
	    float v = vector.Dot(dir, GetAxis(iAxis2, m_mDragBasis));
	
	    return Math.Atan2(v, u) * Math.RAD2DEG;
	}

	//-----------------------------------------------------------------------
	protected bool IntersectRayPlane(vector rayOrigin, vector rayDir, vector planePos, vector planeNormal, out vector intersection)
	{
		float denom = vector.Dot(planeNormal, rayDir);
		if (Math.AbsFloat(denom) > 0.0001)
		{
			float t = vector.Dot(planePos - rayOrigin, planeNormal) / denom;
			if (t >= 0)
			{
				intersection = rayOrigin + rayDir * t;
				return true;
			}
		}
		return false;
	}
	
	//-----------------------------------------------------------------------
	protected float ComputeArcStartAngle(DAB_Axis axis, vector camOffset)
	{
	    vector ax1, ax2;
	    GetRingBasis(axis, ax1, ax2);
	
	    float u = vector.Dot(camOffset, ax1);
	    float v = vector.Dot(camOffset, ax2);
	
	    float uSnap, vSnap;
	    if (u >= 0) uSnap =  1.0; else uSnap = -1.0;
	    if (v >= 0) vSnap =  1.0; else vSnap = -1.0;
	
	    float centerAngle = Math.Atan2(vSnap, uSnap);
		Print("centerAngle: " + centerAngle);
		Print("final: " + (centerAngle - (DAB_VisConfig.GIZMO_ARC_FRACTION * Math.PI2 * 0.5)));
	    return centerAngle - (DAB_VisConfig.GIZMO_ARC_FRACTION * Math.PI2 * 0.5);
	}
}