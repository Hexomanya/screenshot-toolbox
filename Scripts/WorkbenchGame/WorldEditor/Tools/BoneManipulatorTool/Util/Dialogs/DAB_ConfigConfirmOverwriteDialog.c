class DAB_ConfigConfirmOverwriteDialog
{
	void DAB_ConfigConfirmOverwriteDialog();
	void ~DAB_ConfigConfirmOverwriteDialog();
	
	protected int m_iSelectedChoice;
	
	int GetSelectedChoise(){ return m_iSelectedChoice; }
	
    [ButtonAttribute("Overwrite")]
    bool Overwrite()
    {
		m_iSelectedChoice = 1;
        return true;
    }
	
	[ButtonAttribute("Move to List")]
    bool MoveToList()
    {
		m_iSelectedChoice = 2;
        return true;
    }
	
    [ButtonAttribute("Cancel")]
    bool Cancel()
    {
		m_iSelectedChoice = 0;
        return false;
    }
}