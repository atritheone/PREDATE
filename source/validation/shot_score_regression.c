class PredateShotScoreRegression : MissionServer
{
    bool m_ShotStarted;
    PlayerBase m_ShotPlayer;
    eAIBase m_ShotTarget;
    float m_ExpectedRange;
    int m_FiringDeployment;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_ShotStarted || !m_PredateSession.m_ActorsActive || !m_PredateSession.m_HuntBot) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity() || !player.IsPlayerLoaded()) continue;
            Weapon_Base weapon = Weapon_Base.Cast(player.GetEntityInHands());
            if (!weapon) continue;
            m_ShotStarted = true;
            m_ShotPlayer = player;
            m_FiringDeployment = m_PredateSession.m_WorldState.PlayerLife;
            m_ShotTarget = m_PredateSession.m_HuntBot;
            m_ShotTarget.m_PredateProbeShooter = true;
            m_ShotTarget.m_PredateProbeCeaseFire = true;
            // Let this discharge expire without any native stop callback.
            PredateProjectileRound miss = RegisterRound(weapon, "PR_0_Bullet_556x45");
            // Verify confirmation can recover the staged tag without EEFired.
            if (!weapon.m_PredateArmedAmmo) weapon.m_PredateArmedAmmo = new map<int, string>;
            weapon.m_PredateArmedAmmo.Set(0, miss.Tag);
            weapon.OnFire(0);
            weapon.m_PredateArmedAmmo.Remove(0);
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ApplyFixtureImpact, 450, false, weapon);
            return;
        }
    }

    void ApplyFixtureImpact(Weapon_Base weapon)
    {
        PredateProjectileRound hit = SeedRound(weapon, "PR_1_Bullet_556x45");
        // The projectile was fired on the previous deployment.
        m_PredateSession.m_WorldState.PlayerLife = m_FiringDeployment + 1;
        m_ShotPlayer.m_PredateScoringLife = m_FiringDeployment + 1;
        vector position = m_PredateSession.PredateGround(m_ShotPlayer.GetPosition() + "0 0 12");
        m_ShotTarget.SetPosition(position);
        vector impact = position + "0 1.2 0";
        m_ExpectedRange = vector.Distance(m_ShotPlayer.GetPosition(), impact);
        m_ShotTarget.ProcessDirectDamage(DT_FIRE_ARM, weapon, "Torso", hit.Tag, impact, 0.2);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ObserveDuplicateContact, 150, false, hit, impact);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(WriteRegressionResult, 1800, false, weapon, impact);
    }

    PredateProjectileRound SeedRound(Weapon_Base weapon, string tag, Object earlyTarget = null, vector earlyImpact = "0 0 0")
    {
        PredateProjectileRound round = RegisterRound(weapon, tag);
        if (earlyTarget) PredateProjectileRounds.ReportContact(tag, earlyTarget, earlyImpact);
        round.Fired = true;
        m_PredateSession.m_Score.OnFire(m_ShotPlayer, weapon, tag);
        return round;
    }

    PredateProjectileRound RegisterRound(Weapon_Base weapon, string tag)
    {
        PredateProjectileRound round = new PredateProjectileRound;
        round.Tag = tag;
        round.Weapon = weapon;
        round.Origin = m_ShotPlayer.GetPosition();
        round.Lifetime = 0.5;
        PredateProjectileRounds.Active.Set(tag, round);
        return round;
    }

    void ObserveDuplicateContact(PredateProjectileRound round, vector impact)
    {
        m_PredateSession.m_Score.OnProjectileContact(round, m_ShotTarget, impact, false);
    }

    void WriteRegressionResult(Weapon_Base weapon, vector impact)
    {
        PredateScoreService score = m_PredateSession.m_Score;
        PredateLifeScore life = score.Life(m_FiringDeployment);
        if (!life) return;
        PredateLifeScore nextLife = score.Life(m_FiringDeployment + 1);
        int nextHits;
        if (nextLife) nextHits = nextLife.Hits;
        bool expiryReleased = !PredateProjectileRounds.Active.Contains("PR_0_Bullet_556x45");
        m_ShotPlayer.m_PredateScoringLife = m_FiringDeployment;
        SeedRound(weapon, "PR_2_Bullet_556x45", m_ShotTarget, impact);
        SeedRound(weapon, "PR_3_Bullet_556x45");
        score.FinishRounds();
        bool quitReleased = !PredateProjectileRounds.Active.Contains("PR_3_Bullet_556x45");
        PredateLifeScore resumed = new PredateLifeScore;
        resumed.Shots = 1;
        resumed.PendingRounds = 1;
        resumed.RecoverInterruptedRounds();
        resumed.RecoverInterruptedRounds();
        score.Save();
        float range;
        foreach (PredatePursuitScore encounter : life.Encounters)
            if (encounter.ReachedContact) range = encounter.CreditedRange;
        FileHandle report = OpenFile("$profile:ShotScoreRegression.json", FileMode.WRITE);
        if (!report) return;
        FPrintln(report, "{");
        FPrintln(report, "\"Shots\":" + life.Shots + ",");
        FPrintln(report, "\"Hits\":" + life.Hits + ",");
        FPrintln(report, "\"Misses\":" + life.Misses + ",");
        FPrintln(report, "\"Unverified\":" + life.UnverifiedRounds + ",");
        FPrintln(report, "\"Pending\":" + life.PendingRounds + ",");
        FPrintln(report, "\"ExpiryReleased\":" + expiryReleased + ",");
        FPrintln(report, "\"QuitReleased\":" + quitReleased + ",");
        FPrintln(report, "\"ResumedMisses\":" + resumed.Misses + ",");
        FPrintln(report, "\"LaterDeploymentHits\":" + nextHits + ",");
        FPrintln(report, "\"BuckshotLifetime\":" + GetGame().ConfigGetFloat("CfgAmmo PR_0_Bullet_12GaugePellets timeToLive") + ",");
        FPrintln(report, "\"BucaLifetime\":" + GetGame().ConfigGetFloat("CfgAmmo PR_0_BC_Bullet_FN57x28mm timeToLive") + ",");
        FPrintln(report, "\"TargetHealth\":" + m_ShotTarget.GetHealth("", "Health") + ",");
        FPrintln(report, "\"ExpectedRange\":" + m_ExpectedRange + ",");
        FPrintln(report, "\"CreditedRange\":" + range + "}");
        CloseFile(report);
    }
}
