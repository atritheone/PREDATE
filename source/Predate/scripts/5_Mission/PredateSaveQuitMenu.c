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
    Widget m_PredateDebrief;
    TextWidget m_PredateDebriefTitle;
    TextWidget m_PredateDebriefLife;
    TextWidget m_PredateDebriefOutcome;
    int m_PredateDebriefTextSize;
    Widget m_PredateSpawnForm;
    EditBoxWidget m_PredateSpawnX;
    EditBoxWidget m_PredateSpawnZ;
    TextWidget m_PredateSpawnStatus;

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
            UpdatePredatePauseBrand(true);
        }
        m_PredateDebrief = GetGame().GetWorkspace().CreateWidgets("Predate/gui/deployment-report.layout", root);
        if (m_PredateDebrief)
        {
            m_PredateDebriefTitle = TextWidget.Cast(m_PredateDebrief.FindAnyWidget("DeploymentTitle"));
            m_PredateDebriefLife = TextWidget.Cast(m_PredateDebrief.FindAnyWidget("DeploymentLife"));
            m_PredateDebriefOutcome = TextWidget.Cast(m_PredateDebrief.FindAnyWidget("DeploymentOutcome"));
        }
        m_PredateSpawnForm = GetGame().GetWorkspace().CreateWidgets("Predate/gui/redeploy-spawn.layout", root);
        if (m_PredateSpawnForm)
        {
            m_PredateSpawnX = EditBoxWidget.Cast(m_PredateSpawnForm.FindAnyWidget("RedeployX"));
            m_PredateSpawnZ = EditBoxWidget.Cast(m_PredateSpawnForm.FindAnyWidget("RedeployZ"));
            m_PredateSpawnStatus = TextWidget.Cast(m_PredateSpawnForm.FindAnyWidget("RedeploySpawnStatus"));
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
        UpdatePredateDebrief(receiver);
        if (m_PredateSpawnForm) m_PredateSpawnForm.Show(PredateNeedsRedeploy(player));
        if (PredateNeedsRedeploy(player))
        {
            if (receiver && receiver.DeploymentSpawnRejected)
            {
                m_PredateRedeployPending = false;
                if (m_PredateSpawnStatus) m_PredateSpawnStatus.SetText("Choose dry ground at least 1.5 km from other Operators.");
            }
            m_PredateDeploymentScreen = true;
            m_ContinueButton.Show(false);
            m_RestartButton.Show(false);
            m_RespawnButton.Show(true);
            m_RespawnButton.Enable(!m_PredateRedeployPending);
            if (m_PredateRedeployPending) ButtonSetText(m_RespawnButton, "Deploying...");
            else ButtonSetText(m_RespawnButton, "Redeploy");
            if (m_PredateDeploymentTest && !m_PredateRedeployPending && receiver && receiver.NeedsRedeploy && FileExist("$profile:PredateRedeployClick.txt"))
            {
                FileHandle selection = OpenFile("$profile:PredateRedeployClick.txt", FileMode.READ);
                string coordinates;
                if (selection)
                {
                    FGets(selection, coordinates);
                    CloseFile(selection);
                }
                if (coordinates != "" && m_PredateSpawnX && m_PredateSpawnZ)
                {
                    vector point = coordinates.ToVector();
                    m_PredateSpawnX.SetText(point[0].ToString());
                    m_PredateSpawnZ.SetText(point[2].ToString());
                }
                DeleteFile("$profile:PredateRedeployClick.txt");
                bool fits = m_PredateDebrief && m_PredateDebrief.IsVisible() && PredateReceiverValidation.Fits(m_PredateDebriefTitle) && PredateReceiverValidation.Fits(m_PredateDebriefLife) && PredateReceiverValidation.Fits(m_PredateDebriefOutcome);
                Print("[PredateDeploymentTest] DEBRIEF: life=" + receiver.EndedLife + "; elapsed=" + receiver.EndedLifeSeconds + "; bearings=" + receiver.EndedLifeBearings + "; defeated=" + receiver.OperatorDefeated + "; fits=" + fits);
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

    void UpdatePredateDebrief(PredateState receiver)
    {
        if (!m_PredateDebrief) return;
        bool show = receiver && receiver.NeedsRedeploy;
        m_PredateDebrief.Show(show);
        if (!show || !m_PredateDebriefLife || !m_PredateDebriefOutcome) return;
        float width, height;
        m_PredateDebrief.GetScreenSize(width, height);
        int textSize = Math.Clamp(Math.Floor((height * 0.18 - 2) / 1.5), 10, 28);
        if (textSize != m_PredateDebriefTextSize)
        {
            m_PredateDebriefTextSize = textSize;
            if (m_PredateDebriefTitle) m_PredateDebriefTitle.SetTextExactSize(textSize + 3);
            m_PredateDebriefLife.SetTextExactSize(textSize);
            m_PredateDebriefOutcome.SetTextExactSize(textSize);
        }
        int minutes = receiver.EndedLifeSeconds / 60;
        int seconds = receiver.EndedLifeSeconds % 60;
        m_PredateDebriefLife.SetText("Life " + receiver.EndedLife + "   |   " + minutes + "m " + seconds + "s in operation");
        string outcome = "Raven active";
        if (receiver.OperatorDefeated) outcome = "Raven eliminated";
        m_PredateDebriefOutcome.SetText(outcome + "   |   Recent bearings " + receiver.EndedLifeBearings);
        m_PredateDebrief.Update();
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
        bool customSpawn;
        vector point;
        if (m_PredateSpawnX && m_PredateSpawnZ)
        {
            string x = m_PredateSpawnX.GetText();
            string z = m_PredateSpawnZ.GetText();
            customSpawn = x != "" || z != "";
            float east, north;
            if (customSpawn && (!ParsePredateCoordinate(x, east) || !ParsePredateCoordinate(z, north)))
            {
                if (m_PredateSpawnStatus) m_PredateSpawnStatus.SetText("Enter valid X and Z coordinates in metres.");
                return;
            }
            point = Vector(east, 0, north);
        }
        m_PredateRedeployPending = true;
        if (player.m_PredateState) player.m_PredateState.DeploymentSpawnRejected = false;
        if (player.IsAlive())
        {
            player.PredateRequest(5, "raven-01", customSpawn, point);
            return;
        }
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (agent)
        {
            agent.m_DeployAfterRespawn = true;
            agent.m_CustomRedeploy = customSpawn;
            agent.m_RedeployPoint = point;
        }
        GameRespawn(true);
    }

    static bool ParsePredateCoordinate(string text, out float position)
    {
        if (text.Length() == 0 || text.Length() > 16) return false;
        bool decimal;
        for (int index = 0; index < text.Length(); index++)
        {
            string digit = text.Substring(index, 1);
            if (digit == "." && !decimal) decimal = true;
            else if ("0123456789".IndexOf(digit) == -1) return false;
        }
        position = text.ToFloat();
        return position > 10 && position < 15350;
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
        super.Pause();
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
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
