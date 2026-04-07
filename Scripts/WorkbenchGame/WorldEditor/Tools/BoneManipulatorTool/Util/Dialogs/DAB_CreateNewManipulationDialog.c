class DAB_CreateNewManipulationDialog
{
	[Attribute(
	    uiwidget: UIWidgets.ResourceNamePicker,
	    desc: "Select the folder you are currently working in. New configs will be created there!",
	    params: "FileNameFormat=absolute folders",
	    category: "New Config Creation"
	)]
	protected ResourceName m_sWorkingConfigFolder;
	
	[Attribute(
	    desc: "The name of the new config",
	    category: "New Config Creation"
	)]
	protected string m_sNewConfigName;

	
	void DAB_CreateNewManipulationDialog(ResourceName workingConfigFolder)
	{
		m_sWorkingConfigFolder = workingConfigFolder;
	}
	
	
	void ~DAB_CreateNewManipulationDialog();
	
	[ButtonAttribute("Create")]
    bool Create()
    {
        return true;
    }

    [ButtonAttribute("Cancel")]
    bool Cancel()
    {
        return false;
    }
	
	ResourceName GetWorkingConfigFolder(){ return m_sWorkingConfigFolder; }
	string GetNewConfigName(){ return m_sNewConfigName; }
}