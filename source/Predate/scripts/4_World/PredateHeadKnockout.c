modded class UnconsciousnessMdfr
{
    override bool ActivateCondition(PlayerBase player)
    {
        // Low shock alone does not trigger a knockout in PREDATE.
        return false;
    }
}

modded class PlayerBase
{
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (!GetGame().IsServer() || damageType != DT_FIRE_ARM || dmgZone != "Head" || !damageResult)
            return;
        if (!IsAlive() || GetHealth("", "Health") <= 0 || damageResult.GetDamage(dmgZone, "Shock") <= 0)
            return;
        if (GetHealth("", "Shock") > PlayerConstants.UNCONSCIOUS_THRESHOLD)
            return;

        ModifiersManager modifiers = GetModifiersManager();
        if (!modifiers) return;
        ModifierBase unconsciousness = modifiers.GetModifier(eModifiers.MDF_UNCONSCIOUSNESS);
        if (unconsciousness && !unconsciousness.IsActive() && !unconsciousness.IsLocked())
            modifiers.ActivateModifier(eModifiers.MDF_UNCONSCIOUSNESS);
    }
}
