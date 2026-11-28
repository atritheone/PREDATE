class PredateHuntParticipant
{
    string Id;
    PlayerBase Actor;
    vector Position;
    PredateHuntReceiver Receiver;
    PredateHunterState Hunter;
    PredateObservationMemory Memory;
    float InterceptedUntil;

    void PredateHuntParticipant(string id = "", PlayerBase actor = null, PredateHuntReceiver receiver = null, PredateHunterState hunter = null,
        PredateObservationMemory memory = null, float interceptedUntil = 0)
    {
        if (actor) Sample(id, actor, receiver, hunter, memory, interceptedUntil);
    }

    void Sample(string id, PlayerBase actor, PredateHuntReceiver receiver, PredateHunterState hunter, PredateObservationMemory memory, float interceptedUntil)
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

class PredateHuntScratch
{
    ref array<ref PredateHuntParticipant> Participants = {};
    ref map<string, vector> Live = new map<string, vector>;
    protected ref array<ref PredateHuntParticipant> m_Pool = {};
    protected ref array<ref PredateSignalTrack> m_Order = {};
    protected ref array<ref PredateObservation> m_Readings = {};
    protected ref array<float> m_Ranges = {};
    protected ref array<int> m_BearingLanes = {};

    void BeginTick()
    {
        Participants.Clear();
        Live.Clear();
    }

    void AddParticipant(string id, PlayerBase actor, PredateHuntReceiver receiver = null, PredateHunterState hunter = null,
        PredateObservationMemory memory = null, float interceptedUntil = 0)
    {
        int index = Participants.Count();
        if (index == m_Pool.Count()) m_Pool.Insert(new PredateHuntParticipant);
        PredateHuntParticipant participant = m_Pool[index];
        participant.Sample(id, actor, receiver, hunter, memory, interceptedUntil);
        Participants.Insert(participant);
        Live.Insert(id, participant.Position);
    }

    void AddReading(PredateSignalTrack track, PredateObservation reading, float distance)
    {
        int first = 0;
        int end = m_Ranges.Count();
        while (first < end)
        {
            int middle = (first + end) / 2;
            if (m_Ranges[middle] >= distance) first = middle + 1;
            else end = middle;
        }
        m_Order.InsertAt(track, first);
        m_Readings.InsertAt(reading, first);
        m_Ranges.InsertAt(distance, first);
    }

    void PublishReadings()
    {
        // A lane is one above the highest prior overlapping lane. Integer
        // pulse starts let dense receivers query just the neighboring bearings.
        bool dense = m_Readings.Count() >= 64;
        if (dense)
        {
            m_BearingLanes.Resize(360);
            for (int bearingSlot = 0; bearingSlot < 360; bearingSlot++) m_BearingLanes[bearingSlot] = 0;
        }
        for (int index = 0; index < m_Readings.Count(); index++)
        {
            PredateObservation reading = m_Readings[index];
            if (dense)
            {
                int bearing = Math.Floor(reading.Bearing);
                int slot = (bearing + 360 - PredateBearingPulse.DEGREES + 1) % 360;
                for (int offset = 0; offset < 2 * PredateBearingPulse.DEGREES - 1; offset++)
                {
                    reading.OverlapLane = Math.Max(reading.OverlapLane, m_BearingLanes[slot]);
                    slot++;
                    if (slot == 360) slot = 0;
                }
                m_BearingLanes[bearing] = reading.OverlapLane + 1;
            }
            else
            {
                for (int preceding = 0; preceding < index; preceding++)
                    if (PredateBearingPulse.Overlap(reading.Bearing, m_Readings[preceding].Bearing))
                        reading.OverlapLane = Math.Max(reading.OverlapLane, m_Readings[preceding].OverlapLane + 1);
            }
            m_Order[index].RecordSample(reading);
        }
        m_Order.Clear();
        m_Readings.Clear();
        m_Ranges.Clear();
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

    static void Scan(PredateHuntParticipant observer, array<ref PredateHuntParticipant> participants, PredateConfig config, float now,
        PredateHuntScratch workspace = null)
    {
        PredateHuntReceiver receiver = observer.Receiver;
        if (now < receiver.NextPulseAt) return;
        receiver.NextPulseAt = now + PredateBearingPulse.INTERVAL;
        PredateHuntScratch scratch = workspace;
        if (!scratch) scratch = new PredateHuntScratch;
        vector origin = observer.Position;
        foreach (PredateHuntParticipant contact : participants)
        {
            if (contact.Id == observer.Id) continue;
            int reception = PredateSignalRules.Reception(origin, contact.Position, true);
            PredateSignalTrack track = receiver.Find(contact.Id);
            bool existing = track != null;
            if (!existing && reception != PredatePulseReception.BEARING) continue;
            if (!track) track = PredateSignalService.Track(observer.Id, contact.Id);
            vector samplePosition = contact.Position;
            track.Reception = reception;
            PredateObservation sample;
            if (track.Reception == PredatePulseReception.BEARING)
                sample = PredateSignalRules.ReceiverReading(track, origin, samplePosition, now, config);
            if (!sample)
            {
                if (existing) track.RecordSample(null);
                continue;
            }
            if (!existing) receiver.AddTrack(track);
            track.BroadcastTime = now;
            track.NextPulseAt = receiver.NextPulseAt;
            scratch.AddReading(track, sample, PredateHunterBrain.Distance(origin, samplePosition));
        }
        scratch.PublishReadings();
    }

    static void Tick(PredateSession session, PlayerBase player, string playerId)
    {
        if (!session.m_HuntConfig.EnableStrategicHunter) return;
        PredateWorldState worldState = session.m_WorldState;
        float now = worldState.SimulationSeconds;
        PredateHuntScratch scratch = session.m_PredateHuntScratch;
        scratch.BeginTick();
        scratch.AddParticipant("player-01", player);
        if (session.m_HuntBot && session.m_HuntBot.IsAlive())
            scratch.AddParticipant(worldState.Hunter.OwnerId, session.m_HuntBot, worldState.HuntReceiver, worldState.Hunter, worldState.Observations, worldState.BotInterceptionUntil);
        if (session.m_PeerRoster)
        {
            foreach (PredatePeerBinding binding : session.m_PeerRoster.m_Bindings)
            {
                if (!binding.Actor || !binding.Actor.IsAlive()) continue;
                PredatePeerState peer = binding.State;
                scratch.AddParticipant(peer.Operator.Id, binding.Actor, peer.HuntReceiver, peer.Hunter, peer.Memory, peer.InterceptedUntil);
            }
        }
        array<ref PredateHuntParticipant> participants = scratch.Participants;
        if (now >= session.m_PredateNextLightScan)
        {
            session.m_PredateNextLightScan = now + 0.75;
            PredatePEQClues.Scan(participants, now);
        }
        map<string, vector> live = scratch.Live;
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
                if (live.Contains(subject)) continue;
                if (observer.Receiver.TargetId == subject)
                    PredateHunterBrain.Hold(observer.Hunter, "hold", "Contact dropped from the receiver");
                observer.Receiver.Retire(subject);
                if (subject == "player-01") subject = playerId;
                for (int memoryIndex = observer.Memory.Entries.Count() - 1; memoryIndex >= 0; memoryIndex--)
                    if (observer.Memory.Entries[memoryIndex].SubjectId == subject) observer.Memory.Entries.RemoveOrdered(memoryIndex);
            }
            Scan(observer, participants, session.m_HuntConfig, now, scratch);
        }
        PredatePeerBinding selected;
        if (session.m_PeerRoster) selected = session.m_PeerRoster.PlayerLock();
        eAIBase playerTarget;
        if (session.PredatePlayerCloseApproach(player, selected))
        {
            if (selected) playerTarget = selected.Actor;
            else playerTarget = session.m_HuntBot;
        }

        int routeBudget = 3;
        int count = participants.Count();
        int routeCursor = session.m_PredateRouteCursor % count;
        for (int offset = 0; offset < count; offset++)
        {
            int routeSlot = (routeCursor + offset) % count;
            PredateHuntParticipant hunter = participants[routeSlot];
            if (!hunter.Receiver) continue;
            if (Think(session, player, hunter, live, playerId, now, routeBudget > 0, hunter.Actor == playerTarget))
            {
                routeBudget--;
                if (routeBudget == 0) session.m_PredateRouteCursor = routeSlot + 1;
            }
        }
    }

    // The receiver gets a proximity cue only for its selected signal.
    static bool FinalApproach(PredateHuntParticipant observer, map<string, vector> live)
    {
        vector subjectPosition;
        string targetId = observer.Receiver.TargetId;
        return targetId != "" && live.Find(targetId, subjectPosition) && PredateSignalRules.FinalApproach(observer.Position, subjectPosition);
    }

    static bool Think(PredateSession session, PlayerBase player, PredateHuntParticipant hunter, map<string, vector> live, string playerId, float now, bool canPlan, bool playerCloseApproach)
    {
        eAIBase actor = eAIBase.Cast(hunter.Actor);
        if (!actor || !actor.IsAlive()) return false;
        bool combat = actor.PredateTacticalPriority(now);
        bool recover = actor.PredateOwnActionPriority();
        bool limited = actor.GetHealth01("", "Health") <= 0.5 || actor.GetHealth01("", "Blood") < 0.7;
        string previousMode = hunter.Hunter.Mode;
        PredateHuntReceiver receiver = hunter.Receiver;
        vector position = hunter.Position;
        string priorTarget = receiver.TargetId;
        bool finalApproach = FinalApproach(hunter, live);
        PredateHuntSelection.Update(receiver, hunter.Hunter, position, now, combat || recover, finalApproach);
        if (receiver.TargetId != priorTarget) finalApproach = FinalApproach(hunter, live);
        PredateSignalTrack track = receiver.Find(receiver.TargetId);
        string subject = receiver.TargetId;
        actor.m_PredateScoreHuntingPlayer = subject == "player-01";
        if (subject == "player-01") subject = playerId;
        if (!combat && !recover && hunter.Hunter.HasGoal && hunter.Hunter.WatchUntil > 0 && now >= hunter.Hunter.WatchUntil && hunter.Hunter.GoalRevision == hunter.Hunter.SearchWatchRevision)
        {
            PredateRoutePlanner.RecordObservedSectors(actor, hunter.Hunter, now);
            hunter.Hunter.WatchUntil = 0;
        }
        actor.PredateSetFinalApproach(finalApproach && !combat && !recover);
        bool hadRoute = hunter.Hunter.HasGoal;
        PredateHunterBrain.Step(hunter.Hunter, position, now, hunter.Memory, track, combat, recover, now < hunter.InterceptedUntil, session.m_WorldState.PlayerLastDeathAt, playerId, subject, receiver.Replanned, limited, finalApproach);
        if (hadRoute && hunter.Hunter.Mode == "reassess" && hunter.Hunter.RouteFailures == 3)
            actor.PredateQueueRockRecovery(hunter.Hunter, previousMode, position, now);
        if (track && track.LastSample && track.LastSample.ObservedAt == now && session.m_PeerRoster)
            session.m_PeerRoster.MarkInterception(session, track.SubjectId, now);
        else if (track && track.LastSample && track.LastSample.ObservedAt == now && track.SubjectId == "player-01")
            PredateSignalService.MarkPlayerHunted(session, now);
        actor.PredateRecoverProne(now, (finalApproach || playerCloseApproach) && !combat && !recover);
        bool planned;
        if (combat || recover)
        {
            if (actor.m_PredateAppliedGoal != -1) actor.PredateReleaseRoute();
        }
        else
        {
            if (canPlan && actor.PredateTryRockRecovery(player, hunter.Hunter, position, now)) return true;
            if (canPlan && !limited && !hunter.Hunter.HasGoal && receiver.TargetId == "" && now >= hunter.Hunter.ScoutAt && now >= hunter.Hunter.WatchUntil)
            {
                Scout(hunter.Hunter, position, now);
                planned = true;
            }
            bool needsPath = hunter.Hunter.HasGoal && actor.m_PredateAppliedGoal != hunter.Hunter.GoalRevision;
            if (!needsPath || canPlan)
            {
                actor.PredateApplyRoute(hunter.Hunter);
                planned = planned || needsPath;
            }
        }
        if (PredateServerDiagnostics.TraceOperators && previousMode != hunter.Hunter.Mode)
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
