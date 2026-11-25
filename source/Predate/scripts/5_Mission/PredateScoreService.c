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
                if (JsonFileLoader<PredateLifeScore>.LoadFile(path, life, error) && life && life.Life == number && life.Version == 1 && life.Encounters)
                    m_Lives.Set(number, life);
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
        if (!m_Session.m_HuntReady || m_Session.m_Paused || !player.IsAlive()) return;
        PredateWorldState state = m_Session.m_WorldState;
        if (state.PlayerAwaitingRedeploy) return;
        PredateLifeScore life = Life(state.PlayerLife, true);
        if (!life) return;
        player.m_PredateScoringLife = life.Life;
        life.Shots++;
        m_DirtyLives.Insert(life.Life);
        int now = GetGame().GetTime();
        if (!player.m_PredateScoredShots) player.m_PredateScoredShots = new array<ref PredateScoredShot>;
        array<ref PredateScoredShot> shots = player.m_PredateScoredShots;
        while (shots.Count() > 0 && (shots.Count() >= 64 || now - shots[0].At > 4000))
            shots.RemoveOrdered(0);
        PredateScoredShot shot = new PredateScoredShot;
        shot.FirearmEntity = weapon;
        shot.RoundNumber = life.Shots;
        shot.Ammo = ammo;
        shot.At = now;
        shot.Origin = player.GetPosition();
        shots.Insert(shot);
    }

    int MatchShot(PlayerBase player, EntityAI source, string ammo, vector impact, PredateLifeScore life)
    {
        // Human hit callbacks carry no projectile ID. Match a bounded window
        // by weapon, ammo and flight time. Reusing a matched round is free.
        if (!player.m_PredateScoredShots) return 0;
        Weapon_Base weapon = Weapon_Base.Cast(source);
        int now = GetGame().GetTime();
        float speed = GetGame().ConfigGetFloat("CfgAmmo " + ammo + " typicalSpeed");
        speed = Math.Max(speed, 180);
        PredateScoredShot nearest;
        float best = 260;
        foreach (PredateScoredShot shot : player.m_PredateScoredShots)
        {
            if (shot.Ammo != ammo || (weapon && shot.FirearmEntity != weapon) || now - shot.At > 4000) continue;
            float flight = vector.Distance(shot.Origin, impact) * 1000 / speed;
            float error = Math.AbsFloat(now - shot.At - flight);
            if (error >= best) continue;
            nearest = shot;
            best = error;
        }
        if (!nearest) return 0;
        if (!nearest.Hit)
        {
            nearest.Hit = true;
            life.Hits++;
        }
        return nearest.RoundNumber;
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
        int number = player.m_PredateScoringLife;
        if (!number && player.IsAlive() && !state.PlayerAwaitingRedeploy) number = state.PlayerLife;
        PredateLifeScore life = Life(number, number == state.PlayerLife);
        if (!life) return;
        PredateOperatorRecord record = actor.m_PredateOperatorRecord;
        PredatePursuitScore entry = life.Find(record.Id, record.Deployment, true);
        bool firstHit = !entry.ReachedContact;
        if (firstHit)
        {
            if (entry == m_Tracked) RefreshTracking(player, life, true);
            entry.ReachedContact = true;
            entry.Committed = entry.Tracked;
            if (entry.Tracked) entry.TimeToContact = Math.Max(0, state.SimulationSeconds - entry.LockStartedAt);
        }
        record.ScoreInjuryLife = life.Life;
        m_DirtyLives.Insert(life.Life);
        int roundNumber;
        entry.CreditedRange = 0;
        if (damageType == DT_FIRE_ARM)
        {
            entry.CreditedRange = vector.Distance(player.GetPosition(), impact);
            roundNumber = MatchShot(player, source, ammo, impact, life);
        }
        entry.MatchedRound = roundNumber > 0;
        if (firstHit && entry.Tracked && entry.MatchedRound)
            entry.RoundsToFirstHit = Math.Max(0, roundNumber - entry.RoundsAtLock);
        entry.PrecisionRounds = life.Shots;
        entry.PrecisionHits = life.Hits;
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
        if (player.m_PredateScoreKiller != "") life.Killer = PredateProtocol.DisplayName(player.m_PredateScoreKiller);
        foreach (PredatePursuitScore entry : life.Encounters)
            if (entry.Outcome == "") entry.Outcome = "Life ended";
        m_Tracked = null;
        m_Player = null;
        m_TrackActor = null;
        m_TrackSignal = null;
    }
}
