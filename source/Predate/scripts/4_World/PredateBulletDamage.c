modded class eAIBase
{
    bool m_PredateAmplifyingBullet;
}

modded class eAIDamageHandler
{
    static const float PREDATE_BULLET_DAMAGE = 2.0;

    override void ProcessDamage(int damageType, EntityAI source, DayZPlayerImplement sourcePlayer, string dmgZone, string ammo, vector modelPos, float damageCoef = 1.0)
    {
        PlayerBase target = PlayerBase.Cast(m_Entity.GetHierarchyRoot());
        eAIBase aiShooter = eAIBase.Cast(sourcePlayer);
        eAIBase aiTarget = eAIBase.Cast(target);
        bool operatorShot = aiShooter && aiShooter.m_PredateCombatEnabled;
        bool shotAtOperator = !aiShooter && aiTarget && aiTarget.m_PredateCombatEnabled;
        if (damageType == DT_FIRE_ARM && sourcePlayer && target && (operatorShot || shotAtOperator))
        {
            // Process on a later tick so the native engine has released the original hit.
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateProcessBulletDamage, 35, false, damageType, source, sourcePlayer, dmgZone, ammo, modelPos, damageCoef);
            return;
        }
        super.ProcessDamage(damageType, source, sourcePlayer, dmgZone, ammo, modelPos, damageCoef);
    }

    void PredateProcessBulletDamage(int damageType, EntityAI source, DayZPlayerImplement sourcePlayer, string dmgZone, string ammo, vector modelPos, float damageCoef)
    {
        super.ProcessDamage(damageType, source, sourcePlayer, dmgZone, ammo, modelPos, damageCoef);
    }

    override bool OnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        if (damageType != DT_FIRE_ARM || m_ProcessDamage || !source)
            return super.OnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

        PlayerBase shooter = PlayerBase.Cast(source.GetHierarchyRootPlayer());
        PlayerBase target = PlayerBase.Cast(m_Entity.GetHierarchyRoot());
        if (!shooter || !target)
            return super.OnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

        eAIBase aiShooter = eAIBase.Cast(shooter);
        // Expansion can redirect a hit synchronously while validating the original shot.
        if (aiShooter && aiShooter.m_PredateCombatEnabled && !aiShooter.m_PredateAmplifyingBullet)
        {
            float outgoing = aiShooter.m_eAI_DamageMultiplier;
            aiShooter.m_PredateAmplifyingBullet = true;
            aiShooter.m_eAI_DamageMultiplier = outgoing * PREDATE_BULLET_DAMAGE;
            bool applied = super.OnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
            aiShooter.m_eAI_DamageMultiplier = outgoing;
            aiShooter.m_PredateAmplifyingBullet = false;
            return applied;
        }

        eAIBase aiTarget = eAIBase.Cast(target);
        if (!aiShooter && aiTarget && aiTarget.m_PredateCombatEnabled)
        {
            float incoming = aiTarget.m_eAI_DamageReceivedMultiplier;
            aiTarget.m_eAI_DamageReceivedMultiplier = incoming * PREDATE_BULLET_DAMAGE;
            bool accepted = super.OnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
            aiTarget.m_eAI_DamageReceivedMultiplier = incoming;
            return accepted;
        }

        return super.OnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }
}
