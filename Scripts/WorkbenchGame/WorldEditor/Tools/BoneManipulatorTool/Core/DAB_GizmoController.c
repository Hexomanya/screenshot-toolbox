enum DAB_GizmoMode
{
	Deactivated,
	Position,
	Rotation,
	Scale,
}

void DAB_GizmoController_OnTransformChanged(DAB_BoneTransform changedTransform);
typedef func DAB_GizmoController_OnTransformChanged;

//! Owns and coordinates the three gizmos (rotation, move, scale) for a single selected bone.
//! Routes input events to the active gizmo and broadcasts transform changes via OnTransformChanged.
class DAB_GizmoController
{
	// ── State ──────────────────────────────────────────────────────────────
	protected DAB_GizmoMode m_GizmoMode;
	protected DAB_GizmoMode m_LastActiveMode;

	protected ref DAB_RotationGizmo m_RotationGizmo;
	protected ref DAB_MoveGizmo m_PositionGizmo;
	protected ref DAB_ScaleGizmo m_ScaleGizmo;

	protected ref ScriptInvokerBase<DAB_GizmoController_OnTransformChanged> m_OnTransformChanged;

	protected ref DAB_BoneTransform m_currentTransform;
	protected IEntity m_AttachedEntity;

	protected vector m_mAccumRotation[3]; // Accumulated rotation since Attach() — applies bone-local frame rotations.
	protected vector m_mEntityRotation[3]; // Entity world rotation captured at Attach() time.

	protected WorldEditorAPI m_API;

	//-----------------------------------------------------------------------
	void DAB_GizmoController(WorldEditorAPI api)
	{
		m_GizmoMode = DAB_GizmoMode.Deactivated;
		m_LastActiveMode = DAB_GizmoMode.Rotation; // Default to rotation
		m_API = api;
	}

	//-----------------------------------------------------------------------
	void ~DAB_GizmoController() 
	{ 
		m_API = null; 
		m_AttachedEntity = null;
	}

	//-----------------------------------------------------------------------
	//! ScriptInvoker fired whenever the attached bone's transform changes.
	ScriptInvokerBase<DAB_GizmoController_OnTransformChanged> GetOnTransformChanged()
	{
		if (!m_OnTransformChanged)
			m_OnTransformChanged = new ScriptInvokerBase<DAB_GizmoController_OnTransformChanged>();
		return m_OnTransformChanged;
	}

	//-----------------------------------------------------------------------
	//! Attaches to a bone and starts in Rotation mode.
	//! Call Clear() first if already attached.
	void Attach(IEntity entity, DAB_BoneTransform boneToAttach)
	{
		if(!entity || !boneToAttach)
		{
			PrintFormat("DAB_GizmoController.Attach: Arguments for attaching are illegal! entity: %1; boneToAttach: %2", entity, boneToAttach, LogLevel.ERROR);
			Clear();
			return;
		}
		
		if (m_currentTransform)
		{
			Print("DAB_GizmoController.Attach: previous bone was not cleared first.", LogLevel.WARNING);
			Clear();
			return;
		}

		m_AttachedEntity  = entity;
		m_currentTransform = boneToAttach;

		vector entityWorld[4];
		entity.GetWorldTransform(entityWorld);
		m_mEntityRotation[0] = entityWorld[0];
		m_mEntityRotation[1] = entityWorld[1];
		m_mEntityRotation[2] = entityWorld[2];

		vector existingRot = boneToAttach.m_vRotationOffset;
		Math3D.AnglesToMatrix(Vector(existingRot[1], existingRot[0], existingRot[2]), m_mAccumRotation);

		SwitchMode(m_LastActiveMode, m_API);
	}

	//-----------------------------------------------------------------------
	//! Switches to a different gizmo mode while a bone is attached. No-op if already in that mode.
	void SwitchMode(DAB_GizmoMode newMode, WorldEditorAPI api)
	{
		if (!m_currentTransform || m_GizmoMode == newMode) return;

		DestroyActiveGizmo();
		m_GizmoMode = newMode;
		if(m_GizmoMode != DAB_GizmoMode.Deactivated) m_LastActiveMode = m_GizmoMode;

		switch (m_GizmoMode)
		{
			case DAB_GizmoMode.Rotation: CreateRotationGizmo(api); break;
			case DAB_GizmoMode.Position: CreatePositionGizmo(api); break;
			case DAB_GizmoMode.Scale:    CreateScaleGizmo(api);    break;
		}
	}

	//-----------------------------------------------------------------------
	//! Detaches from the current bone and destroys all gizmos.
	void Clear()
	{
		m_AttachedEntity = null;
		m_currentTransform = null;
		m_GizmoMode = DAB_GizmoMode.Deactivated;
		DestroyActiveGizmo();
	}

	//-----------------------------------------------------------------------
	//! Forwards mouse-move input to the active gizmo.
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		switch (m_GizmoMode)
		{
			case DAB_GizmoMode.Rotation: if (m_RotationGizmo) m_RotationGizmo.OnMouseMove(x, y, api); break;
			case DAB_GizmoMode.Position: if (m_PositionGizmo) m_PositionGizmo.OnMouseMove(x, y, api); break;
			case DAB_GizmoMode.Scale:    if (m_ScaleGizmo)    m_ScaleGizmo.OnMouseMove(x, y, api);    break;
		}
	}

	//-----------------------------------------------------------------------
	//! Forwards mouse-press input to the active gizmo.
	void OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		switch (m_GizmoMode)
		{
			case DAB_GizmoMode.Rotation: if (m_RotationGizmo) m_RotationGizmo.OnMousePress(x, y, buttons, api); break;
			case DAB_GizmoMode.Position: if (m_PositionGizmo) m_PositionGizmo.OnMousePress(x, y, buttons, api); break;
			case DAB_GizmoMode.Scale:    if (m_ScaleGizmo)    m_ScaleGizmo.OnMousePress(x, y, buttons, api);    break;
		}
	}

	//-----------------------------------------------------------------------
	//! Forwards mouse-release input to the active gizmo.
	void OnMouseRelease(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		switch (m_GizmoMode)
		{
			case DAB_GizmoMode.Rotation: if (m_RotationGizmo) m_RotationGizmo.OnMouseRelease(buttons, api); break;
			case DAB_GizmoMode.Position: if (m_PositionGizmo) m_PositionGizmo.OnMouseRelease(buttons);      break;
			case DAB_GizmoMode.Scale:    if (m_ScaleGizmo)    m_ScaleGizmo.OnMouseRelease(buttons);         break;
		}
	}

	//-----------------------------------------------------------------------
	//! Updates gizmo size when the camera distance changes.
	void OnCameraDistanceChange(WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
	
		vector gizmoPos    = GetCurrentGizmoPosition();
		float  newRadius   = DAB_VisConfig.ComputeGizmoRadius(api, gizmoPos);
		vector worldAngles = GetCurrentWorldAngles();
	
		// TODO: Use nicer structure for gizmo so we do not need to do this
		switch (m_GizmoMode)
		{
			case DAB_GizmoMode.Rotation:
				if (!m_RotationGizmo) break;
				
				m_RotationGizmo.SetPosition(gizmoPos);
				m_RotationGizmo.SetRadius(newRadius);
				m_RotationGizmo.SetRotation(worldAngles);
				m_RotationGizmo.Render(api);
				break;
	
			case DAB_GizmoMode.Position:
				if (!m_PositionGizmo) break;
				
				m_PositionGizmo.SetPosition(gizmoPos);
				m_PositionGizmo.SetRadius(newRadius);
				m_PositionGizmo.SetRotation(worldAngles);
				m_PositionGizmo.Render(api);
				break;
	
			case DAB_GizmoMode.Scale:
				if (!m_ScaleGizmo) break;
				
				m_ScaleGizmo.SetPosition(gizmoPos);
				m_ScaleGizmo.SetRadius(newRadius);
				m_ScaleGizmo.Render(api);
				break;
		}
	}

	//-----------------------------------------------------------------------
	//! Resets the accumulated rotation to identity without deselecting.
	//! Call this after zeroing a bone's offsets so the next drag starts clean.
	void ResetAccumulatedTransform(WorldEditorAPI api)
	{
		Math3D.MatrixIdentity3(m_mAccumRotation);

		switch (m_GizmoMode)
		{
			case DAB_GizmoMode.Rotation: UpdateRotationGizmo(); break;
			case DAB_GizmoMode.Position: UpdatePositionGizmo(); break;
			case DAB_GizmoMode.Scale:    UpdateScaleGizmo();    break;
		}
	}

	//-----------------------------------------------------------------------
	//! Returns the currently active gizmo mode.
	DAB_GizmoMode GetGizmoMode() { return m_GizmoMode; }

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	//! Uses the actual current bone world position if available.
	protected vector GetCurrentGizmoPosition()
	{
		if (!m_AttachedEntity || !m_currentTransform) return m_currentTransform.GetAdjustedPosition();
		
		vector liveBonePos;
		string simpleBoneName = DAB_SkeletonInfo.ExtractBoneNameFromCompundName(m_currentTransform.GetCompoundBoneName()); //TODO: Better to just build name or extract?
		
		if (DAB_BoneHelper.TryGetBonePosition(m_AttachedEntity, simpleBoneName, liveBonePos))
			return liveBonePos;
		
		return m_currentTransform.GetAdjustedPosition();
	}

	//-----------------------------------------------------------------------
	// Computes combined = entityRotation × boneOriginalRotation × accumRotation.
	// This is the bone's current world orientation, used by all gizmos so they
	// always point along the correct local axes.
	protected void ComputeCombinedMatrix(out vector combined[3])
	{
	    string boneName = DAB_SkeletonInfo.ExtractBoneNameFromCompundName(m_currentTransform.GetCompoundBoneName());
	
	    vector worldMat[4];
	    if (!DAB_BoneHelper.TryGetBoneWorldMatrix(m_AttachedEntity, boneName, worldMat))
	    {
	        PrintFormat("DAB_GizmoController.ComputeCombinedMatrix: Failed to read live bone matrix for '%1'. Gizmo orientation may be wrong.", boneName, LogLevel.ERROR);
	        return;
	    }
	
	    combined[0] = worldMat[0];
	    combined[1] = worldMat[1];
	    combined[2] = worldMat[2];
	}

	//-----------------------------------------------------------------------
	// Returns the gizmo orientation as (pitch, yaw, roll) in degrees,
	// matching the Enfusion (P,Y,Z) convention expected by SetRotation().
	protected vector GetCurrentWorldAngles()
	{
		vector combined[3];
		ComputeCombinedMatrix(combined);
		vector ypr = Math3D.MatrixToAngles(combined);
		return Vector(ypr[1], ypr[0], ypr[2]);
	}

	//-----------------------------------------------------------------------
	protected void CreateRotationGizmo(WorldEditorAPI api)
	{
		vector position = GetCurrentGizmoPosition();
		PrintFormat("Creating gizmo with rotation: %1", GetCurrentWorldAngles());
		m_RotationGizmo = new DAB_RotationGizmo(
			position,
			GetCurrentWorldAngles(),
			DAB_VisConfig.ComputeGizmoRadius(api, position)
		);
		m_RotationGizmo.GetOnRotate().Insert(this.OnGizmoRotate);
		m_RotationGizmo.Render(api);
	}

	//-----------------------------------------------------------------------
	protected void CreatePositionGizmo(WorldEditorAPI api)
	{
		vector position = GetCurrentGizmoPosition();
		m_PositionGizmo = new DAB_MoveGizmo(
			position,
			GetCurrentWorldAngles(),
			DAB_VisConfig.ComputeGizmoRadius(api, position)
		);
		m_PositionGizmo.GetOnMove().Insert(this.OnGizmoMove);
		m_PositionGizmo.Render(api);
	}

	//-----------------------------------------------------------------------
	protected void CreateScaleGizmo(WorldEditorAPI api)
	{
		vector position = GetCurrentGizmoPosition();
		m_ScaleGizmo = new DAB_ScaleGizmo(position, DAB_VisConfig.ComputeGizmoRadius(api, position));
		m_ScaleGizmo.GetOnScale().Insert(this.OnGizmoScale);
		m_ScaleGizmo.Render(api);
	}

	//-----------------------------------------------------------------------
	protected void DestroyActiveGizmo()
	{
		if (m_RotationGizmo) { m_RotationGizmo.GetOnRotate().Clear(); m_RotationGizmo = null; }
		if (m_PositionGizmo) { m_PositionGizmo.GetOnMove().Clear();   m_PositionGizmo = null; }
		if (m_ScaleGizmo)    { m_ScaleGizmo.GetOnScale().Clear();     m_ScaleGizmo    = null; }
	}

	//-----------------------------------------------------------------------
	protected void OnGizmoRotate(DAB_Axis axis, float delta)
	{
		if (!m_currentTransform) return;

		vector deltaMatrix[3];
		switch (axis)
		{
			case DAB_Axis.X_Axis: Math3D.AnglesToMatrix(Vector(0,      -delta, 0     ), deltaMatrix); break;
			case DAB_Axis.Y_Axis: Math3D.AnglesToMatrix(Vector(delta,   0,     0     ), deltaMatrix); break;
			case DAB_Axis.Z_Axis: Math3D.AnglesToMatrix(Vector(0,       0,     -delta), deltaMatrix); break;
			default: return;
		}

		vector newAccum[3];
		Math3D.MatrixMultiply3(m_mAccumRotation, deltaMatrix, newAccum);
		Math3D.MatrixCopy(newAccum, m_mAccumRotation);

		vector ypr = Math3D.MatrixToAngles(m_mAccumRotation);
		m_currentTransform.m_vRotationOffset = Vector(ypr[1], ypr[0], ypr[2]);

		m_OnTransformChanged.Invoke(m_currentTransform);
		UpdateRotationGizmo();
	}

	//-----------------------------------------------------------------------
	protected void OnGizmoMove(DAB_Axis axis, float delta)
	{
		if (!m_currentTransform) return;

		// Resolve the world-space direction for this axis (camera sign already baked in)
		vector combined[3];
		ComputeCombinedMatrix(combined);

		vector worldAxisDir;
		switch (axis)
		{
			case DAB_Axis.X_Axis: worldAxisDir = combined[0] * m_PositionGizmo.GetXSign(); break;
			case DAB_Axis.Y_Axis: worldAxisDir = combined[1] * m_PositionGizmo.GetYSign(); break;
			case DAB_Axis.Z_Axis: worldAxisDir = combined[2] * m_PositionGizmo.GetZSign(); break;
			default: return;
		}

		m_currentTransform.m_vPositionOffset = m_currentTransform.m_vPositionOffset + worldAxisDir * delta;

		m_OnTransformChanged.Invoke(m_currentTransform);
		UpdatePositionGizmo();
	}

	//-----------------------------------------------------------------------
	protected void OnGizmoScale(float delta)
	{
		if (!m_currentTransform) return;

		m_currentTransform.m_fScale += delta * DAB_VisConfig.SCALE_SENSITIVITY;
		if (m_currentTransform.m_fScale < 0.01)
			m_currentTransform.m_fScale = 0.01;

		m_OnTransformChanged.Invoke(m_currentTransform);
		UpdateScaleGizmo();
	}

	//-----------------------------------------------------------------------
	protected void UpdateRotationGizmo()
	{
		if (!m_RotationGizmo || !m_currentTransform) return;

		vector gizmoPos = GetCurrentGizmoPosition();
		m_RotationGizmo.SetPosition(gizmoPos);
		m_RotationGizmo.SetRadius(DAB_VisConfig.ComputeGizmoRadius(m_API, gizmoPos));
		m_RotationGizmo.SetRotation(GetCurrentWorldAngles());
		m_RotationGizmo.Render(m_API);
	}

	//-----------------------------------------------------------------------
	protected void UpdatePositionGizmo()
	{
		if (!m_PositionGizmo || !m_currentTransform) return;
	
		vector gizmoPos = GetCurrentGizmoPosition();
		m_PositionGizmo.SetPosition(gizmoPos);
		m_PositionGizmo.SetRadius(DAB_VisConfig.ComputeGizmoRadius(m_API, gizmoPos));
		m_PositionGizmo.SetRotation(GetCurrentWorldAngles());
		m_PositionGizmo.Render(m_API);
	}

	//-----------------------------------------------------------------------
	protected void UpdateScaleGizmo()
	{
		if (!m_ScaleGizmo || !m_currentTransform) return;

		vector gizmoPos = GetCurrentGizmoPosition();
		m_ScaleGizmo.SetPosition(gizmoPos);
		m_ScaleGizmo.SetRadius(DAB_VisConfig.ComputeGizmoRadius(m_API, gizmoPos));
		m_ScaleGizmo.Render(m_API);
	}
}