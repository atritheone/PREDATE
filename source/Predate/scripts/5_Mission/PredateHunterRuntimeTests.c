// Disposable-mission probe: the real human is stationary and invulnerable.
// Bot movement after setup uses native navigation. The optional hidden-target
// trial relocates only the human once, with no accompanying observation.
class PredateHunterRuntimeReport
{
    int Schema = 1;
    string Site;
    vector Start;
    vector TargetStart;
    int TreesNearby;
    int BuildingsNearby;
    float LocalRelief;
    float Elapsed;
    float Travelled;
    float MaximumStep;
    int SuspiciousSteps;
    bool SameActor = true;
    bool Moved;
    bool Approached;
    bool Reacquired;
    bool TestedPause;
    int Checks;
    int Failures;
}

class PredateHunterRuntimeTests
{
    ref PredateSession Session;
    PlayerBase Player;
    vector Start;
    vector AuthoredBotStart = "4500 0 6500";
    vector AuthoredHumanStart = "4500 0 6900";
    string Site = "baseline";
    ref PredateHunterRuntimeReport Report = new PredateHunterRuntimeReport;
    eAIBase InitialActor;
    vector PreviousPosition;
    float PreviousSampleAt;
    float StartedAt;
    float NextReport;
    bool Moved;
    bool Approached;
    bool Reacquired;
    bool TestedPause;
    float PauseSimulation;
    vector PausePosition;
    int PauseRevision;
    float PauseNextPulse;
    float PauseWallTime;
    int Failures;
    int Checks;
    bool TestHiddenDisplacement;
    bool HiddenDisplaced;
    bool VerifiedStaleEvidence;
    bool SampledAfterDisplacement;
    float DisplacedAt;
    float DisplacedNextPulse;
    string DisplacedSignalKey;
    string DisplacedEvidenceKey;

    void CheckDisplacement(float now, vector botPosition)
    {
        PredateSignalTrack track = Session.m_WorldState.BotSignal;
        PredateHunterState hunter = Session.m_WorldState.Hunter;
        if (!HiddenDisplaced && now - StartedAt >= 70)
        {
            vector relocated = Session.PredateGround(Player.GetPosition() + "400 0 0");
            Check(PredateHunterBrain.Distance(botPosition, relocated) > 200, "authored hidden displacement remains outside native vision and receiver cutoff");
            HiddenDisplaced = true;
            DisplacedAt = now;
            DisplacedNextPulse = track.NextPulseAt;
            if (track.LastSample) DisplacedSignalKey = track.LastSample.EventKey;
            DisplacedEvidenceKey = hunter.EvidenceKey;
            Player.SetPosition(relocated);
            Session.m_HuntBot.m_PredateNextPerceptionAt = 0;
            Check(!Session.m_HuntBot.EnforceLOS(1), "native sensor cannot see the relocated target");
            Print("[PredateHunterRuntimeTest] AUTHORED TARGET DISPLACEMENT: simulation=" + now + "; player=" + relocated + "; bot=" + botPosition + "; nextPulse=" + DisplacedNextPulse);
        }
        if (!HiddenDisplaced) return;
        if (!VerifiedStaleEvidence && now - DisplacedAt >= 8 && now < DisplacedNextPulse)
        {
            VerifiedStaleEvidence = true;
            Check(track.LastSample && track.LastSample.EventKey == DisplacedSignalKey && track.NextPulseAt == DisplacedNextPulse, "hidden relocation does not manufacture a signal or bypass its schedule");
            Check(hunter.EvidenceKey == DisplacedEvidenceKey && Session.m_HuntBot.m_PredateLastVisualAt < DisplacedAt, "hunter retains old evidence while the new route is unseen");
        }
        if (!SampledAfterDisplacement && track.LastSample && track.LastSample.ObservedAt > DisplacedAt)
        {
            SampledAfterDisplacement = true;
            Check(track.LastSample.EventKey != DisplacedSignalKey && !track.LastSample.HasPosition, "a later real pulse supplies a new bearing without target coordinates");
        }
    }

    void Check(bool condition, string label)
    {
        Checks++;
        if (condition) return;
        Failures++;
        Print("[PredateHunterRuntimeTest] FAIL: " + label);
    }

    void Run(PredateSession session, PlayerBase player)
    {
        Session = session;
        Player = player;
        eAIBase bot = session.m_HuntBot;
        player.SetAllowDamage(false);
        bot.SetAllowDamage(true);
        bot.SetHealth("", "Health", 100);
        bot.SetHealth("", "Blood", 5000);
        bot.SetAllowDamage(false);
        vector humanPosition = session.PredateGround(AuthoredHumanStart);
        Start = session.PredateGround(AuthoredBotStart);
        vector snappedHuman, snappedBot;
        bool validHuman = PredateRoutePlanner.Snap(humanPosition, 8, snappedHuman);
        bool validBot = PredateRoutePlanner.Snap(Start, 8, snappedBot);
        Check(validHuman && validBot, "authored fixture starts have native traversable positions");
        if (!validHuman || !validBot)
        {
            Report.Site = Site;
            Report.Checks = Checks;
            Report.Failures = Failures;
            string setupError;
            JsonFileLoader<PredateHunterRuntimeReport>.SaveFile("$profile:HunterRuntimeReport.json", Report, setupError);
            Print("[PredateHunterRuntimeTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
            return;
        }
        humanPosition = snappedHuman;
        Start = snappedBot;
        player.SetPosition(humanPosition);
        bot.SetPosition(Start);
        bot.SetDirection("0 0 1");
        InitialActor = bot;
        PreviousPosition = Start;
        Report.Site = Site;
        Report.Start = Start;
        Report.TargetStart = humanPosition;
        RecordSite();
        session.m_WorldState.Observations.Entries.Clear();
        session.m_WorldState.PendingSounds.Clear();
        session.m_WorldState.Contacts.Clear();
        PredateSignalService.NewDeployment(session);
        bot.PredateBindInformation(false, "raven-01", session.m_WorldState.Observations, session.m_WorldState.PendingSounds, session.m_WorldState.SimulationSeconds);
        bot.PredateBindInformation(true, "raven-01", session.m_WorldState.Observations, session.m_WorldState.PendingSounds, session.m_WorldState.SimulationSeconds);
        bot.m_PredateLastVisualAt = -1;
        bot.m_PredateAppliedGoal = -1;
        bot.m_PredateStrategicControl = false;
        bot.DisableSimulation(false);
        bot.SetMovementSpeedLimits(2, 3);
        StartedAt = session.m_WorldState.SimulationSeconds;
        PreviousSampleAt = StartedAt;
        PredateSignalService.Select(session, true);
        PredateSignalService.Tick(session, player, 0);
        PredateHunterService.Tick(session);
        Check(session.m_WorldState.Hunter.Mode == "observe" || session.m_WorldState.Hunter.Mode == "evade", "initial pulse requests a lateral observation route");
        Check(bot.GetGroup().GetWaypoints().Count() == 2, "native group receives a terrain-scored route and its goal");
        Check(!bot.PredateTacticalPriority(StartedAt), "initial movement has no visual tactical contact");
        Print("[PredateHunterRuntimeTest] START: site=" + Site + "; simulation=" + StartedAt + "; start=" + Start + "; target=" + humanPosition + "; trees=" + Report.TreesNearby + "; buildings=" + Report.BuildingsNearby + "; localRelief=" + Report.LocalRelief);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateProbe, 1000, false);
    }

    void UpdateProbe()
    {
        float now = Session.m_WorldState.SimulationSeconds;
        float elapsed = now - StartedAt;
        eAIBase bot = Session.m_HuntBot;
        PredateHunterState state = Session.m_WorldState.Hunter;
        vector position = bot.GetPosition();
        float step = PredateHunterBrain.Distance(position, PreviousPosition);
        float sampleSeconds = Math.Max(0.1, now - PreviousSampleAt);
        Report.MaximumStep = Math.Max(Report.MaximumStep, step);
        if (step > Math.Max(20, sampleSeconds * 10)) Report.SuspiciousSteps++;
        else Report.Travelled += step;
        if (bot != InitialActor) Report.SameActor = false;
        PreviousPosition = position;
        PreviousSampleAt = now;
        if (!TestedPause && elapsed >= 25)
        {
            TestedPause = true;
            Session.SetPaused(true);
            PauseSimulation = now;
            PausePosition = position;
            PauseRevision = state.GoalRevision;
            PauseNextPulse = Session.m_WorldState.BotSignal.NextPulseAt;
            float wall = GetGame().GetTickTime();
            PauseWallTime = wall;
            PredateServerPauseDriver.Poll.Insert(PollPause);
            return;
        }
        if (PredateHunterBrain.Distance(position, Start) >= 60) Moved = true;
        if (state.Mode == "approach") Approached = true;
        if (TestHiddenDisplacement) CheckDisplacement(now, position);
        if (bot.m_PredateLastVisualAt >= StartedAt && (!TestHiddenDisplacement || (HiddenDisplaced && bot.m_PredateLastVisualAt > DisplacedAt))) Reacquired = true;
        if (elapsed >= NextReport)
        {
            NextReport = elapsed + 10;
            Print("[PredateHunterRuntimeTest] PROGRESS: elapsed=" + elapsed + "; mode=" + state.Mode + "; position=" + position + "; moved=" + Moved + "; approached=" + Approached + "; visual=" + Reacquired + "; nativeThreat=" + bot.GetThreatToSelf() + "; fighting=" + bot.m_eAI_IsFightingFSM + "; " + bot.PredateRouteDiagnostic());
        }
        float limit = 240;
        if (TestHiddenDisplacement) limit = 420;
        bool complete = Moved && Approached && Reacquired;
        if (TestHiddenDisplacement) complete = complete && VerifiedStaleEvidence && SampledAfterDisplacement;
        if (elapsed >= limit || complete)
        {
            Check(Moved, "actual native locomotion moves at least sixty metres");
            Check(Approached, "later independent sample initiates an estimated-area approach");
            Check(Reacquired, "approach permits fresh native visual reacquisition");
            Check(bot.IsAlive() && Player.IsAlive(), "both fixture actors remain alive");
            Check(state.Validate(now), "live hunter state remains persistable");
            Check(Report.SuspiciousSteps == 0 && Report.SameActor, "hunter movement retains its original actor without teleport-sized steps");
            if (TestHiddenDisplacement) Check(VerifiedStaleEvidence && SampledAfterDisplacement, "hidden-route trial verifies both stale knowledge and a subsequent lawful sample");
            Session.PredateSave();
            Report.Elapsed = elapsed;
            Report.Moved = Moved;
            Report.Approached = Approached;
            Report.Reacquired = Reacquired;
            Report.TestedPause = TestedPause;
            Report.Checks = Checks;
            Report.Failures = Failures;
            string error;
            JsonFileLoader<PredateHunterRuntimeReport>.SaveFile("$profile:HunterRuntimeReport.json", Report, error);
            Print("[PredateHunterRuntimeTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateProbe, 1000, false);
    }

    void RecordSite()
    {
        array<Object> objects = new array<Object>;
        array<CargoBase> cargo = new array<CargoBase>;
        GetGame().GetObjectsAtPosition3D(Start, 80, objects, cargo);
        foreach (Object object : objects)
        {
            if (!object) continue;
            if (object.IsTree()) Report.TreesNearby++;
            else if (object.IsBuilding()) Report.BuildingsNearby++;
        }
        float low = Start[1];
        float high = low;
        for (int x = -1; x <= 1; x++)
        {
            for (int z = -1; z <= 1; z++)
            {
                float height = GetGame().SurfaceY(Start[0] + x * 80, Start[2] + z * 80);
                low = Math.Min(low, height);
                high = Math.Max(high, height);
            }
        }
        Report.LocalRelief = high - low;
        if (Site == "woodland") Check(Report.TreesNearby >= 8, "woodland fixture starts near actual mapped trees");
        else if (Site == "village") Check(Report.BuildingsNearby >= 3, "village fixture starts near actual mapped buildings");
        else if (Site == "ridge") Check(Report.LocalRelief >= 8, "ridge fixture has measured local terrain relief");
    }

    void VerifyPause()
    {
        Check(Session.m_Paused && Session.m_WorldState.SimulationSeconds == PauseSimulation, "gameplay pause holds the hunter simulation clock");
        Check(Session.m_HuntBot.GetPosition() == PausePosition && Session.m_HuntBot.GetIsSimulationDisabled(), "gameplay pause holds actual bot locomotion");
        Check(Session.m_WorldState.Hunter.GoalRevision == PauseRevision && Session.m_WorldState.BotSignal.NextPulseAt == PauseNextPulse, "gameplay pause holds route decisions and scheduled pulses");
        Session.SetPaused(false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateProbe, 1000, false);
    }

    void PollPause(float timeslice)
    {
        if (GetGame().GetTickTime() - PauseWallTime < 4) return;
        PredateServerPauseDriver.Poll.Remove(PollPause);
        VerifyPause();
    }
}
