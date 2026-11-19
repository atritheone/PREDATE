    override void Pause()
    {
        super.Pause();
        if (!IsPaused()) return;
        if (m_Human) { m_Human.PredateBeginClockHold(); m_Human.DisableSimulation(true); }
        if (m_Bot) { m_Bot.PredateBeginClockHold(); m_Bot.DisableSimulation(true); }
    }
    override void Continue()
    {
        super.Continue();
        if (IsPaused()) return;
        if (m_Human) { m_Human.PredateEndClockHold(); m_Human.DisableSimulation(false); }
        if (m_Bot) { m_Bot.PredateEndClockHold(); m_Bot.DisableSimulation(false); }
    }
