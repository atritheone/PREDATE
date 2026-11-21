#ifdef SERVER
modded class ItemBase
{
    protected bool m_PredateStoredLightOn;
    protected bool m_PredateLightStowed;

    void PredateSetWeaponLightDrawn(bool drawn)
    {
        ComponentEnergyManager energy = GetCompEM();
        bool stowing = !drawn;
        if (!energy || m_PredateLightStowed == stowing) return;
        m_PredateLightStowed = stowing;
        if (!drawn)
        {
            m_PredateStoredLightOn = energy.IsSwitchedOn();
            if (m_PredateStoredLightOn) energy.SwitchOff();
        }
        else if (m_PredateStoredLightOn && energy.CanWork())
        {
            energy.SwitchOn();
        }
    }
}

modded class PlayerBase
{
    override void EEItemOutOfHands(EntityAI item)
    {
        PredateUpdateStowedWeaponLights(item, false);
        super.EEItemOutOfHands(item);
    }

    override void EEItemIntoHands(EntityAI item)
    {
        super.EEItemIntoHands(item);
        PredateUpdateStowedWeaponLights(item, true);
    }

    protected void PredateUpdateStowedWeaponLights(EntityAI item, bool drawn)
    {
        if (eAIBase.Cast(this) || !Weapon_Base.Cast(item)) return;
        ItemBase rifleLight = ItemBase.Cast(item.FindAttachmentBySlotName("weaponFlashlight"));
        ItemBase pistolLight = ItemBase.Cast(item.FindAttachmentBySlotName("pistolFlashlight"));
        if (rifleLight) rifleLight.PredateSetWeaponLightDrawn(drawn);
        if (pistolLight && pistolLight != rifleLight) pistolLight.PredateSetWeaponLightDrawn(drawn);
    }
}
#endif
