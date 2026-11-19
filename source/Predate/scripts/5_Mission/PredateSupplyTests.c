class PredateSupplyTests
{
    int m_Checks;
    int m_Failures;

    void Check(bool passed, string name)
    {
        m_Checks++;
        if (passed) return;
        m_Failures++;
        Print("[PredateSupplyTest] FAIL " + name);
    }

    void Run(PredateSession session)
    {
        array<Man> players = {};
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            TestAmmo(session, player, 20);
            TestAmmo(session, session.m_HuntBot, 470);
            TestConsumables(session, player);
            TestBattery(player);
            TestCanister(player);
            break;
        }
        Print("[PredateSupplyTest] COMPLETE: checks=" + m_Checks + "; failures=" + m_Failures);
    }

    void TestAmmo(PredateSession session, PlayerBase actor, float lifeStart)
    {
        Weapon_Base rifle = Weapon_Base.Cast(actor.GetHumanInventory().GetEntityInHands());
        Magazine magazine;
        if (rifle) magazine = rifle.GetMagazine(rifle.GetCurrentMuzzle());
        Check(magazine != null, "native magazine available");
        if (!magazine) return;
        PredateSupplyPlan plan = new PredateSupplyPlan;
        PredateSupplyService.CaptureInventory(actor, plan, lifeStart, lifeStart);
        float due = lifeStart + 1800;
        magazine.ServerSetAmmoCount(2);
        bool early = PredateSupplyService.Advance(session, actor, plan, "m4a1", lifeStart, due - 0.1);
        Check(!early && magazine.GetAmmoCount() == 2 && plan.NextAt == due, "no early refill on this character's timer");
        bool first = PredateSupplyService.Advance(session, actor, plan, "m4a1", lifeStart, due);
        Check(first && magazine.GetAmmoCount() == magazine.GetAmmoMax() && plan.NextAt == due + 1800, "refill at the first 30-minute boundary");
        magazine.ServerSetAmmoCount(1);
        bool second = PredateSupplyService.Advance(session, actor, plan, "m4a1", lifeStart, due + 1800);
        Check(second && magazine.GetAmmoCount() == magazine.GetAmmoMax() && plan.NextAt == due + 3600, "refill at the second 30-minute boundary");
        string error;
        bool saved = JsonFileLoader<PredateSupplyPlan>.SaveFile("$profile:SupplyValidation.json", plan, error);
        PredateSupplyPlan restored = new PredateSupplyPlan;
        bool loaded = JsonFileLoader<PredateSupplyPlan>.LoadFile("$profile:SupplyValidation.json", restored, error);
        Check(saved && loaded && restored.NextAt == plan.NextAt && restored.Valid(due + 1800), "native save retains the active timer and allowance");
    }

    void TestConsumables(PredateSession session, PlayerBase player)
    {
        ItemBase dressing = ItemBase.Cast(player.GetInventory().CreateInInventory("BandageDressing"));
        Check(dressing != null, "test dressing fits in inventory");
        if (!dressing) return;
        PredateSupplyPlan plan = new PredateSupplyPlan;
        PredateSupplyService.CaptureInventory(player, plan, 0, 0);
        plan.Add("M67Grenade", PredateSupplyKind.SINGLE, 1);
        plan.Add("SalineBagIV", PredateSupplyKind.SINGLE, 1);
        dressing.SetQuantity(1);
        PredateSupplyService.Refresh(player, plan);
        Check(dressing.GetQuantity() == dressing.GetQuantityMax(), "used consumable quantity refills");
        array<EntityAI> inventory = {};
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        bool found;
        bool medical;
        foreach (EntityAI item : inventory)
        {
            if (item.GetType() == "M67Grenade") found = true;
            if (item.GetType() == "SalineBagIV") medical = true;
        }
        Check(found, "expended single-use item is replaced");
        Check(medical, "an expended medical item is replaced");
    }

    void TestBattery(PlayerBase player)
    {
        ItemBase battery = ItemBase.Cast(player.GetInventory().CreateInInventory("Battery9V"));
        Check(battery && battery.GetCompEM(), "native battery available");
        if (!battery || !battery.GetCompEM()) return;
        ComponentEnergyManager energy = battery.GetCompEM();
        energy.SetEnergy(0);
        Check(energy.GetEnergy() == energy.GetEnergyMax() && energy.GetEnergy() > 0, "depleted batteries start with power");
        bool consumed = energy.ConsumeEnergy(0.5);
        Check(consumed && energy.GetEnergy() == energy.GetEnergyMax(), "gear can draw power without draining the battery");
    }

    void TestCanister(PlayerBase player)
    {
        ItemBase canister = ItemBase.Cast(player.GetInventory().CreateInInventory("SmallGasCanister"));
        Check(canister && PredateSupplyService.Kind(canister) == PredateSupplyKind.FUELLED_ITEM, "a portable fuel canister is a consumable");
        if (!canister || !canister.GetCompEM()) return;
        ComponentEnergyManager energy = canister.GetCompEM();
        energy.SetEnergy0To1(0.25);
        bool spent = energy.GetEnergy() < energy.GetEnergyMax();
        PredateSupplyPlan plan = new PredateSupplyPlan;
        plan.Add("SmallGasCanister", PredateSupplyKind.FUELLED_ITEM, 1);
        PredateSupplyService.Refresh(player, plan);
        Check(spent && energy.GetEnergy() == energy.GetEnergyMax(), "portable fuel tops up during its scheduled restock");
    }
}
