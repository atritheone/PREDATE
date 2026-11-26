class PredateScoreEvents
{
    static ref ScriptInvoker Fired = new ScriptInvoker;
    static ref ScriptInvoker PlayerDamage = new ScriptInvoker;
    static ref ScriptInvoker Damage = new ScriptInvoker;
    static ref ScriptInvoker Defeated = new ScriptInvoker;
}

modded class PlayerBase
{
    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        bool accepted = super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (!accepted || !GetGame().IsServer() || !GetIdentity() || !damageResult) return accepted;
        eAIBase attacker = eAIBase.Cast(PredateOperatorDeaths.Attacker(source));
        float health = damageResult.GetDamage("", "Health");
        if (health > 0 && attacker && attacker.m_PredateOperatorRecord)
            PredateScoreEvents.Damage.Invoke(this, attacker, source, damageType, ammo, health);
        return accepted;
    }
}
