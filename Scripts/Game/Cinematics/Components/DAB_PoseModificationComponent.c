[ComponentEditorProps(category: "DAB_ScreenshotToolbox/Component", description: "Stores pose modification data")]
class DAB_PoseModificationComponentClass : ScriptComponentClass
{
}

class DAB_PoseModificationComponent : ScriptComponent
{
	[Attribute(desc: "Config that will be edited in the pose tool. Can be changed manually or via the tool.")]
	protected ResourceName m_sWorkingPoseModification;
	
	[Attribute(desc: "All Pose modifications data that is used by the cinematic timeline. Can overwrite each other.")]
    protected ref array<ResourceName> m_aPoseModifications = {};
	
	override void EOnInit(IEntity owner)
	{
		Print("Component got initiated in EOnInit");
	}
	
	ResourceName GetWorkingModificationConfig()
	{
		return m_sWorkingPoseModification;
	}
	
	DAB_PoseModification GetWorkingModificationData()
	{
		if(m_sWorkingPoseModification.IsEmpty()) return null;
		
		Resource resource = BaseContainerTools.LoadContainer(m_sWorkingPoseModification);
		if(!resource) return null;	
					
		return DAB_PoseModification.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));	
	}
	
	void MoveWorkingModificationToList()
	{
		if(m_sWorkingPoseModification.IsEmpty()) return;
		if(! m_aPoseModifications) m_aPoseModifications = {};
		if(m_aPoseModifications.Contains(m_sWorkingPoseModification)) return;
		
		m_aPoseModifications.Insert(m_sWorkingPoseModification);
	}
	
	void SetWorkingModificationConfig(ResourceName configPath, WorldEditorAPI api)
	{
	    IEntity ownerEntity = GetOwner();
	    IEntitySource entitySource = api.EntityToSource(ownerEntity);
	    if (!entitySource)
	    {
	        Print("SetWorkingModificationConfig: Could not get entity source!", LogLevel.ERROR);
	        return;
	    }
	
	    // Read the components array to find this component's index
	    // (Reading via BaseContainer methods is valid per the Enfusion API)
	    BaseContainerList components = entitySource.GetObjectArray("components");
	    if (!components)
	    {
	        Print("SetWorkingModificationConfig: No components array on entity!", LogLevel.ERROR);
	        return;
	    }
	
	    int componentIndex = -1;
	    for (int i = 0; i < components.Count(); i++)
	    {
	        BaseContainer comp = components.Get(i);
	        if (comp && comp.GetClassName() == "DAB_PoseModificationComponent")
	        {
	            componentIndex = i;
	            break;
	        }
	    }
	
	    if (componentIndex < 0)
	    {
	        Print("SetWorkingModificationConfig: Could not find DAB_PoseModificationComponent index!", LogLevel.ERROR);
	        return;
	    }
	
	    api.BeginEntityAction();
	    api.SetVariableValue(
	        entitySource,
	        { new ContainerIdPathEntry("components", componentIndex) },
	        "m_sWorkingPoseModification",
	        configPath
	    );
	    api.EndEntityAction();
	
	    // Keep the runtime field in sync for the current session
	    m_sWorkingPoseModification = configPath;
	
	    PrintFormat("SetWorkingModificationConfig: set m_sWorkingPoseModification to %1 at component index %2", configPath, componentIndex);
	}
	
	void SetWorkingModificationData(DAB_PoseModification newModification, WorldEditorAPI api)
	{
		if(m_sWorkingPoseModification.IsEmpty())
		{
			Print("Tried to set modification data, but config was not set!", LogLevel.WARNING);
			return;
		}
		
		// TODO: We are using api in the game section, is this valid?
		api.BeginEntityAction();
		
		Resource resource = BaseContainerTools.LoadContainer(m_sWorkingPoseModification);
		BaseContainer container = resource.GetResource().ToBaseContainer();
		
		BaseContainerTools.ReadFromInstance(newModification, container);
	    BaseContainerTools.SaveContainer(container, m_sWorkingPoseModification);
	    
		api.EndEntityAction();
	}
	
	
}