class PredateReceiverEvents : ScriptedWidgetEventHandler
{
    PredateTrackerMenu m_Menu;
    void PredateReceiverEvents(PredateTrackerMenu menu) { m_Menu = menu; }
    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_Menu) return m_Menu.OnClick(w, x, y, button);
        return false;
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (m_Menu && w == m_Menu.m_Map && button == MouseState.LEFT)
            m_Menu.BeginMapPress(x, y);
        return false;
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (m_Menu && button == MouseState.LEFT)
            m_Menu.EndMapPress(w == m_Menu.m_Map, x, y);
        return false;
    }
}

class PredateTrackerMenu : UIScriptedMenu
{
    ref PredateReceiverEvents m_Events;
    TextWidget m_Status;
    TextWidget m_Bearing;
    TextWidget m_Timing;
    TextWidget m_Origin;
    Widget m_HistoryTitle;
    ref array<TextWidget> m_HistoryRows = new array<TextWidget>;
    ref array<TextWidget> m_MapLabels = new array<TextWidget>;
    ButtonWidget m_Select;
    ButtonWidget m_Deploy;
    ButtonWidget m_ConfirmDeploy;
    MapWidget m_Map;
    CanvasWidget m_MapCanvas;
    ref PredateMapOverlay m_MapOverlay;
    bool m_ConfirmReplacement;
    bool m_MapPressed;
    vector m_MapPress;
    vector m_MapPressCenter;
    float m_MapScale = 1;
    int m_MapTextSize;
    bool m_PredateOwnAimExclude;

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("Predate/gui/tracker.layout");
        m_Events = new PredateReceiverEvents(this);
        layoutRoot.SetHandler(m_Events);
        m_Status = TextWidget.Cast(layoutRoot.FindAnyWidget("Status"));
        m_Bearing = TextWidget.Cast(layoutRoot.FindAnyWidget("Bearing"));
        m_Timing = TextWidget.Cast(layoutRoot.FindAnyWidget("Timing"));
        m_Origin = TextWidget.Cast(layoutRoot.FindAnyWidget("Hint"));
        m_HistoryTitle = layoutRoot.FindAnyWidget("HistoryTitle");
        for (int index = 0; index < PredateProtocol.MAX_BEARING_HISTORY; index++)
            m_HistoryRows.Insert(TextWidget.Cast(layoutRoot.FindAnyWidget("History" + index)));
        m_MapLabels = {m_Status, m_Bearing, m_Timing, m_Origin};
        m_MapLabels.Insert(TextWidget.Cast(m_HistoryTitle));
        m_MapLabels.Insert(TextWidget.Cast(layoutRoot.FindAnyWidget("TrackingTitle")));
        foreach (TextWidget historyLabel : m_HistoryRows) m_MapLabels.Insert(historyLabel);
        m_Select = ButtonWidget.Cast(layoutRoot.FindAnyWidget("Select"));
        m_Select.SetHandler(m_Events);
        layoutRoot.FindAnyWidget("NewHunt").SetHandler(m_Events);
        m_Deploy = ButtonWidget.Cast(layoutRoot.FindAnyWidget("Deploy"));
        m_Deploy.SetHandler(m_Events);
        m_ConfirmDeploy = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ConfirmDeploy"));
        m_ConfirmDeploy.SetHandler(m_Events);
        m_Map = MapWidget.Cast(layoutRoot.FindAnyWidget("Terrain"));
        if (m_Map) m_Map.SetHandler(m_Events);
        m_MapCanvas = CanvasWidget.Cast(layoutRoot.FindAnyWidget("MapOverlay"));
        if (m_Map && m_MapCanvas) m_MapOverlay = new PredateMapOverlay(m_Map, m_MapCanvas);
        layoutRoot.FindAnyWidget("Overview").SetHandler(m_Events);
        layoutRoot.FindAnyWidget("CenterPlayer").SetHandler(m_Events);
        layoutRoot.FindAnyWidget("Close").SetHandler(m_Events);
        Print("[Predate] Operation map created.");
        return layoutRoot;
    }

    override void OnShow()
    {
        super.OnShow();
        Mission mission = GetGame().GetMission();
        if (mission && !mission.IsInputExcludeActive("aiming"))
        {
            mission.AddActiveInputExcludes({"aiming"});
            m_PredateOwnAimExclude = true;
        }
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player) CenterOn(player.GetPosition());
        else ShowOverview();
        if (player && player.m_PredateState && player.m_PredateState.CanDeploy) SetFocus(m_Deploy);
        else SetFocus(m_Select);
    }

    override void OnHide()
    {
        Mission mission = GetGame().GetMission();
        if (mission && m_PredateOwnAimExclude)
            mission.RemoveActiveInputExcludes({"aiming"}, true);
        m_PredateOwnAimExclude = false;
        m_ConfirmReplacement = false;
        m_MapPressed = false;
        super.OnHide();
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;
        PredateState state = player.m_PredateState;
        UpdateMapScale();
        if (m_MapOverlay) m_MapOverlay.Draw(player, state, m_MapScale);
        if (!state) return;
        if (!state.CanDeploy) m_ConfirmReplacement = false;
        layoutRoot.FindAnyWidget("NewHunt").Show(state.Development && !m_ConfirmReplacement);
        m_Deploy.Show(state.CanDeploy);
        m_Deploy.SetText("Start another hunt");
        if (m_ConfirmReplacement) m_Deploy.SetText("Cancel");
        m_ConfirmDeploy.Show(m_ConfirmReplacement);
        m_Select.Show(!state.CanDeploy && state.Locked);
        int available = state.AvailableOperators;
        m_Status.SetText(state.Message);
        if (state.CloseApproach)
            m_Bearing.SetText("Use sight and sound");
        else if (state.HasPulse)
            m_Bearing.SetText("Bearing " + PredateBearingPulse.RangeText(state.Bearing));
        else if (state.CanDeploy) m_Bearing.SetText("HUNT COMPLETE");
        else if (available == 0) m_Bearing.SetText("OPERATION COMPLETE");
        else if (state.Locked && !state.Available) m_Bearing.SetText("TARGET ELIMINATED");
        else if (PredateReceiverText.LostContact(state)) m_Bearing.SetText("CONTACT LOST");
        else m_Bearing.SetText("No pulse");
        if (state.Locked && state.Available && !state.CloseApproach)
            m_Timing.SetText("Sample age " + Math.Floor(state.Age).ToString() + "s    |    Next pulse " + Math.Ceil(state.NextPulse).ToString() + "s");
        else if (m_ConfirmReplacement) m_Timing.SetText("Keep your gear for the next hunt?");
        else if (state.CanDeploy) m_Timing.SetText("Start another hunt to bring in a rival.");
        else m_Timing.SetText("");
        m_Origin.SetText("No rival pulses in range");
        if (state.Pulses.Count() > 0) m_Origin.SetText("Select a rival’s bearing arc");
        ShowHistory(state);
    }

    void ShowHistory(PredateState state)
    {
        int count;
        if (state && state.CanShowHistory()) count = state.History.Count();
        if (m_HistoryTitle) m_HistoryTitle.Show(count > 0);
        for (int index = 0; index < m_HistoryRows.Count(); index++)
        {
            TextWidget row = m_HistoryRows[index];
            if (!row) continue;
            row.Show(index < count);
            if (index < count) row.SetText(PredateReceiverText.HistoryLine(state.History[index]));
        }
    }

    void UpdateMapScale()
    {
        float width, height;
        layoutRoot.GetScreenSize(width, height);
        m_MapScale = Math.Clamp(Math.Min(width / 1280.0, height / 720.0), 0.8, 2.5);
        int size = Math.Round(15 * m_MapScale);
        if (size == m_MapTextSize) return;
        m_MapTextSize = size;
        foreach (TextWidget label : m_MapLabels)
            if (label) label.SetTextExactSize(size);
    }

    void ShowOverview()
    {
        if (!m_Map) return;
        float center = GetGame().GetWorld().GetWorldSize() / 2;
        m_Map.SetScale(1);
        m_Map.SetMapPos(Vector(center, 0, center));
    }

    void CenterOn(vector position)
    {
        if (!m_Map) return;
        if (m_Map.GetScale() > 0.22) m_Map.SetScale(0.22);
        m_Map.SetMapPos(position);
        if (!m_MapOverlay || !m_MapOverlay.UpdateProjection()) return;
        // The native map anchor can be offset from the visible pane's center.
        vector center = m_MapOverlay.ViewportCenter();
        for (int attempt = 0; attempt < 6; attempt++)
        {
            if (vector.DistanceSq(m_MapOverlay.LocalPoint(position), center) <= 64) break;
            vector offset = position - m_MapOverlay.WorldPoint(center);
            offset[1] = 0;
            m_Map.SetMapPos(m_Map.GetMapPos() + offset * 0.55);
        }
    }

    void BeginMapPress(int x, int y)
    {
        m_MapPressed = true;
        m_MapPress = Vector(x, y, 0);
        m_MapPressCenter = m_Map.GetMapPos();
    }

    void EndMapPress(bool overMap, int x, int y)
    {
        bool pressed = m_MapPressed;
        m_MapPressed = false;
        if (!pressed || !overMap || !m_MapOverlay) return;
        if (vector.DistanceSq(Vector(x, y, 0), m_MapPress) > 25 || vector.DistanceSq(m_Map.GetMapPos(), m_MapPressCenter) > 1) return;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player || !player.m_PredateState) return;
        PredateState state = player.m_PredateState;
        string id = m_MapOverlay.HitPulse(x, y, state);
        if (id != "" && (!state.Locked || id != state.OperatorId)) player.PredateRequest(1, id);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!w) return false;
        Print("[Predate] Receiver click: " + w.GetName());
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (w.GetName() == "Close") { Close(); return true; }
        if (w.GetName() == "Overview") { ShowOverview(); return true; }
        if (w.GetName() == "CenterPlayer" && player) { CenterOn(player.GetPosition()); return true; }
        if (!player || !player.m_PredateState) return false;
        PredateState state = player.m_PredateState;
        if (w.GetName() == "Select")
        {
            if (state.Locked) player.PredateRequest(2, state.OperatorId);
            return true;
        }
        if (w.GetName() == "NewHunt") { player.PredateRequest(3); return true; }
        if (w.GetName() == "Deploy")
        {
            if (state.CanDeploy)
                m_ConfirmReplacement = !m_ConfirmReplacement;
            return true;
        }
        if (w.GetName() == "ConfirmDeploy")
        {
            if (m_ConfirmReplacement && state.CanDeploy)
            {
                m_ConfirmReplacement = false;
                player.PredateRequest(4);
            }
            return true;
        }
        return super.OnClick(w, x, y, button);
    }
}

modded class MissionGameplay
{
    Widget m_PredateHud;
    ref PredateHuntFeed m_PredateHuntFeed;
    TextWidget m_PredateHudText;
    TextWidget m_PredateFootprintClue;
    ref PredateFootprintView m_PredateFootprintView;
    string m_PredateReceiverContents;
    ref PredateCompass m_PredateCompass;
    float m_PredateUiTimer;
    bool m_PredateTesting;
    bool m_PredateReceiverValidation;
#ifndef SERVER
    ref PredateScopeValidation m_PredateScopeTest;
#endif
    ref PredatePresentationValidation m_PredatePresentationTest;
    ref PredateRosterReceiverValidation m_PredateRosterReceiverTest;
    ref PredateFootprintPresentationTest m_PredateFootprintTest;
    int m_PredateTestStage;
    float m_PredateTestTime;
    int m_PredateTestBearing;
    PredateTrackerMenu m_PredateTestMenu;
    bool m_PredateAutoRedeploySent;

    override void OnInit()
    {
        super.OnInit();
        m_PredateHud = GetGame().GetWorkspace().CreateWidgets("Predate/gui/hud.layout");
        if (m_PredateHud) m_PredateHudText = TextWidget.Cast(m_PredateHud.FindAnyWidget("Receiver"));
        if (m_PredateHud) m_PredateFootprintClue = TextWidget.Cast(m_PredateHud.FindAnyWidget("FootprintClue"));
        m_PredateFootprintView = new PredateFootprintView(m_PredateHud);
        m_PredateHuntFeed = new PredateHuntFeed;
        m_PredateCompass = new PredateCompass(m_PredateHud);
        m_PredateCompass.SelfTest();
        Print("[Predate] Operation map ready. Press M to open it.");
        string testValue;
        m_PredateTesting = GetGame().CommandlineGetParam("predateUITest", testValue) && GetGame().CommandlineGetParam("predateDevelopment", testValue);
        m_PredateReceiverValidation = GetGame().CommandlineGetParam("predateReceiverValidation", testValue) && FileExist("$profile:PredateReceiverValidation.txt");
#ifndef SERVER
        if (m_PredateReceiverValidation && GetGame().CommandlineGetParam("predateScopeValidation", testValue))
            m_PredateScopeTest = new PredateScopeValidation;
#endif
        if (GetGame().CommandlineGetParam("predatePresentationValidation", testValue) && FileExist("$profile:PredatePresentationValidation.txt"))
            m_PredatePresentationTest = new PredatePresentationValidation;
        if (GetGame().CommandlineGetParam("predateRosterReceiverValidation", testValue) && FileExist("$profile:PredateRosterReceiverValidation.txt"))
            m_PredateRosterReceiverTest = new PredateRosterReceiverValidation;
        if (GetGame().CommandlineGetParam("predateFootprintValidation", testValue) && FileExist("$profile:PredateFootprintValidation.txt"))
            m_PredateFootprintTest = new PredateFootprintPresentationTest;
    }

    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        if (id == PredateProtocol.MENU)
        {
            PredateTrackerMenu tracker = new PredateTrackerMenu;
            tracker.SetID(id);
            return tracker;
        }
        return super.CreateScriptedMenu(id);
    }

    override protected void HandleMapToggleByKeyboardShortcut(Man player)
    {
        PlayerBase localPlayer = PlayerBase.Cast(player);
        if (localPlayer && localPlayer.IsAlive() && !InGameMenu.PredateNeedsRedeploy(localPlayer))
            PredateOpenMap();
    }

    void PredateOpenMap()
    {
        UIManager ui = GetGame().GetUIManager();
        if (!ui.GetMenu()) ui.EnterScriptedMenu(PredateProtocol.MENU, null);
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        bool needsRedeploy = InGameMenu.PredateNeedsRedeploy(player);
        if (m_PredateHuntFeed)
        {
            UIScriptedMenu menu = GetGame().GetUIManager().GetMenu();
            m_PredateHuntFeed.Show(player && (!menu || (needsRedeploy && menu.GetID() == MENU_INGAME)));
            PredateState feedState;
            if (player) feedState = player.m_PredateState;
            m_PredateHuntFeed.Update(player, feedState, timeslice);
        }
        if (needsRedeploy)
        {
            if (m_PredateHud) m_PredateHud.Show(false);
            PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
            if (agent && agent.m_DeployAfterRespawn && player.IsAlive() && player.m_PredateState)
            {
                if (!m_PredateAutoRedeploySent)
                {
                    m_PredateAutoRedeploySent = true;
                    player.PredateRequest(5, "raven-01", agent.m_CustomRedeploy, agent.m_RedeployPoint);
                }
                if (!player.m_PredateState.DeploymentSpawnRejected) return;
                agent.m_DeployAfterRespawn = false;
                m_PredateAutoRedeploySent = false;
            }
            if (!IsPaused() && !IsPlayerRespawning() && player.IsPlayerLoaded() && !GetDayZGame().IsLoading())
            {
                Pause();
                if (IsPaused()) Print("[Predate] Deployment menu opened.");
            }
            return;
        }
        if (m_PredateAutoRedeploySent && player && player.m_PredateState)
        {
            m_PredateAutoRedeploySent = false;
            PredateClientAgent deploymentAgent = GetDayZGame().m_PredateClientAgent;
            if (deploymentAgent) deploymentAgent.m_DeployAfterRespawn = false;
        }
        if (!player || !player.IsAlive()) { if (m_PredateHud) m_PredateHud.Show(false); return; }
        if (GetUApi().GetInputByID(UAMapToggle).LocalPress()) PredateOpenMap();
        if (m_PredateHud) m_PredateHud.Show(!PredateTrackerMenu.Cast(GetGame().GetUIManager().GetMenu()));
        if (!m_PredateHudText) return;
        PredateState state = player.m_PredateState;
        if (m_PredateCompass) m_PredateCompass.Update(player, state, timeslice);
        if (m_PredatePresentationTest && !m_PredatePresentationTest.Update(m_PredateHud, m_PredateCompass, timeslice))
            m_PredatePresentationTest = null;
        if (m_PredateRosterReceiverTest && !m_PredateRosterReceiverTest.Update(state))
            m_PredateRosterReceiverTest = null;
        if (m_PredateTesting && state && state.Development) PredateTestReceiver(player, state, timeslice);
        if (m_PredateFootprintClue)
        {
            bool clueVisible = state && state.FootprintClue > 0 && !GetGame().GetUIManager().GetMenu();
            bool projected = m_PredateFootprintView.Update(player, state, clueVisible);
            m_PredateFootprintClue.Show(clueVisible && !projected);
            if (clueVisible && !projected)
            {
                if (state.FootprintClue == 2) m_PredateFootprintClue.SetText("Recent footprints");
                else m_PredateFootprintClue.SetText("Faint footprints");
            }
        }
        if (m_PredateFootprintTest && !m_PredateFootprintTest.Update(state, m_PredateFootprintClue, m_PredateFootprintView.m_Image))
            m_PredateFootprintTest = null;
        string text = "Map  M";
        if (state && state.Available && state.Locked && state.HasPulse && !state.CloseApproach)
            text = PredateProtocol.DisplayName(state.OperatorId) + "   |   " + PredateBearingPulse.RangeText(state.Bearing) + "   |   Next pulse " + Math.Ceil(state.NextPulse).ToString() + "s";
        if (PredatePresentationOptions.Current().HighContrast) m_PredateHudText.SetColor(0xFFFFFFFF);
        else m_PredateHudText.SetColor(0xFFC4D4D9);
        m_PredateHudText.SetText(text);
        m_PredateReceiverContents = text;
#ifndef SERVER
        if (m_PredateScopeTest)
        {
            if (!m_PredateScopeTest.Update(player, timeslice)) m_PredateScopeTest = null;
            return;
        }
#endif
        if (m_PredateReceiverValidation && state && state.HasPulseOrigin && state.Intercepted && !GetGame().GetUIManager().GetMenu())
        {
            m_PredateReceiverValidation = false;
            PredateReceiverValidation.Run(this, player, state);
        }
    }

    void PredateTestReceiver(PlayerBase player, PredateState state, float timeslice)
    {
        if (m_PredateTestStage > 0 && GetGame().GetUIManager().GetMenu() != m_PredateTestMenu)
        {
            Print("[Predate] Receiver check stopped because its menu was closed.");
            m_PredateTesting = false;
            return;
        }
        m_PredateTestTime += timeslice;
        if (m_PredateTestTime > 20)
        {
            Print("[Predate] UI SELFTEST FAIL: stage " + m_PredateTestStage + " timed out.");
            m_PredateTesting = false;
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateExitTestClient, 3000, false);
            return;
        }
        if (m_PredateTestStage == 0 && state.Available && !GetGame().GetUIManager().GetMenu())
        {
            m_PredateTestMenu = PredateTrackerMenu.Cast(GetGame().GetUIManager().EnterScriptedMenu(PredateProtocol.MENU, null));
            m_PredateTestStage = 1;
            m_PredateTestTime = 0;
        }
        else if (m_PredateTestStage == 1 && m_PredateTestTime > 2 && m_PredateTestMenu)
        {
            if (state.Locked)
            {
                player.PredateRequest(2);
                m_PredateTestStage = 6;
                m_PredateTestTime = 0;
            }
            else
            {
                if (state.Pulses.Count() == 0) return;
                player.PredateRequest(1, state.Pulses[0].OperatorId);
                m_PredateTestStage = 2;
                m_PredateTestTime = 0;
            }
        }
        else if (m_PredateTestStage == 2 && state.Locked && state.HasPulse)
        {
            Print("[Predate] UI SELFTEST PASS: button handler -> client RPC -> server lock -> bearing received.");
            m_PredateTestBearing = state.Bearing;
            m_PredateTestStage = 3;
            m_PredateTestTime = 0;
        }
        else if (m_PredateTestStage == 3 && m_PredateTestTime > 5)
        {
            if (state.Bearing == m_PredateTestBearing && state.NextPulse < PredateBearingPulse.INTERVAL && state.NextPulse > 0)
                Print("[Predate] UI SELFTEST PASS: bearing held between samples; countdown advancing.");
            else Print("[Predate] UI SELFTEST FAIL: pulse did not hold or countdown did not advance.");
            m_PredateTestMenu.m_Events.OnClick(m_PredateTestMenu.m_Select, 0, 0, 0);
            m_PredateTestStage = 4;
            m_PredateTestTime = 0;
        }
        else if (m_PredateTestStage == 3)
        {
            // Exercise the rendered compass around the full circle without
            // altering the player's camera or querying the target's position.
            if (m_PredateCompass) m_PredateCompass.Render(m_PredateTestTime * 144, state, timeslice);
        }
        else if (m_PredateTestStage == 6 && !state.Locked)
        {
            m_PredateTestStage = 1;
            m_PredateTestTime = 2.1;
        }
        else if (m_PredateTestStage == 4 && !state.Locked)
        {
            Print("[Predate] UI SELFTEST PASS: clear target button acknowledged.");
            Widget reset = m_PredateTestMenu.GetLayoutRoot().FindAnyWidget("NewHunt");
            m_PredateTestMenu.m_Events.OnClick(reset, 0, 0, 0);
            m_PredateTestStage = 5;
            m_PredateTestTime = 0;
        }
        else if (m_PredateTestStage == 5 && m_PredateTestTime > 3)
        {
            Widget closeButton = m_PredateTestMenu.GetLayoutRoot().FindAnyWidget("Close");
            m_PredateTestMenu.m_Events.OnClick(closeButton, 0, 0, 0);
            Print("[Predate] UI SELFTEST COMPLETE: reset and close handlers exercised.");
            m_PredateTesting = false;
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateExitTestClient, 3000, false);
        }
    }

    void PredateExitTestClient()
    {
        string flag;
        if (!GetGame().CommandlineGetParam("predateDevelopment", flag)) return;
        if (!GetGame().CommandlineGetParam("predateUITest", flag)) return;
        Print("[Predate] Development receiver test client exiting normally.");
        GetGame().RequestExit(0);
    }

    override void OnMissionFinish()
    {
        if (m_PredateHuntFeed) m_PredateHuntFeed.Close();
        if (m_PredateHud) m_PredateHud.Unlink();
        super.OnMissionFinish();
    }
}
