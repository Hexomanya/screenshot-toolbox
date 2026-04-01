[WorkbenchToolAttribute(
	name: "Bone Manipulator",
	description: "Direct skeleton posing via viewport gizmos",
	awesomeFontCode: 0xf5d7
)]
//! Workbench tool for posing individual bones via viewport gizmos.
//! Select a bone by clicking its sphere; manipulate with rotation (U),
//! position (I) or scale (O) gizmos. Press J to reset the selected bone.
class DAB_BoneManipulatorTool : WorldEditorTool
{

	// ── Tool attributes ────────────────────────────────────────────────────
	// ── Display ──
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide IK target bones from the overlay", category: "Display")]
	protected bool m_bHideIKTargetBones;
	
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide Prop bones from the overlay", category: "Display")]
	protected bool m_bHidePropBones;
	
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide volume bones from the overlay", category: "Display")]
	protected bool m_bHideVolumeBones;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide the camera bone from the overlay", category: "Display")]
	protected bool m_bHideCameraBone;
	
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Hide every child of the 'Head' bone", category: "Display")]
	protected bool m_bHideFaceBones;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Hide lines connecting bones to their parents", category: "Display")]
	protected bool m_bHideBoneConnections;
	
	[Attribute(defvalue: "", desc: "Show only bones that include this string. Does nothing when empty", category: "Display")]
	protected string m_sFilterBoneName;
	
	// ── Saving ──
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Turns on auto-saves. This will lead to some lage after each edit", category: "Saving")]
	protected bool m_bShouldAutoSave;
	
	// ── Current Pose Config ──
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select an existing pose modification to load and edit",
	    params: "conf DAB_PoseModification",
	    category: "Current Pose Config"
	)]
	protected ResourceName m_sWorkingConfig;
	
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select existing pose modifications which will be previewed and copied to the cinematic timeline. Current config overwrites these, which is not guranteed in the timeline.",
	    params: "conf DAB_PoseModification",
	    category: "Current Pose Config"
	)]
	protected ref array<ResourceName> m_aStackedConfigs;
	
	// ── New Config Creation ──
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select the folder you are currently working in. New configs will be created there!",
	    params: "FileNameFormat=absolute folders",
	    category: "New Config Creation"
	)]
	protected ResourceName m_sWorkingConfigFolder;
	
	// ── Constructors ──────────────────────────────────────────────────────────
	void DAB_BoneManipulatorTool();
	void ~DAB_BoneManipulatorTool();
	
	// ── Runtime state ──────────────────────────────────────────────────────
	protected IEntity       m_TargetEntity;
	protected IEntitySource m_TargetEntitySource;
	protected ref DAB_EditorController m_EditorController;
	
	
	// ── Tool Interaction ───────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	[ButtonAttribute("Save Edits")]
	void SaveEdits()
	{
		DAB_ToolButtonInteractions.SaveEdits(this, m_EditorController); 
	}
	
	[ButtonAttribute("Create New Config")]
	void CreateNewConfig()
	{
	    DAB_ToolButtonInteractions.CreateNewConfig(this, m_sWorkingConfig, m_sWorkingConfigFolder);
	}
	
	[ButtonAttribute("Copy To Scene")]
	void CopyToCinematicScene()
	{
		DAB_ToolButtonInteractions.CopyToCinematicScene(m_sWorkingConfig, m_aStackedConfigs, m_TargetEntity, m_API);
	}
	
	// ── Lifecycle ──────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	override void OnActivate()
	{
		m_EditorController = DAB_EditorController(this, m_API); // Can not be in constructor, because api is null otherwise
		RefreshTargetEntity();
		m_EditorController.OnActivate();
	}

	//-----------------------------------------------------------------------
	override void OnDeActivate()
	{
		if(m_EditorController.GetDirtyBoneCount() > 0)
			DAB_ToolButtonInteractions.SaveEdits(this, m_EditorController, true); 
		
		m_EditorController.OnDeActivate();
		m_EditorController = null;
	}

	//-----------------------------------------------------------------------
	override void OnAfterLoadWorld()
	{
		RefreshTargetEntity();
		m_EditorController.OnAfterLoadWorld();
	}

	// ── Input ──────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected override void OnEnterEvent()
	{
		m_EditorController.OnEnterEvent();
	}
	
	//-----------------------------------------------------------------------
	override void OnMouseMoveEvent(float x, float y)
	{
		m_EditorController.OnMouseMoveEvent(x, y);
	}

	//-----------------------------------------------------------------------
	override void OnMousePressEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		m_EditorController.OnMousePressEvent(x, y, buttons);
	}

	//-----------------------------------------------------------------------
	override void OnMouseReleaseEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		m_EditorController.OnMouseReleaseEvent(x, y, buttons);
	}

	//-----------------------------------------------------------------------
	//! Key bindings: U = Rotation, I = Position, O = Scale, J = Reset bone, Escape = Deselect.
	override void OnKeyPressEvent(KeyCode key, bool isAutoRepeat)
	{
		m_EditorController.OnKeyPressEvent(key, isAutoRepeat);
	}
	
	// ── Public Getters ────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	IEntity GetCurrentTargetEntity(){ return m_TargetEntity; }
	IEntitySource GetCurrentTargetEntitySource(){ return m_TargetEntitySource; }
	DAB_BoneDisplaySettings GetNewDisplaySettings() { return new DAB_BoneDisplaySettings(m_bHideIKTargetBones, m_bHidePropBones, m_bHideVolumeBones, m_bHideCameraBone, m_bHideFaceBones, m_bHideBoneConnections, m_sFilterBoneName); }
	ResourceName GetWorkingConfig() { return m_sWorkingConfig; }
	bool GetShouldAutoSave(){ return m_bShouldAutoSave; }
	
	// ── Public Setters ────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	void SetWorkingConfig(ResourceName newConfig){ m_sWorkingConfig = newConfig; }
	
	// ── Private ───────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected void RefreshTargetEntity()
	{
		m_TargetEntitySource = m_API.GetSelectedEntity(0);
		if (m_TargetEntitySource)
			m_TargetEntity = m_API.SourceToEntity(m_TargetEntitySource);

		if(m_EditorController)
			m_EditorController.OnTargetEntityChanged(m_TargetEntity);
	}
}