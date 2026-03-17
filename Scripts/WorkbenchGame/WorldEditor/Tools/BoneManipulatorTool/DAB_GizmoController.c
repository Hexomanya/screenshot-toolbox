enum DAB_GizmoMode
{
	Deactivated,
	Position,
	Rotation
}

void DAB_GizmoController_OnTransformChanged(DAB_BoneTransform changedTransform);
typedef func DAB_GizmoController_OnTransformChanged;

class DAB_GizmoController 
{
	protected DAB_GizmoMode m_gizmoMode;
	
	protected ref DAB_RotationGizmo m_RotationGizmo;
	
	protected ref ScriptInvokerBase<DAB_GizmoController_OnTransformChanged> m_OnTransformChanged;
	
	protected ref DAB_BoneTransform m_currentTransform;
	
	protected WorldEditorAPI m_API;
	
	protected vector m_mAccumRotation[3];
	
	protected vector m_mEntityRotation[3];
	
	
	//-----------------------------------------------------------------------
	void DAB_GizmoController(WorldEditorAPI api)
	{
		m_gizmoMode = DAB_GizmoMode.Deactivated;
		m_API = api; // TODO: Check if safe to safe api
	}
	
	//-----------------------------------------------------------------------
	void ~DAB_GizmoController()
	{
		m_API = null;
	} 
	
	
	//-----------------------------------------------------------------------
	ScriptInvokerBase<DAB_GizmoController_OnTransformChanged> GetOnTransformChanged()
	{
		if (!m_OnTransformChanged)
			m_OnTransformChanged = new ScriptInvokerBase<DAB_GizmoController_OnTransformChanged>();
		
		return m_OnTransformChanged;
	}
	
	//-----------------------------------------------------------------------
	void Attach(IEntity entity, DAB_BoneTransform boneToAttach, WorldEditorAPI api, float cameraDistance)
	{
	    if(m_currentTransform != null)
	    {
	        Print("Tried to attach to new bone, but old one was not cleared first!", LogLevel.WARNING);
	        Clear(api);
	    }
	
	    m_currentTransform = boneToAttach;
	
	    vector position = m_currentTransform.GetAdjustedPosition();
	
	    // Store entity world rotation
	    vector entityWorld[4];
	    entity.GetTransform(entityWorld);
	    m_mEntityRotation[0] = entityWorld[0];
	    m_mEntityRotation[1] = entityWorld[1];
	    m_mEntityRotation[2] = entityWorld[2];
	
	    // AccumRotation tracks only the delta from this point, so start at identity
	    Math3D.MatrixIdentity3(m_mAccumRotation);
	
	    // Combine entity rotation + bone original rotation for initial gizmo display
	    vector boneOrigMat[3];
	    vector orig = m_currentTransform.GetOriginalRotation();
	    Math3D.AnglesToMatrix(Vector(orig[1], orig[0], orig[2]), boneOrigMat);
	
	    vector combined[3];
	    Math3D.MatrixMultiply3(m_mEntityRotation, boneOrigMat, combined);
	    vector worldAngles = Math3D.MatrixToAngles(combined);
	
	    m_RotationGizmo = new DAB_RotationGizmo(position, Vector(worldAngles[1], worldAngles[0], worldAngles[2]), DAB_VisConfig.ComputeGizmoRadius(cameraDistance));
	    m_RotationGizmo.GetOnRotate().Insert(this.OnGizmoRotate);
	    m_RotationGizmo.Render(api);
	}


	
	//-----------------------------------------------------------------------
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		if(!m_currentTransform) return;
		m_RotationGizmo.OnMouseMove(x, y, api);
	}
	
	//-----------------------------------------------------------------------
	void OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if(!m_currentTransform) return;
		m_RotationGizmo.OnMousePress(x, y, buttons, api);
	}
	
	//-----------------------------------------------------------------------
	void OnMouseRelease(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if(!m_currentTransform) return;
		m_RotationGizmo.OnMouseRelease(buttons);
	}
	
	//-----------------------------------------------------------------------
	void Clear(WorldEditorAPI api)
	{
		m_currentTransform = null;
		if(m_RotationGizmo) m_RotationGizmo.GetOnRotate().Clear();
		m_RotationGizmo = null;
	}
	
	//-----------------------------------------------------------------------
	void OnCamerDistanceChange(float newCameraDistance, WorldEditorAPI api)
	{
		if (!m_RotationGizmo) return;

	    m_RotationGizmo.SetRadius(DAB_VisConfig.ComputeGizmoRadius(newCameraDistance));
	    m_RotationGizmo.Render(api);
	}
	
	//-----------------------------------------------------------------------
	void OnGizmoRotate(DAB_Axis axis, float delta)
	{
		//delta *= 0.1;
		
		if(!m_currentTransform)
		{
			Print("Triggered OnGizmoRotate but there was not current transform!", LogLevel.WARNING);
			return;
		}
		
		vector deltaMatrix[3];
		switch (axis)
		{
			case DAB_Axis.X_Axis: Math3D.AnglesToMatrix(Vector(0,      -delta, 0     ), deltaMatrix); break;
			case DAB_Axis.Y_Axis: Math3D.AnglesToMatrix(Vector(delta,  0,      0     ), deltaMatrix); break;
			case DAB_Axis.Z_Axis: Math3D.AnglesToMatrix(Vector(0,      0,      -delta), deltaMatrix); break;
			default: return;
		}
		
		vector newAccum[3];
		Math3D.MatrixMultiply3(m_mAccumRotation, deltaMatrix, newAccum);
		Math3D.MatrixCopy(newAccum, m_mAccumRotation);
		
		vector ypr = Math3D.MatrixToAngles(m_mAccumRotation);
		vector newRotation = Vector(ypr[1], ypr[0], ypr[2]);

		m_currentTransform.m_vRotationOffset = newRotation;
		
		m_OnTransformChanged.Invoke(m_currentTransform);
		
		UpdateGizmo();
	}
	
	//-----------------------------------------------------------------------
	protected void UpdateGizmo()
	{
	    if(!m_RotationGizmo || !m_currentTransform)
	    {
	        Print("There is no gizmo or transform to update!", LogLevel.ERROR);
	        return;
	    }
	
	    m_RotationGizmo.SetPosition(m_currentTransform.GetAdjustedPosition());
	
	    // Combine entity rotation + bone original rotation + accumulated delta
	    vector boneOrigMat[3];
	    vector orig = m_currentTransform.GetOriginalRotation();
	    Math3D.AnglesToMatrix(Vector(orig[1], orig[0], orig[2]), boneOrigMat);
	
	    vector combined[3];
	    Math3D.MatrixMultiply3(m_mEntityRotation, boneOrigMat, combined);
	    Math3D.MatrixMultiply3(combined, m_mAccumRotation, combined);
	    vector worldAngles = Math3D.MatrixToAngles(combined);
	    m_RotationGizmo.SetRotation(Vector(worldAngles[1], worldAngles[0], worldAngles[2]));
	
	    m_RotationGizmo.Render(m_API);
	}
	
	//-----------------------------------------------------------------------
	DAB_GizmoMode GetGizmoMode() { return m_gizmoMode; }
}