class PredateScoreService
{
    PredateSession m_Session;
    ref map<int, ref PredateLifeScore> m_Lives = new map<int, ref PredateLifeScore>;
    ref set<int> m_CheckedLives = new set<int>;
    ref set<int> m_DirtyLives = new set<int>;
    PredatePursuitScore m_Tracked;
    eAIBase m_TrackActor;
    PlayerBase m_Player;
    vector m_PlayerPosition;
    vector m_TargetPosition;
    float m_SampledAt;
    PredateSignalTrack m_TrackSignal;
    ref array<ref PredateProjectileRound> m_FlightRounds = new array<ref PredateProjectileRound>;
    float m_RoundClock;
    float m_NextRoundExpiry;

    void PredateScoreService(PredateSession session)
    {
        m_Session = session;
    }

    string LifePath(int number)
    {
        return "$profile:PredateLifeScore-" + number + ".json";
    }

    void Save()
    {
        // Each life is its own record. Only dirty lives are written at a checkpoint.
        for (int i = m_DirtyLives.Count() - 1; i >= 0; i--)
        {
            int number = m_DirtyLives[i];
            PredateLifeScore life = m_Lives.Get(number);
            string path = LifePath(number);
            string error;
            if (JsonFileLoader<PredateLifeScore>.SaveFile(path, life, error))
                m_DirtyLives.Remove(i);
            else Print("[Predate] Could not save life score " + number + ": " + error);
        }
    }

    PredateLifeScore Life(int number, bool create = false)
    {
        PredateWorldState state = m_Session.m_WorldState;
        if (!state || number < 1) return null;
        PredateLifeScore life;
        if (m_Lives.Find(number, life)) return life;
        if (m_CheckedLives.Find(number) < 0)
        {
            m_CheckedLives.Insert(number);
            string path = LifePath(number);
            if (FileExist(path))
            {
                string error;
                life = new PredateLifeScore;
                if (JsonFileLoader<PredateLifeScore>.LoadFile(path, life, error) && life && life.Life == number && (life.Version == 1 || life.Version == 2) && life.Encounters)
                {
                    if (life.PendingRounds > 0)
                    {
                        life.RecoverInterruptedRounds();
                        m_DirtyLives.Insert(number);
                    }
                    m_Lives.Set(number, life);
                }
                else
                {
                    m_Lives.Set(number, null);
                    Print("[Predate] Could not load life score " + number + ": " + error);
                }
                return m_Lives.Get(number);
            }
        }
        if (!create || number != state.PlayerLife || state.PlayerAwaitingRedeploy) return null;
        life = new PredateLifeScore;
        life.Life = number;
        life.StartedAt = state.SimulationSeconds;
        life.Partial = state.SimulationSeconds > state.PlayerLifeStartedAt;
        m_Lives.Set(number, life);
        m_DirtyLives.Insert(number);
        return life;
    }

    void Tick(PlayerBase player)
    {
        PredateWorldState state = m_Session.m_WorldState;
        if (!player.IsPlayerLoaded() || state.PlayerAwaitingRedeploy) return;
        player.m_PredateScoringLife = state.PlayerLife;
        PredateLifeScore life = Life(state.PlayerLife, true);
        if (!life) return;
        life.UpdateTime(state.SimulationSeconds);
        m_DirtyLives.Insert(life.Life);
        if (m_Player == player && state.SimulationSeconds - m_SampledAt < 1) return;
        RefreshTracking(player, life);
    }

    void RecordPulse(PlayerBase player, eAIBase actor, PredateObservation pulse)
    {
        PredateWorldState state = m_Session.m_WorldState;
        if (!player.IsPlayerLoaded() || state.PlayerAwaitingRedeploy || !actor || !actor.m_PredateOperatorRecord) return;
        PredateLifeScore life = Life(state.PlayerLife, true);
        if (!life || life.EndReason != "") return;
        PredateOperatorRecord record = actor.m_PredateOperatorRecord;
        PredatePursuitScore encounter = life.Find(record.Id, record.Deployment, true);
        if (pulse.Serial <= encounter.LastPulseSerial) return;
        encounter.BeginTracking(state.SimulationSeconds, life.Shots);
        encounter.Pulses++;
        encounter.LastPulseSerial = pulse.Serial;
        m_DirtyLives.Insert(life.Life);
    }

    void RefreshTracking(PlayerBase player, PredateLifeScore life, bool flush = false)
    {
        PredateWorldState state = m_Session.m_WorldState;
        if (m_Tracked && m_Player == player) Sample(player, state.SimulationSeconds);
        if (flush) return;
        if (m_Tracked && m_TrackSignal && m_TrackSignal.HasLostPlayerReception() && m_Tracked.Outcome == "")
            m_Tracked.Outcome = "Signal lost";

        PredateSignalTrack signal = state.PlayerSignal;
        eAIBase actor = m_Session.m_HuntBot;
        // The receiver already scans its bindings during a regular refresh.
        // The locked target alone contributes any sampling work here.
        if (m_Session.m_PeerRoster)
        {
            PredatePeerBinding peer = m_Session.m_PeerRoster.PlayerLock();
            if (peer)
            {
                signal = peer.State.PlayerReceiver;
                actor = peer.Actor;
            }
        }

        PredatePursuitScore next;
        if (actor && actor.IsAlive() && actor.m_PredateOperatorRecord && signal && signal.Selected && !signal.HasLostPlayerReception())
        {
            PredateOperatorRecord record = actor.m_PredateOperatorRecord;
            PredatePursuitScore encounter = life.Find(record.Id, record.Deployment, true);
            if (!encounter.ReachedContact && (encounter.Outcome == "" || encounter.Outcome == "Signal lost"))
            {
                next = encounter;
                next.BeginTracking(state.SimulationSeconds, life.Shots);
                next.Outcome = "";
            }
        }
        m_Player = player;
        m_Tracked = next;
        m_TrackActor = actor;
        m_TrackSignal = signal;
        m_SampledAt = state.SimulationSeconds;
        if (m_Tracked)
        {
            m_PlayerPosition = player.GetPosition();
            m_TargetPosition = actor.GetPosition();
        }
    }

    void Sample(PlayerBase player, float now)
    {
        if (!m_TrackActor || m_Tracked.ReachedContact) return;
        float elapsed = now - m_SampledAt;
        vector playerPosition = player.GetPosition();
        vector targetPosition = m_TrackActor.GetPosition();
        float step = PredateHunterBrain.Distance(m_PlayerPosition, playerPosition);
        if (elapsed > 0 && elapsed <= 3 && step <= 15 * elapsed + 5)
        {
            m_Tracked.LockSeconds += elapsed;
            if (m_Tracked.LockSeconds >= 30) m_Tracked.Committed = true;
            if (PredateSignalRules.FinalApproach(playerPosition, targetPosition)) m_Tracked.FinalApproachSeconds += elapsed;
            m_Tracked.GroundCovered += step;
            float before = PredateHunterBrain.Distance(m_PlayerPosition, m_TargetPosition) + PredateHunterBrain.Distance(m_PlayerPosition, targetPosition);
            float after = PredateHunterBrain.Distance(playerPosition, m_TargetPosition) + PredateHunterBrain.Distance(playerPosition, targetPosition);
            m_Tracked.UsefulClosure += (before - after) * 0.5;
        }
        m_PlayerPosition = playerPosition;
        m_TargetPosition = targetPosition;
        m_SampledAt = now;
    }

    void OnFire(PlayerBase player, Weapon_Base weapon, string ammo)
    {
        if (!m_Session.m_HuntReady || m_Session.m_Suspending) return;
        PredateWorldState state = m_Session.m_WorldState;
        int deployment = player.m_PredateScoringLife;
        if (!deployment && player.IsAlive() && !state.PlayerAwaitingRedeploy) deployment = state.PlayerLife;
        PredateLifeScore life = Life(deployment, true);
        if (!life) return;
        PredateProjectileRound round = PredateProjectileRounds.Find(ammo);
        if (!round)
        {
            string simulation = GetGame().ConfigGetTextOut("CfgAmmo " + ammo + " simulation");
            if (simulation == "shotArrow" || simulation == "shotIlluminating" || simulation == "shotShell") return;
        }
        else if (round.Number > 0) return;
        player.m_PredateScoringLife = life.Life;
        life.BeginExactScoring();
        life.Shots++;
        m_DirtyLives.Insert(life.Life);
        if (round)
        {
            round.Deployment = life.Life;
            round.Number = life.Shots;
            life.PendingRounds++;
            TrackFlight(round);
            if (round.EarlyContacts)
            {
                foreach (PredateProjectileContact contact : round.EarlyContacts)
                    OnProjectileContact(round, contact.Target, contact.Position, true);
                round.EarlyContacts = null;
            }
        }
        else life.UnverifiedRounds++;
        if (PredateServerDiagnostics.TraceShots)
        {
            PredateServerDiagnostics.RememberShotSource(weapon);
            Print("[PredateShot] fired " + GetGame().GetTime() + " " + ammo + " weapon=" + weapon + " deployment=" + life.Life + " round=" + life.Shots);
            if (round)
                Print("[PredateShot] ammo config " + ammo + " simulation=" + GetGame().ConfigGetTextOut("CfgAmmo " + ammo + " simulation") + " speed=" + GetGame().ConfigGetFloat("CfgAmmo " + ammo + " initSpeed") + " lifetime=" + GetGame().ConfigGetFloat("CfgAmmo " + ammo + " timeToLive"));
            else Print("[PredateShot] confirmed discharge has no round tag: " + ammo);
        }
    }

    void TrackFlight(PredateProjectileRound round)
    {
        // Contacts can penetrate, and some runs never receive a stop event.
        // Retain the tag until the native projectile lifetime and event drain end.
        round.ExpiresAt = m_RoundClock + round.Lifetime + 0.5;
        if (m_FlightRounds.Count() == 0 || round.ExpiresAt < m_NextRoundExpiry)
            m_NextRoundExpiry = round.ExpiresAt;
        m_FlightRounds.Insert(round);
    }

    void ResolveRound(PredateLifeScore life, PredateProjectileRound round, bool hit)
    {
        life.PendingRounds--;
        if (hit)
        {
            round.Hit = true;
            life.Hits++;
        }
        else life.Misses++;
        m_DirtyLives.Insert(life.Life);
        foreach (PredatePursuitScore entry : life.Encounters)
        {
            if (entry.PrecisionPending == 0 || round.Number > entry.PrecisionThrough) continue;
            entry.PrecisionPending--;
            if (hit) entry.PrecisionHits++;
            if (entry.KillPoints == 0 || entry.PrecisionPending != 0) continue;
            int previous = entry.PrecisionPoints;
            entry.UpdatePrecision();
            life.KillScore += entry.PrecisionPoints - previous;
            life.Score = life.KillScore + life.SurvivalScore;
        }
    }

    void TickRounds(float timeslice)
    {
        m_RoundClock += Math.Min(timeslice, 0.1);
        if (m_FlightRounds.Count() == 0 || m_RoundClock < m_NextRoundExpiry) return;
        m_NextRoundExpiry = 1.0e20;
        for (int i = m_FlightRounds.Count() - 1; i >= 0; i--)
        {
            PredateProjectileRound round = m_FlightRounds[i];
            if (m_RoundClock < round.ExpiresAt)
            {
                m_NextRoundExpiry = Math.Min(m_NextRoundExpiry, round.ExpiresAt);
                continue;
            }
            CloseRound(round);
            m_FlightRounds.Remove(i);
        }
    }

    void FinishRounds()
    {
        foreach (PredateProjectileRound round : m_FlightRounds)
            CloseRound(round);
        m_FlightRounds.Clear();
    }

    void CloseRound(PredateProjectileRound round)
    {
        if (round.Number > 0 && !round.Hit)
            ResolveRound(Life(round.Deployment), round, false);
        round.Closed = true;
        PredateProjectileRounds.Active.Remove(round.Tag);
    }

    void OnProjectileContact(PredateProjectileRound round, Object hitObject, vector point, bool aliveBeforeConfirmation)
    {
        if (round.Number == 0) return;
        eAIBase actor = eAIBase.Cast(hitObject);
        Clothing worn = Clothing.Cast(hitObject);
        if (!actor && worn) actor = eAIBase.Cast(worn.GetHierarchyRootPlayer());
        if (!actor || !actor.m_PredateOperatorRecord || (!aliveBeforeConfirmation && !actor.IsAlive())) return;
        PredateLifeScore life = Life(round.Deployment);
        if (!life) return;
        if (round.Hit && round.ContactTarget == actor && round.ContactPosition == point) return;
        round.ContactTarget = actor;
        round.ContactPosition = point;
        if (!round.Hit) ResolveRound(life, round, true);
        RecordContact(life, actor.m_PredateOperatorRecord, round);
    }

    PredatePursuitScore RecordContact(PredateLifeScore life, PredateOperatorRecord record, PredateProjectileRound round)
    {
        PredatePursuitScore entry = life.Find(record.Id, record.Deployment, true);
        if (entry.ReachedContact) return entry;
        if (entry == m_Tracked && m_Player) RefreshTracking(m_Player, life, true);
        entry.ReachedContact = true;
        entry.Committed = entry.Tracked;
        if (entry.Tracked)
        {
            entry.TimeToContact = Math.Max(0, m_Session.m_WorldState.SimulationSeconds - entry.LockStartedAt);
            if (round) entry.RoundsToFirstHit = Math.Max(0, round.Number - entry.RoundsAtLock);
        }
        m_DirtyLives.Insert(life.Life);
        return entry;
    }

    void OnDamage(PlayerBase player, eAIBase actor, EntityAI source, int damageType, string ammo, float health)
    {
        if (!m_Session.m_HuntReady || m_Session.m_Paused || !actor || !actor.m_PredateOperatorRecord) return;
        PredateWorldState state = m_Session.m_WorldState;
        if (state.PlayerAwaitingRedeploy) return;
        PredateLifeScore life = Life(state.PlayerLife, true);
        if (!life) return;
        PredateOperatorRecord record = actor.m_PredateOperatorRecord;
        PredatePursuitScore entry = life.Find(record.Id, record.Deployment, true);
        if (entry.KillPoints == 0)
        {
            entry.ReturnDamage += health;
            m_DirtyLives.Insert(life.Life);
        }
    }

    void OnPlayerDamage(PlayerBase player, eAIBase actor, EntityAI source, int damageType, string ammo, vector impact)
    {
        if (!m_Session.m_HuntReady || m_Session.m_Paused || !actor || !actor.m_PredateOperatorRecord) return;
        PredateWorldState state = m_Session.m_WorldState;
        PredateProjectileRound round;
        if (damageType == DT_FIRE_ARM)
        {
            round = PredateProjectileRounds.Find(ammo);
            if (round && round.Number == 0) round = null;
        }
        int number;
        if (round) number = round.Deployment;
        else if (player)
        {
            number = player.m_PredateScoringLife;
            if (!number && player.IsAlive() && !state.PlayerAwaitingRedeploy) number = state.PlayerLife;
        }
        PredateLifeScore life = Life(number, number == state.PlayerLife);
        if (!life) return;
        PredateOperatorRecord record = actor.m_PredateOperatorRecord;
        if (round && !round.Hit) ResolveRound(life, round, true);
        PredatePursuitScore entry = RecordContact(life, record, round);
        record.ScoreInjuryLife = life.Life;
        m_DirtyLives.Insert(life.Life);
        entry.CreditedRange = 0;
        if (round)
        {
            if (round.ContactTarget == actor) impact = round.ContactPosition;
            entry.CreditedRange = vector.Distance(round.Origin, impact);
            if (PredateServerDiagnostics.TraceShots)
                Print("[PredateShot] credited " + GetGame().GetTime() + " " + ammo + " range=" + entry.CreditedRange + " matched=" + (round != null));
        }
        entry.MatchedRound = round != null;
        entry.PrecisionThrough = life.Shots;
        entry.PrecisionRounds = life.Shots - life.LegacyShots;
        entry.PrecisionHits = life.Hits - life.LegacyHits;
        entry.PrecisionPending = life.PendingRounds;
        entry.PrecisionUnknown = life.UnverifiedRounds > 0;
        entry.CounterHunt = actor.m_PredateScoreHuntingPlayer;
    }

    void OnDefeated(PredateOperatorRecord record, PlayerBase credited, bool woundKill)
    {
        if (!m_Session.m_HuntReady || !record) return;
        PredateWorldState state = m_Session.m_WorldState;
        int number = record.ScoreInjuryLife;
        if (credited && !woundKill && !number) number = credited.m_PredateScoringLife;
        PredateLifeScore life;
        if (woundKill || (credited && !eAIBase.Cast(credited))) life = Life(number);
        if (life)
        {
            PredatePursuitScore entry = life.Find(record.Id, record.Deployment, true);
            if (entry.KillPoints == 0)
            {
                entry.WoundKill = woundKill;
                entry.Award();
                life.Kills++;
                m_DirtyLives.Insert(life.Life);
                life.KillScore += entry.Total();
                life.Score = life.KillScore + life.SurvivalScore;
            }
        }
        PredateLifeScore current = Life(state.PlayerLife);
        if (current && current != life)
        {
            PredatePursuitScore other = current.Find(record.Id, record.Deployment);
            if (other && other.KillPoints == 0)
            {
                other.Outcome = "Target eliminated";
                m_DirtyLives.Insert(current.Life);
            }
        }
        if (m_TrackActor && m_TrackActor.m_PredateOperatorRecord == record) m_Tracked = null;
    }

    void EndLife(PlayerBase player)
    {
        PredateWorldState state = m_Session.m_WorldState;
        PredateLifeScore life = Life(state.PlayerLife, true);
        if (!life || life.EndReason != "") return;
        RefreshTracking(player, life, true);
        life.UpdateTime(state.SimulationSeconds);
        m_DirtyLives.Insert(life.Life);
        life.EndReason = "Died";
        if (player.m_PredateScoreSuicide) life.EndReason = "Suicide";
        else if (player.m_PredateScoreKiller != "") life.EndReason = "KIA";
        if (player.m_PredateScoreKiller != "")
        {
            life.Killer = PredateProtocol.DisplayName(player.m_PredateScoreKiller);
            life.KillerDeployment = player.m_PredateScoreKillerDeployment;
        }
        foreach (PredatePursuitScore entry : life.Encounters)
            if (entry.Outcome == "") entry.Outcome = "Life ended";
        m_Tracked = null;
        m_Player = null;
        m_TrackActor = null;
        m_TrackSignal = null;
    }
}
