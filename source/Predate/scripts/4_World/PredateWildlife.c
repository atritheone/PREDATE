modded class AnimalBase
{
    override void EEInit()
    {
        super.EEInit();
        // Also cover animals restored from a save or spawned by another addon.
        if (GetGame().IsServer()) Delete();
    }
}

modded class PlayerBase
{
    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        if (GetGame().IsServer() && DayZAnimal.Cast(source)) return false;
        return super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }
}
