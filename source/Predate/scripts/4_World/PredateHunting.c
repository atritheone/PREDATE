modded class eAIBase
{
    bool m_PredateStrategicControl;
    bool m_PredateRouteActive;
    int m_PredateAppliedGoal = -1;
    int m_PredateLookStep;
    int m_PredateNextLookAt;
    vector m_PredateWatchPoint;
    float m_PredateWatchUntil;
    protected bool m_PredateFinalApproachPace;
    protected bool m_PredateFinalApproachStance;
    protected int m_PredateTravelLimit;
    protected int m_PredateThreatLimit;

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
        if (visible && target && m_PredateVisualTarget && target.GetEntity() == m_PredateVisualTarget)
        {
            m_PredateStrategicControl = false;
            PredateSetFinalApproach(false);
        }
        return visible;
    }

    void PredateSetFinalApproach(bool careful)
    {
        if (careful == m_PredateFinalApproachPace && (!careful || m_eAI_SpeedLimitPreference <= 1)) return;
        m_PredateFinalApproachPace = careful;
        if (careful)
        {
            m_PredateTravelLimit = m_eAI_SpeedLimitPreference;
            m_PredateThreatLimit = GetMovementSpeedLimitUnderThreat();
            SetMovementSpeedLimits(Math.Min(m_PredateTravelLimit, 1), Math.Min(m_PredateThreatLimit, 1));
        }
        else SetMovementSpeedLimits(m_PredateTravelLimit, m_PredateThreatLimit);
    }

    void PredateSetFinalApproachStance(bool active)
    {
        m_PredateFinalApproachStance = active;
        if (!active) return;
        // Expansion's movement command still checks clearance before raising the stance.
        if (m_eAI_StancePreference == DayZPlayerConstants.STANCEIDX_PRONE || (m_eAI_StancePreference == -1 && eAI_GetStance() == DayZPlayerConstants.STANCEIDX_PRONE))
            OverrideStance(DayZPlayerConstants.STANCEIDX_CROUCH);
    }

    override bool OverrideStance(int pStanceIdx, bool force = false)
    {
        if (m_PredateFinalApproachStance && pStanceIdx == DayZPlayerConstants.STANCEIDX_PRONE)
            return super.OverrideStance(DayZPlayerConstants.STANCEIDX_CROUCH, force);
        return super.OverrideStance(pStanceIdx, force);
    }

    static float PredateScanOffset(int step, float angle)
    {
        if (step == 1) return -angle;
        if (step == 3) return angle;
        return 0;
    }

    bool PredateTacticalPriority(float now)
    {
        if (m_PredateLastVisualAt >= 0 && now - m_PredateLastVisualAt <= 2) return true;
        if (!m_PredateMemory) return false;
        foreach (PredateObservation observation : m_PredateMemory.Entries)
            if (observation.Source == PredateObservationSource.IMPACT && now - observation.ObservedAt <= 3) return true;
        return false;
    }

    bool PredateOwnActionPriority()
    {
        if (IsUnconscious() || IsRestrained() || IsInTransport() || IsFighting()) return true;
        if (GetActionManager().GetRunningAction()) return true;
        return IsBleeding() && GetBandageToUse() != null;
    }

    override bool eAI_UpdateLookDirectionPreference()
    {
        if (!m_PredateCombatEnabled || m_eAI_IsFightingFSM || GetCurrentCommandID() != DayZPlayerConstants.COMMANDID_MOVE || !m_eAI_CommandMove || m_eAI_CommandMove.GetBlockingObject())
            return super.eAI_UpdateLookDirectionPreference();

        if (!m_PredateVisualTarget && m_PredateSimulationTime < m_PredateGlanceUntil)
        {
            vector glance = m_PredateGlancePoint;
            vector glanceEyes = GetBonePositionWS(GetBoneIndexByName("Head"));
            glance[1] = glanceEyes[1];
            if (vector.DistanceSq(glance, GetPosition()) > 1)
            {
                LookAtPosition(glance);
                AimAtDirection("0 0 1");
                return true;
            }
        }
        if (!m_PredateStrategicControl) return super.eAI_UpdateLookDirectionPreference();

        int now = GetGame().GetTime();
        if (now >= m_PredateNextLookAt)
        {
            m_PredateLookStep = (m_PredateLookStep + 1) % 4;
            m_PredateNextLookAt = now + Math.RandomIntInclusive(900, 1250);
        }

        float heading;
        if (m_PredateSimulationTime < m_PredateWatchUntil)
        {
            vector eyes = GetBonePositionWS(GetBoneIndexByName("Head"));
            vector toward = m_PredateWatchPoint - eyes;
            if (toward.LengthSq() > 1)
            {
                vector angles = toward.VectorToAngles();
                float scanOffset;
                if (m_PredateFinalApproachPace) scanOffset = PredateScanOffset(m_PredateLookStep, 45);
                float scanHeading = angles[0] + scanOffset;
                float relative = ExpansionMath.AngleDiff2(GetOrientation()[0], scanHeading);
                if (Math.AbsFloat(relative) > 60)
                {
                    vector point = m_PredateWatchPoint;
                    if (scanOffset != 0) point = eyes + PredateInformationMath.BearingDirection(scanHeading) * toward.Length();
                    point[1] = eyes[1];
                    LookAtPosition(point);
                    AimAtPosition(point);
                }
                else
                {
                    LookAtDirection(Vector(relative, Math.Clamp(angles[1], -25, 25), 0).AnglesToVector());
                    AimAtDirection("0 0 1");
                }
                return true;
            }
        }
        if (m_PredateRouteActive)
        {
            vector toWaypoint = m_eAI_CommandMove.GetWaypoint() - GetPosition();
            if (toWaypoint.LengthSq() > 1)
                heading = ExpansionMath.AngleDiff2(GetOrientation()[0], toWaypoint.VectorToAngles()[0]);
        }
        float sideLook = 68;
        if (m_PredateRouteActive) sideLook = 55;
        heading += PredateScanOffset(m_PredateLookStep, sideLook);
        vector gaze = Vector(Math.Clamp(heading, -75, 75), 0, 0);
        LookAtDirection(gaze.AnglesToVector());
        AimAtDirection("0 0 1");
        return true;
    }

    string PredateRouteDiagnostic()
    {
        string value = "active=" + m_PredateRouteActive + "; simulationDisabled=" + GetIsSimulationDisabled() + "; command=" + GetCurrentCommandID();
        if (m_FSM) value += "; dormant=" + m_FSM.IsInState("Dormant") + "; traverse=" + m_FSM.IsInState("TraversingWaypoints") + "; idle=" + m_FSM.IsInState("Idle");
        return value;
    }

    void PredateReleaseRoute()
    {
        if (PredateServerDiagnostics.TraceOperators && m_PredateAppliedGoal >= 0)
            Print("[PredateRoute] release; operator=" + m_PredateOperatorId + "; simulation=" + m_PredateSimulationTime + "; revision=" + m_PredateAppliedGoal);
        m_PredateStrategicControl = false;
        m_PredateRouteActive = false;
        m_PredateAppliedGoal = -1;
        m_PredateWatchUntil = 0;
        m_PredateGlanceUntil = 0;
        GetGroup().ClearWaypoints();
    }

    void PredateApplyRoute(PredateHunterState state)
    {
        m_PredateStrategicControl = true;
        m_PredateRouteActive = state.HasGoal;
        m_PredateWatchPoint = state.WatchPoint;
        m_PredateWatchUntil = state.WatchUntil;
        // Observation stops need the same active perception as travel.
        if (GetIsSimulationDisabled()) DisableSimulation(false);
        if (m_PredateAppliedGoal == state.GoalRevision) return;
        int previousRevision = m_PredateAppliedGoal;
        m_PredateLookStep = 0;
        m_PredateNextLookAt = GetGame().GetTime() + Math.RandomIntInclusive(700, 1200);
        eAIGroup group = GetGroup();
        group.ClearWaypoints();
        group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);
        m_eAI_PositionOverrideTimeout = 0;
        eAI_LeaveCover();
        if (state.HasGoal)
        {
            vector destination = state.RouteDestination();
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
                if (PredateRoutePlanner.Plan(ownPosition, destination, intermediate, state))
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
        m_PredateAppliedGoal = state.GoalRevision;
        if (PredateServerDiagnostics.TraceOperators)
            Print("[PredateHunter] " + state.Mode + "; reason=" + state.Reason + "; evidence=" + state.EvidenceKey + "; goalRevision=" + state.GoalRevision + "; goal=" + state.Goal + "; operator=" + state.OwnerId + "; simulation=" + m_PredateSimulationTime + "; detour=" + state.HasDetour + "; previousRevision=" + previousRevision + "; actor=" + this + "; " + PredateRouteDiagnostic());
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
        // Independent Operators scan during both travel and observation stops.
        if (unit.m_PredateStrategicControl || unit.m_PredateRouteActive) return eAITransition.FAIL;
        return super.Guard();
    }
}
