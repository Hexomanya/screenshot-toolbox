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
	// ── Keybinds ──
	[Attribute("Custom keybinds configuration for the Bone Manipulator Tool. Keys already used somewhere else in the workbench won't work as override! Leave empty to use built-in defaults (ESC, J, U, I, O).", category: "Keybinds")]
	protected ref DAB_BoneManipulatorKeybinds m_KeyBindsOverwrites;
	
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Prints all received KeyCodes in the console.", category: "Keybinds")]
	protected bool m_bShouldPrintKeyDebug;
	
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
	
	[Attribute(defvalue: "", desc: "Show only bones that include this string. Does nothing when empty", category: "Display - Filter")]
	protected string m_sFilterBoneName;
	
	// ── Saving ──
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Turns on auto-saves. This will lead to some lage after each edit", category: "Saving")]
	protected bool m_bShouldAutoSave;
	
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
	protected IEntity m_TargetEntity;
	protected IEntitySource m_TargetEntitySource;
	protected DAB_PoseModificationComponent m_TargetComponent;
	protected ref DAB_EditorController m_EditorController;
	
	// ── Tool Interaction ───────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	[ButtonAttribute("Save Edits")]
	void SaveEdits()
	{
		DAB_ToolButtonInteractions.SaveEdits(this, m_EditorController); 
	}
	
	[ButtonAttribute("Create Config")]
	void CreateNewConfig()
	{
	    DAB_ToolButtonInteractions.CreateNewConfig(this, m_API, m_sWorkingConfigFolder);
	}
	
	[ButtonAttribute("Create Track")]
	void CreateCinematicTrack()
	{
		DAB_ToolButtonInteractions.CreateCinematicTrack(m_TargetEntity, m_API);
	}
	
	[ButtonAttribute("Create Component")]
	void CreateComponent()
	{
		DAB_ToolButtonInteractions.CreateComponent(this, m_API);
	}
	
	// ── Lifecycle ──────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	override void OnActivate()
	{
		if(!m_EditorController) m_EditorController = DAB_EditorController(this, m_API);
		RefetchAndRebuildTargetEntity(true);
		m_EditorController.OnActivate(m_API);
	}

	//-----------------------------------------------------------------------
	override void OnDeActivate()
	{
		if(m_EditorController.GetDirtyBoneCount() > 0)
			DAB_ToolButtonInteractions.SaveEdits(this, m_EditorController, true); 
		
		m_EditorController.OnDeActivate();
		
		m_TargetEntity = null;
		m_TargetComponent = null;
		m_TargetEntitySource = null;
	}

	//-----------------------------------------------------------------------
	override void OnAfterLoadWorld()
	{
		if(!m_EditorController) m_EditorController = DAB_EditorController(this, m_API); // Can not be in constructor, because api is null otherwise
		RefreshTargetEntity();
		m_EditorController.OnAfterLoadWorld();
	}
	
	//-----------------------------------------------------------------------
	override void OnBeforeUnloadWorld()
	{
		m_EditorController.OnBeforeUnloadWorld();
		m_EditorController = null;
	}

	// ── Input ──────────────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	protected override void OnEnterEvent()
	{
		RefetchAndRebuildTargetEntity(true); // If settings are changed in the component we loose the m_TargetEntity referenve
		
		if(!m_EditorController) return;
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
	
	// ── Public ────────────────────────────────────────────────────────────
	void RefreshTargetComponent()
	{
		if(!m_TargetEntity) return;
		m_TargetComponent = DAB_PoseModificationComponent.Cast(m_TargetEntity.FindComponent(DAB_PoseModificationComponent));
	}
	
	void ClearTargetComponent()
	{
	    m_TargetComponent = null;
	}
	
	// TODO: Does the skeleton cache still gives us any benefit?
	void RefetchAndRebuildTargetEntity(bool notifyEditorController = true)
	{
		m_TargetEntitySource = m_API.GetSelectedEntity(0);
		bool didLooseReference = m_EditorController.DidLooseReference(m_TargetEntitySource, m_TargetEntity);
		if(!didLooseReference) return;
		
		Print("Refetching Enity to fix lost references");
		IEntity newEntity = m_API.SourceToEntity(m_TargetEntitySource);
		
		if(newEntity && notifyEditorController) m_EditorController.ForceSkeletonRefresh(newEntity);
		if(newEntity == m_TargetEntity) return;
		
		m_TargetEntity = newEntity;
		RefreshTargetComponent();
		if(notifyEditorController) m_EditorController.OnTargetEntityChanged(m_TargetEntity);
	}
	
	// TODO: Use dedicated functions to clean this mess up?
	// We need forceSkeletonRefresh and notifyEditorController because of when we create a config, we loose references to the entities, because they reinit.
	void RefreshTargetEntity(bool notifyEditorController = true)
	{
		m_TargetEntitySource = m_API.GetSelectedEntity(0);
		IEntity newEntity = m_API.SourceToEntity(m_TargetEntitySource);
		if(newEntity && notifyEditorController) m_EditorController.ForceSkeletonRefresh(newEntity);
		
		if(newEntity == m_TargetEntity) return;
		
		m_TargetEntity = newEntity;
		RefreshTargetComponent();
		if(notifyEditorController) m_EditorController.OnTargetEntityChanged(m_TargetEntity);
	}
	
	
	void ReselectTargetEntity(bool notifyEditorController = true)
	{
		if(!m_TargetEntitySource)
		{
			Print("Target source is null. Can not reselect!", LogLevel.WARNING);
			return;
		}
		
		m_API.SetEntitySelection(m_TargetEntitySource);
		RefetchAndRebuildTargetEntity(notifyEditorController);
	}
	
	// ── Public Getters ────────────────────────────────────────────────────
	//-----------------------------------------------------------------------
	IEntity GetCurrentTargetEntity(){ return m_TargetEntity; }
	IEntitySource GetCurrentTargetEntitySource(){ return m_TargetEntitySource; } //TODO: Could be removed in theory
	DAB_PoseModificationComponent GetTargetComponent(){ return m_TargetComponent; }
	DAB_BoneDisplaySettings GetNewDisplaySettings() { return new DAB_BoneDisplaySettings(m_bHideIKTargetBones, m_bHidePropBones, m_bHideVolumeBones, m_bHideCameraBone, m_bHideFaceBones, m_bHideBoneConnections, m_sFilterBoneName); }
	bool GetShouldAutoSave(){ return m_bShouldAutoSave; }
	DAB_BoneManipulatorKeybinds GetKeybindsOverwrites() { return m_KeyBindsOverwrites; }
	bool GetShouldPrintKeyDebug() { return m_bShouldPrintKeyDebug; }
}