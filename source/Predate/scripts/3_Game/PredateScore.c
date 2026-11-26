class PredatePursuitScore
{
    string OperatorId;
    string Callsign;
    int Deployment;
    bool Tracked;
    bool ReachedContact;
    bool Committed;
    bool WoundKill;
    bool CounterHunt;
    float CreditedRange;
    bool MatchedRound;
    string Outcome;
    float LockSeconds;
    float LockStartedAt;
    float TimeToContact;
    float FinalApproachSeconds;
    float GroundCovered;
    float UsefulClosure;
    float ReturnDamage;
    int Pulses;
    int LastPulseSerial;
    int RoundsAtLock;
    int RoundsToFirstHit;
    int PrecisionRounds;
    int PrecisionHits;
    int PrecisionThrough;
    int PrecisionPending;
    bool PrecisionUnknown;
    int PursuitPoints;
    int EngagementPoints;
    int CounterPoints;
    int PrecisionPoints;
    int RangePoints;
    int KillPoints;

    void BeginTracking(float now, int shots)
    {
        if (Tracked) return;
        Tracked = true;
        LockStartedAt = now;
        RoundsAtLock = shots;
    }

    void Award()
    {
        Outcome = "Converted";
        KillPoints = 1000;
        float closure = Math.Max(0, UsefulClosure);
        float route = Math.Min(1, (1.35 * closure + 150) / Math.Max(1, GroundCovered));
        PursuitPoints = Math.Round(500 * Math.Min(closure / 1000, 1) * route);
        EngagementPoints = Math.Round(200 * Math.Clamp(1 - ReturnDamage / 60, 0, 1));
        if (CounterHunt) CounterPoints = 150;
        RangePoints = Math.Round(Math.Max(0, CreditedRange - 100) * 0.5);
        UpdatePrecision();
    }

    void UpdatePrecision()
    {
        if (MatchedRound && PrecisionRounds > 0 && PrecisionPending == 0 && !PrecisionUnknown)
        {
            float accuracy = Math.Clamp(1.0 * PrecisionHits / PrecisionRounds, 0, 1);
            PrecisionPoints = Math.Round(150 * accuracy * accuracy);
        }
    }

    int Total()
    {
        return KillPoints + PursuitPoints + EngagementPoints + CounterPoints + PrecisionPoints + RangePoints;
    }
}

class PredateLifeScore
{
    int Version = 2;
    int Life;
    float StartedAt;
    float SurvivedSeconds;
    string EndReason;
    string Killer;
    int KillerDeployment;
    bool Partial;
    int Shots;
    int Hits;
    int ExtraOperatorHits;
    int Misses;
    int PendingRounds;
    int UnverifiedRounds;
    int LegacyShots;
    int LegacyHits;
    int Kills;
    int KillScore;
    int NonLethalHitPoints;
    int MultiHitBonusPoints;
    int MultiKillBonusPoints;
    int SurvivalScore;
    int Score;
    ref array<ref PredatePursuitScore> Encounters = new array<ref PredatePursuitScore>;

    void BeginExactScoring()
    {
        if (Version != 1) return;
        LegacyShots = Shots;
        LegacyHits = Hits;
        Version = 2;
    }

    void RecoverInterruptedRounds()
    {
        if (PendingRounds <= 0) return;
        // Native projectiles are transient. A resumed checkpoint has no way
        // for its remaining in-flight rounds to reach an operator.
        Misses += PendingRounds;
        PendingRounds = 0;
        foreach (PredatePursuitScore entry : Encounters)
        {
            if (entry.PrecisionPending <= 0) continue;
            entry.PrecisionPending = 0;
            if (entry.KillPoints == 0) continue;
            int previous = entry.PrecisionPoints;
            entry.UpdatePrecision();
            KillScore += entry.PrecisionPoints - previous;
        }
        UpdateScore();
    }

    void UpdateScore()
    {
        Score = KillScore + NonLethalHitPoints + MultiHitBonusPoints + MultiKillBonusPoints + SurvivalScore;
    }

    void UpdateTime(float now)
    {
        if (EndReason != "") return;
        SurvivedSeconds = Math.Max(0, now - StartedAt);
        SurvivalScore = Math.Floor(Math.Min(SurvivedSeconds, 600) / 6) + Math.Floor(Math.Clamp(SurvivedSeconds - 600, 0, 1200) / 12);
        UpdateScore();
    }

    PredatePursuitScore Find(string id, int deployment, bool create = false)
    {
        foreach (PredatePursuitScore entry : Encounters)
            if (entry.OperatorId == id && entry.Deployment == deployment) return entry;
        if (!create) return null;
        PredatePursuitScore encounter = new PredatePursuitScore;
        encounter.OperatorId = id;
        encounter.Callsign = PredateProtocol.DisplayName(id);
        encounter.Deployment = deployment;
        Encounters.Insert(encounter);
        return encounter;
    }
}
