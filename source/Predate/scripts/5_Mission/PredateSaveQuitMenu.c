class PredateDeathPresentation
{
    static void FitTitle(Widget card)
    {
        Widget title = card.FindAnyWidget("DeathTitle");
        if (!title) return;

        float width, height;
        card.GetScreenSize(width, height);
        float scale = Math.Round(Math.Clamp(height * 0.067, 38, 132)) / 72.0;
        // The 2x texture includes padding around the 187 x 50 px title at 1080p.
        float titleWidth = Math.Round(256 * scale);
        float titleHeight = Math.Round(128 * scale);
        float x, y;
        card.GetScreenPos(x, y);
        title.SetScreenSize(titleWidth, titleHeight);
        title.SetScreenPos(x + Math.Round((width - titleWidth) / 2), y + Math.Round((height - titleHeight) / 2 + 3 * scale));
    }
}

modded class GesturesMenu
{
    override protected void GetGestureItems(out array<ref GestureMenuItem> gesture_items, GestureCategories category)
    {
        super.GetGestureItems(gesture_items, category);
        if (!IsCLIParam("predateSessionId")) return;
        for (int index = 0; index < gesture_items.Count(); index++)
        {
            if (gesture_items[index].GetID() != EmoteConstants.ID_EMOTE_SUICIDE) continue;
            gesture_items.RemoveOrdered(index);
            return;
        }
    }
}

modded class InGameMenu
{
    static const int PREDATE_SUICIDE_DIALOG = 918607;
    static const int PREDATE_SUICIDE_WAIT_MS = 5000;
    bool m_PredateDeploymentScreen;
    bool m_PredateRedeployPending;
    bool m_PredateSuicidePending;
    PlayerBase m_PredateSuicideQueuedFor;
    int m_PredateSuicideWaitUntil;
    bool m_PredateDeploymentTest;
    Widget m_PredateDeathOptions;
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
            ButtonSetText(m_RestartButton, "Suicide");
            m_RestartButton.Show(PredateCanSuicide(PlayerBase.Cast(GetGame().GetPlayer()), GetDayZGame().m_PredateClientAgent));
        }
        return root;
    }

    override void OnHide()
    {
        m_PredateSuicideQueuedFor = null;
        super.OnHide();
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
        if (agent) PredateUpdateSuicide(player, agent);
        UpdatePredatePauseBrand(!PredateNeedsRedeploy(player));
        PredateState receiver;
        if (player) receiver = player.m_PredateState;
        if (PredateNeedsRedeploy(player))
        {
            bool choicesWereVisible = PredateDeathOptionsVisible();
            PredateShowDeathOptions();
            if (receiver && receiver.DeploymentSpawnRejected) m_PredateRedeployPending = false;
            m_PredateDeploymentScreen = true;
            m_ContinueButton.Show(false);
            m_RestartButton.Show(false);
            m_RespawnButton.Show(true);
            m_RespawnButton.Enable(!m_PredateRedeployPending);
            if (m_PredateRedeployPending) ButtonSetText(m_RespawnButton, "Deploying...");
            else ButtonSetText(m_RespawnButton, "Redeploy");
            if (m_PredateDeploymentTest && choicesWereVisible && !m_PredateRedeployPending && receiver && receiver.NeedsRedeploy && FileExist("$profile:PredateRedeployClick.txt"))
            {
                DeleteFile("$profile:PredateRedeployClick.txt");
                Print("[PredateDeploymentTest] Redeploy clicked; native body needed=" + !player.IsAlive());
                OnClick(m_RespawnButton, 0, 0, 0);
            }
            return;
        }
        if (m_PredateDeploymentScreen && player && player.m_PredateState)
        {
            MissionGameplay gameplay = MissionGameplay.Cast(GetGame().GetMission());
            if (gameplay && !gameplay.m_PredateRedeployCovered) gameplay.Continue();
            return;
        }
        if (!agent || (!agent.m_UserPaused && !agent.m_ResumePending)) return;
        m_ContinueButton.Show(true);
        m_ContinueButton.Enable(!agent.m_ResumePending);
        if (agent.m_ResumePending) ButtonSetText(m_ContinueButton, "Resuming...");
        else ButtonSetText(m_ContinueButton, "Resume");
        m_RespawnButton.Show(false);
        m_SeparatorPanel.Show(false);
    }

    void PredateShowDeathOptions()
    {
        if (m_PredateDeathOptions) return;
        Widget card = GetGame().GetWorkspace().CreateWidgets("Predate/gui/death.layout", layoutRoot);
        if (!card) return;
        m_PredateDeathOptions = card.FindAnyWidget("DeathChoices");
        Widget deploy = card.FindAnyWidget("DeathRedeploy");
        Widget quit = card.FindAnyWidget("DeathSaveQuit");
        if (!m_PredateDeathOptions || !deploy || !quit)
        {
            m_PredateDeathOptions = null;
            card.Unlink();
            return;
        }
        Widget nativeOptions = layoutRoot.FindAnyWidget("play_panel_root");
        if (nativeOptions) nativeOptions.Show(false);
        m_RespawnButton = deploy;
        m_ExitButton = quit;
        PredateDeathPresentation.FitTitle(card);
        card.Show(true);
        m_PredateDeathOptions.Show(true);
        SetFocus(m_RespawnButton);
    }

    bool PredateDeathOptionsVisible()
    {
        return m_PredateDeathOptions && m_PredateDeathOptions.IsVisible();
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
        MissionGameplay gameplay = MissionGameplay.Cast(GetGame().GetMission());
        if (gameplay) gameplay.PredateBeginRedeployCover(player);
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

    bool PredateCanSuicide(PlayerBase player, PredateClientAgent agent)
    {
        if (!agent || agent.m_ResumePending || agent.m_QuitRequested || agent.m_PauseDisconnected) return false;
        if (m_PredateSuicidePending || !player || !player.IsAlive() || !player.IsPlayerLoaded()) return false;
        return player.m_PredateState && !player.m_PredateState.NeedsRedeploy;
    }

    void PredateUpdateSuicide(PlayerBase player, PredateClientAgent agent)
    {
        bool available = PredateCanSuicide(player, agent);
        if (m_PredateSuicideQueuedFor)
        {
            if (!available || !agent.m_UserPaused || player != m_PredateSuicideQueuedFor || GetGame().GetUIManager().GetMenu() != this || GetGame().GetTime() >= m_PredateSuicideWaitUntil)
                m_PredateSuicideQueuedFor = null;
            else if (agent.m_ServerFrozen)
            {
                m_PredateSuicideQueuedFor = null;
                m_PredateSuicidePending = true;
                player.PredateRequest(PredateProtocol.REQUEST_SUICIDE);
                available = false;
            }
        }
        m_RestartButton.Show(available);
        m_RestartButton.Enable(!m_PredateSuicideQueuedFor);
    }

    override protected void OnClick_Restart()
    {
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (!agent)
        {
            super.OnClick_Restart();
            return;
        }
        if (!agent.m_UserPaused || m_PredateSuicideQueuedFor || !PredateCanSuicide(PlayerBase.Cast(GetGame().GetPlayer()), agent)) return;
        GetGame().GetUIManager().ShowDialog("Suicide", "End this life? You can redeploy in the same hunt.",
            PREDATE_SUICIDE_DIALOG, DBT_YESNO, DBB_NO, DMT_QUESTION, this);
    }

    override bool OnModalResult(Widget w, int x, int y, int code, int result)
    {
        if (code != PREDATE_SUICIDE_DIALOG) return super.OnModalResult(w, x, y, code, result);
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (result == DBB_YES && agent && agent.m_UserPaused && !m_PredateSuicideQueuedFor && PredateCanSuicide(player, agent))
        {
            m_PredateSuicideQueuedFor = player;
            m_PredateSuicideWaitUntil = GetGame().GetTime() + PREDATE_SUICIDE_WAIT_MS;
        }
        return true;
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
    static const int PREDATE_DEATH_CARD_MS = 2000;
    Widget m_PredateDeathCard;
    int m_PredateDeathMenuAt;
    bool m_PredateDeathValidation;
    bool m_PredateRedeployCovered;
    PlayerBase m_PredateRedeployCorpse;
    bool m_PredateRedeployCursorLeak;

    override void OnEvent(EventType eventTypeId, Param params)
    {
        super.OnEvent(eventTypeId, params);
        if (eventTypeId == WindowsResizeEventTypeID && m_PredateDeathCard)
            PredateDeathPresentation.FitTitle(m_PredateDeathCard);
    }

    void PredateBeginDeathCard(DayZPlayerImplement player)
    {
        if (!m_PredateDeathCard || m_PredateRedeployCovered || m_PredateDeathMenuAt > 0 || player != GetGame().GetPlayer() || IsPlayerRespawning() || GetDayZGame().IsLoading()) return;
        m_PredateDeathMenuAt = GetGame().GetTime() + PREDATE_DEATH_CARD_MS;
        m_PredateDeathCard.Show(true);
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (agent && agent.m_UserPaused) agent.RequestResume();
        CloseAllMenus();
        if (!m_PredateDeathValidation) return;
        Widget title = m_PredateDeathCard.FindAnyWidget("DeathTitle");
        float width, height;
        m_PredateDeathCard.GetScreenSize(width, height);
        int screenWidth, screenHeight;
        GetScreenSize(screenWidth, screenHeight);
        float x, y;
        m_PredateDeathCard.GetScreenPos(x, y);
        bool fullscreen = Math.AbsFloat(x) < 1 && Math.AbsFloat(y) < 1 && Math.AbsFloat(screenWidth - width) < 1 && Math.AbsFloat(screenHeight - height) < 1;
        bool styled = fullscreen && title && title.GetColor() == 0xFF000000 && m_PredateDeathCard.GetColor() == 0xFFFF0000;
        Print("[PredateDeploymentTest] Death card shown; styled=" + styled);
    }

    void PredateBeginRedeployCover(PlayerBase player)
    {
        m_PredateRedeployCovered = true;
        GetDayZGame().PredateSuppressRedeployCursor(true);
        if (!player.IsAlive()) m_PredateRedeployCorpse = player;
        if (m_PredateDeathCard) m_PredateDeathCard.Show(true);
        if (m_PredateDeathValidation)
        {
            m_PredateRedeployCursorLeak = GetGame().IsAppActive() && GetGame().GetUIManager().IsCursorVisible();
            bool visible = m_PredateDeathCard && m_PredateDeathCard.IsVisible();
            Print("[PredateDeploymentTest] Redeploy cover shown; visible=" + visible);
        }
    }

    bool PredateHoldRedeploy(PlayerBase player, PredateState state)
    {
        if (!m_PredateRedeployCovered) return false;
        if (m_PredateDeathValidation && GetGame().IsAppActive() && GetGame().GetUIManager().IsCursorVisible())
            m_PredateRedeployCursorLeak = true;
        if (IsPlayerRespawning() || GetDayZGame().IsLoading() || !player) return true;
        if (!player.IsAlive())
        {
            if (player == m_PredateRedeployCorpse) return true;
            m_PredateRedeployCovered = false;
            m_PredateRedeployCorpse = null;
            m_PredateDeathMenuAt = 0;
            PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
            if (agent) agent.m_DeployAfterRespawn = false;
            PredateBeginDeathCard(player);
            GetDayZGame().PredateSuppressRedeployCursor(false);
            return true;
        }
        if (!state || !player.IsPlayerLoaded()) return true;
        if (state.NeedsRedeploy && state.DeploymentSpawnRejected)
        {
            m_PredateRedeployCovered = false;
            m_PredateRedeployCorpse = null;
            GetDayZGame().PredateSuppressRedeployCursor(false);
            return false;
        }
        if (state.NeedsRedeploy || !state.HasDeploymentPosition || !player.GetCurrentPlayerCamera()) return true;
        if (player.GetDayZPlayerInventory().IsProcessing()) return true;
        if (PredateHunterBrain.Distance(player.GetPosition(), state.DeploymentPosition) > 5) return true;

        m_PredateRedeployCovered = false;
        m_PredateRedeployCorpse = null;
        Continue();
        GetGame().GetUIManager().ShowUICursor(false);
        GetDayZGame().PredateSuppressRedeployCursor(false);
        PredateHideDeathCard();
        if (m_PredateDeathValidation)
        {
            bool visible = m_PredateDeathCard && m_PredateDeathCard.IsVisible();
            Print("[PredateDeploymentTest] Redeploy cover released; visible=" + visible);
            Print("[PredateDeploymentTest] Cursor hidden through redeploy=" + !m_PredateRedeployCursorLeak + "; gameplay owns cursor=" + GetDayZGame().CanDisplayMouseCursor());
        }
        return false;
    }

    bool PredateDeathCardWaiting()
    {
        return m_PredateDeathMenuAt > GetGame().GetTime();
    }

    void PredateHideDeathCard()
    {
        if (!m_PredateDeathCard || !m_PredateDeathCard.IsVisible()) return;
        m_PredateDeathCard.Show(false);
    }

    void PredateRevealDeathOptions(InGameMenu menu)
    {
        if (!menu || !menu.PredateDeathOptionsVisible() || !m_PredateDeathCard || !m_PredateDeathCard.IsVisible()) return;
        PredateHideDeathCard();
        if (m_PredateDeathValidation)
        {
            int elapsed = GetGame().GetTime() - m_PredateDeathMenuAt + PREDATE_DEATH_CARD_MS;
            DayZPlayerImplement player = DayZPlayerImplement.Cast(GetGame().GetPlayer());
            bool animated = player && player.GetCommand_Death() != null;
            Widget red = menu.m_PredateDeathOptions.GetParent();
            bool backdrop = red.IsVisible() && red.GetColor() == 0xFFFF0000;
            Print("[PredateDeploymentTest] Death options revealed; elapsed=" + elapsed + "; animation=" + animated + "; red=" + backdrop);
        }
    }

    override void Pause()
    {
        if (PredateDeathCardWaiting() || m_PredateRedeployCovered)
        {
            m_PauseQueued = false;
            return;
        }
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
