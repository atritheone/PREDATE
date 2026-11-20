class PredateDevelopmentTests
{
    // This file is inert without BOTH the server development flag and its isolated profile marker.
    static void RunScenario(PredateSession s)
    {
        if (!s.m_Development || !s.m_HuntReady || s.m_HuntError != "") return;
        string scenario;
        if (!GetGame().CommandlineGetParam("predateScenario", scenario)) return;
        if (scenario == "defeat")
        {
            if (s.m_HuntBot) s.m_HuntBot.SetHealth("", "Health", 0);
            Print("[Predate] SCENARIO defeat: killed development target; waiting for normal death persistence.");
        }
        else if (scenario == "deployment")
        {
            PredateOperatorService.PredateNewDeployment(s);
            Print("[Predate] SCENARIO deployment COMPLETE");
        }
        else if (scenario == "registry")
        {
            Object object = GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", "12000 0 12500", ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
            eAIBase unrelated = eAIBase.Cast(object);
            eAIGroup group = eAIGroup.CreateGroup(new eAIFactionRaiders);
            if (!unrelated) { Print("[Predate] SCENARIO registry FAIL: cannot create foreign actor"); return; }
            unrelated.SetGroup(group);
            group.m_BaseName = "UnrelatedTestActor";
            string error;
            eAIBase resolvedActor;
            eAIGroup resolvedGroup;
            bool passed = s.m_Operators.Resolve("raven-01", resolvedActor, resolvedGroup, error);
            passed = passed && resolvedActor == s.m_HuntBot;
            // Deliberately collide, verify ambiguity is refused, then remove the foreign actor.
            group.m_BaseName = s.m_HuntProgress.Operator.StorageKey;
            passed = passed && !s.m_Operators.Resolve("raven-01", resolvedActor, resolvedGroup, error);
            group.m_BaseName = "UnrelatedTestActor";
            // Leave this nonpersistent development actor alive until clean shutdown.
            // Deleting temporary player/AI probes during a running session is unsafe.
            if (passed) Print("[Predate] SCENARIO registry PASS: foreign actor ignored; duplicate identity refused.");
            else Print("[Predate] SCENARIO registry FAIL: " + error);
        }
    }

    static void PredateSelfTest(PredateSession s)
    {
        if (!s.m_Development) return;
        array<vector> refreshers = GetGame().GetMission().GetActiveRefresherLocations();
        if (refreshers) Print("[Predate] MISSION CALLBACK SELFTEST PASS: refresher list count=" + refreshers.Count());
        else Print("[Predate] MISSION CALLBACK SELFTEST FAIL: missing refresher list.");
        int version = PredateProtocol.VERSION;
        bool protocol = PredateProtocol.AcceptRequest(version, "raven-01", 1);
        protocol = protocol && !PredateProtocol.AcceptRequest(0, "raven-01", 1);
        protocol = protocol && !PredateProtocol.AcceptRequest(version + 1, "raven-01", 1);
        protocol = protocol && !PredateProtocol.AcceptRequest(version, "foreign-actor", 1);
        protocol = protocol && !PredateProtocol.AcceptRequest(version, "raven-01", 99);
        if (protocol) Print("[Predate] PROTOCOL SELFTEST PASS: unknown versions, actors and actions rejected.");
        else Print("[Predate] PROTOCOL SELFTEST FAIL.");
        bool bearings = PredateMath.Bearing("0 0 0", "0 0 100") == 0;
        bearings = bearings && PredateMath.Bearing("0 0 0", "100 0 0") == 90;
        bearings = bearings && PredateMath.Bearing("0 0 0", "0 0 -100") == 180;
        bearings = bearings && PredateMath.Bearing("0 0 0", "-100 0 0") == 270;
        int alive;
        array<eAIBase> uniqueAI = new array<eAIBase>;
        foreach (eAIGroup group : eAIGroup.GetAll())
        {
            if (!group) continue;
            for (int i = 0; i < group.Count(); i++)
            {
                eAIBase ai = eAIBase.Cast(group.GetMember(i));
                if (ai && ai.IsAlive() && uniqueAI.Find(ai) == -1)
                {
                    uniqueAI.Insert(ai);
                    alive++;
                }
            }
        }
        string result = "FAIL";
        bool expected = s.PredateTargetAvailable();
        if (s.m_HuntProgress.Defeated) expected = !s.PredateTargetAvailable();
        if (bearings && expected) result = "PASS";
        Print("[Predate] SELFTEST " + result + ": cardinal bearings=" + bearings + ", living AI=" + alive);
        Print("[Predate] Target availability self-check=" + s.PredateTargetAvailable() + ", defeated=" + s.m_HuntProgress.Defeated);
        s.PredateSurvivalSelfTest();
        bool firePolicy = PredateFirePolicy.AimAcceptable("0 0 350", "0 0 1");
        firePolicy = firePolicy && PredateFirePolicy.AimAcceptable("0.35 0 800", "0 0 1");
        firePolicy = firePolicy && !PredateFirePolicy.AimAcceptable("0.6 0 800", "0 0 1");
        firePolicy = firePolicy && !PredateFirePolicy.AimAcceptable("0 0 1001", "0 0 1");
        firePolicy = firePolicy && !PredateFirePolicy.AimAcceptable("0 0 100", "0 0 -1");
        firePolicy = firePolicy && PredateFirePolicy.ShotInterval(800) >= 3.2;
        if (firePolicy) Print("[Predate] FIRE POLICY SELFTEST PASS: aligned shot allowed; wide miss, behind-target aim and excessive range rejected.");
        else Print("[Predate] FIRE POLICY SELFTEST FAIL.");
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(s.PredateRunDevelopmentScenario, 1000, false);

    }

    static void PredateSurvivalSelfTest(PredateSession s)
    {
        if (!s.m_Development) return;
        PlayerBase actor = s.m_HuntBot;
        if (!actor || s.m_HuntProgress.Defeated)
        {
            Print("[Predate] SURVIVAL SELFTEST SKIP: defeated target is an expected state.");
            return;
        }
        bool passed = actor.GetStatEnergy().Get() == actor.GetStatEnergy().GetMax();
        passed = passed && actor.GetStatWater().Get() == actor.GetStatWater().GetMax();
        passed = passed && actor.GetStatHeatComfort().Get() == 0;
        passed = passed && actor.GetStatHeatBuffer().Get() == 0;
        passed = passed && actor.GetThermalBiasHandler().Get() == 0;
        passed = passed && actor.GetModifiersManager().GetModifierLock(eModifiers.MDF_HUNGER);
        passed = passed && actor.GetModifiersManager().GetModifierLock(eModifiers.MDF_THIRST);
        passed = passed && !actor.GetModifiersManager().GetModifierLock(eModifiers.MDF_BLEEDING);
        if (passed) Print("[Predate] SURVIVAL SELFTEST PASS: live actor has neutral survival stats; combat bleeding remains unlocked. No actor or stat changes made.");
        else Print("[Predate] SURVIVAL SELFTEST FAIL: live actor survival rules mismatch.");
    }
}
