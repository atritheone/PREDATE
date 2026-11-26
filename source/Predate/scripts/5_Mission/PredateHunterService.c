class PredateHunterService
{
    static bool Load(PredateSession session)
    {
        PredateWorldState state = session.m_WorldState;
        if (state.HunterSchema == 0)
        {
            if (state.Hunter && state.Hunter.GoalRevision != 0) return PredatePersistence.Fail(session, "Hunter state has no schema");
            state.Hunter = new PredateHunterState;
            state.Hunter.OwnerId = session.m_HuntProgress.Operator.Id;
            state.HunterSchema = 1;
        }
        if (state.HunterSchema != 1 || !state.Hunter || !state.Hunter.Validate(state.SimulationSeconds, session.m_HuntProgress.Operator.Id))
            return PredatePersistence.Fail(session, "Invalid strategic hunter state");
        return true;
    }
}
