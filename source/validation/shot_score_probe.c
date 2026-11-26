class PredateShotProbeMission : MissionServer
{
    PlayerBase m_ShotPlayer;
    eAIBase m_ShotTarget;
    float m_ShotRange;
    bool m_ShotEndScheduled;
    bool m_ShotDamageFixture;
    bool m_ShotBurst;
    vector m_ShotMeasuredTarget;
    int m_ShotBone;

    override void OnInit()
    {
        super.OnInit();
        m_ShotDamageFixture = FileExist("$profile:PredateShotDamageFixture.txt");
        m_ShotBurst = FileExist("$profile:PredateShotBurst.txt");
        PredateShotProbeControl.Align.Insert(PredateAlignTarget);
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!m_PredateSession.m_ActorsActive) return;
        if (!m_ShotPlayer)
        {
            if (!m_PredateSession.m_HuntBot) return;
            array<Man> players = new array<Man>;
            GetGame().GetPlayers(players);
            foreach (Man man : players)
            {
                PlayerBase shooter = PlayerBase.Cast(man);
                if (!shooter || !shooter.GetIdentity() || !shooter.IsPlayerLoaded()) continue;
                m_ShotPlayer = shooter;
                m_ShotTarget = m_PredateSession.m_HuntBot;
                m_ShotTarget.m_PredateProbeShooter = true;
                m_ShotTarget.m_PredateProbeCeaseFire = true;
                m_ShotTarget.SetPosition(shooter.GetPosition() - shooter.GetDirection() * 30);
                m_ShotTarget.DisableSimulation(true);
                shooter.SetAllowDamage(false);
                if (m_ShotDamageFixture)
                    GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateRunDamageFixture, 1000, false);
                break;
            }
        }
        if (m_ShotTarget && m_ShotMeasuredTarget != vector.Zero && m_ShotTarget.IsAlive())
        {
            vector bone = m_ShotTarget.GetBonePositionWS(m_ShotBone);
            vector correction = m_ShotMeasuredTarget - bone;
            if (correction.LengthSq() > 0.01 && correction.LengthSq() < 16)
                m_ShotTarget.SetPosition(m_ShotTarget.GetPosition() + correction);
        }

        PredateLifeScore life = m_PredateSession.m_Score.Life(m_PredateSession.m_WorldState.PlayerLife);
        int expectedShots = 3;
        if (m_ShotDamageFixture) expectedShots = 2;
        bool fired = life && life.Shots >= expectedShots;
        if (m_ShotBurst) fired = life && m_ShotPlayer && m_ShotPlayer.m_ShotProbeServerShots >= 12;
        if (fired && !m_ShotEndScheduled)
        {
            m_ShotEndScheduled = true;
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateWriteShotProbe, 8500, false);
        }
    }

    void PredateRunDamageFixture()
    {
        Weapon_Base weapon = Weapon_Base.Cast(m_ShotPlayer.GetEntityInHands());
        if (!weapon) return;
        m_PredateSession.m_Score.OnFire(m_ShotPlayer, weapon, "Bullet_556x45");
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateSecondFixtureShot, 450, false, weapon);
    }

    void PredateSecondFixtureShot(Weapon_Base weapon)
    {
        m_PredateSession.m_Score.OnFire(m_ShotPlayer, weapon, "Bullet_556x45");
        m_ShotTarget.DisableSimulation(false);
        vector position = m_PredateSession.PredateGround(m_ShotPlayer.GetPosition() + "0 0 12");
        m_ShotTarget.SetPosition(position);
        vector impact = position + "0 1.2 0";
        m_ShotRange = vector.Distance(m_ShotPlayer.GetPosition(), impact);
        m_ShotTarget.ProcessDirectDamage(DT_FIRE_ARM, weapon, "Torso", "Bullet_556x45", impact, 0.2);
    }

    void PredateAlignTarget(PlayerBase shooter, vector origin, vector direction)
    {
        Print("[PredateShot] received alignment request, origin=" + origin + " direction=" + direction);
        if (!m_ShotTarget || shooter != m_ShotPlayer || vector.DistanceSq(origin, shooter.GetPosition()) > 9) return;
        m_ShotRange = 15;
        m_ShotTarget.DisableSimulation(false);
        m_ShotTarget.OverrideMovementSpeed(true, 0);
        m_ShotBone = m_ShotTarget.GetBoneIndexByName("Spine3");
        if (m_ShotBone < 0) m_ShotBone = m_ShotTarget.GetBoneIndexByName("neck");
        m_ShotMeasuredTarget = shooter.GetPosition() + "0.2 1.62 15";
        m_ShotTarget.SetPosition(m_PredateSession.PredateGround(m_ShotMeasuredTarget));
        Print("[PredateShot] aligned target=" + m_ShotMeasuredTarget + " muzzle=" + origin + " direction=" + direction);
    }

    void PredateWriteShotProbe()
    {
        PredateScoreService score = m_PredateSession.m_Score;
        PredateLifeScore life = score.Life(m_PredateSession.m_WorldState.PlayerLife);
        if (!life) return;
        score.Save();
        float range;
        foreach (PredatePursuitScore encounter : life.Encounters)
            if (encounter.ReachedContact) range = encounter.CreditedRange;
        FileHandle report = OpenFile("$profile:ShotScoreProbe.json", FileMode.WRITE);
        if (!report) return;
        FPrintln(report, "{");
        FPrintln(report, "\"Shots\":" + life.Shots + ",");
        FPrintln(report, "\"Hits\":" + life.Hits + ",");
        FPrintln(report, "\"Misses\":" + life.Misses + ",");
        FPrintln(report, "\"Pending\":" + life.PendingRounds + ",");
        FPrintln(report, "\"Unverified\":" + life.UnverifiedRounds + ",");
        FPrintln(report, "\"TargetHealth\":" + m_ShotTarget.GetHealth("", "Health") + ",");
        FPrintln(report, "\"ExpectedRange\":" + m_ShotRange + ",");
        FPrintln(report, "\"CreditedRange\":" + range + "}");
        CloseFile(report);
    }
}
