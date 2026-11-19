class PredateHunterService
{
    static bool Load(PredateSession session)
    {
        PredateWorldState state = session.m_WorldState;
        if (state.HunterSchema == 0)
        {
            if (state.Hunter && state.Hunter.GoalRevision != 0) return PredatePersistence.Fail(session, "Hunter state has no schema");
            state.Hunter = new PredateHunterState;
            state.HunterSchema = 1;
        }
        if (state.HunterSchema != 1 || !state.Hunter || !state.Hunter.Validate(state.SimulationSeconds))
            return PredatePersistence.Fail(session, "Invalid strategic hunter state");
        return true;
    }

    static void Tick(PredateSession session, string playerId = "")
    {
        if (!session.m_HuntConfig.EnableStrategicHunter) return;
        eAIBase bot = session.m_HuntBot;
        if (!bot || !bot.IsAlive()) return;
        PredateWorldState state = session.m_WorldState;
        float now = state.SimulationSeconds;
        vector ownPosition = bot.GetPosition();
        bool combat = bot.PredateTacticalPriority(now);
        bool recover = bot.PredateOwnActionPriority();
        PredateHunterBrain.Step(state.Hunter, ownPosition, now, state.Observations, state.BotSignal, combat, recover, now < state.BotInterceptionUntil, state.PlayerLastDeathAt, playerId);
        if (combat || recover)
        {
            if (bot.m_PredateAppliedGoal != -1) bot.PredateReleaseRoute();
            return;
        }
        bot.PredateApplyRoute(state.Hunter);
    }
}
