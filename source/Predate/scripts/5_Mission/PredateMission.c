// Session coordinator: simulation lifecycle, services and engine callbacks.
class PredateSession
{
    ref PredateScoreService m_Score;
    static const float CHECKPOINT_INTERVAL = 30;
    static const float CHECKPOINT_MAX_DEFER = 0.1;
    ref PredateConfig m_HuntConfig;
    ref PredateLoadoutCatalog m_Loadouts;
    ref PredateProgress m_HuntProgress;
    ref eAIGroup m_HuntGroup;
    eAIBase m_HuntBot;
    float m_HuntStateTimer;
    float m_HuntSaveTimer;
    int m_CheckpointPeer = -1;
    int m_CheckpointRosterRow;
    float m_CheckpointWait;
    float m_CheckpointActorMillis;
    float m_CheckpointRosterMillis;
    float m_CheckpointPeakRowMillis;
    float m_DiagnosticElapsed;
    float m_DiagnosticWorstFrame;
    int m_DiagnosticFrames;
    int m_DiagnosticSlowFrames;
    float m_PredateNextLightScan;
    int m_PredateRouteCursor;
    bool m_HuntReady;
    bool m_HuntSentState;
    bool m_HuntSentCloseApproach;
    bool m_HuntSchemaValid;
    bool m_Development;
    string m_HuntError;
    ref PredateOperatorRegistry m_Operators;
    ref PredatePeerRoster m_PeerRoster;
    float m_SoloCombatTimer;
    ref PredateServerBridge m_Bridge;
    ref PredateWorldState m_WorldState;
    bool m_MemoryRestored;
    bool m_ActorsActive;
    bool m_Suspending;
    bool m_SuspendSaved;
    bool m_Paused;
    bool m_PauseReady;
    bool m_DiscardResumeStep;
    int m_PulseClockRevision;
    ref array<Man> m_PauseActors = new array<Man>;
    ref array<Man> m_PauseDamageActors = new array<Man>;
    ref array<Man> m_PauseClockActors = new array<Man>;
    ref PredateWorldPhysicsHold m_PhysicsHold = new PredateWorldPhysicsHold;
    IEntity m_PhysicsHoldAnchor;

    void Init()
    {
        m_Bridge = new PredateServerBridge(this);
        string flag;
        m_Development = GetGame().CommandlineGetParam("predateDevelopment", flag) && FileExist("$profile:PredateDevelopment.txt");
        PredateServerDiagnostics.Enabled = GetGame().CommandlineGetParam("predateServerDiagnostics", flag);
        PredateServerDiagnostics.TraceOperators = FileExist("$profile:PredateAITrace.txt");
        PredateServerDiagnostics.TraceShots = GetGame().CommandlineGetParam("predateShotTrace", flag) || (PredateServerDiagnostics.Enabled && FileExist("$profile:PredateShotTrace.txt"));
        PredateInit();
    }

    void PredateInit()
    {
        m_HuntSchemaValid = PredatePersistence.Load(this);
        if (!m_HuntSchemaValid) return;
        if (m_Score) m_Score.m_SessionStartSeconds = m_WorldState.SimulationSeconds;
        m_Operators = new PredateOperatorRegistry;
        if (!m_Operators.Register(m_HuntProgress.Operator))
        {
            PredatePersistence.Fail(this, "Duplicate operator registration");
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateStart, 6000, false);
    }

    void PredateSave()
    {
        if (m_SuspendSaved) return;
        if (m_PeerRoster) m_PeerRoster.CancelStateSave();
        m_CheckpointPeer = -1;
        m_CheckpointRosterRow = 0;
        m_CheckpointWait = 0;
        m_HuntSaveTimer = 0;
        if (PredatePersistence.Save(this) && m_Suspending) m_SuspendSaved = true;
    }

    void PredateRequestCheckpoint(bool restartRoster = false)
    {
        if (m_CheckpointPeer < 0) m_HuntSaveTimer = CHECKPOINT_INTERVAL;
        // Selection and life changes must not leave conflicting rows in the
        // same save. Keep the paced native writes and restart the staged JSON.
        if (restartRoster && m_PeerRoster && m_CheckpointRosterRow > 0)
        {
            m_PeerRoster.CancelStateSave();
            m_CheckpointRosterRow = 0;
        }
    }

    void PredateUpdateCheckpoint(float timeslice, bool updatedHunt)
    {
        if (!m_ActorsActive || m_Paused || m_Suspending || !PredatePersistence.CanSave(this)) return;
        m_HuntSaveTimer += timeslice;
        if (m_CheckpointPeer < 0)
        {
            if (m_HuntSaveTimer < CHECKPOINT_INTERVAL) return;
            m_CheckpointPeer = 0;
            m_HuntSaveTimer = 0;
            m_CheckpointActorMillis = 0;
            m_CheckpointRosterMillis = 0;
            m_CheckpointPeakRowMillis = 0;
        }
        m_CheckpointWait += timeslice;
        // Give a busy frame room to recover while keeping the checkpoint moving.
        if (m_CheckpointWait < CHECKPOINT_MAX_DEFER && (updatedHunt || timeslice > 0.022)) return;
        m_CheckpointWait = 0;
        int started;
        if (PredateServerDiagnostics.Enabled) started = TickCount(0);
        int peerCount;
        if (m_PeerRoster) peerCount = m_PeerRoster.m_Bindings.Count();
        string error;
        // Schedule a native actor write or a roster JSON row per frame.
        if (m_CheckpointPeer <= peerCount)
        {
            if (m_CheckpointPeer == peerCount) PredatePersistence.SaveRaven(this);
            else if (!m_PeerRoster.SaveActor(this, m_PeerRoster.m_Bindings[m_CheckpointPeer], error))
            {
                PredatePersistence.Fail(this, "Cannot checkpoint operator roster: " + error);
                return;
            }
            if (PredateServerDiagnostics.Enabled) m_CheckpointActorMillis += TickCount(started) * 0.0001;
            m_CheckpointPeer++;
            return;
        }
        if (m_CheckpointRosterRow < peerCount)
        {
            if ((m_CheckpointRosterRow == 0 && !m_PeerRoster.BeginStateSave(error)) || !m_PeerRoster.WriteStateRow(m_CheckpointRosterRow, error))
            {
                m_PeerRoster.CancelStateSave();
                PredatePersistence.Fail(this, "Cannot stage operator roster: " + error);
                return;
            }
            if (PredateServerDiagnostics.Enabled)
            {
                float rowMillis = TickCount(started) * 0.0001;
                m_CheckpointRosterMillis += rowMillis;
                m_CheckpointPeakRowMillis = Math.Max(m_CheckpointPeakRowMillis, rowMillis);
            }
            m_CheckpointRosterRow++;
            return;
        }
        if (!PredatePersistence.SaveWorldState(this))
        {
            if (m_PeerRoster) m_PeerRoster.CancelStateSave();
            return;
        }
        int publishStarted;
        if (PredateServerDiagnostics.Enabled) publishStarted = TickCount(0);
        if (peerCount > 0 && !m_PeerRoster.FinishStateSave(error))
        {
            PredatePersistence.Fail(this, "Cannot publish operator roster: " + error);
            return;
        }
        float publishMillis;
        if (peerCount > 0 && PredateServerDiagnostics.Enabled) publishMillis = TickCount(publishStarted) * 0.0001;
        if (PredateServerDiagnostics.Enabled)
            Print("[PredatePerf] checkpoint actors_ms=" + m_CheckpointActorMillis + "; roster_ms=" + m_CheckpointRosterMillis + "; peak_row_ms=" + m_CheckpointPeakRowMillis + "; publish_ms=" + publishMillis + "; state_ms=" + TickCount(started) * 0.0001 + "; roster=" + (peerCount + 1));
        m_CheckpointPeer = -1;
        m_CheckpointRosterRow = 0;
    }

    void PredateReportFrames(float timeslice)
    {
        if (!PredateServerDiagnostics.Enabled || !m_ActorsActive || m_Paused || m_Suspending) return;
        m_DiagnosticElapsed += timeslice;
        m_DiagnosticFrames++;
        m_DiagnosticWorstFrame = Math.Max(m_DiagnosticWorstFrame, timeslice);
        if (timeslice > 0.033) m_DiagnosticSlowFrames++;
        if (m_DiagnosticElapsed < 60) return;
        Print("[PredatePerf] active_frames=" + m_DiagnosticFrames + "; mean_ms=" + (m_DiagnosticElapsed * 1000 / m_DiagnosticFrames) + "; max_ms=" + m_DiagnosticWorstFrame * 1000 + "; over_33ms=" + m_DiagnosticSlowFrames);
        m_DiagnosticFrames = 0;
        m_DiagnosticSlowFrames = 0;
        m_DiagnosticWorstFrame = 0;
        m_DiagnosticElapsed = 0;
    }

    bool AttachPeerRoster(vector lynxStart, vector rookStart, out string error)
    {
        if (m_PeerRoster) return true;
        PredatePeerRoster roster = new PredatePeerRoster;
        if (!roster.Open(this, lynxStart, rookStart, error))
            return PredatePersistence.Fail(this, "Cannot open operator roster: " + error);
        m_PeerRoster = roster;
        return true;
    }

    ref array<vector> m_PredateOpeningSites = {};

    void HoldPauseActor(Man actor)
    {
        if (!actor) return;
        if (m_PauseClockActors.Find(actor) == -1)
        {
            PlayerBase person = PlayerBase.Cast(actor);
            if (person) person.PredateBeginClockHold();
            m_PauseClockActors.Insert(actor);
        }
        if (actor.GetAllowDamage())
        {
            m_PauseDamageActors.Insert(actor);
            actor.SetAllowDamage(false);
        }
        if (actor.GetIsSimulationDisabled()) return;
        if (m_PauseActors.Find(actor) == -1) m_PauseActors.Insert(actor);
        PredateActorSimulation.SetHeld(actor, true);
    }

    void PredateApplyWorldClock()
    {
        if (m_Paused || (m_HuntConfig && m_HuntConfig.FreezeTimeOfDay))
            GetGame().GetWorld().SetTimeMultiplier(0);
        else
            GetGame().GetWorld().SetTimeMultiplier(-1);
    }

    void SetPaused(bool paused)
    {
        if (paused == m_Paused || m_Suspending) return;
        m_PauseReady = false;
        if (paused)
        {
            PredateMemoryPersistence.Capture(this);
            array<Man> players = new array<Man>;
            GetGame().GetPlayers(players);
            if (m_HuntBot && players.Find(m_HuntBot) == -1) players.Insert(m_HuntBot);
            foreach (Man actor : players)
            {
                HoldPauseActor(actor);
                PlayerBase participant = PlayerBase.Cast(actor);
                if (participant && participant.GetIdentity() && m_HuntReady && m_Score)
                {
                    PredateLifeScore pausedLife = m_Score.Life(m_WorldState.PlayerLife);
                    if (pausedLife)
                    {
                        pausedLife.UpdateTime(m_WorldState.SimulationSeconds);
                        m_Score.RefreshTracking(participant, pausedLife, true);
                    }
                    PredateSend(participant);
                }
                if (!m_PhysicsHoldAnchor && actor) m_PhysicsHoldAnchor = actor;
            }
            if (m_PeerRoster) m_PeerRoster.HoldPause(this);
            m_PhysicsHold.Capture(m_PhysicsHoldAnchor);
        }
        else
        {
            m_PhysicsHold.Resume();
            m_PhysicsHoldAnchor = null;
            foreach (Man clockActor : m_PauseClockActors)
            {
                PlayerBase clockPlayer = PlayerBase.Cast(clockActor);
                if (clockPlayer) clockPlayer.PredateEndClockHold();
            }
            m_PauseClockActors.Clear();
            foreach (Man held : m_PauseActors)
                if (held) PredateActorSimulation.SetHeld(held, false);
            foreach (Man damageHeld : m_PauseDamageActors)
                if (damageHeld) damageHeld.SetAllowDamage(true);
            m_PauseActors.Clear();
            m_PauseDamageActors.Clear();
            m_MemoryRestored = false;
            m_DiscardResumeStep = true;
            m_PulseClockRevision++;
            m_HuntStateTimer = 1;
        }
        m_Paused = paused;
        if (GetGame().IsDedicatedServer()) PredateServerPauseDriver.Held = paused;
        PredateApplyWorldClock();
    }

    void BeginSuspend()
    {
        if (m_Suspending) return;
        // Native shutdown completion needs the system queue. Keep gameplay,
        // actors and physics held while allowing those closing callbacks.
        if (GetGame().IsDedicatedServer()) PredateServerPauseDriver.Held = false;
        PredateMemoryPersistence.Capture(this);
        m_Suspending = true;
        m_ActorsActive = false;
        if (m_HuntBot) PredateActorSimulation.SetHeld(m_HuntBot, true);
        if (m_PeerRoster) m_PeerRoster.StopActors();
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (player && player.GetIdentity()) PredateActorSimulation.SetHeld(player, true);
        }
    }

    bool PredatePlayerCloseApproach(PlayerBase player, PredatePeerBinding chosen)
    {
        if (!player || !player.IsAlive() || m_WorldState.PlayerAwaitingRedeploy) return false;
        PredateSignalTrack track = m_WorldState.PlayerSignal;
        PlayerBase target = m_HuntBot;
        if (chosen)
        {
            track = chosen.State.PlayerReceiver;
            target = chosen.Actor;
        }
        return track && track.Selected && !track.HasLostPlayerReception() && target && target.IsAlive() && PredateSignalRules.FinalApproach(player.GetPosition(), target.GetPosition());
    }

    void PredateSend(PlayerBase player)
    {
        if (player.m_PredateKillNotices.Count() > 0 && m_WorldState && m_WorldState.PlayerSignal)
            PredateSignalService.ConfirmPlayerKills(this, player.m_PredateKillNotices);
        // Keep the payload in local values. Filling a temporary receiver object
        // in this path triggered a native access violation after the first send.
        bool needsRedeploy = m_WorldState && m_WorldState.PlayerAwaitingRedeploy;
        bool receiverReady = !needsRedeploy && m_HuntReady && m_HuntError == "";
        PredateSignalTrack receiverTrack;
        if (m_WorldState) receiverTrack = m_WorldState.PlayerSignal;
        string operatorId = m_HuntConfig.OperatorAtPosition(0);
        int availableOperators = -1;
        PredatePeerBinding chosen;
        bool defeated = m_HuntProgress.Defeated || (m_HuntBot && !m_HuntBot.IsAlive());
        if (m_PeerRoster)
        {
            availableOperators = 0;
            chosen = m_PeerRoster.PlayerLock();
            if (chosen)
            {
                receiverTrack = chosen.State.PlayerReceiver;
                operatorId = chosen.State.Operator.Id;
                defeated = chosen.State.Operator.LifeState == "defeated" || (chosen.Actor && !chosen.Actor.IsAlive());
            }
        }
        string signature = PredateProtocol.SignatureFor(operatorId);
        bool development = m_Development && !m_PeerRoster;
        if (receiverReady && receiverTrack && receiverTrack.Selected)
        {
            if (defeated || receiverTrack.Reception == PredatePulseReception.UNAVAILABLE)
            {
                if (PredateSignalService.RetireDefeatedSignal(this, PredateProtocol.OperatorSlot(operatorId), true))
                    PredateRequestCheckpoint(true);
            }
            else if (PredateSignalService.DropOutOfRangeTrack(receiverTrack))
            {
                if (m_Score)
                {
                    PredateOperatorRecord target = m_HuntProgress.Operator;
                    if (chosen) target = chosen.State.Operator;
                    m_Score.RecordPursuitOutcome(target, "Signal lost");
                }
                if (!chosen) PredateSignalService.SyncReceiverProgress(this);
                PredateRequestCheckpoint(true);
            }
        }
        if (m_PeerRoster && receiverReady && m_WorldState.PlayerSignal) availableOperators = m_PeerRoster.AvailableCount(this);
        bool available = receiverReady && receiverTrack && receiverTrack.Reception != PredatePulseReception.UNAVAILABLE;
        bool locked = available && receiverTrack.Selected;
        bool canDeploy = receiverReady && !available && m_HuntProgress.Defeated && !m_PeerRoster && !m_HuntConfig.EnableOperatorRespawns;
        bool closeApproach = available && locked && PredatePlayerCloseApproach(player, chosen);
        bool intercepted = receiverReady && m_HuntConfig.EnableSharedSignals && m_WorldState.PlayerInterceptionUntil > m_WorldState.SimulationSeconds;
        float now = m_WorldState.SimulationSeconds;
        PredateObservation sample;
        if (receiverTrack) sample = receiverTrack.PlayerDisplaySample();
        bool hasPulse = available && locked && !closeApproach && sample && !sample.Expired(now);
        bool hasPulseOrigin = hasPulse;
        int bearing;
        float age;
        vector pulseOrigin;
        float bearingError;
        if (hasPulse)
        {
            bearing = Math.Round(sample.Bearing);
            bearing = bearing % 360;
            age = now - sample.ObservedAt;
            pulseOrigin = sample.Origin;
            bearingError = sample.BearingError;
        }
        float nextPulse;
        float pulseDeadline;
        if (receiverTrack)
        {
            pulseDeadline = receiverTrack.NextPulseAt;
            nextPulse = Math.Max(0, pulseDeadline - now);
        }
        string message;
        if (needsRedeploy) message = "Deployment ended";
        else if (!receiverReady) message = "Waiting for receiver initialization...";
        else if (locked) message = "Tracking " + PredateProtocol.DisplayName(operatorId);
        else if (availableOperators == 0 || (!m_PeerRoster && !available)) message = "No active Operator signals.";
        else message = "Click a pulse to track it";
        if (m_HuntError != "") message = "Recovery required. " + m_HuntError;
        nextPulse = Math.Clamp(nextPulse, 0, PredateBearingPulse.INTERVAL);
        bool first = !m_HuntSentState;
        m_HuntSentState = true;
        m_HuntSentCloseApproach = closeApproach;
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(PredateProtocol.VERSION);
        rpc.Write(operatorId);
        rpc.Write(development);
        rpc.Write(canDeploy);
        rpc.Write(signature);
        rpc.Write(available);
        rpc.Write(locked);
        rpc.Write(player.m_PredateTrackingChoice);
        rpc.Write(hasPulse);
        rpc.Write(closeApproach);
        rpc.Write(bearing);
        rpc.Write(age);
        rpc.Write(nextPulse);
        rpc.Write(pulseDeadline);
        rpc.Write(m_PulseClockRevision);
        rpc.Write(message);
        rpc.Write(hasPulseOrigin);
        rpc.Write(pulseOrigin);
        rpc.Write(bearingError);
        rpc.Write(intercepted);
        rpc.Write(player.m_PredateKillNotices.Count());
        foreach (int slot : player.m_PredateKillNotices) rpc.Write(slot);
        player.m_PredateKillNotices.Clear();
        rpc.Write(player.m_PredateSupplyNotice);
        player.m_PredateSupplyNotice = false;
        rpc.Write(needsRedeploy);
        if (needsRedeploy) rpc.Write(player.m_PredateSpawnRejected);
        // Only the sampled bearing and coarse distance band reach the receiver.
        PredateSignalService.WriteHistory(this, rpc, locked, receiverTrack);
        PredateSignalService.WriteMapPulses(this, rpc, receiverReady, closeApproach);
        rpc.Write(availableOperators);
        vector footprintGround;
        int footprintHeading;
        int footprintClue = PredateFootprintService.PlayerClue(this, player, footprintGround, footprintHeading);
        rpc.Write(footprintClue);
        if (footprintClue > 0)
        {
            rpc.Write(footprintGround);
            rpc.Write(footprintHeading);
        }
        PredateFootprintSensor footprintSensor = player.m_PredateFootprintSensor;
        bool ownMark = !needsRedeploy && m_HuntConfig.EnableFootprints && footprintSensor && PredateFootprintLedger.Reachable(footprintSensor.m_Own, player.GetPosition(), now);
        rpc.Write(m_HuntConfig.EnableFootprints);
        rpc.Write(ownMark);
        if (ownMark)
        {
            rpc.Write(footprintSensor.m_Own.Position);
            rpc.Write(footprintSensor.m_Own.Heading);
            rpc.Write(footprintSensor.m_InspectingOwn);
        }
        rpc.Write(player.m_PredateFootprintNotice);
        player.m_PredateFootprintNotice = "";
        rpc.Write(player.m_PredateHasDeploymentPosition);
        if (player.m_PredateHasDeploymentPosition) rpc.Write(player.m_PredateDeploymentPosition);
        PredateLifeScore life;
        if (m_WorldState && m_Score) life = m_Score.Life(m_WorldState.PlayerLife);
        int score;
        if (life) score = life.Score;
        rpc.Write(score);
        rpc.Send(player, PredateProtocol.STATE, true, player.GetIdentity());
        if (first) Print("[Predate] Sent receiver state; target available=" + available);
    }


    bool PredateTargetAvailable()
    {
        if (!m_HuntReady || m_HuntError != "") return false;
        if (m_HuntProgress.Defeated) return false;
        eAIBase actor = m_HuntBot;
        if (!actor) return false;
        return actor.IsAlive();
    }

    vector PredateGround(vector position)
    { return PredateOperatorService.PredateGround(this, position); }

    void PredateEquip(PlayerBase player)
    { PredateOperatorService.PredateEquip(this, player); }

    void PredateConfigureBot()
    { PredateOperatorService.PredateConfigureBot(this); }

    void PredateStart()
    { PredateOperatorService.PredateStart(this); }

    void PredateResetHunt(PlayerBase player)
    { PredateOperatorService.PredateResetHunt(this, player); }

    void PredateSelfTest()
    { PredateDevelopmentTests.PredateSelfTest(this); }

    void PredateRunDevelopmentScenario()
    { PredateDevelopmentTests.RunScenario(this); }

    void PredateSurvivalSelfTest()
    { PredateDevelopmentTests.PredateSurvivalSelfTest(this); }

    bool SoloIsSurvivalModifier(int id)
    { return PredateSurvivalRules.SoloIsSurvivalModifier(this, id); }

    void SoloApplyCombatRules(PlayerBase player)
    { PredateSurvivalRules.SoloApplyCombatRules(this, player); }

    void PredateClearWeather()
    {
        Weather weather = GetGame().GetWeather();
        // Freeze forecast changes after the persisted world has loaded.
        weather.MissionWeather(false);
        weather.SetWeatherUpdateFreeze(true);
        array<WeatherPhenomenon> clear = {weather.GetOvercast(), weather.GetFog(), weather.GetRain(), weather.GetSnowfall()};
        foreach (WeatherPhenomenon phenomenon : clear)
        {
            phenomenon.SetLimits(0, 0);
            phenomenon.SetForecastChangeLimits(0, 0);
            phenomenon.Set(0, 0, 86400);
        }
        weather.SetDynVolFogDistanceDensity(0);
        // With zero distance density, height density 1 removes the altitude
        // bias. Height density 0 retains a fog layer near low coastal terrain.
        weather.SetDynVolFogHeightDensity(1);
        weather.SetStorm(0, 1, 86400);
        Sandstorm dust = weather.GetSandstorm();
        if (dust && dust.IsActive()) dust.Stop(0, true);
        weather.GetWindMagnitude().SetLimits(2, 2);
        weather.GetWindMagnitude().Set(2, 0, 86400);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateCheckClearWeather, 2000, false);
    }

    void PredateCheckClearWeather()
    {
        Weather weather = GetGame().GetWeather();
        bool clear = weather.GetOvercast().GetActual() < 0.001;
        clear = clear && weather.GetFog().GetActual() < 0.001;
        clear = clear && weather.GetRain().GetActual() < 0.001;
        clear = clear && weather.GetSnowfall().GetActual() < 0.001;
        clear = clear && weather.GetDynVolFogDistanceDensity() < 0.001;
        clear = clear && weather.GetDynVolFogHeightDensity() > 0.999;
        float lowAltitudeVisibility = Environment.Expansion_GetDynVolFogVisibilityEx(0);
        clear = clear && lowAltitudeVisibility > 0.999;
        if (clear) Print("[Predate] Clear weather confirmed: no cloud, rain, snow or fog; light breeze.");
        else Print("[Predate] WEATHER CHECK FAIL: overcast=" + weather.GetOvercast().GetActual() + "; rain=" + weather.GetRain().GetActual() + "; fog=" + weather.GetFog().GetActual());
    }

    void PredateTick(float seconds)
    {
        if (m_Suspending) return;
        if (!m_HuntReady || m_HuntError != "")
        {
            array<Man> waitingPlayers = new array<Man>;
            GetGame().GetPlayers(waitingPlayers);
            m_HuntStateTimer += seconds;
            if (m_HuntStateTimer < 1) return;
            m_HuntStateTimer = 0;
            foreach (Man waiting : waitingPlayers)
            {
                PlayerBase receiver = PlayerBase.Cast(waiting);
                if (receiver && receiver.GetIdentity()) PredateSend(receiver);
            }
            return;
        }
        if (!m_HuntSchemaValid || m_HuntError != "") return;
        string registryError;
        eAIBase resolvedActor;
        eAIGroup resolvedGroup;
        PredateOperatorIndex index;
        if (m_PeerRoster) index = PredateOperatorIndex.Capture();
        string primaryId = m_HuntProgress.Operator.Id;
        if (!m_Operators.Resolve(primaryId, resolvedActor, resolvedGroup, registryError, index))
        {
            PredatePersistence.Fail(this, registryError);
            return;
        }
        if (resolvedActor != m_HuntBot || resolvedGroup != m_HuntGroup)
        {
            if (!resolvedActor && m_HuntGroup)
            {
                m_HuntGroup.m_Persist = false;
                eAIGroup.s_PersistentGroups.RemoveItem(m_HuntGroup);
            }
            m_HuntBot = resolvedActor;
            m_HuntGroup = resolvedGroup;
            m_MemoryRestored = false;
            if (m_Paused) HoldPauseActor(m_HuntBot);
            PredateConfigureBot();
            Print("[Predate] Rebound registered operator " + primaryId + " by its persistent storage key.");
        }
        if (m_Paused)
        {
            array<Man> heldPlayers = new array<Man>;
            GetGame().GetPlayers(heldPlayers);
            foreach (Man heldPlayer : heldPlayers) HoldPauseActor(heldPlayer);
            if (m_PeerRoster) m_PeerRoster.HoldPause(this);
            m_PhysicsHold.Capture(m_PhysicsHoldAnchor);
            m_PauseReady = true;
            return;
        }
        bool defeated = m_HuntProgress.Operator.LifeState == "defeated";
        if (m_HuntBot && !m_HuntBot.IsAlive()) defeated = true;
        if (defeated && !m_HuntProgress.Defeated)
        {
            m_HuntProgress.Defeated = true;
            m_HuntProgress.Operator.LifeState = "defeated";
            m_HuntProgress.Selected = false;
            m_HuntProgress.HasPulse = false;
            Print("[Predate] " + PredateProtocol.SignatureFor(primaryId) + " eliminated.");
            PredateRequestCheckpoint();
        }
        SoloApplyCombatRules(m_HuntBot);
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        bool activePlayer;
        string activePlayerId;
        PlayerBase activeParticipant;
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            if (!player.IsAlive())
            {
                PredateDeployment.ObserveDeath(this, player);
                if (player.m_PredateKillNotices.Count() > 0 || m_HuntStateTimer >= 1) PredateSend(player);
                continue;
            }
            if (m_WorldState.PlayerAwaitingRedeploy)
            {
                PredateDeployment.StagePlayer(player);
                int deploymentRequest = player.m_PredateRequest;
                player.m_PredateRequest = 0;
                if (deploymentRequest == 5) PredateDeployment.Redeploy(this, player);
                if (deploymentRequest == 5 || m_HuntStateTimer >= 1) PredateSend(player);
                continue;
            }
            if (m_HuntProgress.PlayerPlacementRevision < 1)
            {
                player.SetPosition(PredateGround(m_HuntConfig.PlayerStart));
                m_HuntProgress.PlayerPlacementRevision = 1;
                PredateSave();
            }
            activePlayer = true;
            activePlayerId = player.GetIdentity().GetId();
            activeParticipant = player;
            PredateDeployment.RememberCharacter(m_WorldState, player);
            if (!m_WorldState.PlayerPrimaryAssigned && player.IsPlayerLoaded())
                PredateOperatorService.AssignStartingWeapons(this, player);
            player.PredateSyncWeaponSlots();
            PredateMemoryPersistence.RestoreContact(this, player);
            int request = player.m_PredateRequest;
            player.m_PredateRequest = 0;
            if (request == 3 && m_Development)
            {
                PredateResetHunt(player);
            }
            else if (request == 4)
            {
                PredateOperatorService.PredateNewDeployment(this);
            }
            PredateSignalService.Tick(this, player, seconds);
            if (m_PeerRoster) m_PeerRoster.SampleForPlayer(this, player);
            if (m_Score) m_Score.Tick(player);
            PredatePeerBinding chosen;
            if (m_PeerRoster) chosen = m_PeerRoster.PlayerLock();
            PredateSignalTrack selectedTrack = m_WorldState.PlayerSignal;
            if (chosen) selectedTrack = chosen.State.PlayerReceiver;
            bool lostReception = selectedTrack.Selected && selectedTrack.HasLostPlayerReception();
            bool closeApproach = PredatePlayerCloseApproach(player, chosen);
            if (lostReception || closeApproach != m_HuntSentCloseApproach || player.m_PredateKillNotices.Count() > 0 || player.m_PredateFootprintNotice != "" || m_HuntStateTimer >= 1)
            {
                bool confirmedKill = player.m_PredateKillNotices.Count() > 0;
                PredateSend(player);
                if (confirmedKill) PredateRequestCheckpoint();
            }
        }
        if (activePlayer && m_WorldState)
        {
            PredateMemoryPersistence.Capture(this);
            m_WorldState.SimulationSeconds += seconds;
            PredateOperatorService.RespawnRaven(this, activeParticipant);
            if (m_HuntBot) m_HuntBot.PredateAdvanceInformation(m_WorldState.SimulationSeconds);
            if (m_HuntBot) m_HuntBot.PredateUpdateMedicine(m_WorldState.SimulationSeconds);
            PredateFootprintService.Tick(this, activeParticipant);
        }
        if (activePlayer != m_ActorsActive)
        {
            if (!activePlayer)
            {
                PredateMemoryPersistence.Capture(this);
                m_MemoryRestored = false;
            }
            else if (m_WorldState && m_WorldState.Year > 0)
                GetGame().GetWorld().SetDate(m_WorldState.Year, m_WorldState.Month, m_WorldState.Day, m_WorldState.Hour, m_WorldState.Minute);
            m_ActorsActive = activePlayer;
            if (m_HuntBot)
            {
                PredateActorSimulation.SetHeld(m_HuntBot, !activePlayer);
                if (activePlayer) m_HuntBot.SetMovementSpeedLimits(2, 3);
                else m_HuntBot.SetMovementSpeedLimits(0, 0);
            }
        }
        if (m_PeerRoster && !m_PeerRoster.Advance(this, activeParticipant, registryError, index))
        {
            PredatePersistence.Fail(this, registryError);
            return;
        }
        if (activePlayer)
        {
            PredateHuntRuntime.Tick(this, activeParticipant, activePlayerId);
            PredateSupplyService.Tick(this, activeParticipant);
        }
        m_HuntStateTimer += seconds;
        if (m_HuntStateTimer >= 1.25) m_HuntStateTimer = 0;
    }

    void PredateHandleTrackingRequest()
    {
        PlayerBase player = PlayerBase.s_PredateTrackingRequester;
        if (!player || m_Paused) return;
        PlayerBase.s_PredateTrackingRequester = null;
        int request = player.m_PredateTrackingRequest;
        string operatorId = player.m_PredateRequestedOperator;
        player.m_PredateTrackingRequest = 0;
        player.m_PredateRequestedOperator = "raven-01";
        if (m_Suspending || !m_HuntReady || !m_HuntSchemaValid || m_HuntError != "") return;
        if (!player.GetIdentity() || !player.IsAlive() || m_WorldState.PlayerAwaitingRedeploy) return;

        string selection;
        if (request == 1) selection = operatorId;
        else if (request != 2) return;
        string previousSelection;
        PredatePeerBinding previousPeer;
        if (m_PeerRoster) previousPeer = m_PeerRoster.PlayerLock();
        if (previousPeer) previousSelection = previousPeer.State.Operator.Id;
        else if (m_WorldState.PlayerSignal.Selected) previousSelection = m_HuntProgress.Operator.Id;
        if (selection == previousSelection) return;

        if (m_PeerRoster)
        {
            if (!m_PeerRoster.SelectPlayerTarget(this, selection)) return;
        }
        else
        {
            if (request == 1 && operatorId != m_HuntProgress.Operator.Id) return;
            if (selection != "" && m_WorldState.PlayerSignal.HasLostPlayerReception()) return;
            PredateSignalService.Select(this, selection != "");
        }
        if (m_Score)
        {
            if (previousPeer) m_Score.RecordPursuitOutcome(previousPeer.State.Operator, "Abandoned");
            else if (previousSelection != "") m_Score.RecordPursuitOutcome(m_HuntProgress.Operator, "Abandoned");
            PredateLifeScore trackingLife = m_Score.Life(m_WorldState.PlayerLife, true);
            if (trackingLife) m_Score.RefreshTracking(player, trackingLife);
        }
        player.m_PredateTrackingChoice++;
        // Confirm on the next server frame. Routine receiver refreshes and
        // checkpoint writes remain on their existing schedules.
        PredateSend(player);
        m_HuntStateTimer = 0;
        PredateRequestCheckpoint(true);
    }

    void Update(float timeslice)
    {
        if (m_Bridge) m_Bridge.Update(timeslice);
        if (m_DiscardResumeStep)
        {
            m_DiscardResumeStep = false;
            m_SoloCombatTimer = 0;
            return;
        }
        PredateHandleTrackingRequest();
        m_SoloCombatTimer += timeslice;
        bool updatedHunt = m_SoloCombatTimer >= 0.25;
        if (updatedHunt)
        {
            float elapsed = m_SoloCombatTimer;
            m_SoloCombatTimer = 0;
            PredateTick(elapsed);
            array<Man> players = new array<Man>;
            GetGame().GetPlayers(players);
            foreach (Man man : players)
            {
                PlayerBase player = PlayerBase.Cast(man);
                SoloApplyCombatRules(player);
                if (player) player.PredateUpdateVitalHud();
            }
        }
        PredateReportFrames(timeslice);
        PredateUpdateCheckpoint(timeslice, updatedHunt);
    }

    void Finish()
    {
        PredateSave();
    }
}
