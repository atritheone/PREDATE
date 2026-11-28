class PredateShotScoreRegression : MissionServer
{
    bool m_ShotStarted;
    PlayerBase m_ShotPlayer;
    eAIBase m_ShotTarget;
    eAIBase m_ShotPeer;
    float m_ExpectedRange;
    float m_TargetHealthAfterHit;
    float m_CreditedRangeAfterHit;
    int m_FiringDeployment;
    int m_HitPointsAfterFirstKill;
    int m_BonusAfterFirstKill;
    int m_KillsAfterFirstKill;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_ShotStarted || !m_PredateSession.m_ActorsActive || !m_PredateSession.m_HuntBot) return;
        if (m_PredateSession.m_HuntConfig.OpponentCount > 1)
        {
            if (!m_PredateSession.m_PeerRoster || m_PredateSession.m_PeerRoster.m_Bindings.Count() == 0) return;
            m_ShotPeer = m_PredateSession.m_PeerRoster.m_Bindings[0].Actor;
            if (!m_ShotPeer) return;
        }
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
            PredateSignalService.Select(m_PredateSession, false);
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
        m_PredateSession.m_WorldState.PlayerSignal.Reception = PredatePulseReception.BEARING;
        PredateSignalService.Select(m_PredateSession, true);
        PredateProjectileRound hit = SeedRound(weapon, "PR_1_Bullet_556x45");
        PredateSignalService.Select(m_PredateSession, false);
        // The projectile was fired on the previous deployment.
        m_PredateSession.m_WorldState.PlayerLife = m_FiringDeployment + 1;
        m_ShotPlayer.m_PredateScoringLife = m_FiringDeployment + 1;
        vector position = m_PredateSession.PredateGround(m_ShotPlayer.GetPosition() + "0 0 12");
        m_ShotTarget.SetPosition(position);
        vector impact = position + "0 1.2 0";
        m_ExpectedRange = vector.Distance(m_ShotPlayer.GetPosition(), impact);
        if (m_ShotPeer) PredateProjectileRounds.ReportContact(hit.Tag, m_ShotPeer, m_ShotPeer.GetPosition() + "0 1.2 0");
        m_ShotTarget.ProcessDirectDamage(DT_FIRE_ARM, weapon, "Torso", hit.Tag, impact, 0.2);
        if (m_ShotPeer)
        {
            vector peerImpact = m_ShotPeer.GetPosition() + "0 1.2 0";
            PredateProjectileRounds.ReportContact(hit.Tag, m_ShotPeer, peerImpact);
            PredateProjectileRounds.ReportContact(hit.Tag, m_ShotTarget, impact + "0 0.1 0");
            PredateProjectileRounds.ReportContact(hit.Tag, m_ShotPeer, peerImpact + "0 0.1 0");
        }
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
        m_TargetHealthAfterHit = m_ShotTarget.GetHealth("", "Health");
        foreach (PredatePursuitScore encounter : life.Encounters)
            if (encounter.OperatorId == m_ShotTarget.m_PredateOperatorRecord.Id) m_CreditedRangeAfterHit = encounter.CreditedRange;
        m_PredateSession.m_WorldState.PlayerLife = m_FiringDeployment;
        m_ShotPlayer.m_PredateScoringLife = m_FiringDeployment;
        m_PredateSession.m_WorldState.PlayerSignal.Reception = PredatePulseReception.BEARING;
        PredateSignalService.Select(m_PredateSession, true);
        PredateProjectileRound secondHit = SeedRound(weapon, "PR_2_Bullet_556x45", m_ShotTarget, impact);
        SeedRound(weapon, "PR_3_Bullet_556x45");
        if (m_ShotPeer)
        {
            vector peerImpact = m_ShotPeer.GetPosition() + "0 1.2 0";
            PredateProjectileRounds.ReportContact(secondHit.Tag, m_ShotPeer, peerImpact);
            SeedRound(weapon, "PR_5_Bullet_556x45", m_ShotPeer, peerImpact);
            PredateProjectileRound spread = SeedRound(weapon, "PR_4_Bullet_12GaugePellets", m_ShotTarget, impact);
            PredateProjectileRounds.ReportContact(spread.Tag, m_ShotPeer, peerImpact);
            PredateProjectileRounds.ReportContact(spread.Tag, m_ShotTarget, impact + "0 0.1 0");
            PredateProjectileRounds.ReportContact(spread.Tag, m_ShotPeer, peerImpact + "0 0.1 0");
            int hitPointsBeforeKill = life.NonLethalHitPoints;
            m_ShotPeer.SetHealth("", "Health", 0.1);
            m_ShotPeer.ProcessDirectDamage(DT_FIRE_ARM, weapon, "Torso", spread.Tag, peerImpact, 0.2);
            // Native damage can deliver EEKilled on a later frame.
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ApplySecondKill, 250, false, weapon, spread.Tag, impact, hitPointsBeforeKill, nextHits);
            return;
        }
        SaveRegressionResult(0, nextHits);
    }

    void ApplySecondKill(Weapon_Base weapon, string ammo, vector impact, int hitPointsBeforeKill, int nextHits)
    {
        PredateLifeScore life = m_PredateSession.m_Score.Life(m_FiringDeployment);
        if (!life) return;
        m_HitPointsAfterFirstKill = life.NonLethalHitPoints;
        m_BonusAfterFirstKill = life.MultiKillBonusPoints;
        m_KillsAfterFirstKill = life.Kills;
        m_ShotTarget.SetHealth("", "Health", 0.1);
        m_ShotTarget.ProcessDirectDamage(DT_FIRE_ARM, weapon, "Torso", ammo, impact, 0.2);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SaveRegressionResult, 250, false, hitPointsBeforeKill, nextHits);
    }

    void SaveRegressionResult(int hitPointsBeforeKill, int nextHits)
    {
        PredateScoreService score = m_PredateSession.m_Score;
        PredateLifeScore life = score.Life(m_FiringDeployment);
        if (!life) return;
        bool expiryReleased = !PredateProjectileRounds.Active.Contains("PR_0_Bullet_556x45");
        score.FinishRounds();
        bool quitReleased = !PredateProjectileRounds.Active.Contains("PR_3_Bullet_556x45");
        PredateLifeScore resumed = new PredateLifeScore;
        resumed.Shots = 1;
        resumed.PendingRounds = 1;
        resumed.RecoverInterruptedRounds();
        resumed.RecoverInterruptedRounds();
        PredatePursuitScore pendingPursuit = resumed.Find("raven-01", 1, true);
        pendingPursuit.PursuitShots = 1;
        pendingPursuit.PursuitPending = 1;
        bool recoveredPursuit = resumed.RecoverInterruptedRounds() && pendingPursuit.PursuitPending == 0;
        score.Save();
        FileHandle report = OpenFile("$profile:ShotScoreRegression.json", FileMode.WRITE);
        if (!report) return;
        FPrintln(report, "{");
        FPrintln(report, "\"Shots\":" + life.Shots + ",");
        FPrintln(report, "\"Hits\":" + life.Hits + ",");
        FPrintln(report, "\"ExtraOperatorHits\":" + life.ExtraOperatorHits + ",");
        FPrintln(report, "\"HitPointsBeforeKill\":" + hitPointsBeforeKill + ",");
        FPrintln(report, "\"HitPointsAfterFirstKill\":" + m_HitPointsAfterFirstKill + ",");
        FPrintln(report, "\"BonusAfterFirstKill\":" + m_BonusAfterFirstKill + ",");
        FPrintln(report, "\"KillsAfterFirstKill\":" + m_KillsAfterFirstKill + ",");
        FPrintln(report, "\"NonLethalHitPoints\":" + life.NonLethalHitPoints + ",");
        FPrintln(report, "\"MultiHitBonusPoints\":" + life.MultiHitBonusPoints + ",");
        FPrintln(report, "\"MultiKillBonusPoints\":" + life.MultiKillBonusPoints + ",");
        FPrintln(report, "\"Kills\":" + life.Kills + ",");
        FPrintln(report, "\"Misses\":" + life.Misses + ",");
        FPrintln(report, "\"Unverified\":" + life.UnverifiedRounds + ",");
        FPrintln(report, "\"Pending\":" + life.PendingRounds + ",");
        FPrintln(report, "\"ExpiryReleased\":" + expiryReleased + ",");
        FPrintln(report, "\"QuitReleased\":" + quitReleased + ",");
        FPrintln(report, "\"ResumedMisses\":" + resumed.Misses + ",");
        FPrintln(report, "\"RecoveredPursuit\":" + recoveredPursuit + ",");
        FPrintln(report, "\"LaterDeploymentHits\":" + nextHits + ",");
        FPrintln(report, "\"BuckshotLifetime\":" + GetGame().ConfigGetFloat("CfgAmmo PR_0_Bullet_12GaugePellets timeToLive") + ",");
        FPrintln(report, "\"BucaLifetime\":" + GetGame().ConfigGetFloat("CfgAmmo PR_0_BC_Bullet_FN57x28mm timeToLive") + ",");
        FPrintln(report, "\"TargetHealth\":" + m_TargetHealthAfterHit + ",");
        FPrintln(report, "\"ExpectedRange\":" + m_ExpectedRange + ",");
        FPrintln(report, "\"CreditedRange\":" + m_CreditedRangeAfterHit + "}");
        CloseFile(report);
    }
}
