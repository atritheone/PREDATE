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
        EntityAI headstrap = rival.FindAttachmentBySlotName("Eyewear");
        if (!goggles && headstrap) goggles = headstrap.FindAttachmentBySlotName("NVG");
        EntityAI battery;
        if (goggles) battery = goggles.FindAttachmentBySlotName("BatteryD");
        Check(battery && battery.GetCompEM() && battery.GetCompEM().GetEnergy() > 0, "mounted goggles have a powered battery");

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
        array<EntityAI> issued = {};
        patient.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, issued);
        bool issuedInjector;
        foreach (EntityAI item : issued)
            if (item && PredateSupplyService.IsEpinephrine(item.GetType())) issuedInjector = true;
        Check(!issuedInjector, "new Operators spawn without epinephrine");
        EntityAI legacyInjector = patient.GetInventory().CreateInInventory("Epinephrine");
        PredateSupplyPlan legacyPlan = new PredateSupplyPlan;
        legacyPlan.Add("Epinephrine", PredateSupplyKind.SINGLE, 1);
        Check(legacyInjector && legacyPlan.Find("Epinephrine"), "old issued kit can be reconstructed");
        PredateSupplyService.RemoveEpinephrine(patient, legacyPlan);
        Check(!legacyPlan.Find("Epinephrine"), "old refill allowance loses epinephrine");
        float now = 100;
        patient.SetHealth("", "Blood", patient.GetMaxHealth("", "Blood") * 0.6);
        TestTreatment(patient, "SalineBagIV", eModifiers.MDF_SALINE, now + 30);
        float maxHealth = patient.GetMaxHealth("", "Health");
        patient.SetHealth("", "Health", maxHealth * 0.72);
        Check(!patient.PredateNeedsMedicine("Morphine", now + 60), "keeps morphine for later when low blood limits recovery from a moderate wound");
        patient.SetHealth("", "Health", maxHealth * 0.5);
        Check(patient.PredateNeedsMedicine("Morphine", now + 60), "still considers morphine for a severe wound with low blood");
        patient.SetHealth("", "Blood", patient.GetMaxHealth("", "Blood"));
        patient.SetHealth("", "Health", maxHealth * 0.9);
        Check(!patient.PredateNeedsMedicine("Morphine", now + 60), "keeps morphine for later when a wound is minor");
        patient.SetHealth("", "Health", maxHealth * 0.72);
        TestTreatment(patient, "Morphine", eModifiers.MDF_MORPHINE, now + 60);
        Check(!patient.PredateNeedsMedicine("Morphine", now + 81), "does not inject again while morphine is active");
        ModifierBase morphine = patient.GetModifiersManager().GetModifier(eModifiers.MDF_MORPHINE);
        float healthBeforeTick = patient.GetHealth("", "Health");
        if (morphine) morphine.Tick(1.01);
        Check(patient.GetHealth("", "Health") > healthBeforeTick, "a morphine dose restores operator health");
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
