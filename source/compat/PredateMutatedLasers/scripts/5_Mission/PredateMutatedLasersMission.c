modded class MissionServer
{
    protected float m_PredatePEQUpgradeTime;

    override void MWL_SendUsagePingDeferred()
    {
        Print("[Predate] Suppressed the MutatedLasers Discord usage report.");
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!m_PredateSession || !m_PredateSession.m_ActorsActive || m_PredateSession.m_Paused) return;
        m_PredatePEQUpgradeTime += timeslice;
        if (m_PredatePEQUpgradeTime < 0.5) return;
        m_PredatePEQUpgradeTime = 0;

        array<Man> players = {};
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            ItemBase rifle = player.GetItemInHands();
            PredateWeaponModule oldModule = PredateWeaponModule.OnWeapon(rifle);
            if (oldModule) PredateUpgradeWeaponModule(rifle, oldModule);
        }

        bool dark = GetGame().GetWorld().IsNight();
        PredateUpdateOperatorLaser(m_PredateSession.m_HuntBot, dark);
        if (m_PredateSession.m_PeerRoster)
        {
            foreach (PredatePeerBinding binding : m_PredateSession.m_PeerRoster.m_Bindings)
                PredateUpdateOperatorLaser(binding.Actor, dark);
        }
    }

    protected void PredateUpdateOperatorLaser(eAIBase actor, bool dark)
    {
        if (!actor || !actor.IsAlive()) return;
        Weapon_Base rifle = Weapon_Base.Cast(actor.GetItemInHands());
        if (!rifle) return;
        int slot = InventorySlots.GetSlotIdFromString("weaponFlashlight");
        if (!rifle.GetInventory().HasAttachmentSlot(slot)) return;

        PredateWeaponModule oldModule = PredateWeaponModule.OnWeapon(rifle);
        if (oldModule) PredateUpgradeWeaponModule(rifle, oldModule);
        if (!rifle.FindAttachmentBySlotName("weaponFlashlight"))
            rifle.GetInventory().CreateAttachment("MM_ANPEQ15");
        MM_LaserSight_Base module = MM_LaserSight_Base.PredateOnWeapon(rifle);
        if (!module) return;
        PredateOperatorService.InstallBattery(module);

        EntityAI mount = actor.FindAttachmentBySlotName("Eyewear");
        NVGoggles goggles;
        if (mount) goggles = NVGoggles.Cast(mount.FindAttachmentBySlotName("NVG"));
        if (!goggles)
        {
            mount = actor.FindAttachmentBySlotName("Headgear");
            if (mount) goggles = NVGoggles.Cast(mount.FindAttachmentBySlotName("NVG"));
        }
        if (goggles && !actor.IsUnconscious())
        {
            if (dark && !goggles.m_IsLowered) goggles.Expansion_TryTurningOn();
            else if (!dark && goggles.m_IsLowered) goggles.Expansion_TryTurningOff();
        }
        bool gogglesActive = goggles && goggles.m_IsLowered && goggles.IsWorking();
        module.PredateUseOperatorModule(!actor.IsUnconscious() && !actor.IsRestrained(), gogglesActive, dark);
    }

    protected void PredateUpgradeWeaponModule(ItemBase rifle, PredateWeaponModule oldModule)
    {
        float health = oldModule.GetHealth01("", "");
        ItemBase battery = ItemBase.Cast(oldModule.FindAttachmentBySlotName("BatteryD"));
        bool hasBattery = battery != null;
        float charge, batteryHealth;
        if (battery)
        {
            charge = battery.GetQuantity();
            batteryHealth = battery.GetHealth01("", "");
        }
        GetGame().ObjectDelete(oldModule);
        EntityAI replacement = rifle.GetInventory().CreateAttachment("MM_ANPEQ15");
        if (!replacement)
        {
            Print("[Predate] Could not fit the Workshop PEQ to " + rifle.GetType() + "; restoring its original module.");
            replacement = rifle.GetInventory().CreateAttachment("PredateWeaponModule");
        }
        if (!replacement) return;
        replacement.SetHealth01("", "", health);
        if (hasBattery)
        {
            ItemBase installedBattery = ItemBase.Cast(replacement.GetInventory().CreateAttachment("Battery9V"));
            if (installedBattery)
            {
                installedBattery.SetHealth01("", "", batteryHealth);
                installedBattery.SetQuantity(charge);
            }
        }
    }
}

#ifndef SERVER
modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();
        PredateClientAgent.BeforeExit.Insert(PredateCleanWorkshopLasers);
    }

    override void OnMissionFinish()
    {
        PredateClientAgent.BeforeExit.Remove(PredateCleanWorkshopLasers);
        PredateCleanWorkshopLasers();
        super.OnMissionFinish();
    }

    void PredateCleanWorkshopLasers()
    {
        MM_LaserSight_Base.PredateReleasePEQVisuals();
    }
}
#endif
