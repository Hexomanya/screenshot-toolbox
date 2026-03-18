[WorkbenchToolAttribute(
    name: "Bone Manipulator",
    description: "Direct skeleton posing via viewport gizmos",
    awesomeFontCode: 0xf188
)]
class DAB_BoneManipulatorTool : WorldEditorTool
{
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide volume bones from the overlay, because they can overlap other bones!", category: "Display")]
	protected bool m_bHideVolumeBones;
	
    protected IEntity m_TargetEntity;
    protected IEntitySource m_TargetEntitySource;
	
	protected ref DAB_BoneOverlayRenderer m_Renderer;
	protected ref DAB_GizmoController m_gizmoController;
 	
    protected string m_sSelectedBoneName = ""; 
	protected string m_sHoveredBoneName = "";
	
	protected ref map<string, ref DAB_SkeletonInfo> m_CachedSkeletons = new map<string, ref DAB_SkeletonInfo>();
	protected string m_sCurrentSkeleton;
   
	protected ref map<string, ref DAB_BoneTransform> m_ModifiedBones = new map<string, ref DAB_BoneTransform>();
	
	protected vector m_vLastCameraPos = vector.Zero;
	protected bool m_bPollCameraPosition = false;
	protected float m_fCameraTargetDistance = 0;
	
	protected SlotManagerComponent m_SlotManager;
	
    //-----------------------------------------------------------------------
    override void OnActivate()
    {
        m_Renderer = new DAB_BoneOverlayRenderer();
        m_gizmoController = new DAB_GizmoController(m_API);
		m_gizmoController.GetOnTransformChanged().Insert(this.OnBoneTransformChanged);
		
        RefreshTargetEntity();
        RedrawOverlay();			
    }

    //-----------------------------------------------------------------------
    override void OnDeActivate()
    {
		m_gizmoController.Clear(m_API);
        m_Renderer.Clear();
		m_bPollCameraPosition = false;
		m_sSelectedBoneName = "";
    }

    //-----------------------------------------------------------------------
    override void OnMouseMoveEvent(float x, float y)
    {
		m_gizmoController.OnMouseMove(x, y, m_API);
		CheckBoneHover(x, y);
    }
	
    //-----------------------------------------------------------------------
    override void OnMousePressEvent(float x, float y, WETMouseButtonFlag buttons)
    {
		Print("OnMousePressEvent");
		m_gizmoController.OnMousePress(x, y, buttons, m_API);
		
        if (!(buttons & WETMouseButtonFlag.LEFT)) return;
        if (!m_sSelectedBoneName.IsEmpty()) return; // gizmo handles its own input when a bone is active

        string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_TargetEntity, m_API);
        if (!hitBoneName.IsEmpty()) SelectBone(hitBoneName);
    }

    //-----------------------------------------------------------------------
    override void OnMouseReleaseEvent(float x, float y, WETMouseButtonFlag buttons)
    {
		Print("OnMouseReleaseEvent");
		m_gizmoController.OnMouseRelease(x, y, buttons, m_API);
		if((buttons & WETMouseButtonFlag.RIGHT)) RefreshCameraTargetDistance();
    }

    //-----------------------------------------------------------------------
    override void OnKeyPressEvent(KeyCode key, bool isAutoRepeat)
    {
		
		Print("OnKeyPressEvent");
        if (isAutoRepeat) return;

        switch (key)
        {
            case KeyCode.KC_ESCAPE:
                DeselectBone();
                RedrawOverlay();
                break;
        }
		
		RefreshCameraTargetDistance();
    }

    //-----------------------------------------------------------------------
    override void OnAfterLoadWorld()
    {
        RefreshTargetEntity();
        RedrawOverlay();
    }
	
	//-----------------------------------------------------------------------
	protected void OnBoneTransformChanged(DAB_BoneTransform changedTransform)
	{		
		string boneName = changedTransform.GetBoneName();
	   	m_ModifiedBones.Set(boneName, changedTransform);
		RefreshBone(boneName);
	}
	
	//-----------------------------------------------------------------------
	protected void RefreshBone(string boneName)
	{
		if (!m_TargetEntity)
		{
			Print("RefreshBone: no valid entity selected!", LogLevel.ERROR);
			return;
		}	
		
		DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
		if (!transform)
		{
			Print("RefreshBone: bone '" + boneName + "' has no stored transform!", LogLevel.ERROR);
			return;
		}
	
		TNodeId boneId = DAB_BoneHelper.GetBoneId(m_TargetEntity, boneName);
		
		Print("Setting bone to: " + transform.m_vRotationOffset);
		vector rotRad = transform.m_vRotationOffset * Math.DEG2RAD;
		vector rotRadCorrected = Vector(rotRad[1], rotRad[0], rotRad[2]);
		
		Animation anim = GetSlotDependentAnim(boneName);
		anim.SetBone(m_TargetEntity, boneId, rotRadCorrected, transform.m_vPositionOffset, 1.0);
		m_TargetEntity.Update();
	}
	
	//-----------------------------------------------------------------------
	protected Animation GetSlotDependentAnim(string boneName)
	{
		Print("Searching for " + boneName + "slot.");
		if(!m_SlotManager) return m_TargetEntity.GetAnimation();
		
		array<EntitySlotInfo> slotInfos = {};
		m_SlotManager.GetSlotInfos(slotInfos);
		
		foreach (EntitySlotInfo slotInfo : slotInfos)
		{
			IEntity slotEntity = slotInfo.GetAttachedEntity();
			if(!slotEntity) continue;
			
			Animation anim = slotEntity.GetAnimation();
			if(!anim) continue;
			
			TNodeId boneId = anim.GetBoneIndex(boneName);
			
			if(boneId == -1) continue;
			
			return anim;
		}
		
		return m_TargetEntity.GetAnimation();
	}

    //-----------------------------------------------------------------------
    protected void SelectBone(string boneName)
    {
		m_sSelectedBoneName = boneName;
		
		DAB_BoneTransform transform = m_ModifiedBones.Get(boneName);
		if (!transform) transform = CreateNewTransform(boneName);
		
		m_gizmoController.Attach(m_TargetEntity, transform, m_API, m_fCameraTargetDistance);
        RedrawOverlay();
    }
	
	//-----------------------------------------------------------------------
	protected DAB_BoneTransform CreateNewTransform(string boneName)
	{
		TNodeId boneId = DAB_BoneHelper.GetBoneId(m_TargetEntity, boneName);
		
		vector bonePosition;
		if (!DAB_BoneHelper.TryGetBonePosition(m_TargetEntity, boneId, bonePosition))
		{
			Print("CreateNewTransform: could not retrieve position for bone '" + boneName + "'!", LogLevel.ERROR);
			return null;
		}
		
		vector boneRotation;
		if (!DAB_BoneHelper.TryGetBoneLocalRotation(m_TargetEntity, boneId, boneRotation))
		{
			Print("CreateNewTransform: could not retrieve rotation for bone '" + boneName + "'!", LogLevel.ERROR);
			return null;
		}
		
		return new DAB_BoneTransform(boneName, bonePosition, boneRotation);
	}

    //-----------------------------------------------------------------------
    protected void DeselectBone()
    {
        m_sSelectedBoneName = "";
        m_gizmoController.Clear(m_API);
        if (m_TargetEntitySource)
            m_API.SetEntitySelection(m_TargetEntitySource);
    }

    //-----------------------------------------------------------------------
    protected void RefreshTargetEntity()
    {
		m_sCurrentSkeleton = "";
		
        m_TargetEntitySource = m_API.GetSelectedEntity(0);
        if (m_TargetEntitySource)
            m_TargetEntity = m_API.SourceToEntity(m_TargetEntitySource);
		
		if (!m_TargetEntity)
		{
			Print("RefreshTargetEntity: no entity selected.", LogLevel.NORMAL);
			return;
		}
		
		Animation anim = m_TargetEntity.GetAnimation();
		if (!anim)
		{
			Print("RefreshTargetEntity: entity has no Animation component!", LogLevel.ERROR);
			return;
		}
		
		string skeletonKey = DAB_SkeletonInfo.ComputeSkeletonKey(m_TargetEntity);
		if(skeletonKey.IsEmpty())
		{
			Print("RefreshTargetEntity: skeletonKey was empty!", LogLevel.ERROR);
			return;
		}
		
		if(! m_CachedSkeletons.Contains(skeletonKey))
		{
			array<string> boneNames = {};
			anim.GetBoneNames(boneNames);
			map<string, string> boneParents = DAB_BoneHelper.ComputeBoneParents(anim, boneNames);
			//Start here with computing distances!
			DAB_SkeletonInfo newSkeleton = new DAB_SkeletonInfo(skeletonKey, boneNames, boneParents);
			m_CachedSkeletons.Set(skeletonKey, newSkeleton);
		}
		
		m_sCurrentSkeleton = skeletonKey;
		m_SlotManager = SlotManagerComponent.Cast(m_TargetEntity.FindComponent(SlotManagerComponent));
		RefreshCameraTargetDistance();
    }

    //-----------------------------------------------------------------------
    protected void RedrawOverlay()
    {
        if (!m_TargetEntity) return;

		DAB_SkeletonInfo skeletonInfo = GetCurrentSkeletonInfo();
		if(!skeletonInfo) return;
		
		
        if (m_sSelectedBoneName.IsEmpty())
            m_Renderer.DrawAllBones(m_TargetEntity, skeletonInfo, m_sHoveredBoneName, m_fCameraTargetDistance, m_bHideVolumeBones);
        else
            m_Renderer.DrawSelectedBone(m_TargetEntity, skeletonInfo, m_sSelectedBoneName, m_fCameraTargetDistance, m_API); //TODO: If bone selected, take distance to bone
    }
	
	//-----------------------------------------------------------------------
	protected void RefreshCameraTargetDistance()
	{
		if (!m_TargetEntity)
		{
			Print("RefreshCameraTargetDistance: no entity selected.", LogLevel.ERROR);
			return;
		}
		
		vector camPos = DAB_Helper.GetCameraPosition(m_API);
		vector targetPosition = m_TargetEntity.GetOrigin();
		
		float distance = vector.Distance(camPos, targetPosition);
		
		if(DAB_Helper.AreFloatsEqual(distance, m_fCameraTargetDistance, DAB_VisConfig.CAMERA_POLLING_DISTANCE_EPSILON)) return;
		
		m_fCameraTargetDistance = distance;

		RedrawOverlay();
		m_gizmoController.OnCamerDistanceChange(m_fCameraTargetDistance, m_API);
	}
	
	//-----------------------------------------------------------------------
	protected void CheckBoneHover(float x, float y)
	{
		if(!m_sSelectedBoneName.IsEmpty()) return;
		
		string hitBoneName = m_Renderer.PickBoneAtScreenPos(x, y, m_TargetEntity, m_API);
		if(hitBoneName == m_sHoveredBoneName) return;
		
		m_sHoveredBoneName = hitBoneName;
		RedrawOverlay();
	}
	
	//-----------------------------------------------------------------------
	protected DAB_SkeletonInfo GetCurrentSkeletonInfo()
	{
		if(m_sCurrentSkeleton.IsEmpty())
		{
			Print("Provided key was empty!", LogLevel.ERROR);
			return null;
		}

		DAB_SkeletonInfo info;
		if(!m_CachedSkeletons.Find(m_sCurrentSkeleton, info))
		{
			Print("SkeletonInfo for key " + m_sCurrentSkeleton + " could not be found! All possible keys were:", LogLevel.ERROR);
			return null;
		}

		return info;
	}
	
	
	//-----------------------------------------------------------------------
	// We do not have a normal update method, so we have to use this abomanation.
	/*protected void PollCameraPosition()
	{
		m_bPollCameraPosition = true;
		
		while(m_bPollCameraPosition)
		{
			RefreshCameraTargetDistance();
			Sleep(DAB_VisConfig.CAMERA_POLLING_INTERVAL_SEC);
		}
	}*/
	
}