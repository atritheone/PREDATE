modded class eAIBase
{
    bool m_PredateStrategicControl;
    bool m_PredateRouteActive;
    int m_PredateAppliedGoal = -1;

    override float GetThreatToSelf(bool ignoreLOS = false)
    {
        // Native FSM conflates remembered threat with a current engagement.
        // Keep the evidence intact, but allow travel while strategy owns it.
        if (m_PredateStrategicControl) return 0.0999;
        return super.GetThreatToSelf(ignoreLOS);
    }

    override bool EnforceLOS(float pDt)
    {
        bool visible = super.EnforceLOS(pDt);
        // A genuine visual contact immediately returns control to combat,
        // without waiting for the next strategic/session update.
        eAITarget target = GetTarget();
        if (visible && target && target.IsPlayer()) m_PredateStrategicControl = false;
        return visible;
    }

    bool PredateTacticalPriority(float now)
    {
        if (m_PredateLastVisualAt >= 0 && now - m_PredateLastVisualAt <= 12) return true;
        if (!m_PredateMemory) return false;
        foreach (PredateObservation observation : m_PredateMemory.Entries)
            if (observation.Source == PredateObservationSource.IMPACT && now - observation.ObservedAt <= 3) return true;
        return false;
    }

    bool PredateOwnActionPriority()
    {
        if (IsUnconscious() || IsRestrained() || IsInTransport() || IsFighting()) return true;
        if (GetActionManager().GetRunningAction()) return true;
        if (GetHealth01("", "Health") <= 0.5 || GetHealth01("", "Blood") < 0.7) return true;
        if (GetBleedingManagerServer() && GetBleedingManagerServer().GetBleedingSourcesCount() > 0) return true;
        return false;
    }

    string PredateRouteDiagnostic()
    {
        string value = "active=" + m_PredateRouteActive + "; simulationDisabled=" + GetIsSimulationDisabled() + "; command=" + GetCurrentCommandID();
        if (m_FSM) value += "; dormant=" + m_FSM.IsInState("Dormant") + "; traverse=" + m_FSM.IsInState("TraversingWaypoints") + "; idle=" + m_FSM.IsInState("Idle");
        return value;
    }

    void PredateReleaseRoute()
    {
        m_PredateStrategicControl = false;
        m_PredateRouteActive = false;
        m_PredateAppliedGoal = -1;
        GetGroup().ClearWaypoints();
    }

    void PredateApplyRoute(PredateHunterState state)
    {
        bool claim = !m_PredateStrategicControl;
        m_PredateStrategicControl = true;
        m_PredateRouteActive = state.HasGoal;
        // A restored body can receive a late native deactivation after the
        // route is assigned. Wake owned active travel without rewriting it.
        if (state.HasGoal && GetIsSimulationDisabled()) DisableSimulation(false);
        if (!claim && m_PredateAppliedGoal == state.GoalRevision) return;
        m_PredateAppliedGoal = state.GoalRevision;
        eAIGroup group = GetGroup();
        group.ClearWaypoints();
        group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);
        m_eAI_PositionOverrideTimeout = 0;
        eAI_LeaveCover();
        if (state.HasGoal)
        {
            vector destination = state.Goal;
            float size = GetGame().GetWorld().GetWorldSize();
            bool legal = destination[0] > 10 && destination[2] > 10 && destination[0] < size - 10 && destination[2] < size - 10;
            if (legal) legal = !GetGame().SurfaceIsSea(destination[0], destination[2]) && !GetGame().SurfaceIsPond(destination[0], destination[2]);
            if (legal)
            {
                destination[1] = GetGame().SurfaceY(destination[0], destination[2]);
                vector snappedGoal;
                if (PredateRoutePlanner.Snap(destination, 4, snappedGoal)) destination = snappedGoal;
                else legal = false;
            }
            if (legal)
            {
                vector intermediate;
                vector ownPosition = GetPosition();
                if (PredateRoutePlanner.Plan(ownPosition, destination, intermediate))
                {
                    group.AddWaypoint(intermediate);
                    group.AddWaypoint(destination);
                    group.SetCurrentWaypoint(intermediate);
                    eAI_ResetPathfinding();
                }
                else legal = false;
            }
            if (!legal)
            {
                m_PredateRouteActive = false;
                PredateHunterBrain.Hold(state, "reassess", "Proposed route leaves traversable map terrain");
            }
        }
        Print("[PredateHunter] " + state.Mode + "; reason=" + state.Reason + "; evidence=" + state.EvidenceKey + "; goalRevision=" + state.GoalRevision + "; goal=" + state.Goal + "; " + PredateRouteDiagnostic());
    }
}

modded class eAIState_TraversingWaypoints
{
    override int OnUpdate(float DeltaTime, int SimulationPrecision)
    {
        eAIGroup group = unit.GetGroup();
        if (unit.m_PredateRouteActive && group)
        {
            array<vector> waypoints = group.GetWaypoints();
            int index = group.m_CurrentWaypointIndex;
            if (index >= 0 && index < waypoints.Count() - 1)
            {
                vector ownPosition = unit.GetPosition();
                if (PredateHunterBrain.Distance(ownPosition, waypoints[index]) <= 12)
                {
                    // A cover bend is an area to traverse, not a one-metre
                    // precision destination inside foliage or a static obstacle.
                    m_PreviousWaypoint = waypoints[index];
                    group.m_CurrentWaypointIndex = index + 1;
                    m_Threshold = 1;
                }
            }
        }
        return super.OnUpdate(DeltaTime, SimulationPrecision);
    }
}

modded class eAIState_Dormant
{
    override int Guard()
    {
        // Active strategic travel must not depend on hidden human proximity.
        if (unit.m_PredateRouteActive) return eAITransition.FAIL;
        return super.Guard();
    }
}
