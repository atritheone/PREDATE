// A stationary opposed-fire measurement, injected only into a disposable mission.
class PredateDuelReport
{
    int Schema = 1;
    int Checks;
    int Failures;
    float Range;
    float Elapsed;
    float CommittedShotDrainSeconds;
    string Outcome;
    int PrimaryShots;
    int RivalShots;
    int PrimaryInitialAmmo;
    int RivalInitialAmmo;
    int PrimaryRemainingAmmo;
    int RivalRemainingAmmo;
    int PrimaryBodyHits;
    int RivalBodyHits;
    int PrimaryPhysicalCallbacks;
    int RivalPhysicalCallbacks;
    float PrimaryHealth;
    float RivalHealth;
    float PrimaryBlood;
    float RivalBlood;
    float PrimaryFirstIncapacitation = -1;
    float RivalFirstIncapacitation = -1;
    int SpectatorSelections;
    bool SeparatePrivateMemories;
    int RejectedPreparationLanes;
    vector PrimaryStart;
    vector RivalStart;
    float ActualInitialSeparation;
    float ObserverDistancePrimary;
    float ObserverDistanceRival;
    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateDuelTest] FAIL: " + label);
    }
}

class PredateDuelMission : MissionServer
{
    eAIBase m_DuelPrimary;
    eAIBase m_DuelRival;
    PlayerBase m_DuelSpectator;
    ref PredateObservationMemory m_DuelRivalMemory = new PredateObservationMemory;
    ref array<ref PredateObservation> m_DuelRivalPending = new array<ref PredateObservation>;
    ref PredateDuelReport m_DuelReport = new PredateDuelReport;
    ref PredateCombatRuntimeTests m_DuelMeter = new PredateCombatRuntimeTests;
    int m_DuelStage;
    float m_DuelAt;
    float m_DuelStarted;
    float m_DuelNext;
    float m_DuelDiagnostic;
    float m_DuelCombatEnded;
    int m_DuelLaneSkip;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        float now = GetGame().GetTickTime();
        if (m_DuelStage == 4 || now < m_DuelNext) return;
        m_DuelNext = now + 0.1;
        if (m_DuelStage == 0)
        {
            if (m_PredateSession.m_ActorsActive && m_PredateSession.m_HuntBot) CreateDuel(now);
            return;
        }
        if (!m_DuelPrimary || !m_DuelRival)
        {
            m_DuelReport.Check(false,"both measurement entities remain available through the committed-shot drain");
            m_DuelStage = 4;
            WriteDuelReport();
            return;
        }
        m_DuelRival.PredateAdvanceInformation(m_PredateSession.m_WorldState.SimulationSeconds);
        if (m_DuelPrimary.IsAlive() && m_DuelPrimary.GetIsSimulationDisabled()) m_DuelPrimary.DisableSimulation(false);
        if (m_DuelRival.IsAlive() && m_DuelRival.GetIsSimulationDisabled()) m_DuelRival.DisableSimulation(false);
        if (m_DuelStage == 5 && now >= m_DuelAt) { FinishDuel(now); return; }
        if (m_DuelStage == 1 && now >= m_DuelAt) { PrepareDuel(now); return; }
        if (m_DuelStage == 2 && now >= m_DuelAt)
        {
            bool firstVisible = m_DuelPrimary.EnforceLOS(1);
            bool secondVisible = m_DuelRival.EnforceLOS(1);
            if (firstVisible && secondVisible) { BeginDuel(now); return; }
            if (now >= m_DuelDiagnostic)
            {
                m_DuelDiagnostic = now + 5;
                m_DuelPrimary.m_PredateKnowledgeEnabled = false;
                m_DuelPrimary.m_eAI_TargetChanged = true;
                bool firstNative = m_DuelPrimary.EnforceLOS(1);
                string firstRay = DescribeDuelSight(m_DuelPrimary);
                m_DuelPrimary.m_PredateKnowledgeEnabled = true;
                m_DuelPrimary.m_eAI_TargetChanged = true;
                m_DuelPrimary.m_PredateNextPerceptionAt = 0;
                m_DuelRival.m_PredateKnowledgeEnabled = false;
                m_DuelRival.m_eAI_TargetChanged = true;
                bool secondNative = m_DuelRival.EnforceLOS(1);
                string secondRay = DescribeDuelSight(m_DuelRival);
                m_DuelRival.m_PredateKnowledgeEnabled = true;
                m_DuelRival.m_eAI_TargetChanged = true;
                m_DuelRival.m_PredateNextPerceptionAt = 0;
                Print("[PredateDuelTest] SIGHT: primary=" + firstVisible + "; rival=" + secondVisible + "; primaryNative=" + firstNative + "; rivalNative=" + secondNative + "; primary=" + m_DuelPrimary.PredateCombatDiagnostic() + "; rival=" + m_DuelRival.PredateCombatDiagnostic());
                Print("[PredateDuelTest] RAYS: primary=" + firstRay + "; rival=" + secondRay);
            }
            if (now - m_DuelAt > 15)
            {
                if (m_DuelLaneSkip < 4)
                {
                    m_DuelReport.RejectedPreparationLanes++;
                    m_DuelLaneSkip++;
                    m_DuelStage = 1;
                    m_DuelAt = now + 1;
                    Print("[PredateDuelTest] PREPARE: native sight rejected the authored lane; trying candidate " + m_DuelLaneSkip);
                    return;
                }
                m_DuelReport.Check(false,"both native sensors acquire the prepared opponent");
                FinishDuel(now);
            }
            return;
        }
        if (m_DuelStage != 3) return;
        float elapsed = now - m_DuelStarted;
        if (m_DuelPrimary.IsUnconscious() && m_DuelReport.PrimaryFirstIncapacitation < 0) m_DuelReport.PrimaryFirstIncapacitation = elapsed;
        if (m_DuelRival.IsUnconscious() && m_DuelReport.RivalFirstIncapacitation < 0) m_DuelReport.RivalFirstIncapacitation = elapsed;
        if (m_DuelPrimary.GetTarget() && m_DuelPrimary.GetTarget().GetEntity() == m_DuelSpectator) m_DuelReport.SpectatorSelections++;
        if (m_DuelRival.GetTarget() && m_DuelRival.GetTarget().GetEntity() == m_DuelSpectator) m_DuelReport.SpectatorSelections++;
        if (now >= m_DuelDiagnostic)
        {
            m_DuelDiagnostic = now + 5;
            Print("[PredateDuelTest] STATUS: elapsed=" + elapsed + "; primaryShots=" + m_DuelPrimary.m_PredateProbeShotTimes.Count() + "; rivalShots=" + m_DuelRival.m_PredateProbeShotTimes.Count() + "; primaryHealth=" + m_DuelPrimary.GetHealth("", "Health") + "; rivalHealth=" + m_DuelRival.GetHealth("", "Health"));
        }
        if (!m_DuelPrimary.IsAlive() || !m_DuelRival.IsAlive() || elapsed >= 180) FinishDuel(now);
    }

    string DescribeDuelSight(eAIBase actor)
    {
        eAITarget contact = actor.GetTarget();
        if (!contact || !contact.GetEntity()) return "no entity target";
        vector offset = contact.GetAimOffset();
        vector start = actor.GetBonePositionWS(actor.GetBoneIndexByName("Head"));
        vector endpoint = contact.GetPosition(true)+offset;
        vector direction = vector.Direction(start,endpoint).Normalized();
        set<Object> results = new set<Object>;
        vector point, normal;
        int component;
        bool hit = DayZPhysics.RaycastRV(start,endpoint+direction*0.5,point,normal,component,results,null,actor,false,false,ObjIntersectView,0.05);
        string objects;
        foreach (Object object : results) if (object) objects += object.GetType()+",";
        return "target=" + contact.GetEntity().GetType() + "; offset=" + offset + "; end=" + endpoint + "; hit=" + hit + "; objects=" + objects + "; point=" + point;
    }

    void CreateDuel(float now)
    {
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase person = PlayerBase.Cast(man);
            if (person && person.GetIdentity()) { m_DuelSpectator = person; break; }
        }
        if (!m_DuelSpectator) return;
        m_DuelPrimary = m_PredateSession.m_HuntBot;
        vector initial = m_DuelPrimary.GetPosition();
        Object created = GetGame().CreateObjectEx("eAI_SurvivorM_Mirek",initial + "8 0 8",ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
        m_DuelRival = eAIBase.Cast(created);
        m_DuelReport.Check(m_DuelRival && m_DuelRival != m_DuelPrimary,"a distinct second native combatant exists");
        if (!m_DuelRival) { FinishDuel(now); return; }
        m_DuelRival.SetGroup(eAIGroup.CreateGroup(new eAIFactionMercenaries));
        m_PredateSession.PredateEquip(m_DuelRival);
        m_DuelRivalMemory.OwnerId = "duel-rival";
        m_DuelRival.PredateBindInformation(true,"duel-rival",m_DuelRivalMemory,m_DuelRivalPending,m_PredateSession.m_WorldState.SimulationSeconds);
        m_DuelRival.m_PredateCombatEnabled = true;
        m_DuelReport.Check(m_DuelPrimary.PlayerIsEnemy(m_DuelSpectator) && m_DuelRival.PlayerIsEnemy(m_DuelSpectator),"ordinary human targeting remains active before the explicit spectator fixture is enabled");
        m_DuelSpectator.SetAllowDamage(false);
        ConfigureDuelActor(m_DuelPrimary);
        ConfigureDuelActor(m_DuelRival);
        m_DuelMeter.Target = m_DuelRival;
        m_DuelStage = 1;
        m_DuelAt = now + 5;
    }

    void ConfigureDuelActor(eAIBase actor)
    {
        actor.m_PredateProbeShooter = true;
        actor.m_PredateProbeTarget = true;
        actor.m_PredateProbeProtect = false;
        actor.m_PredateProbeCeaseFire = true;
        actor.m_PredateProbeSpectator = m_DuelSpectator;
        actor.eAI_SetAccuracy(m_PredateSession.m_HuntConfig.BotAccuracyMin,m_PredateSession.m_HuntConfig.BotAccuracyMax);
        actor.eAI_SetUnlimitedReload(0);
        actor.eAI_SetLootingBehavior(0);
        actor.eAI_SetThreatDistanceLimit(600);
        actor.SetMovementSpeedLimits(0,0);
        actor.GetGroup().ClearWaypoints();
        actor.GetGroup().SetWaypointBehaviour(eAIWaypointBehavior.HALT);
        actor.DisableSimulation(false);
    }

    void PrepareDuel(float now)
    {
        vector initial = m_PredateSession.m_HuntConfig.BotStart;
        float range = m_PredateSession.m_HuntConfig.PatrolSize;
        bool found;
        int passedLanes;
        vector start, goal;
        if (range == 300 && m_DuelLaneSkip == 0)
        {
            // This corridor already passed the pinned native 300 m firearm
            // tests. Preload it with a nearby client before judging the poses.
            start = m_DuelMeter.BenchGround("4850 0 2200");
            goal = m_DuelMeter.BenchGround("5139.777832 0 2122.354248");
            m_DuelMeter.Target = m_DuelRival;
            bool firstClear = m_DuelMeter.ClearLane(start+"0 1.4 0",goal+"0 1.4 0",m_DuelPrimary);
            m_DuelMeter.Target = m_DuelPrimary;
            bool secondClear = m_DuelMeter.ClearLane(goal+"0 1.4 0",start+"0 1.4 0",m_DuelRival);
            found = firstClear && secondClear;
        }
        for (int east = -2; east <= 2 && !found; east++)
        {
            for (int north = -2; north <= 2 && !found; north++)
            {
                start = m_DuelMeter.BenchGround(initial + Vector(east*150,0,north*150));
                if (!PredateRoutePlanner.Land(start)) continue;
                for (int heading = 0; heading < 360; heading += 15)
                {
                    goal = m_DuelMeter.BenchGround(start + PredateInformationMath.BearingDirection(heading)*range);
                    if (!PredateRoutePlanner.Land(goal) || Math.AbsFloat(goal[1]-start[1]) > range*0.05) continue;
                    bool clear = true;
                    for (int level = 0; level < 3 && clear; level++)
                    {
                        vector height = Vector(0,1.0+level*0.4,0);
                        m_DuelMeter.Target = m_DuelRival;
                        clear = m_DuelMeter.ClearLane(start+height,goal+height,m_DuelPrimary);
                        m_DuelMeter.Target = m_DuelPrimary;
                        clear = clear && m_DuelMeter.ClearLane(goal+height,start+height,m_DuelRival);
                    }
                    if (clear)
                    {
                        passedLanes++;
                        if (passedLanes <= m_DuelLaneSkip) continue;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (m_DuelLaneSkip == 0 || !found) m_DuelReport.Check(found,"authored geometry provides reciprocal clear firing lanes");
        if (!found) { FinishDuel(now); return; }
        m_DuelPrimary.SetPosition(start);
        m_DuelRival.SetPosition(goal);
        vector forward = goal-start;
        forward[1] = 0;
        forward.Normalize();
        m_DuelSpectator.SetPosition(m_DuelMeter.BenchGround((start+goal)*0.5 + Vector(forward[2],0,-forward[0])*80));
        SetDuelFacing(m_DuelPrimary,m_DuelRival);
        SetDuelFacing(m_DuelRival,m_DuelPrimary);
        bool separate = m_DuelPrimary.m_PredateMemory != m_DuelRival.m_PredateMemory && m_DuelPrimary.m_PredatePendingSounds != m_DuelRival.m_PredatePendingSounds && m_DuelPrimary.m_PredateMemory.OwnerId != m_DuelRival.m_PredateMemory.OwnerId;
        m_DuelReport.SeparatePrivateMemories = separate;
        if (m_DuelLaneSkip == 0)
        {
            bool moving, friendly, targeted;
            m_DuelReport.Check(!m_DuelPrimary.PlayerIsEnemy(m_DuelSpectator,false,moving,friendly,targeted) && friendly,"the primary treats the authored observer as neutral");
            m_DuelReport.Check(!m_DuelRival.PlayerIsEnemy(m_DuelSpectator,false,moving,friendly,targeted) && friendly,"the rival treats the authored observer as neutral");
            m_DuelReport.Check(m_DuelPrimary.PlayerIsEnemy(m_DuelRival) && m_DuelRival.PlayerIsEnemy(m_DuelPrimary),"the two independent groups are mutual enemies");
            m_DuelReport.Check(separate,"both combat adapters have distinct owned memories and pending sounds");
        }
        m_DuelReport.Range = range;
        m_DuelStage = 2;
        m_DuelAt = now + 2;
    }

    void SetDuelFacing(eAIBase actor, eAIBase opponent)
    {
        actor.SetAllowDamage(true);
        actor.SetHealth("","Health",100);
        actor.SetHealth("","Blood",5000);
        actor.SetHealth("","Shock",100);
        vector direction = opponent.GetPosition()-actor.GetPosition();
        direction[1] = 0;
        direction.Normalize();
        actor.SetDirection(direction);
        actor.LookAtPosition(opponent.GetPosition()+"0 1.3 0");
        actor.AimAtPosition(opponent.GetPosition()+"0 1.3 0");
        actor.OverrideStance(DayZPlayerConstants.STANCEIDX_ERECT,true);
        actor.OverrideTurnTarget(true,PredateMath.Bearing(actor.GetPosition(),opponent.GetPosition()));
        bool created;
        eAITarget contact = opponent.GetTargetInformation().AddAI(actor,240000,true,created);
        contact.SetInitial(0.8,opponent.GetPosition());
        contact.m_LOS = false;
        array<ref eAITarget> contacts = actor.GetTargets();
        contacts.Clear();
        contacts.Insert(contact);
        actor.m_eAI_TargetChanged = true;
        actor.m_PredateNextPerceptionAt = 0;
    }

    void BeginDuel(float now)
    {
        m_DuelReport.Check(true,"both combatants acquire native visual contact before release");
        vector firstPosition = m_DuelPrimary.GetPosition();
        vector secondPosition = m_DuelRival.GetPosition();
        vector observerPosition = m_DuelSpectator.GetPosition();
        float separation = PredateHunterBrain.Distance(firstPosition,secondPosition);
        float firstObserverDistance = PredateHunterBrain.Distance(observerPosition,firstPosition);
        float secondObserverDistance = PredateHunterBrain.Distance(observerPosition,secondPosition);
        m_DuelReport.PrimaryStart = firstPosition;
        m_DuelReport.RivalStart = secondPosition;
        m_DuelReport.ActualInitialSeparation = separation;
        m_DuelReport.ObserverDistancePrimary = firstObserverDistance;
        m_DuelReport.ObserverDistanceRival = secondObserverDistance;
        m_DuelReport.Check(Math.AbsFloat(separation-m_DuelReport.Range) < 1,"actual native positions match the authored engagement range");
        m_DuelReport.Check(Math.AbsFloat(firstObserverDistance-secondObserverDistance) < 1,"the real streaming observer is equally distant from both combatants");
        int firstAmmo = m_DuelMeter.CountAmmo(m_DuelPrimary);
        int secondAmmo = m_DuelMeter.CountAmmo(m_DuelRival);
        m_DuelReport.PrimaryInitialAmmo = firstAmmo;
        m_DuelReport.RivalInitialAmmo = secondAmmo;
        m_DuelReport.Check(firstAmmo > 0 && firstAmmo == secondAmmo,"fresh military kits have equal finite starting ammunition");
        m_DuelPrimary.m_PredateSeenSince = -1;
        m_DuelRival.m_PredateSeenSince = -1;
        m_DuelPrimary.m_PredateVisualTarget = null;
        m_DuelRival.m_PredateVisualTarget = null;
        m_DuelPrimary.m_PredateProbeCeaseFire = false;
        m_DuelRival.m_PredateProbeCeaseFire = false;
        m_DuelStarted = now;
        m_DuelStage = 3;
        Print("[PredateDuelTest] START: range=" + m_DuelReport.Range + "; equalAmmo=" + firstAmmo + "; primary=" + firstPosition + "; rival=" + secondPosition);
    }

    void FinishDuel(float now)
    {
        int previousStage = m_DuelStage;
        if (previousStage == 3)
        {
            // Finish committed native weapon actions and in-flight projectiles
            // before classifying a victory or counting the final ammunition.
            m_DuelPrimary.m_PredateProbeCeaseFire = true;
            m_DuelRival.m_PredateProbeCeaseFire = true;
            m_DuelCombatEnded = now;
            m_DuelAt = now + 2;
            m_DuelStage = 5;
            return;
        }
        m_DuelStage = 4;
        if (m_DuelPrimary) m_DuelPrimary.m_PredateProbeCeaseFire = true;
        if (m_DuelRival) m_DuelRival.m_PredateProbeCeaseFire = true;
        if (previousStage == 5)
        {
            m_DuelReport.Elapsed = m_DuelCombatEnded-m_DuelStarted;
            m_DuelReport.CommittedShotDrainSeconds = now-m_DuelCombatEnded;
            int firstShots = m_DuelPrimary.m_PredateProbeShotTimes.Count();
            int secondShots = m_DuelRival.m_PredateProbeShotTimes.Count();
            int firstAmmo = m_DuelMeter.CountAmmo(m_DuelPrimary);
            int secondAmmo = m_DuelMeter.CountAmmo(m_DuelRival);
            float firstHealth = m_DuelPrimary.GetHealth("","Health");
            float secondHealth = m_DuelRival.GetHealth("","Health");
            float firstBlood = m_DuelPrimary.GetHealth("","Blood");
            float secondBlood = m_DuelRival.GetHealth("","Blood");
            m_DuelReport.PrimaryShots = firstShots;
            m_DuelReport.RivalShots = secondShots;
            m_DuelReport.PrimaryRemainingAmmo = firstAmmo;
            m_DuelReport.RivalRemainingAmmo = secondAmmo;
            m_DuelReport.PrimaryHealth = firstHealth;
            m_DuelReport.RivalHealth = secondHealth;
            m_DuelReport.PrimaryBlood = firstBlood;
            m_DuelReport.RivalBlood = secondBlood;
            m_DuelReport.PrimaryBodyHits = m_DuelPrimary.m_PredateProbeHits;
            m_DuelReport.RivalBodyHits = m_DuelRival.m_PredateProbeHits;
            m_DuelReport.PrimaryPhysicalCallbacks = m_DuelPrimary.m_PredateProbePhysicalCallbacks;
            m_DuelReport.RivalPhysicalCallbacks = m_DuelRival.m_PredateProbePhysicalCallbacks;
            if (!m_DuelPrimary.IsAlive() && !m_DuelRival.IsAlive()) m_DuelReport.Outcome = "mutual death";
            else if (!m_DuelPrimary.IsAlive()) m_DuelReport.Outcome = "rival victory";
            else if (!m_DuelRival.IsAlive()) m_DuelReport.Outcome = "primary victory";
            else m_DuelReport.Outcome = "timeout";
            m_DuelReport.Check(firstShots > 0 && secondShots > 0,"both native combatants fire real rounds");
            m_DuelReport.Check(firstHealth < 99 || secondHealth < 99,"native projectiles cause actual combat injuries");
            m_DuelReport.Check(m_DuelReport.PrimaryPhysicalCallbacks + m_DuelReport.RivalPhysicalCallbacks > 0,"a native projectile collision reaches a combatant");
            m_DuelReport.Check(m_DuelReport.Outcome != "timeout","the opposed engagement reaches a native death outcome");
            m_DuelReport.Check(firstAmmo <= m_DuelReport.PrimaryInitialAmmo-firstShots,"the primary consumes finite owned ammunition");
            m_DuelReport.Check(secondAmmo <= m_DuelReport.RivalInitialAmmo-secondShots,"the rival consumes finite owned ammunition");
            m_DuelReport.Check(m_DuelReport.SpectatorSelections == 0,"neither combatant targets the observer");
            bool primaryIdentityRetained = m_PredateSession.m_HuntBot == m_DuelPrimary;
            if (!m_DuelPrimary.IsAlive())
                primaryIdentityRetained = m_PredateSession.m_HuntProgress.Operator.LifeState == "defeated" && (!m_PredateSession.m_HuntBot || m_PredateSession.m_HuntBot == m_DuelPrimary);
            m_DuelReport.Check(primaryIdentityRetained,"the primary retains its original live binding or explicit defeated identity without replacement");
        }
        WriteDuelReport();
    }

    void WriteDuelReport()
    {
        string error;
        JsonFileLoader<PredateDuelReport>.SaveFile("$profile:DuelResult.json",m_DuelReport,error);
        Print("[PredateDuelTest] COMPLETE: checks=" + m_DuelReport.Checks + "; failures=" + m_DuelReport.Failures + "; outcome=" + m_DuelReport.Outcome);
    }
}
