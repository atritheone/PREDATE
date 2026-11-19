// Disposable-client fixture for the actual receiver controls and RPC replies.
class PredateRosterReceiverReport
{
    int Checks;
    int Failures;
    string First;
    string Second;
    int InRange;
    void Check(bool passed, string name)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateRosterReceiverTest] FAIL: " + name);
    }
}

class PredateRosterReceiverValidation
{
    ref PredateRosterReceiverReport m_Result = new PredateRosterReceiverReport;
    PredateTrackerMenu m_Menu;
    int m_Phase;
    string m_First;
    string m_Second;
    int m_Count;
    float m_NextPulse;
    int m_Bearing;
    vector m_Origin;
    int m_ClickAttempts;

    PredateBearingPulse Pulse(PredateState state, string id)
    {
        foreach (PredateBearingPulse pulse : state.Pulses)
            if (pulse.OperatorId == id) return pulse;
        return null;
    }

    void ClickPulse(string id, bool useLabel)
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player || !m_Menu) { m_Result.Check(false, "map closed before click"); Finish(); return; }
        PredateState state = player.m_PredateState;
        m_Menu.Update(0);
        PredateMapOverlay overlay = m_Menu.m_MapOverlay;
        PredateBearingPulse pulse = Pulse(state, id);
        int slot = PredateProtocol.s_OperatorIds.Find(id);
        TextWidget label = overlay.m_Labels[slot];
        if (!pulse || !label || !label.IsVisible() || !PredateReceiverValidation.Fits(label))
        {
            m_ClickAttempts++;
            if (m_ClickAttempts < 12)
            {
                GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(ClickPulse, 100, false, id, useLabel);
                return;
            }
            Print("[PredateRosterReceiverTest] arrow=" + overlay.m_PlayerPoint + "; viewport=" + overlay.m_Width + "x" + overlay.m_Height + "; arc radius=" + overlay.m_ArcRadii[slot]);
            m_Result.Check(false, "pulse label did not draw for " + id);
            Finish();
            return;
        }
        vector position = overlay.m_LabelPositions[slot];
        if (!useLabel) position = overlay.RadialPoint(pulse.Start + 7.5, overlay.m_ArcRadii[slot]);
        int clickX = Math.Round(overlay.m_Left + position[0]);
        int clickY = Math.Round(overlay.m_Top + position[1]);
        if (overlay.HitPulse(clickX, clickY, state) != id)
        {
            m_Result.Check(false, "pulse hit test did not find " + id);
            Finish();
            return;
        }
        m_Menu.m_Events.OnMouseButtonDown(m_Menu.m_Map, clickX, clickY, MouseState.LEFT);
        m_Menu.m_Events.OnMouseButtonUp(m_Menu.m_Map, clickX, clickY, MouseState.LEFT);
        m_Result.Check(true, "clicked " + id);
    }

    void SchedulePulse(string id, bool useLabel)
    {
        m_ClickAttempts = 0;
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(ClickPulse, 200, false, id, useLabel);
    }

    void ClearTarget()
    {
        m_Menu.OnClick(m_Menu.m_Select, 0, 0, 0);
    }

    bool OneBearing(PredateState state)
    {
        if (!state.HasPulse || !state.HasPulseOrigin || state.History.Count() != 1) return false;
        PredateReceiverSample entry = state.History[0];
        return entry.Origin == state.PulseOrigin && entry.Bearing == state.Bearing && entry.Error == state.BearingError;
    }

    bool Update(PredateState state)
    {
        if (!state || (state.AvailableOperators != 7 && state.AvailableOperators != PredateProtocol.ALL_OPERATORS)) return true;
        int rosterSize = 3;
        if (state.AvailableOperators == PredateProtocol.ALL_OPERATORS) rosterSize = PredateProtocol.MAX_OPERATORS;
        UIManager ui = GetGame().GetUIManager();
        if (m_Phase == 0 && !ui.GetMenu() && state.Pulses.Count() >= 2)
        {
            m_Menu = PredateTrackerMenu.Cast(ui.EnterScriptedMenu(PredateProtocol.MENU, null));
            m_Result.Check(m_Menu != null, "receiver opens in the operation");
            if (!m_Menu) return Finish();
            m_Menu.Update(0);
            m_Count = state.Pulses.Count();
            m_First = state.Pulses[0].OperatorId;
            m_Second = state.Pulses[1].OperatorId;
            m_Result.First = m_First;
            m_Result.Second = m_Second;
            m_Result.InRange = m_Count;
            bool countCovered = m_Count <= rosterSize;
            if (rosterSize == 30) countCovered = m_Count < rosterSize;
            m_Result.Check(countCovered && m_First != m_Second && PredateProtocol.DisplayName("raven-01") == "Raven", "only in-range rivals have named arcs");
            m_Result.Check(!state.Locked && state.History.Count() == 0, "untracked arcs show sectors without bearing history");
            m_Phase = 1;
            SchedulePulse(m_First, false);
        }
        else if (m_Phase == 1 && state.Locked && state.OperatorId == m_First && state.HasPulse)
        {
            m_Result.Check(OneBearing(state), "received the first selected bearing");
            m_NextPulse = state.NextPulse;
            m_Bearing = state.Bearing;
            m_Origin = state.PulseOrigin;
            PredateBearingPulse first = Pulse(state, m_First);
            m_Result.Check(first && first.Start == state.Bearing && state.Pulses.Count() == m_Count, "compass and map show the same sector");
            ClearTarget();
            m_Phase = 2;
        }
        else if (m_Phase == 2 && !state.Locked)
        {
            m_Result.Check(!state.HasPulse && state.History.Count() == 0 && state.Pulses.Count() == m_Count, "clearing tracking keeps the named arcs");
            m_Phase = 3;
            SchedulePulse(m_Second, true);
        }
        else if (m_Phase == 3 && state.Locked && state.OperatorId == m_Second && state.HasPulse)
        {
            m_Result.Check(OneBearing(state), "second rival has an independent first bearing");
            PredateBearingPulse second = Pulse(state, m_Second);
            m_Result.Check(second && second.Start == state.Bearing, "the second arc agrees with the second compass bearing");
            m_Phase = 4;
            SchedulePulse(m_First, false);
        }
        else if (m_Phase == 4 && state.Locked && state.OperatorId == m_First)
        {
            m_Result.Check(OneBearing(state) && state.Bearing == m_Bearing && state.PulseOrigin == m_Origin, "reselection retains the original sample");
            PredateBearingPulse cached = Pulse(state, m_First);
            m_Result.Check(cached && cached.Start == m_Bearing, "reselection keeps the same arc");
            m_Result.Check(state.NextPulse > 0 && state.NextPulse <= m_NextPulse && state.NextPulse <= 90, "reselection cannot bypass the 90-second cooldown");
            m_Menu.Close();
            m_Phase = 5;
        }
        else if (m_Phase == 5)
        {
            m_Result.Check(!ui.GetMenu() && GetGame().GetInput().HasGameFocus(INPUT_DEVICE_MOUSE), "closing the receiver returns game input");
            return Finish();
        }
        return true;
    }

    bool Finish()
    {
        string error;
        JsonFileLoader<PredateRosterReceiverReport>.SaveFile("$profile:RosterReceiverValidation.json", m_Result, error);
        Print("[PredateRosterReceiverTest] COMPLETE: checks=" + m_Result.Checks + "; failures=" + m_Result.Failures);
        return false;
    }
}
