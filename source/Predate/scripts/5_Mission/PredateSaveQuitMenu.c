modded class InGameMenu
{
    bool m_PredateDeploymentScreen;
    bool m_PredateRedeployPending;
    bool m_PredateDeploymentTest;
    Widget m_PredatePauseBrand;
    ImageWidget m_PredatePauseWordmark;
    TextWidget m_PredatePauseSlayer;
    bool m_PredatePauseWordmarkLoaded;
    float m_PredatePauseBrandAspect;
    int m_PredatePauseSlayerSize;

    static bool PredateNeedsRedeploy(PlayerBase player)
    {
        return player && (!player.IsAlive() || (player.m_PredateState && player.m_PredateState.NeedsRedeploy));
    }

    override Widget Init()
    {
        Widget root = super.Init();
        m_PredatePauseBrand = GetGame().GetWorkspace().CreateWidgets("Predate/gui/pause_brand.layout", root);
        if (m_PredatePauseBrand)
        {
            m_PredatePauseWordmark = ImageWidget.Cast(m_PredatePauseBrand.FindAnyWidget("PredatePauseWordmark"));
            m_PredatePauseSlayer = TextWidget.Cast(m_PredatePauseBrand.FindAnyWidget("PredatePauseSlayer"));
            if (m_PredatePauseWordmark && LoadWidgetImageSet("Predate/gui/pause_brand.imageset"))
                m_PredatePauseWordmarkLoaded = m_PredatePauseWordmark.LoadImageFile(0, "set:PredatePause image:Wordmark") && m_PredatePauseWordmark.SetImage(0);
            UpdatePredatePauseBrand(!PredateNeedsRedeploy(PlayerBase.Cast(GetGame().GetPlayer())));
        }
        string validation;
        m_PredateDeploymentTest = GetGame().CommandlineGetParam("predateDeploymentValidation", validation) && FileExist("$profile:PredateDeploymentValidation.txt");
        ButtonSetText(m_OptionsButton, "Options");
        // Release the timed hint controller as well as hiding its parent.
        m_HintPanel = null;
        array<string> decorations = {"dayz_logo", "Licensing", "version", "copyright", "ModdedWarning", "hint_frame", "feedbackbtn", "server_info"};
        int hidden;
        foreach (string name : decorations)
        {
            Widget decoration = root.FindAnyWidget(name);
            if (!decoration) continue;
            decoration.Show(false);
            decoration.Enable(false);
            if (!decoration.IsVisible()) hidden++;
        }
        Print("[Predate] Pause decorations hidden: " + hidden + "/" + decorations.Count());
        string session;
        if (GetGame().CommandlineGetParam("predateSessionId", session))
        {
            ButtonSetText(m_ExitButton, "Save and Quit");
            ButtonSetText(m_ContinueButton, "Resume");
        }
        return root;
    }

    override protected void OnClick_Continue()
    {
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (agent && (agent.m_UserPaused || agent.m_ServerFrozen || agent.m_ResumePending))
        {
            agent.RequestResume();
            return;
        }
        super.OnClick_Continue();
    }

    override protected void UpdateGUI()
    {
        super.UpdateGUI();
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        UpdatePredatePauseBrand(!PredateNeedsRedeploy(player));
        PredateState receiver;
        if (player) receiver = player.m_PredateState;
        if (PredateNeedsRedeploy(player))
        {
            if (receiver && receiver.DeploymentSpawnRejected) m_PredateRedeployPending = false;
            m_PredateDeploymentScreen = true;
            m_ContinueButton.Show(false);
            m_RestartButton.Show(false);
            m_RespawnButton.Show(true);
            m_RespawnButton.Enable(!m_PredateRedeployPending);
            if (m_PredateRedeployPending) ButtonSetText(m_RespawnButton, "Deploying...");
            else ButtonSetText(m_RespawnButton, "Redeploy");
            if (m_PredateDeploymentTest && !m_PredateRedeployPending && receiver && receiver.NeedsRedeploy && FileExist("$profile:PredateRedeployClick.txt"))
            {
                DeleteFile("$profile:PredateRedeployClick.txt");
                Print("[PredateDeploymentTest] Redeploy clicked; native body needed=" + !player.IsAlive());
                OnClick(m_RespawnButton, 0, 0, 0);
            }
            return;
        }
        if (m_PredateDeploymentScreen && player && player.m_PredateState)
        {
            GetGame().GetMission().Continue();
            return;
        }
        if (!agent || (!agent.m_UserPaused && !agent.m_ResumePending)) return;
        m_ContinueButton.Show(true);
        m_ContinueButton.Enable(!agent.m_ResumePending);
        if (agent.m_ResumePending) ButtonSetText(m_ContinueButton, "Resuming...");
        else ButtonSetText(m_ContinueButton, "Resume");
        m_RestartButton.Show(false);
        m_RespawnButton.Show(false);
        m_SeparatorPanel.Show(false);
    }

    void UpdatePredatePauseBrand(bool visible)
    {
        if (!m_PredatePauseBrand) return;
        m_PredatePauseBrand.Show(visible);
        float width, height;
        m_PredatePauseBrand.GetScreenSize(width, height);
        if (!visible || width <= 0 || height <= 0) return;
        float aspect = width / height;
        if (m_PredatePauseWordmark && aspect != m_PredatePauseBrandAspect)
        {
            m_PredatePauseBrandAspect = aspect;
            m_PredatePauseWordmark.SetSize(0.22, 0.22 * aspect * 400.0 / 1965.0);
        }
        int size = Math.Round(Math.Clamp(height / 42, 14, 44));
        if (m_PredatePauseSlayer && size != m_PredatePauseSlayerSize)
        {
            m_PredatePauseSlayerSize = size;
            m_PredatePauseSlayer.SetTextExactSize(size);
        }
    }

    override protected void OnClick_Respawn()
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (m_PredateRedeployPending || !PredateNeedsRedeploy(player)) return;
        m_PredateRedeployPending = true;
        if (player.m_PredateState) player.m_PredateState.DeploymentSpawnRejected = false;
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (agent)
        {
            agent.m_DeployAfterRespawn = true;
            agent.m_DeployRetryIn = 1;
        }
        if (player.IsAlive()) player.PredateRequest(5);
        else GameRespawn(true);
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (key == KeyCode.KC_ESCAPE && m_PredateDeploymentScreen) return true;
        if (key == KeyCode.KC_ESCAPE && GetGame().GetUIManager().GetMenu() == this && agent && (agent.m_UserPaused || agent.m_ResumePending))
        {
            agent.RequestResume();
            return true;
        }
        return super.OnKeyDown(w, x, y, key);
    }

    override protected void OnClick_Exit()
    {
        if (GetDayZGame().m_PredateClientAgent)
        {
            GetDayZGame().m_PredateClientAgent.RequestQuit();
            ButtonSetText(m_ExitButton, "Saving...");
            return;
        }
        super.OnClick_Exit();
    }
}

modded class MissionGameplay
{
    override void Pause()
    {
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        // A launch that finishes in the background should still join gameplay.
        // Later Escape presses use the ordinary pause path.
        if (agent && m_PauseQueued && GetTimeStamp() == 0 && !GetGame().IsAppActive() && !IsCLIParam("launchPaused"))
        {
            m_PauseQueued = false;
            return;
        }
        super.Pause();
        if (InGameMenu.PredateNeedsRedeploy(PlayerBase.Cast(GetGame().GetPlayer()))) return;
        if (agent && IsPaused() && GetGame().GetGameState() == DayZGameState.IN_GAME)
            agent.RequestPause();
    }

    override void Continue()
    {
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        UIScriptedMenu current = GetGame().GetUIManager().GetMenu();
        if (current && current.GetID() == MENU_INGAME && !IsPlayerRespawning() && InGameMenu.PredateNeedsRedeploy(PlayerBase.Cast(GetGame().GetPlayer()))) return;
        if (current && current.GetID() == MENU_INGAME && agent && !agent.m_ResumeAuthorized && !agent.m_PauseDisconnected && (agent.m_UserPaused || agent.m_ResumePending))
        {
            if (GetUApi().GetInputByID(UAUIMenu).LocalPress() || GetUApi().GetInputByID(UAUIBack).LocalPress())
            {
                agent.RequestResume();
            }
            return;
        }
        super.Continue();
    }
}

modded class MissionMainMenu
{
    override void OnUpdate(float timeslice)
    {
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        UIScriptedMenu menu = GetGame().GetUIManager().GetMenu();
        if (agent && agent.m_UserPaused && menu && menu.GetID() == MENU_INGAME)
        {
            if (GetUApi().GetInputByID(UAUIMenu).LocalPress() || GetUApi().GetInputByID(UAUIBack).LocalPress())
                agent.RequestResume();
        }
        super.OnUpdate(timeslice);
    }
}
