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
    static const int ELIMINATION = 0xFF77E49C;
    static const int DEATH = 0xFFFF6666;
    static const int ROWS = 6;
    ref array<TextWidget> m_Rows = new array<TextWidget>;
    ref array<ref PredateHuntNotice> m_Notices = new array<ref PredateHuntNotice>;
    Widget m_Root;
    PlayerBase m_Player;
    bool m_WasHunted;
    string m_Target;
    bool m_WasClose;
    bool m_HadBearing;
    bool m_DeathObserved;

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
        if (m_Notices.Count() > ROWS) m_Notices.RemoveOrdered(ROWS);
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
        if (player.m_PredateKillNotices != 0)
        {
            for (int slot = 0; slot < PredateProtocol.MAX_OPERATORS; slot++)
                if ((player.m_PredateKillNotices & (1 << slot)) != 0)
                    Post(PredateProtocol.DisplayName(PredateProtocol.OperatorAt(slot)) + " is KIA.", ELIMINATION);
            player.m_PredateKillNotices = 0;
        }
        if (player.m_PredateSupplyNotice)
        {
            Post("Supplies replenished.");
            player.m_PredateSupplyNotice = false;
        }
        bool dead = !player.IsAlive() || (state && state.NeedsRedeploy);
        if (dead && !m_DeathObserved) Post("You are KIA.", DEATH);
        if (state || dead) m_DeathObserved = dead;
        if (dead || !state) return;
        string target;
        if (state.Locked && state.Available) target = state.OperatorId;
        if (target != "")
        {
            string name = PredateProtocol.DisplayName(target);
            if (state.CloseApproach && (!m_WasClose || target != m_Target)) Post(name + " close. Hunt by sight and sound.");
            else if (target != m_Target) Post("Tracking " + name + ".");
            else if (!state.CloseApproach && !state.HasPulse && m_HadBearing) Post("Lost " + name + "’s trail.");
        }
        m_Target = target;
        m_WasClose = state.CloseApproach;
        m_HadBearing = state.HasPulse;
        if (state.Intercepted && !m_WasHunted) Post("You are being hunted.", HUNTED);
        m_WasHunted = state.Intercepted;
    }

    void Update(PlayerBase player, PredateState state, float timeslice)
    {
        for (int index = m_Notices.Count() - 1; index >= 0; index--)
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
        if (m_Root) m_Root.Show(visible);
    }

    void Close()
    {
        if (m_Root) m_Root.Unlink();
    }
}
