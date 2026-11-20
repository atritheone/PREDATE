// Disposable fresh-character fixture. Never installed in a normal operation.
class PredateKitProbeReport
{
    int Schema = 1;
    string Kit;
    string FirearmClass;
    string MagazineClass;
    int Magazines;
    int FullSpareMagazines;
    int InstalledAttachments;
    int TotalAmmo;
    int AttachedAmmo;
    bool ReadyToFire;
    bool Reopened;
    int Checks;
    int Failures;

    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateKitTest] FAIL: " + label);
    }
}

class PredateKitProbeMission : MissionServer
{
    bool m_KitChecked;
    float m_KitReadyAt;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_PredateSession.m_HuntBot) m_PredateSession.m_HuntBot.DisableSimulation(true);
        if (m_KitChecked || !m_PredateSession.m_ActorsActive) return;
        if (m_KitReadyAt == 0) { m_KitReadyAt = GetGame().GetTickTime()+1; return; }
        if (GetGame().GetTickTime() < m_KitReadyAt) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            if (player.GetDayZPlayerInventory().IsProcessing()) continue;
            CheckKit(player);
            m_KitChecked = true;
            return;
        }
    }

    void CheckKit(PlayerBase player)
    {
        PredateKitProbeReport report = new PredateKitProbeReport;
        string kitId = m_PredateSession.m_HuntConfig.PlayerLoadout;
        report.Kit = kitId;
        PredateStartingKit kit = m_PredateSession.m_Loadouts.Find(kitId);
        bool reopened = FileExist("$profile:KitInitial.json");
        report.Reopened = reopened;
        report.Check(kit != null, "selected native starting-kit definition exists");
        if (kit)
        {
            PredateLoadoutCatalog catalog = m_PredateSession.m_Loadouts;
            PredateOutfit outfit = catalog.FindOutfit(m_PredateSession.m_HuntConfig.PlayerOutfit);
            CheckClothing(report, player, outfit);
            string gogglesClass = kit.NightVisionClass;
            if (gogglesClass == "") gogglesClass = "NVGoggles";
            string mountClass = catalog.NightVisionMount;
            if (outfit.Eyewear != "") mountClass = outfit.Eyewear;
            if (outfit.NightVision != "") gogglesClass = outfit.NightVision;
            if (outfit.WorkshopNightVision != "" && PredateLoadoutCatalog.ClassExists(outfit.WorkshopNightVision))
                gogglesClass = outfit.WorkshopNightVision;
            CheckHeadstrap(report, player, mountClass, gogglesClass);
            if (outfit.Vest != "")
            {
                EntityAI vest = player.FindAttachmentBySlotName("Vest");
                EntityAI pouches;
                if (vest) pouches = vest.FindAttachmentBySlotName("VestPouch");
                EntityAI gloves = player.FindAttachmentBySlotName("Gloves");
                report.Check(vest && vest.GetType() == outfit.Vest, "issued plate carrier matches the clothing preset");
                report.Check(pouches && pouches.GetType() == outfit.Pouches, "issued pouches match the clothing preset");
                report.Check(gloves && gloves.GetType() == outfit.Gloves, "issued gloves match the clothing preset");
            }
            CheckRival(report, m_PredateSession.m_HuntBot, m_PredateSession.m_HuntProgress.Operator);
            PredatePeerRoster roster = m_PredateSession.m_PeerRoster;
            if (roster)
            {
                foreach (PredatePeerBinding peer : roster.m_Bindings)
                    CheckRival(report, peer.Actor, peer.State.Operator);
            }
        }
        Weapon_Base weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
        report.Check(weapon != null, "actual fresh/restored character holds its selected firearm");
        if (kit && weapon)
        {
            if (kit.NightVisionClass != "")
            {
                report.Check(weapon.GetAttachedSuppressor() != null, "the issued rifle has its suppressor");
                ItemOptics optic = weapon.GetAttachedOptics();
                report.Check(optic && optic.IsNVOptic() && optic.FindAttachmentBySlotName("BatteryD"), "the night optic has its own battery");
                PredateWeaponModule module = PredateWeaponModule.OnWeapon(weapon);
                report.Check(module && module.FindAttachmentBySlotName("BatteryD"), "the weapon module has a battery");
                if (module)
                {
                    if (!reopened)
                    {
                        for (int mode = 0; mode < 3; mode++) module.CycleMode(player);
                    }
                    report.Check(!module.IsLightSource(), "the IR mode is selected and does not register as visible light");
                    module.GetCompEM().SwitchOn();
                }
            }
            string firearmClass = weapon.GetType();
            report.FirearmClass = firearmClass;
            report.Check(firearmClass == kit.FirearmClass, "native firearm matches the selected kit");
            Magazine attached = weapon.GetMagazine(weapon.GetCurrentMuzzle());
            report.Check(attached != null, "selected native firearm has an attached magazine");
            if (attached)
            {
                string magazineClass = attached.GetType();
                int attachedAmmo = attached.GetAmmoCount();
                bool ready = weapon.CanFire(weapon.GetCurrentMuzzle());
                report.MagazineClass = magazineClass;
                report.AttachedAmmo = attachedAmmo;
                report.ReadyToFire = ready;
                report.Check(magazineClass == kit.MagazineClass, "attached magazine is compatible with this kit");
                report.Check(ready, "native firearm is mechanically ready to fire");
                array<EntityAI> inventory = new array<EntityAI>;
                player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
                array<string> directAttachments = new array<string>;
                int bandages;
                foreach (EntityAI item : inventory)
                {
                    if (!item) continue;
                    if (item.GetType() == "BandageDressing") bandages++;
                    if (item.GetHierarchyParent() == weapon) directAttachments.Insert(item.GetType());
                    Magazine magazine = Magazine.Cast(item);
                    if (!magazine || magazine.GetType() != kit.MagazineClass) continue;
                    report.Magazines++;
                    if (magazine != attached && magazine.GetAmmoCount() == magazine.GetAmmoMax()) report.FullSpareMagazines++;
                }
                foreach (string expected : kit.Attachments)
                    if (directAttachments.Find(expected) >= 0) report.InstalledAttachments++;
                report.Check(report.InstalledAttachments == kit.Attachments.Count(), "all selected stock/handguard/optic attachments are installed");
                report.Check(report.FullSpareMagazines > 0 && report.FullSpareMagazines <= kit.SpareMagazines && report.Magazines == report.FullSpareMagazines + 1, "the character carries full spare magazines within its cargo capacity");
                report.Check(bandages == 4, "the defined medical supplies accompany either rifle");
                PredateCombatRuntimeTests meter = new PredateCombatRuntimeTests;
                if (!reopened)
                {
                    int chambered;
                    for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
                        if (!weapon.IsChamberEmpty(muzzle) && !weapon.IsChamberFiredOut(muzzle)) chambered++;
                    report.Check(attachedAmmo + chambered >= attached.GetAmmoMax(), "new operation starts with a fully loaded firearm");
                    attached.ServerSetAmmoCount(7);
                    report.AttachedAmmo = 7;
                }
                int total = meter.CountAmmo(player);
                report.TotalAmmo = total;
                if (reopened)
                {
                    PredateKitProbeReport initial = new PredateKitProbeReport;
                    string loadError;
                    bool loaded = JsonFileLoader<PredateKitProbeReport>.LoadFile("$profile:KitInitial.json", initial, loadError);
                    report.Check(loaded && initial.Kit == kitId && initial.TotalAmmo == total && attachedAmmo == 7, "continuation preserves kit and spent ammunition without resupply");
                }
            }
        }
        player.Save();
        m_PredateSession.PredateSave();
        string filename = "$profile:KitInitial.json";
        if (reopened) filename = "$profile:KitReopened.json";
        string error;
        JsonFileLoader<PredateKitProbeReport>.SaveFile(filename, report, error);
        Print("[PredateKitTest] COMPLETE: kit=" + kitId + "; reopened=" + reopened + "; checks=" + report.Checks + "; failures=" + report.Failures);
    }

    void CheckClothing(PredateKitProbeReport report, PlayerBase player, PredateOutfit outfit)
    {
        EntityAI body = player.FindAttachmentBySlotName("Body");
        EntityAI legs = player.FindAttachmentBySlotName("Legs");
        EntityAI feet = player.FindAttachmentBySlotName("Feet");
        report.Check(body && body.GetType() == outfit.Body, "casual top is worn");
        report.Check(legs && legs.GetType() == outfit.Legs, "casual trousers are worn");
        report.Check(feet && feet.GetType() == outfit.Feet, "casual footwear is worn");
        report.Check(!player.FindAttachmentBySlotName("Headgear"), "no helmet is issued with the head strap");
        report.Check(!player.FindAttachmentBySlotName("Back"), "fresh default has no backpack");
        EntityAI vest = player.FindAttachmentBySlotName("Vest");
        report.Check(vest && vest.FindAttachmentBySlotName("VestPouch"), "vest pouches provide cargo");
    }

    void CheckHeadstrap(PredateKitProbeReport report, PlayerBase player, string mountClass, string gogglesClass)
    {
        EntityAI strap = player.FindAttachmentBySlotName("Eyewear");
        report.Check(strap && strap.GetType() == mountClass, "NVG head strap is worn");
        EntityAI goggles;
        if (strap) goggles = strap.FindAttachmentBySlotName("NVG");
        report.Check(goggles && goggles.GetType() == gogglesClass && goggles.FindAttachmentBySlotName("BatteryD"), "head strap carries the selected powered goggles");
    }

    void CheckRival(PredateKitProbeReport report, eAIBase rival, PredateOperatorRecord record)
    {
        report.Check(rival && record, "AI rival spawned");
        if (!rival || !record) return;
        int slot = Math.Max(0, PredateProtocol.s_OperatorIds.Find(record.Id));
        CheckClothing(report, rival, PredateOperatorService.RivalOutfit(m_PredateSession, record, slot));
        bool tan = slot % 2 == 1;
        string mount = "PredateOperatorHeadstrap_Black";
        string goggles = "PredateOperatorNVG_Black";
        string vest = "PlateCarrierVest_Black";
        string pouches = "PlateCarrierPouches_Black";
        string gloves = "TacticalGloves_Black";
        string mask = "BalaclavaMask_Black";
        if (tan)
        {
            mount = "PredateOperatorHeadstrap_Tan";
            goggles = "PredateOperatorNVG_Tan";
            vest = "PlateCarrierVest_Desert";
            pouches = "PlateCarrierPouches_Desert";
            gloves = "TacticalGloves_Beige";
            mask = "BalaclavaMask_Beige";
        }
        if (GetGame().ConfigIsExisting("CfgVehicles FOG_GPNVG_Tan"))
        {
            goggles = "FOG_GPNVG_Black";
            if (tan) goggles = "FOG_GPNVG_Tan";
        }
        CheckHeadstrap(report, rival, mount, goggles);
        EntityAI equippedVest = rival.FindAttachmentBySlotName("Vest");
        EntityAI equippedPouches;
        if (equippedVest) equippedPouches = equippedVest.FindAttachmentBySlotName("VestPouch");
        EntityAI equippedGloves = rival.FindAttachmentBySlotName("Gloves");
        EntityAI equippedMask = rival.FindAttachmentBySlotName("Mask");
        report.Check(equippedVest && equippedVest.GetType() == vest && equippedPouches && equippedPouches.GetType() == pouches, "rival chest rig matches its accent");
        report.Check(equippedGloves && equippedGloves.GetType() == gloves, "rival gloves match its accent");
        report.Check(equippedMask && equippedMask.GetType() == mask, "rival eye-opening balaclava matches its accent");
    }
}
