class PredateLoadingStyle
{
    static bool s_BlueStyleLoaded;

    static void Artwork(Widget root, WorkspaceWidget workspace)
    {
        if (!root || !workspace) return;
        Widget original = root.FindAnyWidget("ImageBackground");
        if (!original) original = root.FindAnyWidget("Background");
        if (!original) return;
        Widget frame = root.FindAnyWidget("PredateArtwork");
        if (!frame)
        {
            frame = workspace.CreateWidgets("Predate/gui/loading_artwork.layout", original.GetParent());
            if (!frame) return;
            frame.SetSort(-1000);
            ImageWidget image = ImageWidget.Cast(frame.FindAnyWidget("ArcadeImage"));
            if (!image || !image.LoadImageFile(0, "Predate/gui/predate_arcade_loading_v2_co.paa"))
            {
                Print("[Predate] LOADING ART ERROR: approved texture failed to load.");
                return;
            }
            image.SetImage(0);
            Print("[Predate] Approved arcade loading artwork initialized.");
        }
        original.Show(false);
        frame.Show(true);
        float screenWidth, screenHeight;
        frame.GetParent().GetScreenSize(screenWidth, screenHeight);
        if (screenWidth <= 0 || screenHeight <= 0) return;
        float screenAspect = screenWidth / screenHeight;
        float artAspect = 1672.0 / 941.0;
        float width = 1;
        float height = 1;
        if (screenAspect > artAspect) width = artAspect / screenAspect;
        else height = screenAspect / artAspect;
        Widget artwork = frame.FindAnyWidget("ArcadeImage");
        artwork.SetPos((1 - width) * 0.5, (1 - height) * 0.5);
        artwork.SetSize(width, height);
    }

    static ProgressBarWidget BlueBar(Widget root, WorkspaceWidget workspace)
    {
        if (!root || !workspace) return null;
        Widget original = root.FindAnyWidget("LoadingBar");
        if (!original) original = root.FindAnyWidget("ProgressBarWidget");
        if (!original) return null;
        if (!s_BlueStyleLoaded)
        {
            if (!LoadWidgetImageSet("Predate/gui/loading.imageset"))
            {
                Print("[Predate] LOADING BAR ERROR: blue image set failed to load.");
                return null;
            }
            LoadWidgetStyles("Predate/gui/loading.styles");
            s_BlueStyleLoaded = true;
        }
        Widget frame = workspace.CreateWidgets("Predate/gui/loading_blue.layout", original.GetParent());
        if (!frame) return null;
        ProgressBarWidget blue = ProgressBarWidget.Cast(frame.FindAnyWidget("PredateLoadingBar"));
        if (!blue) return null;
        float x, y, width, height;
        original.GetPos(x, y);
        original.GetSize(width, height);
        frame.SetPos(x, y);
        frame.SetSize(width, height);
        original.Show(false);
        Print("[Predate] Blue loading bar initialized: " + blue.GetStyleName());
        return blue;
    }

    static void Clean(Widget root)
    {
        if (!root) return;
        // The native bottom panel also parents the loading bar. Clear its fill
        // rather than hiding the parent, which would hide the bar as well.
        Widget bottom = root.FindAnyWidget("BottomPanel");
        if (bottom && bottom.GetColor() != 0)
        {
            bottom.SetColor(ARGB(0, 0, 0, 0));
            Print("[Predate] Bottom loading-panel background removed.");
        }
        array<string> hidden = {"ImageLogoMid", "ImageLogoCorner", "ModdedWarning", "HintTestLongText", "hintIcon", "LinesImageLeft", "LinesRightImage"};
        foreach (string name : hidden)
        {
            Widget widget = root.FindAnyWidget(name);
            if (widget) widget.Show(false);
        }
    }
}

modded class LoadingScreen
{
    bool m_PredateJoining;
    bool m_PredateNativeLoading;
    bool m_PredateWorldPhase;

    void LoadingScreen(DayZGame game)
    {
        string sessionId;
        bool managedJoin = game.CommandlineGetParam("predateSessionId", sessionId);
        m_PredateJoining = managedJoin;
        m_PredateNativeLoading = m_WidgetRoot.IsVisible();
        PredateLoadingStyle.Clean(m_WidgetRoot);
        PredateLoadingStyle.Artwork(m_WidgetRoot, game.GetLoadingWorkspace());
        ProgressBarWidget blue = PredateLoadingStyle.BlueBar(m_WidgetRoot, game.GetLoadingWorkspace());
        if (blue)
        {
            m_ProgressLoading = blue;
            if (m_PredateJoining) blue.SetSize(0.5, 1);
            ProgressAsync.SetProgressData(m_ProgressLoading);
        }
    }

    override void Show()
    {
        super.Show();
        m_PredateNativeLoading = true;
        PredateLoadingStyle.Clean(m_WidgetRoot);
        PredateLoadingStyle.Artwork(m_WidgetRoot, m_DayZGame.GetLoadingWorkspace());
    }

    // Preserve native loading state while artwork covers the join handoff.
    override bool IsLoading()
    {
        return m_PredateNativeLoading;
    }

    override void EndLoading()
    {
        super.EndLoading();
        m_PredateNativeLoading = false;
        PredateHoldJoinScreen();
    }

    override void Hide(bool force)
    {
        super.Hide(force);
        if (force || m_Counter <= 0) m_PredateNativeLoading = false;
        PredateHoldJoinScreen();
    }

    void PredateHoldJoinScreen()
    {
        if (!m_PredateJoining) return;
        if (!m_PredateWorldPhase)
        {
            // Both native stages share one bar; world loading owns its latter half.
            m_PredateWorldPhase = true;
            Widget completed = m_WidgetRoot.FindAnyWidget("PredateLoadedPhase");
            if (completed) completed.Show(true);
            m_ProgressLoading.SetPos(0.5, 0);
            m_ProgressLoading.SetSize(0.5, 1);
            m_ProgressLoading.SetCurrent(0);
            Print("[Predate] Continuous loading: world phase; artwork retained.");
        }
        m_WidgetRoot.Show(true);
        m_TextWidgetTitle.Show(false);
        m_TextWidgetStatus.Show(false);
        m_ProgressText.Show(false);
    }

    void PredateFinishJoin()
    {
        if (!m_PredateJoining) return;
        m_PredateJoining = false;
        m_WidgetRoot.Show(false);
        Print("[Predate] Continuous loading: released.");
    }
}

modded class DayZGame
{
    bool m_PredateRestoreJoinInput;
    float m_PredateJoinInputAfter;

    override void OnUpdate(bool doSim, float timeslice)
    {
        super.OnUpdate(doSim, timeslice);
        if (m_PredateRestoreJoinInput) PredateRestoreJoinInput();
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        if (eventTypeId == MPSessionPlayerReadyEventTypeID || eventTypeId == MPSessionFailEventTypeID || eventTypeId == MPSessionEndEventTypeID)
        {
            m_PredateRestoreJoinInput = eventTypeId == MPSessionPlayerReadyEventTypeID && m_loading && m_loading.m_PredateJoining;
            // DayZ schedules its device check 100 ms after player-ready.
            if (m_PredateRestoreJoinInput) m_PredateJoinInputAfter = GetTickTime() + 0.2;
            if (m_loading) m_loading.PredateFinishJoin();
        }
        super.OnEvent(eventTypeId, params);
    }

    void PredateRestoreJoinInput()
    {
        if (GetTickTime() < m_PredateJoinInputAfter || GetGameState() != DayZGameState.IN_GAME || !GetPlayer() || IsLoading() || !IsAppActive()) return;
        UIManager ui = GetUIManager();
        if (!ui || ui.GetMenu() || ui.IsDialogVisible()) return;
        Mission mission = GetMission();
        if (!mission || mission.IsPaused() || mission.IsControlDisabled()) return;

        m_PredateRestoreJoinInput = false;
        Input input = GetInput();
        bool alreadyFocused = input.HasGameFocus();
        input.UpdateConnectedInputDeviceList();
        input.ResetGameFocus();
        GetUApi().UpdateControls();
        ui.ShowUICursor(false);
        Print("[Predate] Startup input ready: prior focus=" + alreadyFocused + "; game focus=" + input.HasGameFocus());
    }
}

modded class UiHintPanelLoading
{
    override void Init(DayZGame game)
    {
        super.Init(game);
        // Hide the parent so timed hint updates cannot reveal its children.
        if (m_ParentWidget) m_ParentWidget.Show(false);
        if (m_RootFrame) m_RootFrame.Show(false);
    }
}

modded class LoginTimeBase
{
    override Widget Init()
    {
        Widget root = super.Init();
        PredateLoadingStyle.Clean(root);
        PredateLoadingStyle.Artwork(root, GetGame().GetWorkspace());
        array<string> countdown = {"txtDescription", "txtLabel", "btnLeave", "hint_frame0"};
        foreach (string name : countdown)
        {
            Widget widget = root.FindAnyWidget(name);
            if (widget) widget.Show(false);
        }
        return root;
    }
}

modded class LoginQueueBase
{
    override Widget Init()
    {
        Widget root = super.Init();
        PredateLoadingStyle.Clean(root);
        PredateLoadingStyle.Artwork(root, GetGame().GetWorkspace());
        return root;
    }
}
