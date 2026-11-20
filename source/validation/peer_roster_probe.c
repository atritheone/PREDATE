class PredatePeerTrialRow
{
    string Id;
    string Key;
    string Life;
    vector Position;
    float Health;
    float Blood;
    int MagazineRounds;
    string FirearmClass;
    int ObservationCount;
    string Target;
    float NextPulse;
    string RouteMode;
    string RouteEvidence;
    bool HasRouteGoal;
    vector RouteGoal;
    int RouteRevision;
    float InterceptedUntil;
    int FiredShots;
    bool ObservedRival;

    static PredatePeerTrialRow Capture(PredatePeerBinding binding)
    {
        PredatePeerState peer = binding.State;
        PredatePeerTrialRow row = new PredatePeerTrialRow;
        row.Id = peer.Operator.Id;
        row.Key = peer.Operator.StorageKey;
        row.Life = peer.Operator.LifeState;
        row.Target = peer.HuntReceiver.TargetId;
        row.NextPulse = peer.HuntReceiver.NextPulseAt;
        row.ObservationCount = peer.Memory.Entries.Count();
        row.RouteMode = peer.Hunter.Mode;
        row.RouteEvidence = peer.Hunter.EvidenceKey;
        row.HasRouteGoal = peer.Hunter.HasGoal;
        row.RouteGoal = peer.Hunter.Goal;
        row.RouteRevision = peer.Hunter.GoalRevision;
        row.InterceptedUntil = peer.InterceptedUntil;
        if (binding.Actor)
        {
            row.Position = binding.Actor.GetPosition();
            row.Health = binding.Actor.GetHealth("", "Health");
            row.Blood = binding.Actor.GetHealth("", "Blood");
            row.MagazineRounds = PredateServerBridge.MagazineCount(binding.Actor);
            row.FiredShots = binding.Actor.m_PredateFiredShots;
            EntityAI held = binding.Actor.GetHumanInventory().GetEntityInHands();
            if (held) row.FirearmClass = held.GetType();
        }
        return row;
    }
}

class PredatePeerTrialReport
{
    int Schema = 1;
    int Checks;
    int Failures;
    string Error;
    float SimulationSeconds;
    ref array<ref PredatePeerTrialRow> Actors = new array<ref PredatePeerTrialRow>;
    ref array<string> Diagnostics = new array<string>;

    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredatePeerTest] FAIL: " + label);
    }
}

class PredatePeerValidationMission : MissionServer
{
    ref PredatePeerRoster m_Peers;
    bool m_PeerTrialRan;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_PeerTrialRan || !m_PredateSession.m_HuntReady) return;
        m_PeerTrialRan = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunPeerTrial, 500, false);
    }

    void Remember(PredatePeerBinding binding, vector area, float now, int serial)
    {
        PredateObservation sound = new PredateObservation;
        sound.EventKey = "peer-validation-" + binding.State.Operator.Id;
        sound.ObserverId = binding.State.Operator.Id;
        sound.Source = PredateObservationSource.AUDIBLE;
        sound.ObservedAt = now;
        sound.ExpiresAt = now + 60;
        sound.Confidence = 0.75;
        sound.HasPosition = true;
        sound.Estimate = area;
        sound.Radius = 20;
        binding.State.Memory.Add(sound, now);
        binding.State.HuntReceiver.OwnerId = binding.State.Operator.Id;
        binding.State.HuntReceiver.NextPulseAt = now + 60 + serial;
    }

    void Seed(PredatePeerBinding binding, float health, float blood, int magazineCount, int clockOffset)
    {
        eAIBase actor = binding.Actor;
        actor.SetHealth("", "Health", health);
        actor.SetHealth("", "Blood", blood);
        Weapon_Base weapon = Weapon_Base.Cast(actor.GetHumanInventory().GetEntityInHands());
        if (weapon)
        {
            Magazine magazine = weapon.GetMagazine(0);
            if (magazine) magazine.ServerSetAmmoCount(magazineCount);
        }
        Remember(binding, actor.GetPosition() + "40 0 0", m_PredateSession.m_WorldState.SimulationSeconds, clockOffset);
    }

    void RunPeerTrial()
    {
        string file = "$profile:PeerFirst.json";
        bool first = !FileExist(file);
        bool second = !first && !FileExist("$profile:PeerBeforeDefeat.json");
        if (!first && second) file = "$profile:PeerBeforeDefeat.json";
        else if (!first) file = "$profile:PeerAfterDefeat.json";

        PredatePeerTrialReport report = new PredatePeerTrialReport;
        m_Peers = new PredatePeerRoster;
        string error;
        if (!m_Peers.Open(m_PredateSession, "4850 0 2200", "5139 0 2122", error))
        {
            report.Error = error;
            report.Check(false, "three-operator roster loads");
            WritePeerTrial(file, report);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MeasurePeerTrial, 500, false, file, first, second, report);
    }

    void MeasurePeerTrial(string file, bool first, bool second, PredatePeerTrialReport report)
    {
        PredatePeerBinding lynx = m_Peers.m_Bindings[0];
        PredatePeerBinding rook = m_Peers.m_Bindings[1];
        eAIBase raven = m_PredateSession.m_HuntBot;
        report.Check(raven != null && m_PredateSession.m_Operators.m_Slots.Count() == 3, "three registered identities");
        report.Check(rook.Actor && rook.Actor.GetIsSimulationDisabled(), "no unattended simulation before a player connects");
        if (first && lynx.Actor && rook.Actor)
        {
            Seed(lynx, 73, 4321, 17, 11);
            Seed(rook, 84, 4543, 23, 23);
        }
        if (first || second)
        {
            report.Check(lynx.Actor && rook.Actor && raven && lynx.Actor != rook.Actor && lynx.Actor != raven && rook.Actor != raven, "distinct native actors");
            report.Check(lynx.Group != rook.Group && lynx.Group != m_PredateSession.m_HuntGroup && rook.Group != m_PredateSession.m_HuntGroup, "separate native groups");
            report.Check(lynx.Actor && rook.Actor && lynx.Actor.PlayerIsEnemy(rook.Actor) && rook.Actor.PlayerIsEnemy(lynx.Actor), "rivals classify each other as enemies");
        }
        else report.Check(!lynx.Actor && lynx.State.Operator.LifeState == "defeated", "defeated rival remains absent after cold start");
        report.Check(lynx.State.Memory != rook.State.Memory && lynx.State.PendingSounds != rook.State.PendingSounds, "separate private memory and pending sound lists");
        report.Check(lynx.State.Memory.Entries.Count() == 1 && rook.State.Memory.Entries.Count() == 1 && lynx.State.Memory.Entries[0].EventKey != rook.State.Memory.Entries[0].EventKey, "each peer keeps only its own retained test evidence");
        report.Check(lynx.State.HuntReceiver != rook.State.HuntReceiver && lynx.State.HuntReceiver.TargetId == "" && rook.State.HuntReceiver.TargetId == "", "independent receivers have no authored targets");
        report.Check(lynx.State.HuntReceiver.NextPulseAt == 71 && rook.State.HuntReceiver.NextPulseAt == 83, "independent pulse schedules");

        foreach (PredatePeerBinding peer : m_Peers.m_Bindings)
            report.Actors.Insert(PredatePeerTrialRow.Capture(peer));
        if (second && lynx.Actor) lynx.Actor.SetHealth("", "Health", 0);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinishPeerTrial, 300, false, file, second, report);
    }

    void FinishPeerTrial(string file, bool second, PredatePeerTrialReport report)
    {
        string error;
        report.Check(m_Peers.Save(m_PredateSession, error), "native inventories and private state checkpoint");
        report.Error = error;
        if (second) report.Check(m_Peers.m_Bindings[0].State.Operator.LifeState == "defeated", "one opponent is marked defeated without changing the others");
        WritePeerTrial(file, report);
    }

    void WritePeerTrial(string path, PredatePeerTrialReport report)
    {
        string error;
        JsonFileLoader<PredatePeerTrialReport>.SaveFile(path, report, error);
        Print("[PredatePeerTest] COMPLETE: checks=" + report.Checks + "; failures=" + report.Failures + "; " + report.Error);
    }
}

// Separate live trial: no synthetic bearings, hidden player coordinates, or
// fixture memories are provided to either actor.
class PredatePeerActiveMission : MissionServer
{
    ref PredatePeerRoster m_Peers;
    ref PredatePeerTrialReport m_Result = new PredatePeerTrialReport;
    ref array<vector> m_PausedPositions = new array<vector>;
    float m_StartedAt;
    float m_PausedAt;
    float m_PauseWallSeconds;
    int m_Phase;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Phase == 3 || !m_PredateSession.m_HuntReady) return;
        string error;
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        if (!m_Peers && FileExist("$profile:PeerActive.json"))
        {
            m_Phase = 3;
            if (!m_PredateSession.AttachPeerRoster("4850 0 2200", "5139 0 2122", error)) WriteActiveReport("$profile:PeerActiveReopened.json", error);
            else
            {
                m_Peers = m_PredateSession.m_PeerRoster;
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckCold, 500, false);
            }
            return;
        }
        if (!m_Peers)
        {
            if (!m_PredateSession.m_ActorsActive) return;
            m_StartedAt = now;
            if (!m_PredateSession.AttachPeerRoster("4850 0 2200", "5139 0 2122", error))
            {
                Finish(error);
                return;
            }
            m_Peers = m_PredateSession.m_PeerRoster;
        }
        if (m_PredateSession.m_HuntError != "")
        {
            Finish(m_PredateSession.m_HuntError);
            return;
        }
        if (m_Phase == 0 && now >= m_StartedAt + 9)
        {
            foreach (PredatePeerBinding binding : m_Peers.m_Bindings)
            {
                CheckReceiver(binding, now);
                vector position = binding.Actor.GetPosition();
                float travel = PredateHunterBrain.Distance(position, binding.State.Start);
                m_Result.Check(travel >= 2 && travel < 60, "native travel with no teleport for " + binding.State.Operator.Id + ": " + travel);
                m_PausedPositions.Insert(position);
            }
            m_PausedAt = now;
            m_Phase = 1;
            m_PredateSession.SetPaused(true);
            PredateServerPauseDriver.Poll.Insert(PollPeerPause);
        }
        else if (m_Phase == 2 && now > m_PausedAt + 1)
        {
            foreach (PredatePeerBinding resumed : m_Peers.m_Bindings)
                m_Result.Check(resumed.Active && !resumed.Actor.GetIsSimulationDisabled(), "native simulation resumes for " + resumed.State.Operator.Id);
            Finish("");
        }
    }

    void CheckReceiver(PredatePeerBinding binding, float now)
    {
        PredatePeerState peer = binding.State;
        PredateSignalTrack track = peer.HuntReceiver.Find(peer.HuntReceiver.TargetId);
        PredateObservation sample;
        if (track) sample = track.LastSample;
        string id = peer.Operator.Id;
        m_Result.Check(sample && sample.ObserverId == id && sample.SubjectId == peer.HuntReceiver.TargetId, "privately samples an independent rival for " + id);
        m_Result.Check(sample && sample.HasBearing && !sample.HasPosition && sample.Estimate == vector.Zero && PredateHunterBrain.Distance(sample.Origin, peer.Start) <= 2, "bearing retains its own historical origin without target coordinates for " + id);
        m_Result.Check(track && track.History.Count() == 1 && sample && sample.DistanceBand > 0 && sample.PulseDistance == 0, "private memory records earned bearing for " + id);
        m_Result.Check(sample && peer.Hunter.HasGoal && peer.Hunter.EvidenceKey == sample.EventKey && (peer.Hunter.Mode == "observe" || peer.Hunter.Mode == "evade") && binding.Actor.m_PredateRouteActive, "private evidence produces a native route for " + id);
        m_Result.Check(peer.HuntReceiver.NextPulseAt > m_StartedAt + 10, "pulse schedule retained for " + id);
    }

    void PollPeerPause(float elapsed)
    {
        m_PauseWallSeconds += elapsed;
        if (m_PauseWallSeconds < 1.4) return;
        PredateServerPauseDriver.Poll.Remove(PollPeerPause);
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        m_Result.Check(now == m_PausedAt, "operation simulation clock stopped");
        for (int index = 0; index < m_Peers.m_Bindings.Count(); index++)
        {
            PredatePeerBinding binding = m_Peers.m_Bindings[index];
            m_Result.Check(binding.Actor.GetIsSimulationDisabled() && PredateHunterBrain.Distance(m_PausedPositions[index], binding.Actor.GetPosition()) < 0.2, "rival position and simulation stopped for " + binding.State.Operator.Id);
        }
        m_Phase = 2;
        m_PredateSession.SetPaused(false);
    }

    void CheckCold()
    {
        m_Result.SimulationSeconds = m_PredateSession.m_WorldState.SimulationSeconds;
        foreach (PredatePeerBinding restored : m_Peers.m_Bindings)
        {
            m_Result.Check(restored.Actor && restored.Actor.GetIsSimulationDisabled(), "restored operator waits for the player");
            m_Result.Actors.Insert(PredatePeerTrialRow.Capture(restored));
        }
        WriteActiveReport("$profile:PeerActiveReopened.json", "");
    }

    void Finish(string error)
    {
        m_Phase = 3;
        if (error == "" && m_Result.Failures == 0)
        {
            m_PredateSession.SetPaused(true);
            foreach (PredatePeerBinding held : m_Peers.m_Bindings)
                m_Result.Actors.Insert(PredatePeerTrialRow.Capture(held));
            m_Result.SimulationSeconds = m_PredateSession.m_WorldState.SimulationSeconds;
            m_PredateSession.PredateSave();
            error = m_PredateSession.m_HuntError;
            m_Result.Check(error == "" && FileExist(PredatePeerRoster.SAVE_PATH), "active private state saved at the same operation time");
        }
        WriteActiveReport("$profile:PeerActive.json", error);
    }

    void WriteActiveReport(string path, string error)
    {
        if (error != "") m_Result.Check(false, error);
        m_Result.Error = error;
        string writeError;
        JsonFileLoader<PredatePeerTrialReport>.SaveFile(path, m_Result, writeError);
        Print("[PredatePeerTest] ACTIVE COMPLETE: checks=" + m_Result.Checks + "; failures=" + m_Result.Failures + "; " + error);
    }
}

class PredatePeerRetargetMission : MissionServer
{
    ref PredatePeerTrialReport m_Result = new PredatePeerTrialReport;
    int m_Phase;
    float m_NextSlot;
    float m_ResampleAt;
    string m_PlayerSample;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Phase == 4 || !m_PredateSession.m_ActorsActive || !m_PredateSession.m_PeerRoster) return;
        PredatePeerBinding rook = m_PredateSession.m_PeerRoster.Find("rook-01");
        PredatePeerBinding lynx = m_PredateSession.m_PeerRoster.Find("lynx-01");
        PredateHuntReceiver receiver = rook.State.HuntReceiver;
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        if (m_Phase == 0)
        {
            array<Man> players = {};
            GetGame().GetPlayers(players);
            foreach (Man man : players)
            {
                PlayerBase player = PlayerBase.Cast(man);
                if (!player || !player.GetIdentity()) continue;
                rook.Actor.SetPosition(m_PredateSession.PredateGround("4500 0 6500"));
                lynx.Actor.SetPosition(m_PredateSession.PredateGround("4500 0 6800"));
                player.SetPosition(m_PredateSession.PredateGround("4500 0 7150"));
                m_PredateSession.m_HuntBot.SetPosition(m_PredateSession.PredateGround("4500 0 7600"));
                receiver.NextPulseAt = now;
                m_Phase = 1;
                return;
            }
        }
        else if (m_Phase == 1 && receiver.NextPulseAt > now)
        {
            PredateSignalTrack human = receiver.Find("player-01");
            PredateSignalTrack peer = receiver.Find("lynx-01");
            m_Result.Check(human && human.LastSample && peer && peer.LastSample, "both player and Operator appear on this receiver");
            m_Result.Check(receiver.TargetId == "lynx-01", "nearer Operator wins the first decision");
            m_Result.Check(human && human.LastSample && human.LastSample.DistanceBand == 3, "human pulse has the expected band");
            m_Result.Check(peer && peer.LastSample && peer.LastSample.DistanceBand == 2, "Operator pulse has the expected band");
            m_Result.Check(human && human.LastSample && !human.LastSample.HasPosition && human.LastSample.PulseDistance == 0, "no private position or exact range in the pulse");
            if (human && human.LastSample) m_PlayerSample = human.LastSample.EventKey;
            m_NextSlot = receiver.NextPulseAt;
            lynx.Actor.SetHealth("", "Health", 0);
            m_Phase = 2;
        }
        else if (m_Phase == 2 && lynx.State.Operator.LifeState == "defeated")
        {
            eAIBase resolvedActor = rook.Actor;
            eAIGroup resolvedGroup = rook.Group;
            string resolutionError;
            bool resolved = m_PredateSession.m_Operators.Resolve("lynx-01", resolvedActor, resolvedGroup, resolutionError);
            m_Result.Check(resolved && !resolvedActor && !resolvedGroup && !lynx.Actor, "a defeated slot clears reused actor and group references");
            m_Result.Check(receiver.NextPulseAt == m_NextSlot && receiver.Find("lynx-01") && receiver.TargetId == "lynx-01", "remote defeat stays private between pulses");
            // Advance only this disposable fixture's scheduled scan.
            m_ResampleAt = now;
            receiver.NextPulseAt = now;
            m_Phase = 3;
        }
        else if (m_Phase == 3 && receiver.NextPulseAt > m_ResampleAt)
        {
            PredateSignalTrack retained = receiver.Find("player-01");
            m_Result.Check(!receiver.Find("lynx-01") && receiver.TargetId == "player-01", "the next scan removes the lost contact and selects a legitimate alternative");
            m_Result.Check(retained && retained.LastSample && retained.LastSample.EventKey != m_PlayerSample, "switch uses a new scheduled reading");
            m_Result.Check(receiver.Valid(now, "rook-01"), "private receiver still validates");
            m_Result.Actors.Insert(PredatePeerTrialRow.Capture(rook));
            m_Result.SimulationSeconds = now;
            m_PredateSession.PredateSave();
            m_Result.Error = m_PredateSession.m_HuntError;
            m_Result.Check(m_Result.Error == "", "switch and receiver can be saved");
            string error;
            JsonFileLoader<PredatePeerTrialReport>.SaveFile("$profile:PeerRetarget.json", m_Result, error);
            m_Phase = 4;
        }
    }
}

class PredatePeerEncounterMission : MissionServer
{
    ref PredatePeerTrialReport m_Result = new PredatePeerTrialReport;
    eAIBase m_LynxActor;
    eAIBase m_RookActor;
    float m_StartedAt = -1;
    float m_NextDiagnostic;
    bool m_Done;

    bool ObservedAnonymously(eAIBase observer, eAIBase candidate, float now)
    {
        eAITargetInformationState known = observer.eAI_GetTargetInformationState(candidate.GetTargetInformation(), false);
        if (!known || known.m_PredateVisualContactKey == "") return false;
        foreach (PredateObservation sighting : observer.m_PredateMemory.Entries)
        {
            if (sighting.Source == PredateObservationSource.VISUAL && sighting.VisualContactKey == known.m_PredateVisualContactKey && sighting.SubjectId == "" && !sighting.Expired(now)) return true;
        }
        return false;
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Done || !m_PredateSession.m_PeerRoster) return;
        PredatePeerBinding lynx = m_PredateSession.m_PeerRoster.Find("lynx-01");
        PredatePeerBinding rook = m_PredateSession.m_PeerRoster.Find("rook-01");
        if (m_StartedAt < 0 && lynx.Actor && rook.Actor)
        {
            m_LynxActor = lynx.Actor;
            m_RookActor = rook.Actor;
            // Author the opening poses. Acquisition and firing remain native.
            m_LynxActor.SetDirection(vector.Direction(m_LynxActor.GetPosition(), m_RookActor.GetPosition()));
            m_RookActor.SetDirection(vector.Direction(m_RookActor.GetPosition(), m_LynxActor.GetPosition()));
            m_Result.Check(m_LynxActor.GetTarget() == null && m_RookActor.GetTarget() == null, "opening poses contain no enrolled native target");
            m_Result.Check(m_LynxActor.PlayerIsEnemy(m_RookActor) && m_RookActor.PlayerIsEnemy(m_LynxActor), "opponents are mutual enemies");
            m_StartedAt = m_PredateSession.m_WorldState.SimulationSeconds;
        }
        if (!m_LynxActor || !m_RookActor || !m_PredateSession.m_ActorsActive) return;
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        if (now >= m_NextDiagnostic)
        {
            m_NextDiagnostic = now + 2;
            m_Result.Diagnostics.Insert("Lynx at " + now + ": " + m_LynxActor.PredateCombatDiagnostic());
            m_Result.Diagnostics.Insert("Rook at " + now + ": " + m_RookActor.PredateCombatDiagnostic());
        }
        bool lynxSawRook = ObservedAnonymously(m_LynxActor, m_RookActor, now);
        bool rookSawLynx = ObservedAnonymously(m_RookActor, m_LynxActor, now);
        int fired = m_LynxActor.m_PredateFiredShots + m_RookActor.m_PredateFiredShots;
        float health = m_LynxActor.GetHealth("", "Health") + m_RookActor.GetHealth("", "Health");
        if (now < m_StartedAt + 50 && !(fired > 0 && health < 200 && (lynxSawRook || rookSawLynx))) return;
        m_Result.Check(lynxSawRook || rookSawLynx, "native sight acquires an anonymous contact");
        m_Result.Check(fired > 0, "one or both independent rivals fired after acquiring contact");
        m_Result.Check(health < 200, "the encounter produces native damage");
        m_Result.Check(!lynx.State.HuntReceiver.Find("rook-01") && !rook.State.HuntReceiver.Find("lynx-01"), "the encounter stayed within the bearing cutoff");
        PredatePeerTrialRow lynxResult = PredatePeerTrialRow.Capture(lynx);
        PredatePeerTrialRow rookResult = PredatePeerTrialRow.Capture(rook);
        lynxResult.ObservedRival = lynxSawRook;
        rookResult.ObservedRival = rookSawLynx;
        lynxResult.FiredShots = m_LynxActor.m_PredateFiredShots;
        rookResult.FiredShots = m_RookActor.m_PredateFiredShots;
        m_Result.Actors.Insert(lynxResult);
        m_Result.Actors.Insert(rookResult);
        m_Result.SimulationSeconds = now;
        string error;
        JsonFileLoader<PredatePeerTrialReport>.SaveFile("$profile:PeerEncounter.json", m_Result, error);
        m_Done = true;
    }
}
