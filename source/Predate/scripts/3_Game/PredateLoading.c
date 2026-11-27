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
            frame = CreateArtwork(workspace, original.GetParent());
            if (!frame) return;
            frame.SetSort(-1000);
        }
        original.Show(false);
        frame.Show(true);
        FitArtwork(frame);
    }

    static Widget CreateArtwork(WorkspaceWidget workspace, Widget parent = null)
    {
        if (!workspace) return null;
        Widget frame = workspace.CreateWidgets("Predate/gui/loading_artwork.layout", parent);
        if (!frame) return null;
        ImageWidget image = ImageWidget.Cast(frame.FindAnyWidget("ArcadeImage"));
        if (!image || !image.LoadImageFile(0, "Predate/gui/predate_arcade_loading_v2_co.paa"))
        {
            frame.Unlink();
            Print("[Predate] LOADING ART ERROR: approved texture failed to load.");
            return null;
        }
        image.SetImage(0);
        Print("[Predate] Approved arcade loading artwork initialized.");
        return frame;
    }

    static void FitArtwork(Widget frame)
    {
        float screenWidth, screenHeight;
        frame.GetScreenSize(screenWidth, screenHeight);
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
        Widget frame = CreateBlueFrame(workspace, original.GetParent());
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

    static Widget CreateBlueFrame(WorkspaceWidget workspace, Widget parent)
    {
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
        return workspace.CreateWidgets("Predate/gui/loading_blue.layout", parent);
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
        array<string> hidden = {"ImageLogoMid", "ImageLogoCorner", "ModdedWarning", "notification_root", "HintTestLongText", "hintIcon", "LinesImageLeft", "LinesRightImage"};
        foreach (string name : hidden)
        {
            Widget widget = root.FindAnyWidget(name);
            if (widget) widget.Show(false);
        }
    }
}

class PredateLoadingProgress
{
    static const float INITIAL_FILL = 2;
    float Value = INITIAL_FILL;
    bool WorldPhase;
    bool PlayerReady;
    protected float m_NativeProgress;
    protected float m_PhaseElapsed;
    protected float m_FinishElapsed;
    protected float m_FinishFrom;

    void BeginWorld()
    {
        WorldPhase = true;
        m_PhaseElapsed = 0;
        m_NativeProgress = 0;
    }

    void BeginFinish()
    {
        PlayerReady = true;
        m_FinishFrom = Value;
    }

    void Advance(float timeslice, float nativeFraction)
    {
        float seconds = Math.Clamp(timeslice, 0, 0.1);
        if (PlayerReady)
        {
            m_FinishElapsed += seconds;
            float t = Math.Min(m_FinishElapsed / 0.45, 1);
            Value = Math.Lerp(m_FinishFrom, 100, t * t * (3 - 2 * t));
            return;
        }

        m_PhaseElapsed += seconds;
        m_NativeProgress = Math.Max(m_NativeProgress, Math.Clamp(nativeFraction, 0, 1));
        float phaseStart = INITIAL_FILL;
        float phaseWidth = 46 - INITIAL_FILL;
        if (WorldPhase)
        {
            phaseStart = 46;
            phaseWidth = 51;
        }
        // Time keeps the estimate moving even when native progress is idle.
        float elapsedFraction = m_PhaseElapsed / (m_PhaseElapsed + 6);
        float fraction = Math.Lerp(elapsedFraction, 1, m_NativeProgress * 0.8);
        float target = phaseStart + phaseWidth * fraction;
        float advance = Math.Min((target - Value) * seconds / (0.55 + seconds), 32 * seconds);
        Value += Math.Max(0, advance);
    }
}

modded class LoadingScreen
{
    bool m_PredateJoining;
    bool m_PredateNativeLoading;
    bool m_PredatePreloading;
    Widget m_PredatePreloadCover;
    protected ProgressBarWidget m_PredateVisibleProgress;
    protected ProgressBarWidget m_PredateCoverProgress;
    protected ref PredateLoadingProgress m_PredateProgress;

    void LoadingScreen(DayZGame game)
    {
        string sessionId;
        bool managedJoin = game.CommandlineGetParam("predateSessionId", sessionId);
        m_PredateJoining = managedJoin;
        string preload;
        m_PredatePreloading = managedJoin && game.CommandlineGetParam("predateWaitForServer", preload);
        m_PredateNativeLoading = m_WidgetRoot.IsVisible();
        PredateLoadingStyle.Clean(m_WidgetRoot);
        PredateLoadingStyle.Artwork(m_WidgetRoot, game.GetLoadingWorkspace());
        ProgressBarWidget blue = PredateLoadingStyle.BlueBar(m_WidgetRoot, game.GetLoadingWorkspace());
        if (managedJoin)
        {
            // Keep the native widget as the asynchronous progress source. DayZ
            // can reset it between passes without resetting the displayed fill.
            m_PredateProgress = new PredateLoadingProgress;
            m_PredateVisibleProgress = blue;
            if (blue) blue.SetCurrent(m_PredateProgress.Value);
        }
        else if (blue)
        {
            m_ProgressLoading = blue;
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
        PredateCoverPreload();
        if (!m_PredatePreloading) PredateBeginWorldLoading();
        m_WidgetRoot.Show(true);
        m_TextWidgetTitle.Show(false);
        m_TextWidgetStatus.Show(false);
        m_ProgressText.Show(false);
    }

    void PredateBeginWorldLoading()
    {
        if (!m_PredateJoining || m_PredateProgress.WorldPhase) return;
        m_PredateProgress.BeginWorld();
        Print("[Predate] Continuous loading: world phase; artwork retained.");
    }

    bool PredateMasksFrontend()
    {
        return m_PredateJoining && m_PredatePreloading;
    }

    void PredateCoverPreload()
    {
        if (!m_PredateJoining || m_PredatePreloadCover || (!m_PredatePreloading && !m_PredateProgress.PlayerReady)) return;
        // The ordinary workspace can render between native loading passes.
        // Use a PREDATE-only cover there, without the stock notification layer.
        WorkspaceWidget workspace = m_DayZGame.GetWorkspace();
        m_PredatePreloadCover = PredateLoadingStyle.CreateArtwork(workspace);
        if (!m_PredatePreloadCover) return;
        m_PredatePreloadCover.SetSort(10000);
        PredateLoadingStyle.FitArtwork(m_PredatePreloadCover);
        Widget progress = PredateLoadingStyle.CreateBlueFrame(workspace, m_PredatePreloadCover);
        if (progress && m_PredateVisibleProgress)
        {
            float x, y, width, height;
            Widget nativeProgress = m_PredateVisibleProgress.GetParent();
            nativeProgress.GetScreenPos(x, y);
            nativeProgress.GetScreenSize(width, height);
            if (width > 0 && height > 0)
            {
                progress.SetScreenSize(width, height);
                progress.SetScreenPos(x, y);
            }
        }
        m_PredateCoverProgress = ProgressBarWidget.Cast(m_PredatePreloadCover.FindAnyWidget("PredateLoadingBar"));
        if (m_PredateCoverProgress) m_PredateCoverProgress.SetCurrent(m_PredateProgress.Value);
        m_PredatePreloadCover.Show(true);
        Print("[Predate] Continuous loading: PREDATE-only frontend cover visible.");
    }

    void PredateUpdateLoading(float timeslice)
    {
        if (!m_PredateJoining) return;
        PredateCoverPreload();
        // Keep the completed fill visible for a frame before handing over.
        if (m_PredateProgress.PlayerReady && m_PredateProgress.Value >= 100)
        {
            PredateFinishJoin();
            return;
        }

        float nativeFraction;
        if (m_PredateNativeLoading && m_ProgressLoading)
        {
            float range = m_ProgressLoading.GetMax() - m_ProgressLoading.GetMin();
            if (range > 0)
                nativeFraction = (m_ProgressLoading.GetCurrent() - m_ProgressLoading.GetMin()) / range;
        }
        m_PredateProgress.Advance(timeslice, nativeFraction);
        if (m_PredateVisibleProgress) m_PredateVisibleProgress.SetCurrent(m_PredateProgress.Value);
        if (m_PredateCoverProgress) m_PredateCoverProgress.SetCurrent(m_PredateProgress.Value);
    }

    void PredatePlayerLoaded()
    {
        if (!m_PredateJoining || m_PredateProgress.PlayerReady) return;
        m_PredateProgress.BeginFinish();
        PredateCoverPreload();
        if (!m_PredateVisibleProgress && !m_PredateCoverProgress) PredateFinishJoin();
    }

    void PredateFinishJoin()
    {
        if (!m_PredateJoining) return;
        m_PredateJoining = false;
        if (m_PredatePreloadCover) m_PredatePreloadCover.Unlink();
        m_PredatePreloadCover = null;
        m_PredateCoverProgress = null;
        m_WidgetRoot.Show(false);
        Print("[Predate] Continuous loading: released.");
    }
}

modded class DayZGame
{
    bool m_PredateRestoreJoinInput;
    float m_PredateJoinInputAfter;
    protected bool m_PredateHideRedeployCursor;
    protected bool m_PredateDeathCursorMasked;

    void PredateSuppressRedeployCursor(bool suppress)
    {
        if (m_PredateHideRedeployCursor == suppress) return;
        m_PredateHideRedeployCursor = suppress;
        RefreshMouseCursorVisibility();
    }

    void PredateSuppressDeathCursor(bool suppress)
    {
        if (m_PredateDeathCursorMasked == suppress) return;
        m_PredateDeathCursorMasked = suppress;
        RefreshMouseCursorVisibility();
    }

    bool PredateDeathCursorMasked()
    {
        return m_PredateDeathCursorMasked;
    }

    override bool CanDisplayMouseCursor()
    {
        return !m_PredateHideRedeployCursor && !m_PredateDeathCursorMasked && super.CanDisplayMouseCursor();
    }

    override void OnUpdate(bool doSim, float timeslice)
    {
        super.OnUpdate(doSim, timeslice);
        if (m_loading) m_loading.PredateUpdateLoading(timeslice);
        if (m_PredateRestoreJoinInput) PredateRestoreJoinInput();
    }

    override void OnActivateMessage()
    {
        super.OnActivateMessage();
        if (m_PredateRestoreJoinInput) m_PredateJoinInputAfter = GetTickTime() + 0.2;
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        super.OnEvent(eventTypeId, params);
        if (eventTypeId == MPSessionPlayerReadyEventTypeID)
        {
            m_PredateRestoreJoinInput = m_PredateClientAgent || (m_loading && m_loading.m_PredateJoining);
            // DayZ schedules its device check 100 ms after player-ready.
            if (m_PredateRestoreJoinInput) m_PredateJoinInputAfter = GetTickTime() + 0.2;
            if (m_loading) m_loading.PredatePlayerLoaded();
        }
        else if (eventTypeId == MPSessionFailEventTypeID || eventTypeId == MPSessionEndEventTypeID)
        {
            m_PredateRestoreJoinInput = false;
            if (m_loading) m_loading.PredateFinishJoin();
        }
    }

    void PredateRestoreJoinInput()
    {
        if (m_loading && m_loading.m_PredateJoining) return;
        if (GetTickTime() < m_PredateJoinInputAfter || GetGameState() != DayZGameState.IN_GAME || !GetPlayer() || IsLoading() || !IsAppActive()) return;
        UIManager ui = GetUIManager();
        if (!ui || ui.GetMenu() || ui.IsDialogVisible()) return;
        Mission mission = GetMission();
        if (!mission || !mission.IsMissionGameplay() || mission.IsPaused() || mission.IsPlayerRespawning() || !GetPlayer().IsAlive() || GetPlayer().IsUnconscious()) return;

        Input input = GetInput();
        bool alreadyFocused = input.HasGameFocus();
        // A dismissed join dialog can still own widget focus or leave input
        // exclusions behind. Release both once the live mission owns the screen.
        SetFocus(null);
        if (mission.IsControlDisabled()) mission.EnableAllInputs(true);
        input.UpdateConnectedInputDeviceList();
        input.ResetGameFocus();
        GetUApi().UpdateControls();
        ui.ShowUICursor(false);
        m_PredateRestoreJoinInput = false;
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
