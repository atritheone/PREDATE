class PredateFootprintPresentationTest
{
    int Checks;
    int Failures;
    int m_Phase;
    int m_ReportAt;
    bool Projected;
    bool NightVisionProjected;
    ref array<ref PredateHuntNotice> m_TrackNotices = {};

    void Check(bool passed)
    {
        Checks++;
        if (!passed) Failures++;
    }

    bool Update(PlayerBase player, PredateState state, PredateFootprintView foreign, PredateFootprintView own, TextWidget label, PredateHuntFeed feed)
    {
        if (!player || !state || !foreign.m_Image || !own.m_Image || !label || !feed) return true;
        foreach (PredateHuntNotice notice : feed.m_Notices)
            if (notice.Text.Contains("Tracks") && m_TrackNotices.Find(notice) < 0) m_TrackNotices.Insert(notice);
        if (m_Phase == 0 && state.FootprintClue == 0)
        {
            Check(!label.IsVisible() && !foreign.m_Image.IsVisible());
            m_Phase = 1;
        }
        else if (m_Phase == 1 && foreign.m_Image.IsVisible() && own.m_Image.IsVisible())
        {
            float width, height;
            foreign.m_Image.GetScreenSize(width, height);
            Projected = width >= 1 && height >= 1 && foreign.m_Loaded && own.m_Loaded;
            Check(Projected && !PPERequesterBank.IsRequesterRunning(PPERequesterBank.REQ_CAMERANV));
            Check(label.IsVisible());
            MakeScreenshot("$profile:PredateTracks.dds");
            m_Phase = 2;
        }
        else if (m_Phase == 2 && !foreign.m_Image.IsVisible() && m_TrackNotices.Count() > 0)
            m_Phase = 3;
        else if (m_Phase == 3 && foreign.m_Image.IsVisible())
        {
            Check(m_TrackNotices[0].Text.Contains("Headed"));
            m_Phase = 4;
            m_ReportAt = GetGame().GetTime() + 1800;
        }
        else if (m_Phase == 4 && GetGame().GetTime() >= m_ReportAt)
        {
            Check(m_TrackNotices.Count() == 1);
            player.AddActiveNV(NVTypes.NV_GOGGLES);
            m_Phase = 5;
            m_ReportAt = GetGame().GetTime() + 900;
        }
        else if (m_Phase == 5 && GetGame().GetTime() >= m_ReportAt)
        {
            NightVisionProjected = foreign.m_Image.IsVisible() && PPERequesterBank.IsRequesterRunning(PPERequesterBank.REQ_CAMERANV);
            Check(NightVisionProjected);
            MakeScreenshot("$profile:PredateTracks_NV.dds");
            player.RemoveActiveNV(NVTypes.NV_GOGGLES);
            string error;
            JsonFileLoader<PredateFootprintPresentationTest>.SaveFile("$profile:FootprintPresentation.json", this, error);
            return false;
        }
        return true;
    }

}
