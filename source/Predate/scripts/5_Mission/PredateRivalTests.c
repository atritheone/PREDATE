class PredateRivalTests
{
    int m_Checks;
    int m_Failures;

    void Check(bool passed, string name)
    {
        m_Checks++;
        if (passed) return;
        m_Failures++;
        Print("[PredateRivalTest] FAIL " + name);
    }

    void Run(PredateSession session)
    {
        eAIBase rival = session.m_HuntBot;
        Weapon_Base rifle = Weapon_Base.Cast(rival.GetHumanInventory().GetEntityInHands());
        Check(rifle && rifle.GetAttachedSuppressor(), "saved rival retains its rifle and receives a compatible suppressor");
        EntityAI helmet = rival.FindAttachmentBySlotName("Headgear");
        EntityAI goggles;
        if (helmet) goggles = helmet.FindAttachmentBySlotName("NVG");
        EntityAI battery;
        if (goggles) battery = goggles.FindAttachmentBySlotName("BatteryD");
        Check(battery && battery.GetCompEM() && battery.GetCompEM().GetEnergy() > 0, "helmet goggles have a powered battery");

        // The saved rival may be tracking the player or reacting to the other
        // live fixtures. Exercise treatment with a separate actor out of contact.
        vector site = session.PredateGround("9000 0 8000");
        eAIBase patient = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", site, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        if (!patient)
        {
            Check(false, "isolated patient can be created");
            Report();
            return;
        }
        patient.SetGroup(eAIGroup.CreateGroup(new eAIFactionRaiders));
        patient.DisableSimulation(true);
        PredateOperatorService.PredateEquip(session, patient, "m4a1");
        patient.GetInventory().CreateInInventory("SalineBagIV");
        float now = 100;
        patient.SetHealth("", "Shock", patient.GetMaxHealth("", "Shock") * 0.5);
        TestTreatment(patient, "Epinephrine", eModifiers.MDF_EPINEPHRINE, now);
        patient.SetHealth("", "Blood", patient.GetMaxHealth("", "Blood") * 0.6);
        TestTreatment(patient, "SalineBagIV", eModifiers.MDF_SALINE, now + 30);
        patient.SetHealth("", "Health", patient.GetMaxHealth("", "Health") * 0.5);
        TestTreatment(patient, "Morphine", eModifiers.MDF_MORPHINE, now + 60);
        GetGame().ObjectDelete(patient);
        Report();
    }

    void Report()
    {
        Print("[PredateRivalTest] COMPLETE: checks=" + m_Checks + "; failures=" + m_Failures);
    }

    void TestTreatment(eAIBase rival, string item, int modifier, float now)
    {
        rival.PredateUpdateMedicine(now);
        ItemBase treatment = rival.m_PredateTreatment;
        bool matched = treatment && treatment.GetType() == item;
        if (!matched) Print("[PredateRivalTest] treatment diagnostics: safe=" + rival.PredateCanTreat() + "; need=" + rival.PredateNeedsMedicine(item, now) + "; unconscious=" + rival.IsUnconscious() + "; LOS=" + rival.eAI_HasLOS() + "; blood=" + rival.GetHealth01("", "Blood") + "; shock=" + rival.GetHealth01("", "Shock"));
        Check(matched, "selected " + item + " for the current injury");
        rival.PredateUpdateMedicine(now + 0.5);
        Check(!rival.GetModifiersManager().IsModifierActive(modifier), "treatment takes time");
        rival.PredateUpdateMedicine(now + 20);
        // Native OnApply queues activation for the modifier's next tick.
        ModifierBase effect = rival.GetModifiersManager().GetModifier(modifier);
        if (effect) effect.Tick(1);
        Check(!rival.m_PredateTreatment && rival.GetModifiersManager().IsModifierActive(modifier), "completed native " + item + " effect");
    }
}
