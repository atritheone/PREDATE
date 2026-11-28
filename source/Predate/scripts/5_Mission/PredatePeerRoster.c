class PredatePeerBinding
{
    ref PredatePeerState State;
    ref eAIGroup Group;
    eAIBase Actor;
    bool Active;
}

class PredatePeerRosterCheckpoint
{
    int RosterIndex = -1;
}

// Raven retains the original storage path. Each additional operator owns a
// stable slot and a separate native group, receiver and observation memory.
class PredatePeerRoster
{
    static const string SAVE_PATH = "$profile:PredatePeerRoster.json";
    static const string CHECKPOINT_PATH = "$profile:PredatePeerRosterCheckpoint.json";
    ref PredatePeerRosterSave m_State = new PredatePeerRosterSave;
    ref array<ref PredatePeerBinding> m_Bindings = new array<ref PredatePeerBinding>;
    ref map<string, PredatePeerBinding> m_ById = new map<string, PredatePeerBinding>;
    float m_LastTick;
    protected FileHandle m_StateFile;
    protected int m_CheckpointSlot = -1;

    static string StorageKey(int slot, int deployment = 1)
    {
        string key = "PredateRival" + (slot + 1);
        if (slot == 1) key = "PredateLynx01";
        if (slot == 2) key = "PredateRook01";
        if (slot == 3) key = "PredateViper01";
        if (slot == 4) key = "PredateKestrel01";
        if (slot == 5) key = "PredateJackal01";
        if (deployment > 1) key += "_d" + deployment;
        return key;
    }

    static PredatePeerState MakePeer(int slot, vector start, string id = "")
    {
        PredatePeerState peer = new PredatePeerState;
        if (id == "") id = PredateProtocol.OperatorAt(slot);
        peer.Operator.Id = id;
        peer.Operator.Signature = PredateProtocol.PersistentSignatureFor(id);
        peer.Operator.StorageKey = StorageKey(slot);
        peer.Start = start;
        peer.Memory.OwnerId = id;
        peer.PlayerReceiver = PredateSignalService.Track("player-01", id);
        peer.Hunter.OwnerId = id;
        return peer;
    }

    bool Open(PredateSession session, vector lynxStart, vector rookStart, out string error)
    {
        float now = session.m_WorldState.SimulationSeconds;
        int count = session.m_HuntConfig.OpponentCount;
        if (count == 1) count = 3; // Explicit three-Operator developer fixture.
        string rosterPath = SAVE_PATH;
        if (FileExist(CHECKPOINT_PATH))
        {
            PredatePeerRosterCheckpoint checkpoint;
            if (!JsonFileLoader<PredatePeerRosterCheckpoint>.LoadFile(CHECKPOINT_PATH, checkpoint, error)) return false;
            if (!checkpoint || (checkpoint.RosterIndex != 0 && checkpoint.RosterIndex != 1))
            {
                error = "Invalid peer roster checkpoint slot";
                return false;
            }
            m_CheckpointSlot = checkpoint.RosterIndex;
            rosterPath = CheckpointRosterPath(m_CheckpointSlot);
        }
        if (m_CheckpointSlot >= 0 || FileExist(rosterPath))
        {
            if (!JsonFileLoader<PredatePeerRosterSave>.LoadFile(rosterPath, m_State, error)) return false;
        }
        else
        {
            array<vector> sites = {lynxStart, rookStart};
            if (count == 6) sites = {"6050 0 7650", "10300 0 9100", "3200 0 9250", "7500 0 12600", "10300 0 4200"};
            array<vector> occupied = {session.m_HuntConfig.PlayerStart};
            if (session.m_HuntBot) occupied.Insert(session.m_HuntBot.GetPosition());
            bool randomSpawns = session.m_HuntConfig.RandomInitialSpawns;
            if (randomSpawns && session.m_PredateOpeningSites.Count() == count)
            {
                sites.Clear();
                for (int siteIndex = 1; siteIndex < count; siteIndex++)
                    sites.Insert(session.m_PredateOpeningSites[siteIndex]);
            }
            else if (randomSpawns || count > 6)
            {
                sites.Clear();
                bool found;
                if (randomSpawns)
                    found = PredateSpawnPlanner.InitialSites(session, session.m_HuntConfig.PlayerStart, occupied, count - 1, sites);
                else
                    found = PredateSpawnPlanner.RegionalSites(session, occupied, count - 1, sites);
                if (!found)
                {
                    error = "The selected operation could not fit its opponents at the required separation";
                    return false;
                }
            }
            for (int slot = 1; slot < count; slot++)
            {
                vector site = sites[slot - 1];
                if (!randomSpawns && count <= 6 && (count == 6 || session.m_HuntConfig.ExplicitPlayerStart || session.m_HuntConfig.EnableOperatorRespawns))
                {
                    vector insertion;
                    if (!PredateSpawnPlanner.OperatorSite(session, site, occupied, PredateDeployment.MINIMUM_DISTANCE, insertion))
                    {
                        error = "No navigable insertion for " + session.m_HuntConfig.OperatorAtPosition(slot);
                        return false;
                    }
                    site = insertion;
                }
                if (count <= 6) occupied.Insert(site);
                m_State.Peers.Insert(MakePeer(slot, site, session.m_HuntConfig.OperatorAtPosition(slot)));
            }
        }
        if (!m_State || m_State.Schema < 1 || m_State.Schema > 2 || !m_State.Peers || m_State.Peers.Count() != count - 1)
        {
            error = "Invalid peer roster shape";
            return false;
        }
        m_LastTick = now;
        int selected;
        if (session.m_HuntProgress.Selected) selected++;
        for (int index = 0; index < m_State.Peers.Count(); index++)
        {
            PredatePeerState peer = m_State.Peers[index];
            if (peer && !peer.HuntReceiver) peer.HuntReceiver = new PredateHuntReceiver;
            if (peer && peer.Memory && peer.Memory.Entries) peer.Memory.AnonymizeSightings();
            if (peer) peer.Receiver = null;
            if (peer && peer.Operator && m_State.Schema == 1)
                peer.PlayerReceiver = PredateSignalService.Track("player-01", peer.Operator.Id);
            if (peer && peer.PlayerReceiver) peer.PlayerReceiver.NormalizeLoaded();
            if (peer && peer.Operator && !peer.Operator.Supplies) peer.Operator.Supplies = new PredateSupplyPlan;
            if (!peer || !peer.Validate(now) || !MatchesSlot(index, peer, session.m_HuntConfig))
            {
                error = "Invalid peer roster record";
                return false;
            }
            peer.Operator.NormalizeRespawnClock(now, session.m_HuntConfig.OperatorRespawnDelay);
            peer.PlayerReceiver.UseBearingSectors(now);
            if (peer.PlayerReceiver.Selected) selected++;
        }
        if (selected > 1)
        {
            error = "Multiple player receiver locks in the roster save";
            return false;
        }
        m_State.Schema = 2;
        foreach (PredatePeerState saved : m_State.Peers)
        {
            PredatePeerBinding binding = new PredatePeerBinding;
            binding.State = saved;
            if (!session.m_Operators.Register(saved.Operator))
            {
                error = "Duplicate peer operator or storage key";
                return false;
            }
            if (saved.Operator.LifeState != "defeated" && !BindNative(session, binding, error)) return false;
            m_Bindings.Insert(binding);
            m_ById.Insert(saved.Operator.Id, binding);
        }
        return true;
    }

    bool MatchesSlot(int index, PredatePeerState peer, PredateConfig config)
    {
        int slot = index + 1;
        string id = config.OperatorAtPosition(slot);
        return peer.Operator.Id == id && peer.Operator.Signature == PredateProtocol.PersistentSignatureFor(id) && peer.Operator.StorageKey == StorageKey(slot, peer.Operator.Deployment);
    }

    bool BindNative(PredateSession session, PredatePeerBinding binding, out string error)
    {
        PredatePeerState peer = binding.State;
        PredateOperatorRecord record = peer.Operator;
        eAIGroup group;
        eAIBase actor;
        if (record.LifeState == "alive")
        {
            string file = eAIGroup.GetStorageDirectory(record.StorageKey) + eAIGroup.BASENAME;
            if (!FileExist(file) || !eAIGroup.Load(file, group) || !group || group.Count() != 1)
            {
                error = "Missing native actor for " + record.Id;
                return false;
            }
            actor = eAIBase.Cast(group.GetMember(0));
        }
        else
        {
            group = eAIGroup.CreateGroup(new eAIFactionRaiders);
            vector position = peer.Start;
            if (record.Deployment == 1) position = session.PredateGround(position);
            actor = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", position, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
            if (actor)
            {
                actor.SetGroup(group);
                if (!PredateOperatorService.EquipRival(session, actor, record))
                {
                    error = "Starting kit could not be equipped for " + record.Id;
                    return false;
                }
                record.LifeState = "alive";
            }
        }
        if (!actor || !actor.IsAlive() || !group)
        {
            error = "Invalid native actor for " + record.Id;
            return false;
        }
        group.SetName(PredateProtocol.SignatureFor(record.Id));
        group.m_BaseName = record.StorageKey;
        if (!session.m_Operators.Bind(record.Id, group, actor))
        {
            error = "Registry rejected " + record.Id;
            return false;
        }
        ConfigureNative(session, binding, actor, group);
        if (peer.HasVitals)
        {
            if (peer.Health <= 0 || peer.Health > actor.GetMaxHealth("", "Health") || peer.Blood < 0 || peer.Blood > actor.GetMaxHealth("", "Blood") || peer.Shock < 0 || peer.Shock > actor.GetMaxHealth("", "Shock"))
            {
                error = "Invalid native vitals for " + record.Id;
                return false;
            }
            actor.SetHealth("", "Health", peer.Health);
            actor.SetHealth("", "Blood", peer.Blood);
            actor.SetHealth("", "Shock", peer.Shock);
        }
        return true;
    }

    void ConfigureNative(PredateSession session, PredatePeerBinding binding, eAIBase actor, eAIGroup group)
    {
        actor.PredateTakeSaveOwnership(group);
        PredatePeerState peer = binding.State;
        actor.m_PredateOperatorRecord = peer.Operator;
        PredateOperatorService.UpgradeRivalEquipment(session, actor, peer.Operator);
        actor.PredateBindInformation(true, peer.Operator.Id, peer.Memory, peer.PendingSounds, session.m_WorldState.SimulationSeconds);
        actor.m_PredateCombatEnabled = true;
        actor.Expansion_SetCanBeLooted(false);
        actor.m_PredateSignatureRegistered = true;
        actor.eAI_SetAccuracy(session.m_HuntConfig.BotAccuracyMin, session.m_HuntConfig.BotAccuracyMax);
        actor.eAI_SetThreatDistanceLimit(session.m_HuntConfig.ThreatDistance);
        actor.eAI_SetNoiseInvestigationDistanceLimit(PredateConfig.DEFAULT_HEARING_DISTANCE);
        actor.eAI_SetUnlimitedReload(0);
        session.SoloApplyCombatRules(actor);
        actor.SetMovementSpeedLimits(0, 0);
        PredateActorSimulation.SetHeld(actor, true);
        binding.Group = group;
        binding.Actor = actor;
        binding.Active = false;
    }

    PredatePeerBinding Find(string id)
    {
        PredatePeerBinding binding;
        m_ById.Find(id, binding);
        return binding;
    }

    bool RefreshSupplies(PredateSession session, float now)
    {
        bool refreshed = false;
        foreach (PredatePeerBinding binding : m_Bindings)
        {
            PredateOperatorRecord record = binding.State.Operator;
            if (PredateSupplyService.Advance(session, binding.Actor, record.Supplies, PredateOperatorService.RivalKit(session, record), now, now)) refreshed = true;
        }
        return refreshed;
    }

    int AvailableCount(PredateSession session)
    {
        int count;
        if (session.m_WorldState.PlayerSignal.Reception != PredatePulseReception.UNAVAILABLE) count++;
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.State.PlayerReceiver.Reception != PredatePulseReception.UNAVAILABLE) count++;
        return count;
    }

    PredatePeerBinding PlayerLock()
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.State.PlayerReceiver.Selected) return binding;
        return null;
    }

    bool SelectPlayerTarget(PredateSession session, string id)
    {
        bool wantsRaven = id == session.m_HuntProgress.Operator.Id;
        if (id != "")
        {
            PredateSignalTrack receiver;
            if (wantsRaven) receiver = session.m_WorldState.PlayerSignal;
            else
            {
                PredatePeerBinding selected = Find(id);
                if (selected) receiver = selected.State.PlayerReceiver;
            }
            if (!receiver || receiver.HasLostPlayerReception()) return false;
        }
        foreach (PredatePeerBinding peer : m_Bindings)
            peer.State.PlayerReceiver.SetPlayerTracking(peer.State.Operator.Id == id);
        PredateSignalService.Select(session, wantsRaven);
        return true;
    }

    void SampleForPlayer(PredateSession session, PlayerBase player)
    {
        float now = session.m_WorldState.SimulationSeconds;
        foreach (PredatePeerBinding peer : m_Bindings)
        {
            PredateSignalTrack track = peer.State.PlayerReceiver;
            bool sampled = PredateSignalService.SamplePlayerPulse(track, player, peer.Actor, now, session.m_HuntConfig);
            if (sampled && track.Selected && track.LastSample)
            {
                peer.State.InterceptedUntil = now + PredateBearingPulse.INTERVAL + 30;
                if (session.m_Score) session.m_Score.RecordPulse(player, peer.Actor, track.LastSample);
            }
        }
    }

    void AddMapPulses(array<PredateObservation> samples, float now, bool hideSelected)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            PredateSignalService.AddMapPulse(samples, binding.State.PlayerReceiver, now, hideSelected);
    }

    bool ClearOfActors(vector position, float minimumDistance)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
        {
            if (binding.Actor && binding.Actor.IsAlive() && PredateHunterBrain.Distance(position, binding.Actor.GetPosition()) < minimumDistance)
                return false;
        }
        return true;
    }

    void LivingPositions(array<vector> positions)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.Actor && binding.Actor.IsAlive()) positions.Insert(binding.Actor.GetPosition());
    }

    bool RespawnDefeated(PredateSession session, PlayerBase player, float now, out string error)
    {
        if (!session.m_HuntConfig.EnableOperatorRespawns) return true;
        for (int index = 0; index < m_Bindings.Count(); index++)
        {
            PredatePeerBinding binding = m_Bindings[index];
            PredatePeerState previous = binding.State;
            PredateOperatorRecord record = previous.Operator;
            if (!record.RespawnDue(now, session.m_HuntConfig.OperatorRespawnDelay)) continue;
            vector site;
            if (!PredateDeployment.OperatorStartPosition(session, player, record, previous.Start, site))
            {
                record.RespawnAt = now + session.m_HuntConfig.OperatorRespawnDelay;
                return true;
            }
            if (binding.Group)
            {
                binding.Group.m_Persist = false;
                eAIGroup.s_PersistentGroups.RemoveItem(binding.Group);
            }
            PredateOperatorDeaths.Keys.RemoveItem(record.StorageKey);
            record.Deployment++;
            record.StorageKey = StorageKey(index + 1, record.Deployment);
            record.LifeState = "unspawned";
            record.ClearPlayerWoundCredit();
            record.RespawnAt = 0;
            record.EquipmentRevision = 0;
            record.RifleKit = "";
            record.Supplies = new PredateSupplyPlan;
            PredatePeerState fresh = MakePeer(index + 1, site, record.Id);
            fresh.PlayerReceiver = previous.PlayerReceiver;
            fresh.Operator = record;
            binding.State = fresh;
            m_State.Peers[index] = fresh;
            if (!BindNative(session, binding, error)) return false;
            record.RememberInsertion(site);
            // A checkpoint already in progress may have passed this slot.
            // The new deployment must have native storage before publishing it.
            if (!SaveActor(session, binding, error)) return false;
            session.PredateRequestCheckpoint(true);
            return true;
        }
        return true;
    }

    void HoldPause(PredateSession session)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            session.HoldPauseActor(binding.Actor);
    }

    void StopActors()
    {
        foreach (PredatePeerBinding binding : m_Bindings)
        {
            if (!binding.Actor) continue;
            binding.Actor.SetMovementSpeedLimits(0, 0);
            PredateActorSimulation.SetHeld(binding.Actor, true);
            binding.Active = false;
        }
    }

    bool UpdateBindings(PredateSession session, bool active, out string error, PredateOperatorIndex index)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
        {
            eAIBase actor;
            eAIGroup group;
            if (!session.m_Operators.Resolve(binding.State.Operator.Id, actor, group, error, index)) return false;
            if (!actor)
            {
                binding.Actor = null;
                continue;
            }
            if (binding.Actor != actor) ConfigureNative(session, binding, actor, group);
            if (binding.Active == active) continue;
            binding.Active = active;
            PredateActorSimulation.SetHeld(actor, !active);
            if (active) actor.SetMovementSpeedLimits(2, 3);
            else actor.SetMovementSpeedLimits(0, 0);
        }
        return true;
    }

    void RetirePlayerContact(string playerId, float now)
    {
        foreach (PredatePeerBinding peer : m_Bindings)
        {
            bool tracksPreviousLife = peer.State.HuntReceiver.TargetId == "player-01";
            PredateDeployment.RetireOldContact(peer.State.Hunter, peer.State.Memory, peer.Actor, playerId, now, tracksPreviousLife);
            peer.State.HuntReceiver.Retire("player-01");
            peer.State.PlayerReceiver.SetPlayerTracking(false);
        }
    }

    void MarkInterception(PredateSession session, string subject, float now)
    {
        PredateWorldState state = session.m_WorldState;
        float until = now + PredateBearingPulse.INTERVAL + 30;
        if (subject == session.m_HuntProgress.Operator.Id) state.BotInterceptionUntil = Math.Max(until, state.BotInterceptionUntil);
        else if (subject == "player-01") PredateSignalService.MarkPlayerHunted(session, now);
        else
        {
            PredatePeerBinding peer = Find(subject);
            if (peer) peer.State.InterceptedUntil = Math.Max(until, peer.State.InterceptedUntil);
        }
    }

    bool Advance(PredateSession session, PlayerBase player, out string error, PredateOperatorIndex index)
    {
        float now = session.m_WorldState.SimulationSeconds;
        bool active = session.m_ActorsActive && !session.m_Paused && !session.m_Suspending;
        if (!UpdateBindings(session, active, error, index)) return false;
        if (!active || !player || now <= m_LastTick) return true;
        if (!RespawnDefeated(session, player, now, error)) return false;
        float elapsed = now - m_LastTick;
        m_LastTick = now;

        foreach (PredatePeerBinding tracked : m_Bindings)
        {
            if (!tracked.Actor) continue;
            session.SoloApplyCombatRules(tracked.Actor);
            PredateSignalService.CaptureActor(tracked.Actor, tracked.State.SignatureActivity);
            tracked.State.SignatureActivity.Advance(elapsed);
            tracked.Actor.PredateAdvanceInformation(now);
            tracked.Actor.PredateUpdateMedicine(now);
        }
        return true;
    }

    bool SaveActor(PredateSession session, PredatePeerBinding binding, out string error, PredateOperatorIndex index = null)
    {
        PredatePeerState peer = binding.State;
        PredateOperatorRecord record = peer.Operator;
        eAIBase actor = binding.Actor;
        eAIGroup group = binding.Group;
        // Tick keeps the binding current. Re-resolve if it changed between
        // that tick and this checkpoint frame, or when saving explicitly.
        if (index || !actor || !actor.IsAlive() || !group || actor.GetGroup() != group || group.m_BaseName != record.StorageKey)
        {
            if (!session.m_Operators.Resolve(record.Id, actor, group, error, index)) return false;
        }
        if (record.LifeState == "defeated")
        {
            if (binding.Group)
            {
                binding.Group.m_Persist = false;
                eAIGroup.s_PersistentGroups.RemoveItem(binding.Group);
            }
            binding.Actor = null;
            binding.Group = null;
            return true;
        }
        if (!group || !actor)
        {
            error = "Cannot checkpoint peer " + record.Id;
            return false;
        }
        // Keep vitals in step with the native snapshot of gear and position.
        PredateSignalService.CaptureActor(actor, peer.SignatureActivity);
        group.Save(true);
        peer.Health = actor.GetHealth("", "Health");
        peer.Blood = actor.GetHealth("", "Blood");
        peer.Shock = actor.GetHealth("", "Shock");
        peer.HasVitals = true;
        return true;
    }

    bool Save(PredateSession session, out string error)
    {
        PredateOperatorIndex index = PredateOperatorIndex.Capture();
        foreach (PredatePeerBinding binding : m_Bindings)
            if (!SaveActor(session, binding, error, index)) return false;
        return SaveState(session.m_WorldState.SimulationSeconds, error);
    }

    bool SaveState(float now, out string error)
    {
        CancelStateSave();
        foreach (PredatePeerState peer : m_State.Peers)
            PredatePersistence.TrimExpiredPrivateHistory(peer.HuntReceiver, now);
        // Explicit saves continue to produce the standard roster file.
        string data;
        if (!JsonFileLoader<PredatePeerRosterSave>.MakeData(m_State, data, error, false)) return false;
        FileHandle file = OpenFile(SAVE_PATH, FileMode.WRITE);
        if (!file)
        {
            error = "Cannot open " + SAVE_PATH + " for writing";
            return false;
        }
        FPrint(file, data);
        CloseFile(file);
        if (FileExist(CHECKPOINT_PATH) && !DeleteFile(CHECKPOINT_PATH))
        {
            error = "Cannot clear " + CHECKPOINT_PATH;
            return false;
        }
        m_CheckpointSlot = -1;
        for (int slot = 0; slot < 2; slot++)
        {
            string path = CheckpointRosterPath(slot);
            if (FileExist(path)) DeleteFile(path);
        }
        return true;
    }

    static string CheckpointRosterPath(int slot)
    {
        return "$profile:PredatePeerRoster-" + slot + ".json";
    }

    protected int NextCheckpointSlot()
    {
        if (m_CheckpointSlot == 0) return 1;
        return 0;
    }

    bool BeginStateSave(out string error)
    {
        string path = CheckpointRosterPath(NextCheckpointSlot());
        m_StateFile = OpenFile(path, FileMode.WRITE);
        if (!m_StateFile)
        {
            error = "Cannot open " + path + " for writing";
            return false;
        }
        FPrint(m_StateFile, "{\"Schema\":" + m_State.Schema + ",\"Peers\":[");
        return true;
    }

    bool WriteStateRow(int row, out string error, float now = -1)
    {
        string data;
        PredatePersistence.TrimExpiredPrivateHistory(m_State.Peers[row].HuntReceiver, now);
        if (!JsonFileLoader<PredatePeerState>.MakeData(m_State.Peers[row], data, error, false)) return false;
        if (row > 0) FPrint(m_StateFile, ",");
        FPrint(m_StateFile, data);
        return true;
    }

    bool FinishStateSave(out string error)
    {
        FPrint(m_StateFile, "]}");
        CloseFile(m_StateFile);
        m_StateFile = null;
        // The previous slot stays readable until the new file is complete.
        PredatePeerRosterCheckpoint checkpoint = new PredatePeerRosterCheckpoint;
        checkpoint.RosterIndex = NextCheckpointSlot();
        if (!JsonFileLoader<PredatePeerRosterCheckpoint>.SaveFile(CHECKPOINT_PATH, checkpoint, error)) return false;
        m_CheckpointSlot = checkpoint.RosterIndex;
        return true;
    }

    void CancelStateSave()
    {
        if (!m_StateFile) return;
        CloseFile(m_StateFile);
        m_StateFile = null;
        DeleteFile(CheckpointRosterPath(NextCheckpointSlot()));
    }
}
