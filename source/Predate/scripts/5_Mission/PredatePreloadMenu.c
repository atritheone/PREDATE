modded class MissionMainMenu
{
    override void OnInit()
    {
        LoadingScreen loading = GetDayZGame().m_loading;
        if (loading && loading.PredateMasksFrontend())
        {
            m_NoCutscene = true;
            loading.PredateCoverPreload();
        }
        super.OnInit();
    }

    override void OnMissionStart()
    {
        super.OnMissionStart();
        LoadingScreen loading = GetDayZGame().m_loading;
        if (loading && loading.PredateMasksFrontend()) GetGame().GetUIManager().ShowUICursor(false);
    }
}

modded class MainMenu
{
    bool m_PredateMenuMasked;

    override Widget Init()
    {
        Widget root = super.Init();
        if (IsCLIParam("predateSessionId"))
        {
            m_Play.Show(false);
            m_ChooseServer.Show(false);
        }
        PredateMaskMenu();
        return root;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if ((w == m_Play || w == m_ChooseServer) && IsCLIParam("predateSessionId")) return true;
        return super.OnClick(w, x, y, button);
    }

    override void OpenMenuServerBrowser()
    {
        if (!IsCLIParam("predateSessionId")) super.OpenMenuServerBrowser();
    }

    override void OnShow()
    {
        super.OnShow();
        PredateMaskMenu();
    }

    override void Update(float timeslice)
    {
        if (PredateMaskMenu()) return;
        super.Update(timeslice);
    }

    bool PredateMaskMenu()
    {
        if (!layoutRoot) return false;
        LoadingScreen loading = GetDayZGame().m_loading;
        bool masked = loading && loading.PredateMasksFrontend();
        if (!masked && !m_PredateMenuMasked) return false;
        layoutRoot.Show(!masked);
        if (!m_PredateMenuMasked && masked) Print("[Predate] Continuous loading: stock frontend suppressed.");
        m_PredateMenuMasked = masked;
        return masked;
    }
}
