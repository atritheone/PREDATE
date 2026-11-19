class PredatePeerBinding
{
    ref PredatePeerState State;
    ref eAIGroup Group;
    eAIBase Actor;
    bool Active;
}

// Raven retains the original storage path. Each additional operator owns a
// stable slot and a separate native group, receiver and observation memory.
class PredatePeerRoster
{
    static const string SAVE_PATH = "$profile:PredatePeerRoster.json";
    ref PredatePeerRosterSave m_State = new PredatePeerRosterSave;
    ref array<ref PredatePeerBinding> m_Bindings = new array<ref PredatePeerBinding>;
    float m_LastTick;

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

    static PredatePeerState MakePeer(int slot, string target, vector start, float firstPulse)
    {
        PredatePeerState peer = new PredatePeerState;
        string id = PredateProtocol.OperatorAt(slot);
        peer.Operator.Id = id;
        peer.Operator.Signature = PredateProtocol.SignatureFor(id);
        peer.Operator.StorageKey = StorageKey(slot);
        peer.Start = start;
        peer.Memory.OwnerId = id;
        peer.Receiver = PredateSignalService.Track(id, target);
        peer.Receiver.SetSelected(true, firstPulse, 1);
        peer.PlayerReceiver = PredateSignalService.Track("player-01", id);
        peer.Hunter.OwnerId = id;
        return peer;
    }

    bool Open(PredateSession session, vector lynxStart, vector rookStart, out string error)
    {
        float now = session.m_WorldState.SimulationSeconds;
        int count = 3;
        if (session.m_HuntConfig.OpponentCount > 3) count = session.m_HuntConfig.OpponentCount;
        if (FileExist(SAVE_PATH))
        {
            if (!JsonFileLoader<PredatePeerRosterSave>.LoadFile(SAVE_PATH, m_State, error)) return false;
        }
        else
        {
            array<vector> sites = {lynxStart, rookStart};
            if (count == 6) sites = {"6050 0 7650", "10300 0 9100", "3200 0 9250", "7500 0 12600", "10300 0 4200"};
            array<vector> occupied = {session.m_HuntConfig.PlayerStart};
            if (session.m_HuntBot) occupied.Insert(session.m_HuntBot.GetPosition());
            if (count > 6)
            {
                sites.Clear();
                if (!PredateSpawnPlanner.RegionalSites(session, occupied, count - 1, sites))
                {
                    error = "The selected operation could not fit its opponents at the required separation";
                    return false;
                }
            }
            for (int slot = 1; slot < count; slot++)
            {
                vector site = sites[slot - 1];
                if (count <= 6 && (count == 6 || session.m_HuntConfig.ExplicitPlayerStart || session.m_HuntConfig.EnableOperatorRespawns))
                {
                    vector insertion;
                    if (!PredateSpawnPlanner.OperatorSite(session, site, occupied, PredateDeployment.MINIMUM_DISTANCE, insertion))
                    {
                        error = "No navigable insertion for " + PredateProtocol.OperatorAt(slot);
                        return false;
                    }
                    site = insertion;
                }
                if (count <= 6) occupied.Insert(site);
                string target = PredateProtocol.OperatorAt(slot + 1);
                if (slot == count - 1) target = PredateProtocol.OperatorAt(1);
                float stagger = 3;
                if (count > 6)
                {
                    target = RegionalTarget(slot - 1, sites);
                    stagger = 0.45;
                }
                m_State.Peers.Insert(MakePeer(slot, target, site, now + 2 + (slot - 1) * stagger));
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
            if (peer && peer.Receiver) peer.Receiver.NormalizeLoaded();
            if (peer && peer.Operator && m_State.Schema == 1)
                peer.PlayerReceiver = PredateSignalService.Track("player-01", peer.Operator.Id);
            if (peer && peer.PlayerReceiver) peer.PlayerReceiver.NormalizeLoaded();
            if (peer && peer.Operator && !peer.Operator.Supplies) peer.Operator.Supplies = new PredateSupplyPlan;
            if (!peer || !peer.Validate(now) || !MatchesSlot(index, peer))
            {
                error = "Invalid peer roster record";
                return false;
            }
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
        }
        return true;
    }

    static string RegionalTarget(int origin, array<vector> sites)
    {
        int best = -1;
        float distance = 40000;
        for (int index = 0; index < sites.Count(); index++)
        {
            if (index == origin) continue;
            float candidate = PredateHunterBrain.Distance(sites[origin], sites[index]);
            if (candidate >= distance) continue;
            distance = candidate;
            best = index;
        }
        return PredateProtocol.OperatorAt(best + 1);
    }

    bool MatchesSlot(int index, PredatePeerState peer)
    {
        int slot = index + 1;
        string id = PredateProtocol.OperatorAt(slot);
        return peer.Operator.Id == id && peer.Operator.Signature == PredateProtocol.SignatureFor(id) && peer.Operator.StorageKey == StorageKey(slot, peer.Operator.Deployment);
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
            actor = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", session.PredateGround(peer.Start), ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
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
        group.SetName(record.Signature);
        group.m_BaseName = record.StorageKey;
        group.m_Persist = true;
        if (eAIGroup.s_PersistentGroups.Find(group) == -1) eAIGroup.s_PersistentGroups.Insert(group);
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
        PredatePeerState peer = binding.State;
        PredateOperatorService.UpgradeRivalEquipment(session, actor, peer.Operator);
        actor.PredateBindInformation(true, peer.Operator.Id, peer.Memory, peer.PendingSounds, session.m_WorldState.SimulationSeconds);
        actor.m_PredateCombatEnabled = true;
        actor.m_PredateSignatureRegistered = true;
        actor.eAI_SetAccuracy(session.m_HuntConfig.BotAccuracyMin, session.m_HuntConfig.BotAccuracyMax);
        actor.eAI_SetThreatDistanceLimit(session.m_HuntConfig.ThreatDistance);
        actor.eAI_SetUnlimitedReload(0);
        session.SoloApplyCombatRules(actor);
        actor.SetMovementSpeedLimits(0, 0);
        actor.DisableSimulation(true);
        binding.Group = group;
        binding.Actor = actor;
        binding.Active = false;
    }

    PredatePeerBinding Find(string id)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.State.Operator.Id == id) return binding;
        return null;
    }

    bool RefreshSupplies(PredateSession session, float now)
    {
        bool refreshed = false;
        foreach (PredatePeerBinding binding : m_Bindings)
        {
            PredateOperatorRecord record = binding.State.Operator;
            if (PredateSupplyService.Advance(session, binding.Actor, record.Supplies, PredateOperatorService.RivalKit(record), now, now)) refreshed = true;
        }
        return refreshed;
    }

    int AvailableMask(PredateSession session)
    {
        int mask;
        if (session.PredateTargetAvailable()) mask = PredateProtocol.OperatorBit("raven-01");
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.Actor && binding.Actor.IsAlive())
                mask = mask | PredateProtocol.OperatorBit(binding.State.Operator.Id);
        return mask;
    }

    PredatePeerBinding PlayerLock()
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.State.PlayerReceiver.Selected) return binding;
        return null;
    }

    bool SelectPlayerTarget(PredateSession session, string id)
    {
        bool wantsRaven = id == "raven-01";
        if (id != "" && (AvailableMask(session) & PredateProtocol.OperatorBit(id)) == 0) return false;
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
            if (sampled && track.Selected && track.LastSample) peer.State.InterceptedUntil = now + 30;
        }
    }

    void AddMapPulses(array<PredateSignalTrack> tracks, float now)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
            if (binding.Actor && binding.Actor.IsAlive())
                PredateSignalService.AddMapPulse(tracks, binding.State.PlayerReceiver, now);
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
            array<vector> occupied = {};
            PredateDeployment.OperatorOccupancy(session, player, occupied);
            vector site;
            if (!PredateSpawnPlanner.ReplacementSite(session, previous.Start, occupied, site))
            {
                record.RespawnAt = now + 15;
                continue;
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
            record.RespawnAt = 0;
            record.EquipmentRevision = 0;
            record.RifleKit = "";
            record.Supplies = new PredateSupplyPlan;
            PredatePeerState fresh = MakePeer(index + 1, previous.Receiver.SubjectId, site, now + 2);
            fresh.Operator = record;
            binding.State = fresh;
            m_State.Peers[index] = fresh;
            if (!BindNative(session, binding, error)) return false;
            session.PredateSave();
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
            binding.Actor.DisableSimulation(true);
            binding.Active = false;
        }
    }

    bool UpdateBindings(PredateSession session, bool active, out string error)
    {
        PredateOperatorIndex index = PredateOperatorIndex.Capture();
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
            actor.DisableSimulation(!active);
            if (active) actor.SetMovementSpeedLimits(2, 3);
            else actor.SetMovementSpeedLimits(0, 0);
        }
        return true;
    }

    void RetirePlayerContact(string playerId, float now)
    {
        foreach (PredatePeerBinding peer : m_Bindings)
        {
            PredateSignalTrack track = peer.State.Receiver;
            bool tracksPreviousLife = track.SubjectId == "player-01";
            PredateDeployment.RetireOldContact(peer.State.Hunter, peer.State.Memory, peer.Actor, playerId, now, tracksPreviousLife);
            if (tracksPreviousLife)
            {
                track.LastSample = null;
                track.History.Clear();
            }
            peer.State.PlayerReceiver.SetPlayerTracking(false);
        }
    }

    PlayerBase ResolveSubject(PredateSession session, PlayerBase player, string id, out float energy)
    {
        if (id == "raven-01")
        {
            energy = session.m_WorldState.BotActivity.Energy;
            return session.m_HuntBot;
        }
        if (id == "player-01")
        {
            energy = session.m_WorldState.PlayerActivity.Energy;
            return player;
        }
        PredatePeerBinding peer = Find(id);
        if (peer)
        {
            energy = peer.State.SignatureActivity.Energy;
            return peer.Actor;
        }
        return null;
    }

    string FallbackSubject(PredateSession session, PredatePeerState observer, PlayerBase player)
    {
        // Choose among remaining signatures without testing their positions.
        foreach (PredatePeerBinding rival : m_Bindings)
        {
            if (rival.State == observer || !rival.Actor || !rival.Actor.IsAlive()) continue;
            return rival.State.Operator.Id;
        }
        if (session.PredateTargetAvailable()) return "raven-01";
        if (player && player.IsAlive()) return "player-01";
        return "";
    }

    void MarkInterception(PredateSession session, string subject, float until)
    {
        PredateWorldState state = session.m_WorldState;
        if (subject == "raven-01") state.BotInterceptionUntil = Math.Max(until, state.BotInterceptionUntil);
        else if (subject == "player-01") state.PlayerInterceptionUntil = Math.Max(until, state.PlayerInterceptionUntil);
        else
        {
            PredatePeerBinding peer = Find(subject);
            if (peer) peer.State.InterceptedUntil = Math.Max(until, peer.State.InterceptedUntil);
        }
    }

    bool Advance(PredateSession session, PlayerBase player, out string error)
    {
        float now = session.m_WorldState.SimulationSeconds;
        bool active = session.m_ActorsActive && !session.m_Paused && !session.m_Suspending;
        if (!UpdateBindings(session, active, error)) return false;
        if (!active || !player || now <= m_LastTick) return true;
        if (!RespawnDefeated(session, player, now, error)) return false;
        string playerId = player.GetIdentity().GetId();
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
        foreach (PredatePeerBinding receiverOwner : m_Bindings)
        {
            if (!receiverOwner.Actor) continue;
            PredatePeerState peer = receiverOwner.State;
            PredateSignalTrack receiver = peer.Receiver;
            float energy;
            PlayerBase target = ResolveSubject(session, player, receiver.SubjectId, energy);
            float interval = session.m_HuntConfig.PulseInterval;
            if (!target || !target.IsAlive())
            {
                receiver.LastSample = null;
                if (peer.Hunter.HasGoal && peer.Hunter.EvidenceKey.IndexOf("signal-") == 0)
                    PredateHunterBrain.Hold(peer.Hunter, "hold", "Selected rival is unavailable");
                string replacement = FallbackSubject(session, peer, player);
                if (replacement != "")
                {
                    receiver.SubjectId = replacement;
                    receiver.History.Clear();
                    target = ResolveSubject(session, player, replacement, energy);
                }
            }
            bool available = target && target.IsAlive();
            receiver.SetSelected(available, now, interval);
            if (available && receiver.Due(now))
            {
                PredateObservation sample = PredateSignalRules.Sample(receiver, receiverOwner.Actor.GetPosition(), target.GetPosition(), now, session.m_HuntConfig, energy, Math.RandomFloat(-1, 1));
                receiver.Consume(now, interval, sample);
                if (sample)
                {
                    peer.Memory.Add(sample, now);
                    MarkInterception(session, receiver.SubjectId, now + 30);
                }
            }
        }
        foreach (PredatePeerBinding observer : m_Bindings)
        {
            if (!observer.Actor) continue;
            PredatePeerState thinking = observer.State;
            bool combat = observer.Actor.PredateTacticalPriority(now);
            bool recover = observer.Actor.PredateOwnActionPriority();
            PredateHunterBrain.Step(thinking.Hunter, observer.Actor.GetPosition(), now, thinking.Memory, thinking.Receiver, combat, recover, now < thinking.InterceptedUntil, session.m_WorldState.PlayerLastDeathAt, playerId);
            if (combat || recover)
            {
                if (observer.Actor.m_PredateAppliedGoal != -1) observer.Actor.PredateReleaseRoute();
            }
            else observer.Actor.PredateApplyRoute(thinking.Hunter);
        }
        return true;
    }

    bool Save(PredateSession session, out string error)
    {
        foreach (PredatePeerBinding binding : m_Bindings)
        {
            PredatePeerState peer = binding.State;
            PredateOperatorRecord record = peer.Operator;
            eAIBase actor;
            eAIGroup group;
            if (!session.m_Operators.Resolve(record.Id, actor, group, error)) return false;
            if (record.LifeState == "defeated")
            {
                if (binding.Group)
                {
                    binding.Group.m_Persist = false;
                    eAIGroup.s_PersistentGroups.RemoveItem(binding.Group);
                }
                binding.Actor = null;
                binding.Group = null;
                continue;
            }
            if (!group || !actor)
            {
                error = "Cannot checkpoint peer " + record.Id;
                return false;
            }
            // Store the bound native actor, including weapons and clothing.
            PredateSignalService.CaptureActor(actor, peer.SignatureActivity);
            group.Save(true);
            peer.Health = actor.GetHealth("", "Health");
            peer.Blood = actor.GetHealth("", "Blood");
            peer.Shock = actor.GetHealth("", "Shock");
            peer.HasVitals = true;
        }
        return JsonFileLoader<PredatePeerRosterSave>.SaveFile(SAVE_PATH, m_State, error);
    }
}
