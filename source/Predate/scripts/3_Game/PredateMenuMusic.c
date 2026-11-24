modded class DynamicMusicPlayer
{
    override protected void DetermineTrackByCategory(EDynamicMusicPlayerCategory category)
    {
        if (category == EDynamicMusicPlayerCategory.MENU) return;
        super.DetermineTrackByCategory(category);
    }
}
