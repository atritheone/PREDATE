class PredateAcquisitionMeasurement
{
    int Range;
    bool SideExposure;
    bool Night;
    bool Peer;
    bool ClearOpening;
    bool ClearTravelLane;
    bool StartedWithoutTarget;
    bool StartedMoving;
    float SightLimit;
    int SightSamples;
    int BodyVisibleSamples;
    float LongestInViewSeconds;
    float FirstCueSeconds = -1;
    float ConfirmedSeconds = -1;
    float Movement;
    float MovementAtConfirmation;
    float MovementSpeedAtConfirmation;
}

class PredateAcquisitionReport
{
    ref array<ref PredateAcquisitionMeasurement> Trials = new array<ref PredateAcquisitionMeasurement>;
}

class PredateAcquisitionMission : MissionServer
{
    protected eAIBase m_AcquisitionObserver;
    protected eAIBase m_AcquisitionPeer;
    protected PlayerBase m_AcquisitionPlayer;
    protected PlayerBase m_AcquisitionTarget;
    protected ref PredateAcquisitionReport m_AcquisitionReport = new PredateAcquisitionReport;
    protected ref PredateAcquisitionMeasurement m_AcquisitionMeasurement;
    protected ref PredateObservationMemory m_AcquisitionMemory;
    protected ref PredateCombatRuntimeTests m_AcquisitionGeometry = new PredateCombatRuntimeTests;
    protected vector m_AcquisitionStart;
    protected vector m_AcquisitionTargetPoint;
    protected float m_AcquisitionAt;
    protected float m_AcquisitionNext;
    protected int m_AcquisitionStage;
    protected bool m_AcquisitionDone;
    protected float m_LastSightSample;
    protected float m_ExposureStarted = -1;

    override void OnInit()
    {
        Math.Randomize(13215);
        super.OnInit();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_AcquisitionDone || !m_PredateSession.m_ActorsActive || !m_PredateSession.m_HuntBot) return;
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        if (now < m_AcquisitionNext) return;
        if (!m_AcquisitionPlayer)
        {
            array<Man> participants = {};
            GetGame().GetPlayers(participants);
            foreach (Man participant : participants)
            {
                if (!participant.GetIdentity()) continue;
                m_AcquisitionPlayer = PlayerBase.Cast(participant);
                break;
            }
            if (!m_AcquisitionPlayer) return;
            m_AcquisitionObserver = m_PredateSession.m_HuntBot;
            m_AcquisitionPeer = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", "12000 200 12600", ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
            if (!m_AcquisitionPeer) return;
            m_AcquisitionPeer.SetGroup(eAIGroup.CreateGroup(new eAIFactionRaiders));
            m_AcquisitionPeer.SetMovementSpeedLimits(0, 0);
            m_AcquisitionPeer.SetAllowDamage(false);
            m_AcquisitionPlayer.SetAllowDamage(false);
            m_AcquisitionObserver.SetAllowDamage(false);
            m_AcquisitionObserver.m_PredateProbeShooter = true;
            m_AcquisitionObserver.m_PredateProbeCeaseFire = true;
            m_AcquisitionObserver.m_PredateEliteAI = m_PredateSession.m_HuntConfig.EliteAI;
            m_AcquisitionNext = now + 2;
            return;
        }
        if (m_AcquisitionStage == 0)
        {
            BeginAcquisition(now);
            return;
        }
        if (m_AcquisitionStage == 1)
        {
            StartTravel(now);
            return;
        }
        if (m_AcquisitionStage == 2)
        {
            bool moving = m_AcquisitionObserver.Expansion_GetMovementSpeed() >= 1 && PredateHunterBrain.Distance(m_AcquisitionStart, m_AcquisitionObserver.GetPosition()) >= 1;
            if (moving || now - m_AcquisitionAt >= 5) StartExposure(now, moving);
            return;
        }
        if (now > m_LastSightSample && m_AcquisitionMeasurement.ConfirmedSeconds < 0) SampleExposure(now);
        PredateObservation clue = m_AcquisitionMemory.Latest("", PredateObservationSource.GLIMPSE, now);
        if (clue && m_AcquisitionMeasurement.FirstCueSeconds < 0) m_AcquisitionMeasurement.FirstCueSeconds = clue.ObservedAt - m_AcquisitionAt;
        m_AcquisitionMeasurement.Movement = PredateHunterBrain.Distance(m_AcquisitionStart, m_AcquisitionObserver.GetPosition());
        if (m_AcquisitionObserver.m_PredateVisualTarget == m_AcquisitionTarget && m_AcquisitionMeasurement.ConfirmedSeconds < 0)
        {
            m_AcquisitionMeasurement.ConfirmedSeconds = now - m_AcquisitionAt;
            m_AcquisitionMeasurement.MovementAtConfirmation = m_AcquisitionMeasurement.Movement;
            m_AcquisitionMeasurement.MovementSpeedAtConfirmation = m_AcquisitionObserver.Expansion_GetMovementSpeed();
        }
        if (now - m_AcquisitionAt >= 12 || (m_AcquisitionMeasurement.ConfirmedSeconds >= 0 && m_AcquisitionMeasurement.Movement >= 2))
            FinishAcquisition(now);
    }

    void BeginAcquisition(float now)
    {
        int trial = m_AcquisitionReport.Trials.Count();
        if (trial >= 24)
        {
            m_AcquisitionDone = true;
            string error;
            JsonFileLoader<PredateAcquisitionReport>.SaveFile("$profile:EliteAcquisition.json", m_AcquisitionReport, error);
            return;
        }
        m_AcquisitionMeasurement = new PredateAcquisitionMeasurement;
        array<int> ranges = {150, 300, 550};
        m_AcquisitionMeasurement.Range = ranges[trial % 3];
        int variation = trial / 3;
        m_AcquisitionMeasurement.SideExposure = (variation & 1) != 0;
        m_AcquisitionMeasurement.Night = (variation & 2) != 0;
        m_AcquisitionMeasurement.Peer = (variation & 4) != 0;
        m_AcquisitionTarget = m_AcquisitionPlayer;
        if (m_AcquisitionMeasurement.Peer) m_AcquisitionTarget = m_AcquisitionPeer;

        vector prepare = "11700 0 12200";
        m_AcquisitionObserver.SetPosition(m_PredateSession.PredateGround(prepare));
        m_AcquisitionPeer.SetPosition(m_PredateSession.PredateGround("10500 0 12100"));
        m_AcquisitionPlayer.SetPosition(m_PredateSession.PredateGround("10500 0 12200"));
        m_AcquisitionObserver.SetMovementSpeedLimits(0, 0);
        m_AcquisitionObserver.PredateReleaseRoute();
        m_AcquisitionObserver.m_PredateProbeSpectator = null;
        if (m_AcquisitionMeasurement.Peer) m_AcquisitionObserver.m_PredateProbeSpectator = m_AcquisitionPlayer;
        int year, month, day, hour, minute;
        GetGame().GetWorld().GetDate(year, month, day, hour, minute);
        hour = 12;
        if (m_AcquisitionMeasurement.Night) hour = 0;
        GetGame().GetWorld().SetDate(year, month, day, hour, 0);
        m_AcquisitionObserver.Expansion_UpdateVisibility(true);
        m_AcquisitionStage = 1;
        m_AcquisitionNext = now + 1.75;
    }

    void StartTravel(float now)
    {
        float heading;
        m_AcquisitionMeasurement.ClearTravelLane = FindTravelLane(heading);
        array<ref eAITarget> oldTargets = m_AcquisitionObserver.GetTargets();
        for (int i = oldTargets.Count() - 1; i >= 0; i--) oldTargets[i].Remove();
        m_AcquisitionMemory = new PredateObservationMemory;
        m_AcquisitionMemory.OwnerId = "raven-01";
        array<ref PredateObservation> pending = {};
        m_AcquisitionObserver.PredateBindInformation(false, "raven-01", m_AcquisitionMemory, pending, now);
        m_AcquisitionObserver.PredateBindInformation(true, "raven-01", m_AcquisitionMemory, pending, now);
        m_AcquisitionObserver.SetPosition(m_AcquisitionStart);
        vector direction = PredateInformationMath.BearingDirection(heading);
        m_AcquisitionObserver.SetDirection(direction);
        m_AcquisitionObserver.SetMovementSpeedLimits(2, 3);
        m_AcquisitionObserver.DisableSimulation(false);
        PredateHunterState walking = new PredateHunterState;
        walking.GoalRevision = m_AcquisitionReport.Trials.Count() * 2;
        PredateHunterBrain.Goal(walking, m_AcquisitionStart, m_AcquisitionStart + direction * 65.0, now, "scout", "Authored travel leg", "");
        m_AcquisitionObserver.PredateApplyRoute(walking);
        m_AcquisitionAt = now;
        m_AcquisitionStage = 2;
        m_AcquisitionNext = now;
    }

    bool FindTravelLane(out float heading)
    {
        array<vector> starts = {"11700 0 12200", "11950 0 12400", "4700 0 2200"};
        float distance = m_AcquisitionMeasurement.Range;
        foreach (vector start : starts)
        {
            m_AcquisitionStart = m_PredateSession.PredateGround(start);
            for (int alternative = 0; alternative < 24; alternative++)
            {
                float bearing = PredateMath.WrapHeading(60 + alternative * 15);
                m_AcquisitionTargetPoint = m_PredateSession.PredateGround(m_AcquisitionStart + PredateInformationMath.BearingDirection(bearing) * distance);
                if (!PredateRoutePlanner.Land(m_AcquisitionTargetPoint)) continue;
                heading = bearing;
                if (!m_AcquisitionMeasurement.SideExposure)
                {
                    if (ClearTravelLane(heading)) return true;
                    continue;
                }
                heading = PredateMath.WrapHeading(bearing + 70);
                if (ClearTravelLane(heading)) return true;
                heading = PredateMath.WrapHeading(bearing - 70);
                if (ClearTravelLane(heading)) return true;
            }
        }
        return false;
    }

    bool ClearTravelLane(float heading)
    {
        vector direction = PredateInformationMath.BearingDirection(heading);
        for (int step = 0; step <= 4; step++)
        {
            vector pathPoint = m_PredateSession.PredateGround(m_AcquisitionStart + direction * (step * 2.0));
            if (!PredateRoutePlanner.Land(pathPoint)) return false;
            if (!m_AcquisitionGeometry.ClearLane(pathPoint + "0 1.7 0", m_AcquisitionTargetPoint + "0 1.5 0", m_AcquisitionObserver)) return false;
        }
        return true;
    }

    void StartExposure(float now, bool moving)
    {
        m_AcquisitionMeasurement.StartedWithoutTarget = m_AcquisitionObserver.GetTarget() == null;
        m_AcquisitionMeasurement.StartedMoving = moving;
        m_AcquisitionStart = m_AcquisitionObserver.GetPosition();
        vector eyes = m_AcquisitionObserver.GetBonePositionWS(m_AcquisitionObserver.GetBoneIndexByName("Head"));
        m_AcquisitionMeasurement.ClearOpening = m_AcquisitionGeometry.ClearLane(eyes, m_AcquisitionTargetPoint + "0 1.5 0", m_AcquisitionObserver);
        m_AcquisitionTarget.SetPosition(m_AcquisitionTargetPoint);
        m_AcquisitionMeasurement.SightLimit = m_AcquisitionObserver.eAI_GetVisibilityDistanceLimit(m_AcquisitionTarget);
        m_AcquisitionAt = now;
        m_LastSightSample = now;
        m_ExposureStarted = -1;
        m_AcquisitionStage = 3;
    }

    void SampleExposure(float now)
    {
        m_LastSightSample = now;
        m_AcquisitionMeasurement.SightSamples++;
        vector eyes = m_AcquisitionObserver.GetBonePositionWS(m_AcquisitionObserver.GetBoneIndexByName("Head"));
        vector neck = m_AcquisitionTarget.GetBonePositionWS(m_AcquisitionTarget.GetBoneIndexByName("Neck"));
        if (m_AcquisitionObserver.PredateContactRay(m_AcquisitionTarget, eyes, neck)) m_AcquisitionMeasurement.BodyVisibleSamples++;
        if (m_AcquisitionObserver.PredateVisualQuality(m_AcquisitionTarget, true) <= 0)
        {
            m_ExposureStarted = -1;
            return;
        }
        if (m_ExposureStarted < 0) m_ExposureStarted = now;
        m_AcquisitionMeasurement.LongestInViewSeconds = Math.Max(m_AcquisitionMeasurement.LongestInViewSeconds, now - m_ExposureStarted);
    }

    void FinishAcquisition(float now)
    {
        m_AcquisitionReport.Trials.Insert(m_AcquisitionMeasurement);
        Print("[PredateAcquisition] range=" + m_AcquisitionMeasurement.Range + "; side=" + m_AcquisitionMeasurement.SideExposure + "; night=" + m_AcquisitionMeasurement.Night + "; peer=" + m_AcquisitionMeasurement.Peer + "; lane=" + m_AcquisitionMeasurement.ClearOpening + "; sightLimit=" + m_AcquisitionMeasurement.SightLimit + "; confirmation=" + m_AcquisitionMeasurement.ConfirmedSeconds + "; movement=" + m_AcquisitionMeasurement.Movement);
        m_AcquisitionStage = 0;
        m_AcquisitionNext = now + 0.25;
    }
}
