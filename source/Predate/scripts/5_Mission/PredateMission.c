// Session coordinator: simulation lifecycle, services and engine callbacks.
class PredateSession
{
    ref PredateConfig m_HuntConfig;
    ref PredateLoadoutCatalog m_Loadouts;
    ref PredateProgress m_HuntProgress;
    ref eAIGroup m_HuntGroup;
    eAIBase m_HuntBot;
    float m_HuntStateTimer;
    float m_HuntSaveTimer;
    float m_PredateNextLightScan;
    bool m_HuntReady;
    bool m_HuntSentState;
    bool m_HuntSentCloseApproach;
    bool m_HuntSchemaValid;
    bool m_Development;
    string m_HuntError;
    ref PredateOperatorRegistry m_Operators;
    ref PredatePeerRoster m_PeerRoster;
    float m_SoloCombatTimer;
    float m_SoloCombatReportTimer;
    ref PredateServerBridge m_Bridge;
    ref PredateWorldState m_WorldState;
    bool m_MemoryRestored;
    bool m_ActorsActive;
    bool m_Suspending;
    bool m_Paused;
    bool m_PauseReady;
    bool m_DiscardResumeStep;
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
        PredateInit();
    }

    void PredateInit()
    {
        m_HuntSchemaValid = PredatePersistence.Load(this);
        if (!m_HuntSchemaValid) return;
        m_Operators = new PredateOperatorRegistry;
        if (!m_Operators.Register(m_HuntProgress.Operator))
        {
            PredatePersistence.Fail(this, "Duplicate operator registration");
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateStart, 6000, false);
    }

    void PredateSave() { PredatePersistence.Save(this); }

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
        actor.DisableSimulation(true);
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
                if (held) held.DisableSimulation(false);
            foreach (Man damageHeld : m_PauseDamageActors)
                if (damageHeld) damageHeld.SetAllowDamage(true);
            m_PauseActors.Clear();
            m_PauseDamageActors.Clear();
            m_MemoryRestored = false;
            m_DiscardResumeStep = true;
        }
        m_Paused = paused;
        if (GetGame().IsDedicatedServer()) PredateServerPauseDriver.Held = paused;
        if (paused) GetGame().GetWorld().SetTimeMultiplier(0);
        else GetGame().GetWorld().SetTimeMultiplier(-1);
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
        if (m_HuntBot) m_HuntBot.DisableSimulation(true);
        if (m_PeerRoster) m_PeerRoster.StopActors();
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (player && player.GetIdentity()) player.DisableSimulation(true);
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
        return track && track.Selected && track.Reception != PredatePulseReception.UNAVAILABLE && target && target.IsAlive() && PredateSignalRules.FinalApproach(player.GetPosition(), target.GetPosition());
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
        string operatorId = "raven-01";
        int availableOperators = -1;
        PredatePeerBinding chosen;
        if (m_PeerRoster)
        {
            chosen = m_PeerRoster.PlayerLock();
            if (chosen)
            {
                receiverTrack = chosen.State.PlayerReceiver;
                operatorId = chosen.State.Operator.Id;
            }
            if (receiverReady && m_WorldState.PlayerSignal) availableOperators = m_PeerRoster.AvailableCount(this);
        }
        string signature = PredateProtocol.SignatureFor(operatorId);
        bool development = m_Development && !m_PeerRoster;
        bool available = receiverReady && receiverTrack && receiverTrack.Reception != PredatePulseReception.UNAVAILABLE;
        bool locked = receiverReady && receiverTrack && receiverTrack.Selected;
        bool canDeploy = receiverReady && !available && m_HuntProgress.Defeated && !m_PeerRoster && !m_HuntConfig.EnableOperatorRespawns;
        bool closeApproach = available && locked && PredatePlayerCloseApproach(player, chosen);
        bool intercepted = receiverReady && m_HuntConfig.EnableSharedSignals && m_WorldState.PlayerInterceptionUntil > m_WorldState.SimulationSeconds;
        float now = m_WorldState.SimulationSeconds;
        PredateObservation sample;
        if (receiverTrack) sample = receiverTrack.LastSample;
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
        if (receiverTrack) nextPulse = Math.Max(0, receiverTrack.NextPulseAt - now);
        string message;
        if (needsRedeploy) message = "Deployment ended";
        else if (!receiverReady) message = "Waiting for receiver initialization...";
        else if (locked && available) message = "Tracking " + PredateProtocol.DisplayName(operatorId);
        else if (locked) message = PredateProtocol.DisplayName(operatorId) + "’s signal ended.";
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
        rpc.Write(hasPulse);
        rpc.Write(closeApproach);
        rpc.Write(bearing);
        rpc.Write(age);
        rpc.Write(nextPulse);
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
        if (!m_Operators.Resolve("raven-01", resolvedActor, resolvedGroup, registryError))
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
            Print("[Predate] Rebound registered operator raven-01 by its persistent storage key.");
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
            Print("[Predate] RAVEN-01 eliminated.");
            PredateSave();
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
            string requestedOperator = player.m_PredateRequestedOperator;
            player.m_PredateRequest = 0;
            player.m_PredateRequestedOperator = "raven-01";
            if ((request == 1 || request == 2) && m_PeerRoster)
            {
                string selection;
                if (request == 1) selection = requestedOperator;
                if (m_PeerRoster.SelectPlayerTarget(this, selection)) PredateSave();
            }
            else if (request == 1 && requestedOperator == "raven-01" && m_WorldState.PlayerSignal.Reception != PredatePulseReception.UNAVAILABLE)
            {
                PredateSignalService.Select(this, true);
                Print("[Predate] Player selected RAVEN-01.");
                PredateSave();
            }
            else if (request == 2 && requestedOperator == "raven-01")
            {
                PredateSignalService.Select(this, false);
                PredateSave();
            }
            else if (request == 3 && m_Development)
            {
                PredateResetHunt(player);
            }
            else if (request == 4)
            {
                PredateOperatorService.PredateNewDeployment(this);
            }
            PredateSignalService.Tick(this, player, seconds);
            if (m_PeerRoster) m_PeerRoster.SampleForPlayer(this, player);
            PredatePeerBinding chosen;
            if (m_PeerRoster) chosen = m_PeerRoster.PlayerLock();
            bool closeApproach = PredatePlayerCloseApproach(player, chosen);
            if (closeApproach != m_HuntSentCloseApproach || player.m_PredateKillNotices.Count() > 0 || m_HuntStateTimer >= 1)
            {
                bool confirmedKill = player.m_PredateKillNotices.Count() > 0;
                PredateSend(player);
                if (confirmedKill) PredateSave();
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
                m_HuntBot.DisableSimulation(!activePlayer);
                if (activePlayer) m_HuntBot.SetMovementSpeedLimits(2, 3);
                else m_HuntBot.SetMovementSpeedLimits(0, 0);
            }
        }
        if (m_PeerRoster && !m_PeerRoster.Advance(this, activeParticipant, registryError))
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
        m_HuntSaveTimer += seconds;
        if (m_HuntSaveTimer >= 30) { m_HuntSaveTimer = 0; PredateSave(); }
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
        m_SoloCombatTimer += timeslice;
        m_SoloCombatReportTimer += timeslice;
        if (m_SoloCombatTimer < 0.25)
            return;
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
        if (m_SoloCombatReportTimer >= 60)
        {
            m_SoloCombatReportTimer = 0;
            Print("[SoloCombat] Rules applied to connected players: " + players.Count());
        }
    }

    void Finish()
    {
        PredateSave();
    }
}
