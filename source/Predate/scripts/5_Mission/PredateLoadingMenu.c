modded class LoadingMenu
{
    override Widget Init()
    {
        Widget root = super.Init();
        PredateLoadingStyle.Clean(root);
        PredateLoadingStyle.Artwork(root, GetGame().GetWorkspace());
        ProgressBarWidget blue = PredateLoadingStyle.BlueBar(root, GetGame().GetWorkspace());
        if (blue) m_progressBar = blue;
        return root;
    }
}

modded class InGameMenu
{
    override Widget Init()
    {
        Widget root = super.Init();
        Widget warning = root.FindAnyWidget("ModdedWarning");
        if (warning) warning.Show(false);
        return root;
    }
}
