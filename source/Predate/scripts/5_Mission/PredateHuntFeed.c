class PredateHuntNotice
{
    string Text;
    int Color;
    float Remaining;
}

class PredateHuntFeed
{
    static const int NORMAL = 0xFFFFFFFF;
    static const int HUNTED = 0xFFFFB253;
    static const int ROWS = 6;
    ref array<TextWidget> m_Rows = new array<TextWidget>;
    ref array<ref PredateHuntNotice> m_Notices = new array<ref PredateHuntNotice>;
    Widget m_Root;
    PlayerBase m_Player;
    bool m_WasHunted;
    string m_Target;
    int m_TrackingChoice;
    bool m_HadBearing;
    bool m_Visible = true;

    void PredateHuntFeed()
    {
        m_Root = GetGame().GetWorkspace().CreateWidgets("Predate/gui/hunt_feed.layout");
        if (!m_Root) return;
        for (int index = 0; index < ROWS; index++)
            m_Rows.Insert(TextWidget.Cast(m_Root.FindAnyWidget("HuntNotice" + index)));
    }

    void Post(string text, int color = NORMAL)
    {
        PredateHuntNotice notice = new PredateHuntNotice;
        notice.Text = text;
        notice.Color = color;
        notice.Remaining = 6;
        m_Notices.InsertAt(notice, 0);
        if (m_Notices.Count() > ROWS)
        {
            int victim = ROWS;
            while (victim > 0 && m_Notices[victim].Color == HUNTED) victim--;
            m_Notices.RemoveOrdered(victim);
        }
    }

    void PostLostSignal(string target)
    {
        string text = "Lost " + PredateProtocol.DisplayName(target) + "'s signal.";
        foreach (PredateHuntNotice notice : m_Notices)
            if (notice.Text == text) return;
        Post(text);
    }

    void Observe(PlayerBase player, PredateState state)
    {
        if (!player) return;
        if (player != m_Player)
        {
            m_Player = player;
            m_Notices.Clear();
            m_WasHunted = false;
            m_Target = "";
        }
        int priorSlot = PredateProtocol.OperatorSlot(m_Target);
        bool killedTarget = priorSlot >= 0 && player.m_PredateKillNotices.Find(priorSlot) >= 0;
        if (player.m_PredateKillNotices.Count() > 0)
        {
            foreach (int slot : player.m_PredateKillNotices)
            {
                player.m_PredateLostSignalNotices.RemoveItem(slot);
                Post(PredateProtocol.DisplayName(PredateProtocol.OperatorAt(slot)) + " is KIA.");
            }
            player.m_PredateKillNotices.Clear();
        }
        if (player.m_PredateSupplyNotice)
        {
            Post("Supplies replenished.");
            player.m_PredateSupplyNotice = false;
        }
        foreach (int lostSlot : player.m_PredateLostSignalNotices)
        {
            string lostTarget = PredateProtocol.OperatorAt(lostSlot);
            PostLostSignal(lostTarget);
            if (lostTarget == m_Target) m_HadBearing = false;
        }
        player.m_PredateLostSignalNotices.Clear();
        if (!player.IsAlive() || !state || state.NeedsRedeploy) return;
        if (player.m_PredateFootprintNotice != "")
        {
            Post(player.m_PredateFootprintNotice, 0xFFBFC8CB);
            player.m_PredateFootprintNotice = "";
        }
        string target;
        if (state.Locked && state.Available) target = state.OperatorId;
        if (m_Target != "" && target != m_Target && !killedTarget && state.TrackingChoice == m_TrackingChoice)
            PostLostSignal(m_Target);
        if (target != "")
        {
            string name = PredateProtocol.DisplayName(target);
            if (target != m_Target) Post("Tracking " + name + ".");
            else if (!state.CloseApproach && !state.HasPulse && m_HadBearing && !killedTarget) PostLostSignal(target);
        }
        m_Target = target;
        m_TrackingChoice = state.TrackingChoice;
        m_HadBearing = state.HasPulse;
        if (state.Intercepted && !m_WasHunted) Post("You are being hunted.", HUNTED);
        m_WasHunted = state.Intercepted;
    }

    void Update(PlayerBase player, PredateState state, float timeslice)
    {
        for (int index = m_Notices.Count() - 1; m_Visible && index >= 0; index--)
        {
            m_Notices[index].Remaining -= timeslice;
            if (m_Notices[index].Remaining <= 0) m_Notices.RemoveOrdered(index);
        }
        Observe(player, state);
        for (int row = 0; row < m_Rows.Count(); row++)
        {
            TextWidget label = m_Rows[row];
            if (!label) continue;
            label.Show(row < m_Notices.Count());
            if (row >= m_Notices.Count()) continue;
            PredateHuntNotice notice = m_Notices[row];
            label.SetText(notice.Text);
            label.SetColor(notice.Color);
            label.SetAlpha(Math.Min(1, notice.Remaining / 0.6));
        }
    }

    void Show(bool visible)
    {
        m_Visible = visible;
        if (m_Root) m_Root.Show(visible);
    }

    void Close()
    {
        if (m_Root) m_Root.Unlink();
    }
}
