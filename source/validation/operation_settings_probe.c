// Disposable fixture for the real inventory and world clock.
class PredateOperationSettingsReport
{
    int Hour;
    int Minute;
    int Life;
    int MagazineRounds = -1;
    int BandageAmount = -1;
    int CargoRow = -1;
    int CargoColumn = -1;
    bool CargoFlipped;
    bool Respawns;
    float RespawnDelay;
    ref array<string> Items = new array<string>;
    ref array<string> NightVisionChain = new array<string>;
}

class PredateOperationSettingsMission : MissionServer
{
    int m_CheckedLife;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        PredateSession operation = m_PredateSession;
        if (!operation.m_ActorsActive || operation.m_WorldState.PlayerAwaitingRedeploy) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.IsAlive() || !player.GetIdentity() || player.GetDayZPlayerInventory().IsProcessing()) continue;
            int life = operation.m_WorldState.PlayerLife;
            if (m_CheckedLife != life)
            {
                Record(player, life);
                m_CheckedLife = life;
            }
            if (FileExist("$profile:SpendValidationAmmo.txt"))
            {
                DeleteFile("$profile:SpendValidationAmmo.txt");
                Weapon_Base weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
                if (!weapon || !weapon.GetMagazine(0)) return;
                weapon.GetMagazine(0).ServerSetAmmoCount(7);
                int year, month, day, hour, minute;
                GetGame().GetWorld().GetDate(year, month, day, hour, minute);
                GetGame().GetWorld().SetDate(year, month, day, 7, 26);
                player.Save();
                operation.PredateSave();
            }
            if (FileExist("$profile:EndPlayerLife.txt"))
            {
                DeleteFile("$profile:EndPlayerLife.txt");
                player.SetHealth("", "Health", 0);
            }
            return;
        }
    }

    void Record(PlayerBase player, int life)
    {
        PredateOperationSettingsReport report = new PredateOperationSettingsReport;
        report.Life = life;
        report.Respawns = m_PredateSession.m_HuntConfig.EnableOperatorRespawns;
        report.RespawnDelay = m_PredateSession.m_HuntConfig.OperatorRespawnDelay;
        int year, month, day, hour, minute;
        GetGame().GetWorld().GetDate(year, month, day, hour, minute);
        report.Hour = hour;
        report.Minute = minute;
        Weapon_Base weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
        if (weapon && weapon.GetMagazine(0)) report.MagazineRounds = weapon.GetMagazine(0).GetAmmoCount();
        array<EntityAI> inventory = new array<EntityAI>;
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
        {
            if (!entity || entity == player) continue;
            string kind = entity.GetType();
            report.Items.Insert(kind);
            if (kind == "BandageDressing")
            {
                InventoryLocation location = new InventoryLocation;
                if (entity.GetInventory().GetCurrentInventoryLocation(location))
                {
                    report.CargoRow = location.GetRow();
                    report.CargoColumn = location.GetCol();
                    report.CargoFlipped = location.GetFlip();
                }
                report.BandageAmount = ItemBase.Cast(entity).GetQuantity();
            }
        }
        EntityAI mount = player.GetInventory().FindAttachmentByName("Eyewear");
        if (mount)
        {
            report.NightVisionChain.Insert(mount.GetType());
            EntityAI goggles = mount.GetInventory().FindAttachmentByName("NVG");
            if (goggles)
            {
                report.NightVisionChain.Insert(goggles.GetType());
                EntityAI battery = goggles.GetInventory().FindAttachmentByName("BatteryD");
                if (battery) report.NightVisionChain.Insert(battery.GetType());
            }
        }
        string error;
        JsonFileLoader<PredateOperationSettingsReport>.SaveFile("$profile:OperationSettingsLife" + life + ".json", report, error);
    }
}
