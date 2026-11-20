// Authoritative event producer. Receivers and the hunter only consume samples.
class PredateSignalService
{
    static PredateSignalTrack Track(string owner, string subject)
    {
        PredateSignalTrack track = new PredateSignalTrack;
        track.OwnerId = owner;
        track.SubjectId = subject;
        return track;
    }

    static bool Load(PredateSession s)
    {
        PredateWorldState state = s.m_WorldState;
        float now = state.SimulationSeconds;
        if (state.SignalSchema == 0)
        {
            // Native JSON constructs default child objects for missing reference fields.
            bool hasLegacyData;
            if (state.PlayerSignal && (state.PlayerSignal.OwnerId != "" || state.PlayerSignal.EverLocked || state.PlayerSignal.History.Count() > 0)) hasLegacyData = true;
            if (state.BotSignal && (state.BotSignal.OwnerId != "" || state.BotSignal.EverLocked || state.BotSignal.History.Count() > 0)) hasLegacyData = true;
            if (state.PlayerActivity && state.PlayerActivity.Energy != 0) hasLegacyData = true;
            if (state.BotActivity && state.BotActivity.Energy != 0) hasLegacyData = true;
            if (hasLegacyData)
                return PredatePersistence.Fail(s, "Signal data has no schema");
            state.PlayerSignal = Track("player-01", "raven-01");
            state.PlayerActivity = new PredateSignatureActivity;
            state.BotActivity = new PredateSignatureActivity;
            // Retain the legacy cooldown, but never fabricate an old sample origin.
            state.PlayerSignal.Selected = s.m_HuntProgress.Selected;
            state.PlayerSignal.EverLocked = s.m_HuntProgress.Selected || s.m_HuntProgress.HasPulse || s.m_HuntProgress.PulseRemaining > 0;
            state.PlayerSignal.NextPulseAt = now + s.m_HuntProgress.PulseRemaining;
            s.m_HuntProgress.HasPulseOrigin = false;
            state.SignalSchema = 1;
        }
        if (state.SignalSchema != 1 || !state.PlayerSignal || !state.PlayerActivity || !state.BotActivity)
            return PredatePersistence.Fail(s, "Unsupported signal state");
        if (state.PlayerSignal.OwnerId != "player-01" || state.PlayerSignal.SubjectId != "raven-01")
            return PredatePersistence.Fail(s, "Invalid signal ownership");
        state.PlayerSignal.NormalizeLoaded();
        state.BotSignal = null;
        if (!state.PlayerSignal.Validate(now)) return PredatePersistence.Fail(s, "Invalid signal schedule/history");
        if (!state.HuntReceiver) state.HuntReceiver = new PredateHuntReceiver;
        if (!state.HuntReceiver.Valid(now, s.m_HuntProgress.Operator.Id)) return PredatePersistence.Fail(s, "Invalid private hunt receiver");
        state.PlayerSignal.UseBearingSectors(now);
        SyncReceiverProgress(s);
        if (!ValidEnergy(state.PlayerActivity.Energy) || !ValidEnergy(state.BotActivity.Energy)) return PredatePersistence.Fail(s, "Invalid signature activity");
        if (!PredateInformationMath.Finite(state.PlayerInterceptionUntil) || !PredateInformationMath.Finite(state.BotInterceptionUntil) || state.PlayerInterceptionUntil < 0 || state.BotInterceptionUntil < 0)
            return PredatePersistence.Fail(s, "Invalid interception warning");
        return true;
    }

    static bool ValidEnergy(float energy)
    {
        return PredateInformationMath.Finite(energy) && energy >= 0 && energy <= 1;
    }

    static void CaptureActor(PlayerBase actor, PredateSignatureActivity activity)
    {
        if (!actor || !activity) return;
        int count = actor.m_PredatePendingShots;
        actor.m_PredatePendingShots = 0;
        activity.RecordShots(count);
        int blasts = actor.m_PredatePendingExplosions;
        actor.m_PredatePendingExplosions = 0;
        activity.RecordExplosions(blasts);
    }

    static bool OperatingEngine(PlayerBase actor)
    {
        if (!actor) return false;
        HumanCommandVehicle command = actor.GetCommand_Vehicle();
        if (!command) return false;
        Transport transport = command.GetTransport();
        // Seat animation types are not crew indexes. Attribute the engine to
        // the actual driver, never another occupant using the same animation.
        if (!transport || transport.CrewMemberIndex(actor) != 0) return false;
        ExpansionVehicle vehicle;
        if (!ExpansionVehicle.Get(vehicle, transport)) return false;
        return vehicle.EngineIsOn();
    }

    static void Capture(PredateSession s)
    {
        if (!s.m_HuntConfig.EnableSharedSignals || !s.m_WorldState) return;
        CaptureActor(s.m_HuntBot, s.m_WorldState.BotActivity);
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (player && player.GetIdentity()) CaptureActor(player, s.m_WorldState.PlayerActivity);
        }
    }

    static void Select(PredateSession s, bool selected)
    {
        s.m_HuntProgress.Selected = selected;
        s.m_WorldState.PlayerSignal.SetPlayerTracking(selected);
        SyncReceiverProgress(s);
    }

    static void ClearDefeatedSignal(PredateSignalTrack track)
    {
        track.SetPlayerTracking(false);
        track.LastSample = null;
        track.History.Clear();
        track.Reception = PredatePulseReception.UNAVAILABLE;
    }

    static void ConfirmPlayerKills(PredateSession session, array<int> kills)
    {
        foreach (int slot : kills)
        {
            if (slot == 0)
            {
                ClearDefeatedSignal(session.m_WorldState.PlayerSignal);
                session.m_WorldState.BotInterceptionUntil = 0;
                SyncReceiverProgress(session);
                continue;
            }
            if (!session.m_PeerRoster || slot > session.m_PeerRoster.m_Bindings.Count()) continue;
            PredatePeerBinding peer = session.m_PeerRoster.m_Bindings[slot - 1];
            ClearDefeatedSignal(peer.State.PlayerReceiver);
            peer.State.InterceptedUntil = 0;
        }
    }

    static void WriteHistory(PredateSession s, ScriptRPC rpc, bool visible, PredateSignalTrack track)
    {
        if (!visible || !track)
        {
            int emptyCount;
            rpc.Write(emptyCount);
            return;
        }
        int count = Math.Min(track.History.Count(), PredateProtocol.MAX_BEARING_HISTORY);
        rpc.Write(count);
        for (int index = 0; index < count; index++)
        {
            PredateObservation sample = track.History[track.History.Count() - 1 - index];
            int bearing = Math.Round(sample.Bearing);
            bearing = bearing % 360;
            rpc.Write(bearing);
            rpc.Write(sample.BearingError);
            float age = Math.Max(0, s.m_WorldState.SimulationSeconds - sample.ObservedAt);
            rpc.Write(age);
            rpc.Write(sample.Origin);
            int distanceBand = Math.Ceil(sample.PulseDistance / PredateBearingPulse.BAND_METRES);
            rpc.Write(distanceBand);
        }
    }

    static bool SamplePlayerPulse(PredateSignalTrack track, PlayerBase player, PlayerBase actor, float now, PredateConfig config)
    {
        if (!track) return false;
        track.UseBearingSectors(now);
        if (now < track.NextPulseAt) return false;
        track.BroadcastTime = now;
        track.NextPulseAt = now + PredateBearingPulse.INTERVAL;
        vector origin = player.GetPosition();
        vector position;
        bool transmitting = actor && actor.IsAlive();
        if (transmitting) position = actor.GetPosition();
        track.Reception = PredateSignalRules.Reception(origin, position, transmitting, config);
        PredateObservation reading;
        if (track.Reception == PredatePulseReception.BEARING)
            reading = PredateSignalRules.ReceiverReading(track, origin, position, now, config);
        track.RecordSample(reading);
        return true;
    }

    static void AddMapPulse(array<PredateSignalTrack> tracks, PredateSignalTrack track, float now)
    {
        if (!track || track.PlayerPulseFormat != 2 || !track.LastSample || track.LastSample.Expired(now)) return;
        // Sort on the server so the overlay can stack even rivals in the same distance band.
        int position;
        while (position < tracks.Count() && tracks[position].LastSample.PulseDistance >= track.LastSample.PulseDistance)
            position++;
        tracks.InsertAt(track, position);
    }

    static void WriteMapPulses(PredateSession s, ScriptRPC rpc, bool visible)
    {
        int count;
        if (!visible || !s.m_WorldState)
        {
            rpc.Write(count);
            return;
        }
        array<PredateSignalTrack> tracks = {};
        float now = s.m_WorldState.SimulationSeconds;
        AddMapPulse(tracks, s.m_WorldState.PlayerSignal, now);
        if (s.m_PeerRoster) s.m_PeerRoster.AddMapPulses(tracks, now);
        count = tracks.Count();
        rpc.Write(count);
        foreach (PredateSignalTrack track : tracks)
        {
            rpc.Write(track.SubjectId);
            int sector = track.LastSample.Bearing;
            rpc.Write(sector);
            int distanceBand = Math.Ceil(track.LastSample.PulseDistance / PredateBearingPulse.BAND_METRES);
            rpc.Write(distanceBand);
        }
    }

    static void MarkPlayerHunted(PredateSession s, float now)
    {
        // Renew a single hunt episode across successive interception pulses.
        float until = now + PredateBearingPulse.INTERVAL + 30;
        s.m_WorldState.PlayerInterceptionUntil = Math.Max(s.m_WorldState.PlayerInterceptionUntil, until);
    }

    static void SyncReceiverProgress(PredateSession s)
    {
        PredateSignalTrack track = s.m_WorldState.PlayerSignal;
        float now = s.m_WorldState.SimulationSeconds;
        s.m_HuntProgress.Selected = track.Selected;
        s.m_HuntProgress.PulseRemaining = Math.Max(0, track.NextPulseAt - now);
        s.m_HuntProgress.HasPulse = false;
        s.m_HuntProgress.HasPulseOrigin = false;
        if (track.LastSample)
        {
            s.m_HuntProgress.HasPulse = !track.LastSample.Expired(now);
            s.m_HuntProgress.HasPulseOrigin = true;
            s.m_HuntProgress.PulseOrigin = track.LastSample.Origin;
            s.m_HuntProgress.LastBearing = track.LastSample.Bearing;
            s.m_HuntProgress.LastBearingError = track.LastSample.BearingError;
            s.m_HuntProgress.PulseAge = Math.Max(0, now - track.LastSample.ObservedAt);
        }
    }

    static void Tick(PredateSession s, PlayerBase player, float seconds)
    {
        PredateWorldState state = s.m_WorldState;
        float now = state.SimulationSeconds;
        bool sampled = SamplePlayerPulse(state.PlayerSignal, player, s.m_HuntBot, now, s.m_HuntConfig);
        if (sampled && state.PlayerSignal.Selected && state.PlayerSignal.LastSample) state.BotInterceptionUntil = now + PredateBearingPulse.INTERVAL + 30;
        SyncReceiverProgress(s);
        if (!s.m_HuntConfig.EnableSharedSignals) return;
        player.m_PredateSignatureRegistered = true;
        Capture(s);
        if (OperatingEngine(player)) state.PlayerActivity.RecordEngineUse(seconds);
        if (OperatingEngine(s.m_HuntBot)) state.BotActivity.RecordEngineUse(seconds);
        state.PlayerActivity.Advance(seconds);
        state.BotActivity.Advance(seconds);
    }

    static void NewDeployment(PredateSession s, bool preserveReceiver = false)
    {
        s.m_WorldState.Hunter = new PredateHunterState;
        if (!preserveReceiver) s.m_WorldState.PlayerSignal = Track("player-01", "raven-01");
        s.m_WorldState.HuntReceiver = new PredateHuntReceiver;
        s.m_WorldState.BotActivity = new PredateSignatureActivity;
        if (!preserveReceiver) s.m_WorldState.PlayerInterceptionUntil = 0;
        s.m_WorldState.BotInterceptionUntil = 0;
        s.m_HuntProgress.HasPulseOrigin = false;
    }

    static void PlayerRedeployed(PredateSession s)
    {
        PredateWorldState state = s.m_WorldState;
        state.PlayerSignal.SetPlayerTracking(false);
        state.PlayerActivity = new PredateSignatureActivity;
        state.PlayerInterceptionUntil = 0;
        state.BotInterceptionUntil = 0;
        s.m_HuntProgress.Selected = false;
        s.m_HuntProgress.HasPulse = false;
        s.m_HuntProgress.HasPulseOrigin = false;
        s.m_HuntProgress.PulseAge = 0;
        s.m_HuntProgress.PulseRemaining = 0;
        s.m_HuntProgress.MapRadius = 0;
    }

}
