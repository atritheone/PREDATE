// Injected only into an isolated server mission by engine_physics_pause.py.
class PredatePhysicsPauseReport
{
    int Checks;
    int Failures;
    vector InitialPosition;
    vector BeforePause;
    vector HeldPosition;
    vector AfterPause;
    vector ResumedPosition;
    float HeldSimulationTime;
    int DynamicBodies;
    int HeldBodies;
    int SessionHeldBodies;
    bool SystemTimerWhileHeld;
    bool SystemTimerAfterResume;
    bool ImmediateHousekeepingWhileHeld;
}

class PredateBodyPauseEntry
{
    IEntity BodyEntity;
    vector LinearVelocity;
    vector AngularVelocity;
    bool WasActive;
}

class PredatePhysicsValidationMission : PredateValidationMission
{
    ItemBase m_PhysicsBody;
    ref PredatePhysicsPauseReport m_PhysicsReport = new PredatePhysicsPauseReport;
    ref array<ref PredateBodyPauseEntry> m_HeldBodies = new array<ref PredateBodyPauseEntry>;
    float m_PhysicsHoldWallStart;
    bool m_PhysicsSystemTimerFired;
    bool m_PhysicsHousekeepingQueued;
    bool m_PhysicsHousekeepingRan;

    void HoldPhysicsBodies()
    {
        int count = PhysicsWorld.GetNumDynamicBodies(m_PhysicsBody);
        m_PhysicsReport.DynamicBodies = count;
        // Capture the complete list before changing its membership.
        for (int i = 0; i < count; i++)
        {
            IEntity entity = PhysicsWorld.GetDynamicBody(m_PhysicsBody, i);
            if (!entity || Man.Cast(entity)) continue;
            Physics body = entity.GetPhysics();
            if (!body || !body.IsDynamic() || body.GetSimulationState() != SimulationState.SIMULATION) continue;
            PredateBodyPauseEntry entry = new PredateBodyPauseEntry;
            vector linear = GetVelocity(entity);
            vector angular = body.GetAngularVelocity();
            bool active = body.IsActive();
            entry.BodyEntity = entity;
            entry.LinearVelocity = linear;
            entry.AngularVelocity = angular;
            entry.WasActive = active;
            m_HeldBodies.Insert(entry);
        }
        foreach (PredateBodyPauseEntry held : m_HeldBodies)
        {
            Physics physics = held.BodyEntity.GetPhysics();
            if (physics) physics.ChangeSimulationState(SimulationState.COLLISION);
        }
        m_PhysicsReport.HeldBodies = m_HeldBodies.Count();
    }

    void ResumePhysicsBodies()
    {
        foreach (PredateBodyPauseEntry entry : m_HeldBodies)
        {
            if (!entry.BodyEntity) continue;
            Physics body = entry.BodyEntity.GetPhysics();
            if (!body) continue;
            body.ChangeSimulationState(SimulationState.SIMULATION);
            SetVelocity(entry.BodyEntity, entry.LinearVelocity);
            dBodySetAngularVelocity(entry.BodyEntity, entry.AngularVelocity);
            if (entry.WasActive) body.SetActive(ActiveState.ACTIVE);
            else body.SetActive(ActiveState.INACTIVE);
        }
        m_HeldBodies.Clear();
    }

    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PreparePhysicsProbe, 1000, false);
    }

    void CheckPhysics(bool condition, string label)
    {
        m_PhysicsReport.Checks++;
        if (condition) return;
        m_PhysicsReport.Failures++;
        Print("[PredatePhysicsPause] FAIL: " + label);
    }

    void PreparePhysicsProbe()
    {
        if (!m_PredateSession.m_HuntReady || !m_PredateSession.m_ActorsActive)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PreparePhysicsProbe, 1000, false);
            return;
        }
        vector position = m_PredateSession.m_HuntBot.GetPosition() + "3 25 0";
        m_PhysicsBody = ItemBase.Cast(GetGame().CreateObjectEx("WoodenStick", position, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        CheckPhysics(m_PhysicsBody != null, "dynamic physics fixture created");
        if (!m_PhysicsBody) { FinishPhysics(); return; }
        m_PhysicsBody.CreateDynamicPhysics(PhxInteractionLayers.DYNAMICITEM);
        m_PhysicsBody.SetDynamicPhysicsLifeTime(30);
        SetVelocity(m_PhysicsBody, "0 -1 0");
        vector initial = m_PhysicsBody.GetPosition();
        m_PhysicsReport.InitialPosition = initial;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BeginPhysicsHold, 400, false);
    }

    void BeginPhysicsHold()
    {
        vector before = m_PhysicsBody.GetPosition();
        m_PhysicsReport.BeforePause = before;
        CheckPhysics(vector.Distance(before, m_PhysicsReport.InitialPosition) > 0.1, "native body is genuinely falling before pause");
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PhysicsSystemTimer, 1000, false);
        m_PredateSession.SetPaused(true);
        /*PREDATE_HOLD_BODIES*/
        CapturePhysicsHold();
    }

    void PhysicsSystemTimer() { m_PhysicsSystemTimerFired = true; }
    void PhysicsHousekeeping() { m_PhysicsHousekeepingRan = true; }

    void CapturePhysicsHold()
    {
        vector held = m_PhysicsBody.GetPosition();
        float clock = m_PredateSession.m_WorldState.SimulationSeconds;
        m_PhysicsReport.HeldPosition = held;
        m_PhysicsReport.HeldSimulationTime = clock;
        m_PhysicsReport.SessionHeldBodies = m_PredateSession.m_PhysicsHold.m_Entries.Count();
        float wallStart = GetGame().GetTickTime();
        m_PhysicsHoldWallStart = wallStart;
        PredateServerPauseDriver.Poll.Insert(PollPhysicsHold);
    }

    void PollPhysicsHold(float timeslice)
    {
        if (!m_PhysicsHousekeepingQueued && GetGame().GetTickTime() - m_PhysicsHoldWallStart >= 1)
        {
            m_PhysicsHousekeepingQueued = true;
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(PhysicsHousekeeping);
        }
        if (GetGame().GetTickTime() - m_PhysicsHoldWallStart < /*PREDATE_HOLD_SECONDS*/) return;
        PredateServerPauseDriver.Poll.Remove(PollPhysicsHold);
        VerifyPhysicsHold();
    }

    void VerifyPhysicsHold()
    {
        vector after = m_PhysicsBody.GetPosition();
        m_PhysicsReport.AfterPause = after;
        m_PhysicsReport.SystemTimerWhileHeld = m_PhysicsSystemTimerFired;
        m_PhysicsReport.ImmediateHousekeepingWhileHeld = m_PhysicsHousekeepingRan;
        CheckPhysics(m_PredateSession.m_Paused && m_PredateSession.m_WorldState.SimulationSeconds == m_PhysicsReport.HeldSimulationTime, "session clock stays held during the real interval");
        CheckPhysics(vector.Distance(after, m_PhysicsReport.HeldPosition) < 0.001, "unheld native dynamic physics stays frozen with the server");
        CheckPhysics(!m_PhysicsSystemTimerFired, "system-queue callback does not consume paused time");
        CheckPhysics(m_PhysicsHousekeepingRan, "immediate engine housekeeping can still drain while held");
        m_PredateSession.SetPaused(false);
        /*PREDATE_RESUME_BODIES*/
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(VerifyPhysicsResume, 400, false);
    }

    void VerifyPhysicsResume()
    {
        vector resumed = m_PhysicsBody.GetPosition();
        m_PhysicsReport.ResumedPosition = resumed;
        CheckPhysics(vector.Distance(resumed, m_PhysicsReport.AfterPause) > 0.1, "native dynamic body moves again after resume");
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(VerifyPhysicsTimer, 1000, false);
    }

    void VerifyPhysicsTimer()
    {
        m_PhysicsReport.SystemTimerAfterResume = m_PhysicsSystemTimerFired;
        CheckPhysics(m_PhysicsSystemTimerFired, "held system callback resumes with its remaining delay");
        FinishPhysics();
    }

    void FinishPhysics()
    {
        if (m_PredateSession.m_Paused) m_PredateSession.SetPaused(false);
        string error;
        JsonFileLoader<PredatePhysicsPauseReport>.SaveFile("$profile:PredatePhysicsPauseResult.json", m_PhysicsReport, error);
        Print("[PredatePhysicsPause] COMPLETE: checks=" + m_PhysicsReport.Checks + "; failures=" + m_PhysicsReport.Failures);
    }
}
