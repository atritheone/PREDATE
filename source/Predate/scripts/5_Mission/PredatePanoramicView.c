#ifndef SERVER
modded class MissionGameplay
{
    protected ImageWidget m_PredatePanoramicMask;
    protected Widget m_PredatePanoramicRoot;
    protected bool m_PredateMaskLoaded;
    protected int m_PredateMaskImage = -1;

    bool PredatePanoramicMaskShown()
    {
        return m_PredateMaskLoaded && m_PredatePanoramicRoot && m_PredatePanoramicRoot.IsVisible();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        bool show;
        if (PredatePanoramicNVG.Active(player) && !GetGame().GetUIManager().GetMenu() && !GetDayZGame().IsLoading())
        {
            DayZPlayerCamera camera = player.GetCurrentPlayerCamera();
            if (camera)
                show = camera.GetCameraName() == "DayZPlayerCamera1stPerson" || camera.GetCameraName() == "DayZPlayerCameraIronsights";
        }
        if (!m_PredatePanoramicRoot && show)
        {
            m_PredatePanoramicRoot = GetGame().GetWorkspace().CreateWidgets("Predate/gui/panoramic_nvg.layout");
            if (m_PredatePanoramicRoot)
                m_PredatePanoramicMask = ImageWidget.Cast(m_PredatePanoramicRoot.FindAnyWidget("PredatePanoramicMask"));
            if (m_PredatePanoramicMask)
            {
                bool standard = m_PredatePanoramicMask.LoadImageFile(0, "Predate/gui/panoramic_nvg_ca.paa");
                bool wide = m_PredatePanoramicMask.LoadImageFile(1, "Predate/gui/panoramic_nvg_wide_ca.paa");
                m_PredateMaskLoaded = standard && wide;
            }
        }
        if (m_PredatePanoramicMask)
        {
            if (show)
            {
                float width, height;
                m_PredatePanoramicMask.GetScreenSize(width, height);
                int image = 0;
                if (width > height * 2.03) image = 1;
                if (image != m_PredateMaskImage)
                {
                    m_PredatePanoramicMask.SetImage(image);
                    m_PredateMaskImage = image;
                }
            }
        }
        if (m_PredatePanoramicRoot) m_PredatePanoramicRoot.Show(show && m_PredateMaskLoaded);
    }

    override void OnMissionFinish()
    {
        if (m_PredatePanoramicRoot) m_PredatePanoramicRoot.Unlink();
        m_PredatePanoramicMask = null;
        m_PredatePanoramicRoot = null;
        super.OnMissionFinish();
    }
}
#endif
