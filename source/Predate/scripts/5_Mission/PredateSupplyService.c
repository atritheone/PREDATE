class PredateSupplyService
{
    static bool IsEpinephrine(string name)
    {
        return name == "Epinephrine" || GetGame().IsKindOf(name, "Epinephrine");
    }

    static void RemoveEpinephrine(PlayerBase actor, PredateSupplyPlan plan)
    {
        array<EntityAI> inventory = {};
        actor.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
            if (entity && IsEpinephrine(entity.GetType())) GetGame().ObjectDelete(entity);
        if (!plan) return;
        for (int i = plan.Entries.Count() - 1; i >= 0; i--)
            if (IsEpinephrine(plan.Entries[i].ClassName)) plan.Entries.RemoveOrdered(i);
    }

    static bool IsNavigationGear(string name)
    {
        if (GetGame().IsKindOf(name, "Compass") || GetGame().IsKindOf(name, "OrienteeringCompass")) return true;
        return GetGame().IsKindOf(name, "ChernarusMap") || GetGame().IsKindOf(name, "ChernarusMap_Open");
    }

    static void UpdateRivalSupplies(eAIBase actor, PredateSupplyPlan plan, int maxBandages)
    {
        array<EntityAI> inventory = {};
        actor.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        int bandages;
        foreach (EntityAI entity : inventory)
        {
            if (!entity) continue;
            string name = entity.GetType();
            if (IsNavigationGear(name)) GetGame().ObjectDelete(entity);
            else if (name == "BandageDressing")
            {
                bandages++;
                if (bandages > maxBandages) GetGame().ObjectDelete(entity);
            }
        }
        for (int i = plan.Entries.Count() - 1; i >= 0; i--)
            if (IsNavigationGear(plan.Entries[i].ClassName)) plan.Entries.RemoveOrdered(i);
        PredateSupplyEntry bandageEntry = plan.Find("BandageDressing");
        if (bandageEntry && bandageEntry.Kind == PredateSupplyKind.QUANTITY)
        {
            float stackSize = GetGame().ConfigGetFloat("CfgVehicles BandageDressing varQuantityMax");
            bandageEntry.Units = Math.Min(bandageEntry.Units, stackSize * maxBandages);
        }
    }

    static int Kind(ItemBase item)
    {
        if (IsEpinephrine(item.GetType())) return PredateSupplyKind.NONE;
        Magazine magazine = Magazine.Cast(item);
        if (magazine)
        {
            if (magazine.IsAmmoPile()) return PredateSupplyKind.AMMUNITION;
            return PredateSupplyKind.MAGAZINE;
        }
        ComponentEnergyManager energy = item.GetCompEM();
        if (energy && energy.GetEnergyMax() > 0 && !energy.PredatePermanentBattery()) return PredateSupplyKind.FUELLED_ITEM;
        if (item.HasQuantity() && !energy) return PredateSupplyKind.QUANTITY;
        if (ExplosivesBase.Cast(item) || item.IsKindOf("Morphine") || item.IsKindOf("SalineBagIV"))
            return PredateSupplyKind.SINGLE;
        return PredateSupplyKind.NONE;
    }

    static int LiquidContentType(ItemBase item)
    {
        if (item.IsLiquidContainer()) return item.GetLiquidType();
        return 0;
    }

    static float Units(ItemBase item, int kind, bool capacity = false)
    {
        Magazine magazine = Magazine.Cast(item);
        if (kind == PredateSupplyKind.AMMUNITION && magazine)
        {
            if (capacity) return magazine.GetAmmoMax();
            return magazine.GetAmmoCount();
        }
        if (kind == PredateSupplyKind.QUANTITY)
        {
            if (capacity) return item.GetQuantityMax();
            return item.GetQuantity();
        }
        return 1;
    }

    static void AddConfigured(PredateSupplyPlan plan, string name, int objects, float specified = -1)
    {
        if (IsEpinephrine(name)) return;
        string magazines = "CfgMagazines " + name;
        if (GetGame().ConfigIsExisting(magazines))
        {
            if (GetGame().IsKindOf(name, "Ammunition_Base"))
            {
                float rounds = GetGame().ConfigGetInt(magazines + " count");
                if (specified >= 0) rounds = specified;
                plan.Add(name, PredateSupplyKind.AMMUNITION, rounds * objects);
            }
            else plan.Add(name, PredateSupplyKind.MAGAZINE, objects);
            return;
        }
        string config = "CfgVehicles " + name;
        float maximum = GetGame().ConfigGetFloat(config + " varQuantityMax");
        float minimum = GetGame().ConfigGetFloat(config + " varQuantityMin");
        bool powered = GetGame().ConfigIsExisting(config + " EnergyManager");
        bool liquidContainer = GetGame().ConfigGetInt(config + " liquidContainerType") != 0;
        if (powered && GetGame().ConfigGetFloat(config + " EnergyManager energyStorageMax") > 0 && !GetGame().IsKindOf(name, "Battery9V") && !GetGame().IsKindOf(name, "CarBattery") && !GetGame().IsKindOf(name, "TruckBattery"))
        {
            plan.Add(name, PredateSupplyKind.FUELLED_ITEM, objects);
        }
        else if (maximum > minimum && !powered && !liquidContainer)
        {
            float quantity = maximum;
            if (specified >= 0) quantity = specified;
            plan.Add(name, PredateSupplyKind.QUANTITY, quantity * objects);
        }
        else if (GetGame().IsKindOf(name, "ExplosivesBase") || name == "Morphine" || name == "SalineBagIV")
            plan.Add(name, PredateSupplyKind.SINGLE, objects);
    }

    static void IncludeStartingSupplies(PredateSession session, PlayerBase actor, string kitId, PredateSupplyPlan plan)
    {
        PredateSupplyPlan starting = new PredateSupplyPlan;
        if (!eAIBase.Cast(actor) && session.m_HuntConfig.HasCustomInventory)
        {
            foreach (PredateInventoryNode node : session.m_HuntConfig.PlayerInventory)
                AddConfigured(starting, node.ClassName, 1, node.Amount);
        }
        else
        {
            PredateStartingKit kit = session.m_Loadouts.Find(kitId);
            if (kit) AddConfigured(starting, kit.MagazineClass, kit.SpareMagazines + 1);
            PredateStartingSidearm sidearm = session.m_Loadouts.Sidearm;
            AddConfigured(starting, sidearm.IssuedMagazineClass(), sidearm.SpareMagazines + 1);
            AddConfigured(starting, "BandageDressing", session.m_Loadouts.StartingBandages);
            AddConfigured(starting, "Morphine", 1);
            if (eAIBase.Cast(actor)) AddConfigured(starting, "SalineBagIV", 1);
        }
        foreach (PredateSupplyEntry entry : starting.Entries)
        {
            PredateSupplyEntry recorded = plan.Find(entry.ClassName, entry.LiquidType);
            if (!recorded) plan.Add(entry.ClassName, entry.Kind, entry.Units, entry.LiquidType);
            else if (entry.Units > recorded.Units) recorded.Units = entry.Units;
        }
    }

    static void CaptureInventory(PlayerBase actor, PredateSupplyPlan plan, float start, float now)
    {
        plan.NextAt = PredateSupplyPlan.NextBoundary(start, now);
        array<EntityAI> inventory = {};
        actor.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
        {
            ItemBase item = ItemBase.Cast(entity);
            if (!item || item.IsRuined()) continue;
            int kind = Kind(item);
            if (kind == PredateSupplyKind.NONE) continue;
            int liquid = LiquidContentType(item);
            if (item.IsLiquidContainer() && (liquid == 0 || item.GetQuantity() <= 0)) continue;
            plan.Add(item.GetType(), kind, Units(item, kind, true), liquid);
        }
    }

    static void FillStack(ItemBase item, PredateSupplyEntry entry, float amount)
    {
        Magazine magazine = Magazine.Cast(item);
        if (magazine)
        {
            magazine.ServerSetAmmoCount(Math.Min(magazine.GetAmmoMax(), Math.Round(amount)));
        }
        else if (entry.Kind == PredateSupplyKind.QUANTITY)
        {
            if (entry.LiquidType != 0) item.SetLiquidType(entry.LiquidType);
            item.SetQuantity(Math.Min(item.GetQuantityMax(), amount));
        }
    }

    static bool Matches(ItemBase item, PredateSupplyEntry entry)
    {
        if (!item || item.IsRuined() || item.GetType() != entry.ClassName) return false;
        int liquid = LiquidContentType(item);
        return liquid == entry.LiquidType || (entry.LiquidType != 0 && item.GetQuantity() == 0);
    }

    static void Refresh(PlayerBase actor, PredateSupplyPlan plan)
    {
        array<EntityAI> inventory = {};
        actor.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
        {
            Magazine magazine = Magazine.Cast(entity);
            if (magazine && !magazine.IsAmmoPile() && !magazine.IsRuined())
                magazine.ServerSetAmmoCount(magazine.GetAmmoMax());
            ItemBase supply = ItemBase.Cast(entity);
            if (supply && !supply.IsRuined() && Kind(supply) == PredateSupplyKind.FUELLED_ITEM)
                supply.GetCompEM().SetEnergy0To1(1);
            Weapon_Base weapon = Weapon_Base.Cast(entity);
            if (weapon && !weapon.IsRuined() && weapon.HasInternalMagazine(-1))
            {
                int muzzle = weapon.GetCurrentMuzzle();
                string ammunition = weapon.GetChamberAmmoTypeName(muzzle);
                float damage;
                if (weapon.GetInternalMagazineCartridgeCount(muzzle) > 0)
                    weapon.GetInternalMagazineCartridgeInfo(muzzle, 0, damage, ammunition);
                weapon.FillInnerMagazine(ammunition, 0);
            }
        }
        foreach (PredateSupplyEntry entry : plan.Entries)
        {
            if (IsEpinephrine(entry.ClassName)) continue;
            float missing = entry.Units;
            foreach (EntityAI candidate : inventory)
            {
                ItemBase carried = ItemBase.Cast(candidate);
                if (Matches(carried, entry)) missing -= Units(carried, entry.Kind);
            }
            if (entry.Kind == PredateSupplyKind.AMMUNITION || entry.Kind == PredateSupplyKind.QUANTITY)
            {
                foreach (EntityAI stack : inventory)
                {
                    if (missing <= 0.01) break;
                    ItemBase partial = ItemBase.Cast(stack);
                    if (!Matches(partial, entry)) continue;
                    float before = Units(partial, entry.Kind);
                    float target = Math.Min(before + missing, Units(partial, entry.Kind, true));
                    if (target <= before) continue;
                    FillStack(partial, entry, target);
                    missing -= Units(partial, entry.Kind) - before;
                }
            }
            for (int attempt = 0; missing > 0.01 && attempt < PredateSupplyPlan.MAX_ENTRIES; attempt++)
            {
                ItemBase replacement = ItemBase.Cast(actor.GetInventory().CreateInInventory(entry.ClassName));
                if (!replacement) break;
                if (entry.Kind == PredateSupplyKind.MAGAZINE || entry.Kind == PredateSupplyKind.AMMUNITION || entry.Kind == PredateSupplyKind.QUANTITY)
                {
                    float amount = missing;
                    if (entry.Kind == PredateSupplyKind.MAGAZINE)
                    {
                        Magazine replacementMagazine = Magazine.Cast(replacement);
                        if (!replacementMagazine) break;
                        amount = replacementMagazine.GetAmmoMax();
                    }
                    FillStack(replacement, entry, amount);
                }
                else if (entry.Kind == PredateSupplyKind.FUELLED_ITEM && replacement.GetCompEM())
                    replacement.GetCompEM().SetEnergy0To1(1);
                float added = Units(replacement, entry.Kind);
                if (added <= 0) break;
                missing -= added;
            }
        }
    }

    static bool Advance(PredateSession session, PlayerBase actor, PredateSupplyPlan plan, string kitId, float start, float now)
    {
        if (!actor || !actor.IsAlive() || !plan) return false;
        if (plan.NextAt == 0)
        {
            CaptureInventory(actor, plan, start, now);
            IncludeStartingSupplies(session, actor, kitId, plan);
            return false;
        }
        if (now < plan.NextAt || actor.GetDayZPlayerInventory().IsProcessing()) return false;
        Refresh(actor, plan);
        plan.NextAt = PredateSupplyPlan.NextBoundary(plan.NextAt, now);
        return true;
    }

    static void Tick(PredateSession session, PlayerBase player)
    {
        PredateWorldState state = session.m_WorldState;
        float now = state.SimulationSeconds;
        bool refreshed = Advance(session, player, state.PlayerSupplies, session.m_HuntConfig.PlayerLoadout, state.PlayerLifeStartedAt, now);
        if (refreshed)
        {
            player.m_PredateSupplyNotice = true;
            player.Save();
        }
        PredateOperatorRecord record = session.m_HuntProgress.Operator;
        bool raven = Advance(session, session.m_HuntBot, record.Supplies, PredateOperatorService.RivalKit(session, record), now, now);
        bool peers;
        if (session.m_PeerRoster) peers = session.m_PeerRoster.RefreshSupplies(session, now);
        if (refreshed || raven || peers) session.PredateRequestCheckpoint();
    }
}
