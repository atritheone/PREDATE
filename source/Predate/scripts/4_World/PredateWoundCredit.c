modded class eAIBase
{
    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        float previousHealth;
        if (GetGame().IsServer() && m_PredateOperatorRecord && damageResult && damageResult.GetDamage("", "Health") > 0)
            previousHealth = GetHealth("", "Health");
        bool accepted = super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (!accepted || previousHealth <= 0) return accepted;

        PlayerBase attacker = PredateOperatorDeaths.Attacker(source);
        if (attacker && attacker.GetIdentity())
            m_PredateOperatorRecord.PlayerHealthBeforeInjury = Math.Max(m_PredateOperatorRecord.PlayerHealthBeforeInjury, previousHealth);
        else if ((source && source != this) || ammo == "FallDamageHealth")
            m_PredateOperatorRecord.PlayerHealthBeforeInjury = 0;
        return accepted;
    }
}

modded class BleedingSourcesManagerServer
{
    override void ProcessHit(float damage, EntityAI source, int component, string zone, string ammo, vector modelPos)
    {
        eAIBase rival = eAIBase.Cast(m_Player);
        int previousWounds;
        if (rival && rival.m_PredateOperatorRecord)
        {
            previousWounds = rival.GetBleedingBits();
            rival.m_PredateOperatorRecord.PlayerWoundBits &= previousWounds;
        }
        super.ProcessHit(damage, source, component, zone, ammo, modelPos);
        if (!rival || !rival.m_PredateOperatorRecord) return;

        int newWounds = rival.GetBleedingBits() & ~previousWounds;
        if (!newWounds) return;
        PlayerBase attacker = PredateOperatorDeaths.Attacker(source);
        if (attacker && attacker.GetIdentity()) rival.m_PredateOperatorRecord.PlayerWoundBits |= newWounds;
    }

    override protected bool RemoveBleedingSource(int bit)
    {
        bool removed = super.RemoveBleedingSource(bit);
        eAIBase rival = eAIBase.Cast(m_Player);
        if (rival && rival.m_PredateOperatorRecord) rival.m_PredateOperatorRecord.PlayerWoundBits &= rival.GetBleedingBits();
        return removed;
    }
}

modded class BleedingCheckMdfr
{
    override void OnActivate(PlayerBase player)
    {
        eAIBase rival = eAIBase.Cast(player);
        if (rival && rival.m_PredateOperatorRecord)
        {
            rival.m_PredateBleedingDeath = true;
            rival.m_PredatePlayerBleedout = (rival.m_PredateOperatorRecord.PlayerWoundBits & rival.GetBleedingBits()) != 0;
        }
        super.OnActivate(player);
        if (rival)
        {
            rival.m_PredateBleedingDeath = false;
            rival.m_PredatePlayerBleedout = false;
        }
    }
}

modded class HealthRegenMdfr
{
    override void OnTick(PlayerBase player, float deltaT)
    {
        super.OnTick(player, deltaT);
        eAIBase rival = eAIBase.Cast(player);
        if (!rival || !rival.m_PredateOperatorRecord || rival.m_PredateOperatorRecord.PlayerHealthBeforeInjury <= 0) return;
        if (rival.GetHealth("", "Health") >= rival.m_PredateOperatorRecord.PlayerHealthBeforeInjury)
            rival.m_PredateOperatorRecord.PlayerHealthBeforeInjury = 0;
    }
}
