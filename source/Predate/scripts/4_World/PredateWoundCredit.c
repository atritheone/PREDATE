modded class eAIBase
{
    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        float previousHealth;
        vector impact;
        bool tagged;
        if (GetGame().IsServer() && m_PredateOperatorRecord && damageResult)
        {
            if (damageResult.GetDamage("", "Health") > 0) previousHealth = GetHealth("", "Health");
            tagged = damageType == DT_FIRE_ARM && ammo.IndexOf(PredateProjectileRounds.PREFIX) == 0;
            if (tagged)
            {
                impact = PredateWorldImpact(modelPos);
                PredateProjectileRounds.ReportContact(ammo, this, impact, true);
            }
        }
        if (PredateServerDiagnostics.TraceShots && damageType == DT_FIRE_ARM && m_PredateOperatorRecord && PredateServerDiagnostics.TraceShotSource(source))
            Print("[PredateShot] damage " + GetGame().GetTime() + " " + ammo + " source=" + source + " target=" + GetPosition() + " raw=" + modelPos + " converted=" + ModelToWorld(modelPos));
        bool accepted = super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (!accepted || previousHealth <= 0) return accepted;

        PlayerBase attacker = PredateOperatorDeaths.Attacker(source);
        PredateProjectileRound round;
        if (damageType == DT_FIRE_ARM) round = PredateProjectileRounds.Find(ammo);
        if ((attacker && attacker.GetIdentity()) || (round && round.Number > 0))
        {
            m_PredateOperatorRecord.PlayerHealthBeforeInjury = Math.Max(m_PredateOperatorRecord.PlayerHealthBeforeInjury, previousHealth);
            if (!tagged) impact = PredateWorldImpact(modelPos);
            PredateScoreEvents.PlayerDamage.Invoke(attacker, this, source, damageType, ammo, impact);
        }
        else if ((source && source != this) || ammo == "FallDamageHealth")
            m_PredateOperatorRecord.PlayerHealthBeforeInjury = 0;
        return accepted;
    }

    vector PredateWorldImpact(vector modelPos)
    {
        // Native damage can report model coordinates. Expansion's
        // replacement damage can retain the world-space impact.
        vector impact = ModelToWorld(modelPos);
        vector target = GetPosition();
        if (vector.DistanceSq(modelPos, target) < vector.DistanceSq(impact, target)) return modelPos;
        return impact;
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
