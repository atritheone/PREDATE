modded class ItemBase
{
    protected bool PredateKeepEquipmentCondition()
    {
        // Keep native destruction for world structures and items whose use
        // deliberately spends their health, such as flares and grenades.
        if (IsExplosive() || IsFireplace() || IsInherited(BaseBuildingBase) || IsInherited(TentBase) || IsInherited(TrapBase) || IsInherited(HescoBox))
            return false;
        if (IsInherited(Ammo_40mm_Base) || IsInherited(Chemlight_ColorBase) || IsInherited(FireworksBase) || IsInherited(Heatpack))
            return false;
        return !IsInherited(Rag) && !IsInherited(Roadflare) && !IsInherited(ShippingContainerKeys_ColorBase);
    }

    override void EEInit()
    {
        super.EEInit();
        if (PredateKeepEquipmentCondition()) SetAllowDamage(false);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version)) return false;
        if (GetGame().IsServer() && PredateKeepEquipmentCondition())
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(PredateRestoreEquipmentCondition);
        return true;
    }

    protected void PredateRestoreEquipmentCondition()
    {
        // The engine also blocks scripted health changes while protected.
        // Restore worn gear once after storage has completely loaded it.
        SetAllowDamage(true);
        SetFullHealth();
        SetAllowDamage(false);
    }
}
