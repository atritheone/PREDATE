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
        Weapon_Base weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
        report.Check(weapon != null, "actual fresh/restored character holds its selected firearm");
        if (kit && weapon)
        {
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
                report.Check(report.Magazines == kit.SpareMagazines+1 && report.FullSpareMagazines == kit.SpareMagazines, "the character owns the defined finite spare magazines");
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
}
