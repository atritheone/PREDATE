// Explicit disposable-client fixture; never enabled by ordinary startup.
class PredateReceiverValidationResult
{
    int Schema = 1;
    int Checks;
    int Failures;
    vector PulseOrigin;
    float BearingError;
    bool Intercepted;
    int PulseStart;
    bool PauseBrandVerified;

    void Check(bool passed, string label = "")
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateReceiverTest] FAIL: check " + Checks + " " + label);
    }
}

class PredateReceiverValidation
{
    static bool Fits(TextWidget widget)
    {
        if (!widget) return false;
        int width, height;
        float availableWidth, availableHeight;
        widget.GetTextSize(width, height);
        widget.GetScreenSize(availableWidth, availableHeight);
        bool fits = width > 0 && height > 0 && width <= availableWidth + 1 && height <= availableHeight + 1;
        if (!fits) Print("[Predate] Text fit: " + widget.GetName() + "; text=" + width + "x" + height + "; box=" + availableWidth + "x" + availableHeight);
        return fits;
    }

    static void Run(MissionGameplay mission, PlayerBase player, PredateState state)
    {
        PredateReceiverValidationResult result = new PredateReceiverValidationResult;
        result.PulseOrigin = state.PulseOrigin;
        ActionToggleNVG switchNVG = new ActionToggleNVG;
        result.Check(GetUApi().GetInputByName("UAPredateToggleNVG") && switchNVG.GetInputType() == PredateNVGInput);
        result.BearingError = state.BearingError;
        result.Intercepted = state.Intercepted;
        result.Check(GetGame().GetWorld().GetThirdPersonViewMode() == ThirdPersonMode.DISABLED);
        player.SetIsInThirdPerson(true);
        result.Check(player.CameraHandler(0) == DayZPlayerCameras.DAYZCAMERA_1ST && !player.IsInThirdPerson());
        result.Check(state.HasPulseOrigin && state.PulseOrigin[0] == 4500 && state.PulseOrigin[2] == 6500 && state.BearingError > 0 && state.Intercepted);
        result.Check(Fits(mission.m_PredateHudText) && mission.m_PredateReceiverContents.Contains("Raven") && mission.m_PredateReceiverContents.Contains("Next"));
        PredateHuntFeed feed = mission.m_PredateHuntFeed;
        result.Check(HuntWarningVisible(feed));
        player.m_PredateKillNotices = PredateProtocol.OperatorBit("raven-01");
        feed.Update(player, state, 0);
        result.Check(feed.m_Notices[0].Text == "Raven is KIA." && Fits(feed.m_Rows[0]) && feed.m_Rows[0].GetColor() == PredateHuntFeed.ELIMINATION && HuntWarningVisible(feed));
        feed.Post("Tracking Lynx.");
        feed.Update(player, state, 0);
        result.Check(feed.m_Rows[0].GetColor() == 0xFFFFFFFF);
        PredateState death = new PredateState;
        death.NeedsRedeploy = true;
        feed.Update(player, death, 0);
        int noticeCount = feed.m_Notices.Count();
        feed.Update(player, death, 0);
        result.Check(feed.m_Notices[0].Text == "You are KIA." && feed.m_Rows[0].GetColor() == PredateHuntFeed.DEATH && feed.m_Notices.Count() == noticeCount);
        feed.Show(false);
        feed.Update(player, state, 7);
        result.Check(feed.m_Notices.Count() == noticeCount - 1 && HuntWarningVisible(feed), "live notices remain queued and the death notice clears");
        feed.Show(true);
        feed.Update(player, state, 7);
        result.Check(!feed.m_Rows[0].IsVisible());
        CheckReadyInteraction(result, player);
        mission.PredateToggleMap();
        PredateTrackerMenu menu = PredateTrackerMenu.Cast(GetGame().GetUIManager().GetMenu());
        result.Check(menu != null);
        if (menu)
        {
            result.Check(mission.IsInputExcludeActive("aiming"));
            result.Check(!GetGame().GetInput().HasGameFocus(INPUT_DEVICE_MOUSE));
            menu.Update(0);
            result.Check(menu.m_Map && menu.m_MapCanvas && menu.m_MapOverlay && menu.m_Map.IsVisible());
            result.Check(state.Pulses.Count() == 1);
            if (menu.m_MapOverlay && state.Pulses.Count() == 1)
            {
                PredateBearingPulse pulse = state.Pulses[0];
                result.PulseStart = pulse.Start;
                result.Check(PredateBearingPulse.Contains(pulse.Start, 0) && pulse.Start == state.Bearing && pulse.DistanceBand == 2);
                mission.m_PredateCompass.Render(pulse.Start + 7.5, state, 0);
                result.Check(mission.m_PredateCompass.m_Sector.GetColor() == pulse.Color());
                result.Check(pulse.DistanceText() == "250–500 m");
                result.Check(state.NextPulse > 0 && state.NextPulse <= 90);
                result.Check(menu.m_MapOverlay.m_Bearings.Count() == 24);
                result.Check(menu.m_MapOverlay.m_Bearings[0].IsVisible() && Fits(menu.m_MapOverlay.m_Bearings[0]));
                menu.m_Map.SetScale(0.33);
                Widget centerPlayer = menu.GetLayoutRoot().FindAnyWidget("CenterPlayer");
                menu.OnClick(centerPlayer, 0, 0, 0);
                PredateMapOverlay overlay = menu.m_MapOverlay;
                bool centeredPlayer = PredateSignalRules.WithinFinalApproach(menu.m_Map.GetMapPos(), player.GetPosition(), 1);
                if (!centeredPlayer) Print("[PredateMapTest] could not center player");
                result.Check(centeredPlayer);
                float width, height;
                menu.m_MapCanvas.GetScreenSize(width, height);
                int pulseSlot = PredateProtocol.s_OperatorIds.Find(pulse.OperatorId);
                float arcRadius;
                array<float> zooms = {0.085, 0.33, 0.68};
                foreach (float zoom : zooms)
                {
                    menu.m_Map.SetScale(zoom);
                    menu.m_Map.SetMapPos(player.GetPosition());
                    menu.Update(0);
                    vector arrow = overlay.m_PlayerPoint;
                    // The native map clamps panning when the viewport reaches a world edge.
                    // The blue marker should still land on the player at any zoom.
                    vector screenArrow = arrow + Vector(overlay.m_Left, overlay.m_Top, 0);
                    vector underArrow = menu.m_Map.ScreenToMap(screenArrow);
                    bool pointsToPlayer = PredateSignalRules.WithinFinalApproach(player.GetPosition(), underArrow, 25);
                    if (!pointsToPlayer) Print("[PredateMapTest] arrow " + arrow + " is over " + underArrow + " at zoom " + zoom);
                    result.Check(pointsToPlayer && arrow[0] >= 0 && arrow[0] <= width && arrow[1] >= 0 && arrow[1] <= height);
                    if (arcRadius == 0) arcRadius = overlay.m_ArcRadii[pulseSlot];
                    result.Check(arcRadius > 0 && Math.AbsFloat(overlay.m_ArcRadii[pulseSlot] - arcRadius) < 0.1, "pulse radius stays fixed as the map zooms");
                }
                menu.m_Map.SetScale(zooms[0]);
                menu.m_Map.SetMapPos(player.GetPosition());
                menu.Update(0);
                vector centeredArrow = overlay.m_PlayerPoint;
                vector pan = Vector(overlay.m_Left + width * 0.72, overlay.m_Top + height * 0.72, 0);
                menu.m_Map.SetMapPos(menu.m_Map.ScreenToMap(pan));
                menu.Update(0);
                result.Check(vector.Distance(centeredArrow, overlay.m_PlayerPoint) > 20, "panning moves the map marker");
                result.Check(Math.AbsFloat(overlay.m_ArcRadii[pulseSlot] - arcRadius) < 0.1, "panning does not resize the bearing dial");
                menu.m_Map.SetMapPos(player.GetPosition());
                menu.Update(0);
                CheckOverlappingPulses(result, menu, player);
                string town;
                GetGame().ConfigGetText("CfgWorlds ChernarusPlus Names Settlement_Chernogorsk name", town);
                array<float> townPosition = {};
                GetGame().ConfigGetFloatArray("CfgWorlds ChernarusPlus Names Settlement_Chernogorsk position", townPosition);
                Print("[PredateMapTest] Chernogorsk name=" + town + "; position elements=" + townPosition.Count());
                result.Check(town == "Chernogorsk" && townPosition.Count() == 2 && townPosition[0] > 6700);
            }
            result.Check(Fits(menu.m_Origin));
            result.Check(Fits(menu.m_Bearing));
            result.Check(menu.m_InterceptionStatus && menu.m_InterceptionStatus.IsVisible() && Fits(menu.m_InterceptionStatus), "active interception is visible on the map");
            bool historyMatches;
            if (state.History.Count() == 1)
            {
                PredateReceiverSample entry = state.History[0];
                historyMatches = entry.Origin == state.PulseOrigin && entry.Bearing == state.Bearing && entry.Error == state.BearingError && entry.Age >= 0 && entry.DistanceBand == 2;
                PredateMapOverlay historicalMap = menu.m_MapOverlay;
                vector savedMarker = historicalMap.m_PlayerPoint;
                historicalMap.m_PlayerPoint += "200 100 0";
                result.Check(vector.Distance(historicalMap.HistoryPoint(entry, entry.Bearing, 0), historicalMap.LocalPoint(entry.Origin)) < 0.1, "historical sector stays at the observer's sampled location");
                historicalMap.m_PlayerPoint = savedMarker;
            }
            result.Check(historyMatches);
            result.Check(Fits(menu.m_HistoryRows[0]) && menu.m_HistoryRows[0].IsVisible() && !menu.m_HistoryRows[1].IsVisible());
            state.HasPulse = false;
            result.Check(PredateReceiverText.LostContact(state));
            state.CloseApproach = true;
            menu.ShowHistory(state);
            result.Check(menu.m_HistoryTitle.IsVisible() && menu.m_HistoryRows[0].IsVisible(), "history stays visible on the final approach");
            result.Check(!PredateReceiverText.LostContact(state));
            state.CloseApproach = false;
            state.Locked = false;
            menu.ShowHistory(state);
            result.Check(!menu.m_HistoryTitle.IsVisible() && !menu.m_HistoryRows[0].IsVisible());
            state.Locked = true;
            state.HasPulse = true;
            string buttonText;
            menu.m_Select.GetText(buttonText);
            result.Check(buttonText == "Clear target lock");
            menu.OnClick(menu.m_Deploy, 0, 0, 0);
            menu.Update(0);
            result.Check(!menu.m_ConfirmReplacement && !menu.m_ConfirmDeploy.IsVisible());
            state.CanDeploy = true;
            menu.OnClick(menu.m_Deploy, 0, 0, 0);
            menu.Update(0);
            result.Check(menu.m_ConfirmReplacement && menu.m_ConfirmDeploy.IsVisible() && Fits(menu.m_Timing));
            menu.OnClick(menu.m_Deploy, 0, 0, 0);
            menu.Update(0);
            result.Check(!menu.m_ConfirmReplacement && !menu.m_ConfirmDeploy.IsVisible());
            state.CanDeploy = false;
            mission.PredateToggleMap(true);
        }
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(Finish, 100, false, result, mission);
    }

    static void CheckReadyInteraction(PredateReceiverValidationResult result, PlayerBase player)
    {
        ActionManagerClient manager = ActionManagerClient.Cast(player.GetActionManager());
        ActionBase open = ActionManagerBase.GetAction(ActionOpenBarrel);
        vector position = player.GetPosition() + player.GetDirection() * 0.75;
        Barrel_ColorBase barrel = Barrel_ColorBase.Cast(GetGame().CreateObjectEx("Barrel_Blue", position, ECE_LOCAL | ECE_KEEPHEIGHT));
        bool ready = manager && open && barrel && player.PredateMayPreviewInteraction();
        result.Check(ready, "weapon raised with interaction fixture ready");
        if (ready)
        {
            barrel.Close();
            ActionTarget target = new ActionTarget(barrel, null, -1, position, 0);
            ItemBase item = player.GetItemInHands();
            int conditions = ActionBase.ComputeConditionMask(player, target, item);
            ActionInput interact = open.GetInput();
            interact.UpdatePossibleActions(player, target, item, conditions);
            result.Check(manager.GetPossibleActions(InteractActionInput).Find(open) >= 0, "the Open action appears while raised");
            result.Check(!open.Can(player, target, item, conditions) && !player.m_PredateInteractionPreview, "preview does not authorize execution while raised");
            player.PredateUpdateInteraction(0, true, true);
            interact.Update();
            result.Check(player.m_PredateInteractionTime > 0 && player.m_PredateInteractionTap && !interact.JustActivate(), "tap waits for the weapon to lower");
            result.Check(!player.GetMeleeFightLogic().CanFight(), "a shared F press is reserved for interaction");
            player.PredateUpdateInteraction(1, false, false);
            result.Check(player.GetMeleeFightLogic().CanFight(), "melee is available again when the input clears");
            interact.ActionsSelectReset();
        }
        if (barrel) GetGame().ObjectDelete(barrel);
    }

    static void CheckOverlappingPulses(PredateReceiverValidationResult result, PredateTrackerMenu menu, PlayerBase player)
    {
        PredateState example = new PredateState;
        for (int index = 0; index < 3; index++)
        {
            PredateBearingPulse pulse = new PredateBearingPulse;
            pulse.OperatorId = PredateProtocol.OperatorAt(index);
            pulse.DistanceBand = 16 - index * 6;
            example.Pulses.Insert(pulse);
        }
        example.Pulses[0].Start = 355;
        example.Pulses[1].Start = 6;
        example.Pulses[2].Start = 10;
        PredateMapOverlay overlay = menu.m_MapOverlay;
        overlay.Draw(player, example, menu.m_MapScale);
        result.Check(overlay.m_ArcRadii[0] > overlay.m_ArcRadii[1] && overlay.m_ArcRadii[1] > overlay.m_ArcRadii[2] && overlay.m_ArcRadii[2] > 0);
        vector click = overlay.RadialPoint(2, overlay.m_ArcRadii[0]);
        result.Check(overlay.HitPulse(click[0] + overlay.m_Left, click[1] + overlay.m_Top, example) == example.Pulses[0].OperatorId);
    }

    static void Finish(PredateReceiverValidationResult result, MissionGameplay mission)
    {
        result.Check(!mission.IsInputExcludeActive("aiming"));
        result.Check(!GetGame().GetUIManager().GetMenu());
        result.Check(!GetGame().GetUIManager().GetMenu() && !mission.IsPaused(), "map close returns to play without pausing");
        mission.PredateToggleMap(true);
        result.Check(!GetGame().GetUIManager().GetMenu(), "back-only input leaves the closed map closed");
        mission.Pause();
        InGameMenu pause = InGameMenu.Cast(GetGame().GetUIManager().GetMenu());
        result.PauseBrandVerified = PauseBrandVisible(pause);
        result.Check(result.PauseBrandVerified);
        PredateClientAgent agent = GetDayZGame().m_PredateClientAgent;
        if (agent) agent.RequestResume();
        string error;
        JsonFileLoader<PredateReceiverValidationResult>.SaveFile("$profile:ReceiverValidation.json", result, error);
        Print("[PredateReceiverTest] COMPLETE: checks=" + result.Checks + "; failures=" + result.Failures);
    }

    static bool HuntWarningVisible(PredateHuntFeed feed)
    {
        for (int row = 0; row < feed.m_Notices.Count(); row++)
        {
            TextWidget label = feed.m_Rows[row];
            if (feed.m_Notices[row].Text == "You are being hunted." && Fits(label) && label.IsVisible() && label.GetColor() == PredateHuntFeed.HUNTED)
                return true;
        }
        return false;
    }

    static bool PauseBrandVisible(InGameMenu pause)
    {
        if (!pause || !pause.m_PredatePauseBrand || !pause.m_PredatePauseWordmarkLoaded || !Fits(pause.m_PredatePauseSlayer)) return false;
        ImageWidget wordmark = pause.m_PredatePauseWordmark;
        if (!wordmark || !wordmark.IsVisible() || !pause.m_PredatePauseSlayer.IsVisible()) return false;
        float width, height, logoX, logoY, slayerX, slayerY, frameWidth, frameHeight;
        wordmark.GetScreenSize(width, height);
        wordmark.GetScreenPos(logoX, logoY);
        pause.m_PredatePauseSlayer.GetScreenPos(slayerX, slayerY);
        pause.m_PredatePauseBrand.GetScreenSize(frameWidth, frameHeight);
        return height > 0 && Math.AbsFloat(width / height - 1965.0 / 400.0) < 0.08 && Math.AbsFloat(logoX - slayerX) < 1 && logoY < frameHeight * 0.1 && slayerY > frameHeight * 0.85;
    }
}
