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
	protected ref DAB_MoveGizmo     m_PositionGizmo;

	protected ref ScriptInvokerBase<DAB_GizmoController_OnTransformChanged> m_OnTransformChanged;

	protected ref DAB_BoneTransform m_currentTransform;

	// Cached so CreatePositionGizmo/CreateRotationGizmo can rebuild at any time
	protected IEntity m_Entity;

	protected WorldEditorAPI m_API;

	// Accumulated rotation delta since Attach (rotation mode only)
	protected vector m_mAccumRotation[3];

	// Entity world rotation captured at Attach time
	protected vector m_mEntityRotation[3];


	//-----------------------------------------------------------------------
	void DAB_GizmoController(WorldEditorAPI api)
	{
		m_gizmoMode = DAB_GizmoMode.Deactivated;
		m_API = api;
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
	// Attach to a bone. Always starts in Rotation mode.
	// Call Clear() first if already attached.
	void Attach(IEntity entity, DAB_BoneTransform boneToAttach, WorldEditorAPI api, float cameraDistance)
	{
		if (m_currentTransform != null)
		{
			Print("Tried to attach to new bone, but old one was not cleared first!", LogLevel.WARNING);
			Clear(api);
		}

		m_currentTransform = boneToAttach;
		m_Entity           = entity;

		// Capture entity orientation so we can express bone axes in world space
		vector entityWorld[4];
		entity.GetTransform(entityWorld);
		m_mEntityRotation[0] = entityWorld[0];
		m_mEntityRotation[1] = entityWorld[1];
		m_mEntityRotation[2] = entityWorld[2];

		// Rotation delta starts at identity on every new attachment
		Math3D.MatrixIdentity3(m_mAccumRotation);

		m_gizmoMode = DAB_GizmoMode.Rotation;
		CreateRotationGizmo(api);
	}

	//-----------------------------------------------------------------------
	// Switch between Rotation and Position (move) modes while a bone is
	// attached. Safe to call even if already in the requested mode (no-op).
	void SwitchMode(DAB_GizmoMode newMode, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		if (m_gizmoMode == newMode) return;

		// Tear down whichever gizmo is currently live
		if (m_RotationGizmo)
		{
			m_RotationGizmo.GetOnRotate().Clear();
			m_RotationGizmo = null;
		}
		if (m_PositionGizmo)
		{
			m_PositionGizmo.GetOnMove().Clear();
			m_PositionGizmo = null;
		}

		m_gizmoMode = newMode;

		switch (m_gizmoMode)
		{
			case DAB_GizmoMode.Rotation: CreateRotationGizmo(api); break;
			case DAB_GizmoMode.Position: CreatePositionGizmo(api); break;
		}
	}

	//-----------------------------------------------------------------------
	void OnMouseMove(float x, float y, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		switch (m_gizmoMode)
		{
			case DAB_GizmoMode.Rotation: if (m_RotationGizmo) m_RotationGizmo.OnMouseMove(x, y, api); break;
			case DAB_GizmoMode.Position: if (m_PositionGizmo) m_PositionGizmo.OnMouseMove(x, y, api); break;
		}
	}

	//-----------------------------------------------------------------------
	void OnMousePress(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		switch (m_gizmoMode)
		{
			case DAB_GizmoMode.Rotation: if (m_RotationGizmo) m_RotationGizmo.OnMousePress(x, y, buttons, api); break;
			case DAB_GizmoMode.Position: if (m_PositionGizmo) m_PositionGizmo.OnMousePress(x, y, buttons, api); break;
		}
	}

	//-----------------------------------------------------------------------
	void OnMouseRelease(float x, float y, WETMouseButtonFlag buttons, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;
		switch (m_gizmoMode)
		{
			case DAB_GizmoMode.Rotation: if (m_RotationGizmo) m_RotationGizmo.OnMouseRelease(buttons); break;
			case DAB_GizmoMode.Position: if (m_PositionGizmo) m_PositionGizmo.OnMouseRelease(buttons); break;
		}
	}

	//-----------------------------------------------------------------------
	void Clear(WorldEditorAPI api)
	{
		m_currentTransform = null;
		m_Entity           = null;
		m_gizmoMode        = DAB_GizmoMode.Deactivated;

		if (m_RotationGizmo) m_RotationGizmo.GetOnRotate().Clear();
		m_RotationGizmo = null;

		if (m_PositionGizmo) m_PositionGizmo.GetOnMove().Clear();
		m_PositionGizmo = null;
	}

	//-----------------------------------------------------------------------
	void OnCameraDistanceChange(float newCameraDistance, WorldEditorAPI api)
	{
		if (!m_currentTransform) return;

		vector gizmoPos = m_currentTransform.GetAdjustedPosition();
		float  newRadius = DAB_VisConfig.ComputeGizmoRadius(api, gizmoPos);

		switch (m_gizmoMode)
		{
			case DAB_GizmoMode.Rotation:
				if (m_RotationGizmo) { m_RotationGizmo.SetRadius(newRadius); m_RotationGizmo.Render(api); }
				break;
			case DAB_GizmoMode.Position:
				if (m_PositionGizmo) { m_PositionGizmo.SetRadius(newRadius); m_PositionGizmo.Render(api); }
				break;
		}
	}

	//-----------------------------------------------------------------------
	DAB_GizmoMode GetGizmoMode() { return m_gizmoMode; }

	// ── Private ────────────────────────────────────────────────────────────

	//-----------------------------------------------------------------------
	// Computes combined = entityRotation × boneOriginalRotation × accumRotation.
	// This is the bone's current world orientation and is used by both gizmos
	// so they always point along the correct local axes.
	protected void ComputeCombinedMatrix(out vector combined[3])
	{
		vector boneOrigMat[3];
		vector orig = m_currentTransform.GetOriginalRotation();
		Math3D.AnglesToMatrix(Vector(orig[1], orig[0], orig[2]), boneOrigMat);

		Math3D.MatrixMultiply3(m_mEntityRotation, boneOrigMat, combined);
		Math3D.MatrixMultiply3(combined, m_mAccumRotation, combined);
	}

	//-----------------------------------------------------------------------
	protected void CreateRotationGizmo(WorldEditorAPI api)
	{
		vector combined[3];
		ComputeCombinedMatrix(combined);
		vector worldAngles = Math3D.MatrixToAngles(combined);

		vector position = m_currentTransform.GetAdjustedPosition();

		m_RotationGizmo = new DAB_RotationGizmo(
			position,
			Vector(worldAngles[1], worldAngles[0], worldAngles[2]),
			DAB_VisConfig.ComputeGizmoRadius(api, position)
		);
		m_RotationGizmo.GetOnRotate().Insert(this.OnGizmoRotate);
		m_RotationGizmo.Render(api);
	}

	//-----------------------------------------------------------------------
	// The move gizmo arrows follow the bone's current local orientation
	// (entity × boneOrig × accumRot), matching the rotation gizmo rings.
	protected void CreatePositionGizmo(WorldEditorAPI api)
	{
		vector combined[3];
		ComputeCombinedMatrix(combined);
		vector worldAngles = Math3D.MatrixToAngles(combined);

		vector position = m_currentTransform.GetAdjustedPosition();

		m_PositionGizmo = new DAB_MoveGizmo(
			position,
			Vector(worldAngles[1], worldAngles[0], worldAngles[2]),
			DAB_VisConfig.ComputeGizmoRadius(api, position)
		);
		m_PositionGizmo.GetOnMove().Insert(this.OnGizmoMove);
		m_PositionGizmo.Render(api);
	}

	//-----------------------------------------------------------------------
	protected void OnGizmoRotate(DAB_Axis axis, float delta)
	{
		if (!m_currentTransform)
		{
			Print("OnGizmoRotate: no current transform!", LogLevel.WARNING);
			return;
		}

		vector deltaMatrix[3];
		switch (axis)
		{
			case DAB_Axis.X_Axis: Math3D.AnglesToMatrix(Vector(0,     -delta, 0     ), deltaMatrix); break;
			case DAB_Axis.Y_Axis: Math3D.AnglesToMatrix(Vector(delta,  0,     0     ), deltaMatrix); break;
			case DAB_Axis.Z_Axis: Math3D.AnglesToMatrix(Vector(0,      0,     -delta), deltaMatrix); break;
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
	// Fired by DAB_MoveGizmo each frame during a drag.
	// delta is a signed world-space distance along the axis (metres).
	//
	// Position offset is stored as a world-space displacement so that
	// GetAdjustedPosition() (= originalWorldPos + positionOffset) stays valid.
	// The tool's RefreshBone() converts it to entity-local space before
	// passing it to anim.SetBone().
	protected void OnGizmoMove(DAB_Axis axis, float delta)
	{
		if (!m_currentTransform)
		{
			Print("OnGizmoMove: no current transform!", LogLevel.WARNING);
			return;
		}

		// Resolve which world-space direction this axis points right now
		vector combined[3];
		ComputeCombinedMatrix(combined);

		vector worldAxisDir;
		switch (axis)
		{
			case DAB_Axis.X_Axis: worldAxisDir = combined[0] * m_PositionGizmo.GetXSign(); break;
			case DAB_Axis.Y_Axis: worldAxisDir = combined[1] * m_PositionGizmo.GetYSign(); break;
			// Use GetZSign() from the gizmo instead of a hardcoded -1.
			// The move gizmo flips the Z arrow each Render() to always face the
			// camera; GetZSign() returns that same flip so the drag displacement
			// stays in the arrow direction regardless of which side the camera is on.
			case DAB_Axis.Z_Axis: worldAxisDir = combined[2] * m_PositionGizmo.GetZSign(); break;
			default: return;
		}

		// Accumulate displacement in world space
		m_currentTransform.m_vPositionOffset = m_currentTransform.m_vPositionOffset + worldAxisDir * delta;

		m_OnTransformChanged.Invoke(m_currentTransform);

		UpdatePositionGizmo();
	}

	//-----------------------------------------------------------------------
	protected void UpdateRotationGizmo()
	{
		if (!m_RotationGizmo || !m_currentTransform)
		{
			Print("UpdateRotationGizmo: gizmo or transform is missing!", LogLevel.ERROR);
			return;
		}

		vector gizmoPos = m_currentTransform.GetAdjustedPosition();
		m_RotationGizmo.SetPosition(gizmoPos);
		m_RotationGizmo.SetRadius(DAB_VisConfig.ComputeGizmoRadius(m_API, gizmoPos));

		vector combined[3];
		ComputeCombinedMatrix(combined);
		vector worldAngles = Math3D.MatrixToAngles(combined);
		m_RotationGizmo.SetRotation(Vector(worldAngles[1], worldAngles[0], worldAngles[2]));

		m_RotationGizmo.Render(m_API);
	}

	//-----------------------------------------------------------------------
	// After a position drag: move the gizmo to follow the bone.
	// Arrow orientation is unchanged — only position and scale are updated.
	protected void UpdatePositionGizmo()
	{
		if (!m_PositionGizmo || !m_currentTransform)
		{
			Print("UpdatePositionGizmo: gizmo or transform is missing!", LogLevel.ERROR);
			return;
		}

		vector gizmoPos = m_currentTransform.GetAdjustedPosition();
		m_PositionGizmo.SetPosition(gizmoPos);
		m_PositionGizmo.SetRadius(DAB_VisConfig.ComputeGizmoRadius(m_API, gizmoPos));
		m_PositionGizmo.Render(m_API);
	}
}