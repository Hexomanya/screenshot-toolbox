class DAB_ConfirmDialog
{
	void DAB_ConfirmDialog();
	void ~DAB_ConfirmDialog();
	
    [ButtonAttribute("OK")]
    bool OK()
    {
        return true;
    }

    [ButtonAttribute("Cancel")]
    bool Cancel()
    {
        return false;
    }
}