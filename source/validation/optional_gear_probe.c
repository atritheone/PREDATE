class PredateOptionalGearReport
{
    bool Registered;
    int ItemCount;
    float Condition;
    bool Reopened;
    int PlayerMagazine;
}

class PredateOptionalGearMission : MissionServer
{
    bool m_Measured;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Measured || !m_PredateSession.m_ActorsActive) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity() || !player.IsAlive()) continue;
            m_Measured = true;
            Measure(player);
            return;
        }
    }

    void Measure(PlayerBase player)
    {
        string path = "$profile:OptionalGearInitial.json";
        string gear = "PredateValidationCompass";
        PredateOptionalGearReport report = new PredateOptionalGearReport;
        report.Registered = GetGame().ConfigIsExisting("CfgVehicles " + gear);
        report.Reopened = FileExist(path);
        if (report.Reopened) path = "$profile:OptionalGearReopened.json";
        if (!report.Reopened && report.Registered)
        {
            EntityAI created = player.GetInventory().CreateInInventory(gear);
            if (created) created.SetHealth01("", "Health", 0.42);
        }
        array<EntityAI> inventory = new array<EntityAI>;
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI item : inventory)
        {
            if (!item || item.GetType() != gear) continue;
            report.ItemCount++;
            report.Condition = item.GetHealth01("", "Health");
        }
        report.PlayerMagazine = PredateServerBridge.MagazineCount(player);
        string error;
        JsonFileLoader<PredateOptionalGearReport>.SaveFile(path, report, error);
    }
}
