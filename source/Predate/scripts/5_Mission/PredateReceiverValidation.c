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
        string bearing = PredateBearingPulse.RangeText(state.Bearing, " – ");
        string readout = mission.m_PredateReceiverContents;
        result.Check(Fits(mission.m_PredateHudText) && readout.Contains("Raven") && readout.Contains(bearing) && readout.Contains("250 – 500 m") && readout.Contains("Next Pulse "), "the compass readout fits the tracked pulse range");
        PredateHuntFeed feed = mission.m_PredateHuntFeed;
        result.Check(HuntWarningVisible(feed));
        player.m_PredateKillNotices.Insert(0);
        feed.Update(player, state, 0);
        result.Check(feed.m_Notices[0].Text == "Raven is KIA." && Fits(feed.m_Rows[0]) && feed.m_Rows[0].GetColor() == PredateHuntFeed.NORMAL && HuntWarningVisible(feed));
        feed.Post("Tracking Lynx.");
        feed.Update(player, state, 0);
        result.Check(feed.m_Rows[0].GetColor() == 0xFFFFFFFF);
        PredateState death = new PredateState;
        death.NeedsRedeploy = true;
        int noticeCount = feed.m_Notices.Count();
        feed.Update(player, death, 0);
        result.Check(feed.m_Notices.Count() == noticeCount, "player death adds no notice");
        feed.Show(false);
        feed.Update(player, state, 7);
        result.Check(feed.m_Notices.Count() == noticeCount && HuntWarningVisible(feed), "live notices remain queued while the feed is hidden");
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
        array<string> hits = {};
        overlay.HitPulses(click[0] + overlay.m_Left, click[1] + overlay.m_Top, example, hits);
        result.Check(hits.Count() == 1 && hits[0] == example.Pulses[0].OperatorId);
        int readableNames;
        foreach (int placed : overlay.m_PlacedSlots)
        {
            TextWidget nameplate = overlay.m_Labels[placed];
            vector namePoint = overlay.m_LabelPositions[placed];
            vector anchorOffset = overlay.m_LabelAnchors[placed] - overlay.m_PlayerPoint;
            overlay.PreviewPulse(namePoint[0] + overlay.m_Left, namePoint[1] + overlay.m_Top, example);
            bool clear = true;
            foreach (int shown : overlay.m_ActiveSlots)
            {
                if (overlay.ArcInBox(shown, namePoint, overlay.m_LabelHalfWidths[placed], overlay.m_LabelHeight / 2)) clear = false;
            }
            bool correctPreview = overlay.m_HoveredPulse && overlay.m_HoveredPulse.OperatorId == PredateProtocol.OperatorAt(placed);
            float anchorBearing = PredateMath.WrapHeading(Math.Atan2(anchorOffset[0], -anchorOffset[1]) * Math.RAD2DEG);
            bool correctAnchor = Math.AbsFloat(anchorOffset.Length() - overlay.m_ArcRadii[placed]) < 0.01 && PredateBearingPulse.Contains(overlay.m_ArcStarts[placed], anchorBearing);
            result.Check(clear && Fits(nameplate) && nameplate.GetColor() == 0xFFFFFFFF && correctPreview && correctAnchor, "a white nameplate clears all pulse strokes and highlights the arc its leader touches");
            readableNames++;
        }
        result.Check(readableNames >= 2, "nearby bearings still get distinct attached names");
        overlay.PreviewPulse(click[0] + overlay.m_Left, click[1] + overlay.m_Top, example);
        result.Check(overlay.m_HoveredPulse && overlay.m_HoveredPulse.OperatorId == example.Pulses[0].OperatorId, "hovering an arc identifies its Operator");

        example.Pulses.Clear();
        int crowdedBearing = 82;
        if (overlay.m_PlayerPoint[0] > overlay.m_Width / 2) crowdedBearing = 262;
        for (int slot = 0; slot < PredateProtocol.MAX_OPERATORS; slot++)
        {
            PredateBearingPulse dense = new PredateBearingPulse;
            dense.OperatorId = PredateProtocol.OperatorAt(slot);
            dense.Start = crowdedBearing;
            dense.DistanceBand = 20 - slot / 5;
            example.Pulses.Insert(dense);
        }
        example.Locked = true;
        example.OperatorId = example.Pulses[0].OperatorId;
        overlay.Draw(player, example, menu.m_MapScale);
        int last = example.Pulses.Count() - 1;
        result.Check(overlay.m_ArcRadii[0] - overlay.m_ArcRadii[1] >= PredateMapOverlay.MIN_LANE_SPACING * menu.m_MapScale - 0.1, "a dense stack retains readable lane spacing");
        result.Check(overlay.m_ArcRadii[last] >= PredateMapOverlay.INNER_CLEARANCE * menu.m_MapScale - 0.1, "the dense stack leaves room for the player arrow");
        result.Check(overlay.m_Labels[0].IsVisible() && overlay.m_Labels[0].GetColor() == PredateMapOverlay.SELECTED_COLOR, "the tracked pulse has a white label");
        vector inside = overlay.RadialPoint(crowdedBearing + 7.5, overlay.m_ArcRadii[last]);
        overlay.HitPulses(inside[0] + overlay.m_Left, inside[1] + overlay.m_Top, example, hits);
        result.Check(hits.Count() > 1 && hits.Find(example.Pulses[last].OperatorId) >= 0 && hits.Find(example.Pulses[0].OperatorId) < 0, "overlapping contacts on the shared lane are selectable");
        overlay.PreviewPulse(inside[0] + overlay.m_Left, inside[1] + overlay.m_Top, example);
        result.Check(!overlay.m_HoveredPulse && overlay.m_HoverHits.Count() == hits.Count(), "shared arcs preview the group instead of implying a single Operator");
        menu.ShowPulsePicker(example, hits);
        result.Check(menu.m_PulsePicker.IsVisible() && menu.m_PulseRows.GetNumItems() == hits.Count() && menu.m_PulseChoices[0] == example.Pulses[last].OperatorId, "the crowded lane opens a nearest-first list of every matching contact");
        result.Check(menu.m_Events.OnItemSelected(menu.m_PulseRows, 0, 0, 0, 0, -1, -1) && !menu.m_PulsePicker.IsVisible(), "choosing a list row dismisses the picker");
        bool everyNameSelectable = overlay.m_PlacedSlots.Count() == example.Pulses.Count();
        foreach (int named : overlay.m_PlacedSlots)
        {
            vector nameCenter = overlay.m_LabelPositions[named];
            overlay.HitPulses(nameCenter[0] + overlay.m_Left, nameCenter[1] + overlay.m_Top, example, hits);
            if (hits.Count() != 1 || hits[0] != PredateProtocol.OperatorAt(named) || !Fits(overlay.m_Labels[named])) everyNameSelectable = false;
        }
        result.Check(everyNameSelectable, "all 100 map nameplates fit and each can be clicked directly");

        example.Pulses.Clear();
        array<int> bearings = {337, 337, 302, 302, 57, 57, 82, 269, 243, 243, 245, 351, 204, 195, 12, 14, 101, 102, 103, 105, 114, 327, 321, 320, 2, 8, 166, 177};
        for (int exampleSlot = 0; exampleSlot < bearings.Count(); exampleSlot++)
        {
            PredateBearingPulse typical = new PredateBearingPulse;
            typical.OperatorId = PredateProtocol.OperatorAt(exampleSlot);
            typical.Start = bearings[exampleSlot];
            typical.DistanceBand = 20 - exampleSlot / 2;
            example.Pulses.Insert(typical);
        }
        overlay.Draw(player, example, menu.m_MapScale, false);
        int displayed = overlay.m_PlacedSlots.Count();
        Print("[PredateMapTest] attached nameplates " + displayed + "/" + bearings.Count());
        result.Check(displayed == bearings.Count(), "the representative spread places every name directly on the map");
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
