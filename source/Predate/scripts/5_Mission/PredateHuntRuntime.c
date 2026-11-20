class PredateHuntParticipant
{
    string Id;
    PlayerBase Actor;
    vector Position;
    PredateHuntReceiver Receiver;
    PredateHunterState Hunter;
    PredateObservationMemory Memory;
    float InterceptedUntil;

    void PredateHuntParticipant(string id, PlayerBase actor, PredateHuntReceiver receiver = null, PredateHunterState hunter = null, PredateObservationMemory memory = null, float interceptedUntil = 0)
    {
        Id = id;
        Actor = actor;
        Position = actor.GetPosition();
        Receiver = receiver;
        Hunter = hunter;
        Memory = memory;
        InterceptedUntil = interceptedUntil;
    }
}

class PredateHuntRuntime
{
    static void Prepare(PredateHuntReceiver receiver, PredateHunterState hunter, string owner, float now)
    {
        if (receiver.OwnerId != "") return;
        receiver.OwnerId = owner;
        receiver.LastDecisionAt = now;
        PredateHunterBrain.Hold(hunter, "hold", "Await private receiver readings");
        hunter.LastContactKey = "";
        hunter.LastSignalKey = "";
    }

    static void Scan(PredateHuntParticipant observer, array<ref PredateHuntParticipant> participants, PredateConfig config, float now)
    {
        PredateHuntReceiver receiver = observer.Receiver;
        if (now < receiver.NextPulseAt) return;
        receiver.NextPulseAt = now + PredateBearingPulse.INTERVAL;
        array<ref PredateSignalTrack> order = {};
        array<ref PredateObservation> readings = {};
        array<float> ranges = {};
        vector origin = observer.Position;
        foreach (PredateHuntParticipant contact : participants)
        {
            if (contact.Id == observer.Id) continue;
            PredateSignalTrack track = receiver.Find(contact.Id);
            bool existing = track != null;
            if (!track) track = PredateSignalService.Track(observer.Id, contact.Id);
            vector samplePosition = contact.Position;
            track.Reception = PredateSignalRules.Reception(origin, samplePosition, true, config);
            PredateObservation sample;
            if (track.Reception == PredatePulseReception.BEARING)
                sample = PredateSignalRules.ReceiverReading(track, origin, samplePosition, now, config);
            if (!sample)
            {
                if (existing) track.RecordSample(null);
                continue;
            }
            if (!existing) receiver.Tracks.Insert(track);
            track.BroadcastTime = now;
            track.NextPulseAt = receiver.NextPulseAt;
            float distance = PredateHunterBrain.Distance(origin, samplePosition);
            int at;
            while (at < ranges.Count() && ranges[at] >= distance) at++;
            order.InsertAt(track, at);
            readings.InsertAt(sample, at);
            ranges.InsertAt(distance, at);
        }
        for (int index = 0; index < order.Count(); index++)
        {
            PredateObservation reading = readings[index];
            for (int preceding = 0; preceding < index; preceding++)
                if (PredateBearingPulse.Overlap(reading.Bearing, readings[preceding].Bearing))
                    reading.OverlapLane = Math.Max(reading.OverlapLane, readings[preceding].OverlapLane + 1);
            order[index].RecordSample(reading);
        }
    }

    static void Tick(PredateSession session, PlayerBase player, string playerId)
    {
        if (!session.m_HuntConfig.EnableStrategicHunter) return;
        PredateWorldState worldState = session.m_WorldState;
        float now = worldState.SimulationSeconds;
        array<ref PredateHuntParticipant> participants = {new PredateHuntParticipant("player-01", player)};
        if (session.m_HuntBot && session.m_HuntBot.IsAlive())
            participants.Insert(new PredateHuntParticipant(worldState.Hunter.OwnerId, session.m_HuntBot, worldState.HuntReceiver, worldState.Hunter, worldState.Observations, worldState.BotInterceptionUntil));
        if (session.m_PeerRoster)
        {
            foreach (PredatePeerBinding binding : session.m_PeerRoster.m_Bindings)
            {
                if (!binding.Actor || !binding.Actor.IsAlive()) continue;
                PredatePeerState peer = binding.State;
                participants.Insert(new PredateHuntParticipant(peer.Operator.Id, binding.Actor, peer.HuntReceiver, peer.Hunter, peer.Memory, peer.InterceptedUntil));
            }
        }
        if (now >= session.m_PredateNextLightScan)
        {
            session.m_PredateNextLightScan = now + 0.75;
            PredatePEQClues.Scan(participants, now);
        }
        array<string> live = {};
        foreach (PredateHuntParticipant participant : participants) live.Insert(participant.Id);
        foreach (PredateHuntParticipant observer : participants)
        {
            if (!observer.Receiver) continue;
            if (observer.Receiver.OwnerId == "")
                for (int legacy = observer.Memory.Entries.Count() - 1; legacy >= 0; legacy--)
                    if (observer.Memory.Entries[legacy].Source == PredateObservationSource.SIGNAL) observer.Memory.Entries.RemoveOrdered(legacy);
            Prepare(observer.Receiver, observer.Hunter, observer.Id, now);
            if (now < observer.Receiver.NextPulseAt) continue;
            for (int index = observer.Receiver.Tracks.Count() - 1; index >= 0; index--)
            {
                string subject = observer.Receiver.Tracks[index].SubjectId;
                if (live.Find(subject) >= 0) continue;
                if (observer.Receiver.TargetId == subject)
                    PredateHunterBrain.Hold(observer.Hunter, "hold", "Contact dropped from the receiver");
                observer.Receiver.Retire(subject);
                if (subject == "player-01") subject = playerId;
                for (int memoryIndex = observer.Memory.Entries.Count() - 1; memoryIndex >= 0; memoryIndex--)
                    if (observer.Memory.Entries[memoryIndex].SubjectId == subject) observer.Memory.Entries.RemoveOrdered(memoryIndex);
            }
            Scan(observer, participants, session.m_HuntConfig, now);
        }
        int routeBudget = 3;
        foreach (PredateHuntParticipant hunter : participants)
        {
            if (!hunter.Receiver) continue;
            if (Think(session, hunter, playerId, now, routeBudget > 0)) routeBudget--;
        }
    }

    static bool Think(PredateSession session, PredateHuntParticipant hunter, string playerId, float now, bool canPlan)
    {
        eAIBase actor = eAIBase.Cast(hunter.Actor);
        if (!actor || !actor.IsAlive()) return false;
        if (!actor.m_PredateAIConfigured)
        {
            actor.m_PredateEliteAI = session.m_HuntConfig.EliteAI;
            actor.m_PredateAITrace = FileExist("$profile:PredateAITrace.txt");
            actor.m_PredateAIConfigured = true;
        }
        bool combat = actor.PredateTacticalPriority(now);
        bool recover = actor.PredateOwnActionPriority();
        bool limited = actor.m_PredateEliteAI && (actor.GetHealth01("", "Health") <= 0.5 || actor.GetHealth01("", "Blood") < 0.7);
        string previousMode = hunter.Hunter.Mode;
        PredateHuntReceiver receiver = hunter.Receiver;
        vector position = hunter.Position;
        PredateHuntSelection.Update(receiver, hunter.Hunter, position, now, combat || recover);
        PredateSignalTrack track = receiver.Find(receiver.TargetId);
        string subject = receiver.TargetId;
        if (subject == "player-01") subject = playerId;
        if (!combat && !recover && !limited && now >= hunter.Hunter.WatchUntil && track && track.Reception == PredatePulseReception.CLOSE && track.History.Count() > 0)
        {
            PredateObservation last = track.History[track.History.Count() - 1];
            if (last.DistanceBand <= 2 && !last.Expired(now) && receiver.FinalSearchKey != last.EventKey)
            {
                receiver.FinalSearchKey = last.EventKey;
                hunter.Hunter.SearchCenter = PredateHuntSelection.SectorCenter(last);
                hunter.Hunter.SearchRadius = Math.Min(200, PredateHuntSelection.SectorRadius(last, now));
                hunter.Hunter.SearchObservedAt = now;
                hunter.Hunter.SearchUntil = now + PredateBearingPulse.INTERVAL;
                hunter.Hunter.SearchStep = 0;
                hunter.Hunter.CheckedSectors = 0;
                PredateHunterBrain.Goal(hunter.Hunter, position, hunter.Hunter.SearchCenter, now, "search", "Check the area from the last close pulse", last.EventKey);
            }
        }
        if (actor.m_PredateEliteAI && !combat && !recover && hunter.Hunter.HasGoal && hunter.Hunter.WatchUntil > 0 && now >= hunter.Hunter.WatchUntil && hunter.Hunter.GoalRevision == hunter.Hunter.SearchWatchRevision)
        {
            PredateRoutePlanner.RecordObservedSectors(actor, hunter.Hunter, now);
            hunter.Hunter.WatchUntil = 0;
        }
        PredateHunterBrain.Step(hunter.Hunter, position, now, hunter.Memory, track, combat, recover, now < hunter.InterceptedUntil, session.m_WorldState.PlayerLastDeathAt, playerId, subject, receiver.Replanned, actor.m_PredateEliteAI, limited);
        if (track && track.LastSample && track.LastSample.ObservedAt == now && session.m_PeerRoster)
            session.m_PeerRoster.MarkInterception(session, track.SubjectId, now);
        else if (track && track.LastSample && track.LastSample.ObservedAt == now && track.SubjectId == "player-01")
            PredateSignalService.MarkPlayerHunted(session, now);
        bool planned;
        if (combat || recover)
        {
            if (actor.m_PredateAppliedGoal != -1) actor.PredateReleaseRoute();
        }
        else
        {
            if (!limited && !hunter.Hunter.HasGoal && receiver.TargetId == "" && now >= hunter.Hunter.ScoutAt && now >= hunter.Hunter.WatchUntil)
                Scout(hunter.Hunter, position, now);
            bool needsPath = hunter.Hunter.HasGoal && actor.m_PredateAppliedGoal != hunter.Hunter.GoalRevision;
            if (!needsPath || canPlan)
            {
                actor.PredateApplyRoute(hunter.Hunter);
                planned = needsPath;
            }
        }
        if (previousMode != hunter.Hunter.Mode && actor.m_PredateAITrace)
            Print("[PredateIntent] operator=" + hunter.Id + "; simulation=" + now + "; previous=" + previousMode + "; next=" + hunter.Hunter.Mode + "; evidence=" + hunter.Hunter.EvidenceKey + "; revision=" + hunter.Hunter.GoalRevision + "; reason=" + hunter.Hunter.Reason);
        return planned;
    }

    static void Scout(PredateHunterState state, vector own, float now)
    {
        state.ScoutAt = now + 12;
        float heading = PredateMath.WrapHeading(state.ScoutHeading + Math.RandomFloat(-55, 55));
        if (state.GoalRevision == 0) heading = Math.RandomFloat(0, 359);
        float best = 1000000;
        vector destination;
        for (int index = 0; index < 4; index++)
        {
            float bearing = PredateMath.WrapHeading(heading + index * 90);
            vector point = own + PredateInformationMath.BearingDirection(bearing) * 160;
            if (!PredateRoutePlanner.Land(point)) continue;
            point[1] = GetGame().SurfaceY(point[0], point[2]);
            vector grounded;
            if (!PredateRoutePlanner.Snap(point, 12, grounded)) continue;
            float cost = PredateRoutePlanner.LegCost(own, grounded) + index * 5;
            if (cost >= best) continue;
            best = cost;
            destination = grounded;
        }
        if (best >= 1000000)
        {
            state.ScoutHeading = PredateMath.WrapHeading(heading + 90);
            return;
        }
        state.ScoutHeading = PredateMath.Bearing(own, destination);
        state.SearchUntil = now + 100;
        PredateHunterBrain.Goal(state, own, destination, now, "scout", "Scout traversable terrain while the receiver is quiet", "");
    }
}
