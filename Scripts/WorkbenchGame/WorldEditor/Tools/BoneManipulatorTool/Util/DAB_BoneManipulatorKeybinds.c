[BaseContainerProps(configRoot: true)]
class DAB_BoneManipulatorKeybinds
{
	// ── Keybind Configuration ──────────────────────────────────────────────

	[Attribute(defvalue: "KC_ESCAPE", uiwidget: UIWidgets.ComboBox, desc: "Key used to deselect the currently selected bone (Escape by default)", enumType: KeyCode)]
	protected KeyCode m_BoneDeselect;

	[Attribute("KC_J", UIWidgets.ComboBox, "Key used to reset the currently selected bone to its original transform", "", NULL, "", 3, KeyCode)]
	protected KeyCode m_Reset;

	[Attribute("KC_U", UIWidgets.ComboBox, "Key used to activate the Rotation gizmo/tool", "", NULL, "", 3, KeyCode)]
	protected KeyCode m_RotationTool;

	[Attribute("KC_I", UIWidgets.ComboBox, "Key used to activate the Position/Move gizmo/tool", "", NULL, "", 3, KeyCode)]
	protected KeyCode m_MoveTool;

	[Attribute("KC_O", UIWidgets.ComboBox, "Key used to activate the Scale gizmo/tool", "", NULL, "", 3, KeyCode)]
	protected KeyCode m_ScaleTool;

	// ── Public Getters ─────────────────────────────────────────────────────
	KeyCode GetBoneDeselect() { return m_BoneDeselect; }
	KeyCode GetReset() { return m_Reset; }
	KeyCode GetRotationTool() { return m_RotationTool; }
	KeyCode GetMoveTool() { return m_MoveTool; }
	KeyCode GetScaleTool() { return m_ScaleTool; }
}