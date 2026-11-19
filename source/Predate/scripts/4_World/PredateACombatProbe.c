// Inert unless an authored disposable validation mission enables a probe.
// These fields are not network requests and are never consulted by the hunter.
class PredateProbeEntityInventory
{
    int DynamicCount;
    int ActiveCount;
    ref map<string, int> DynamicTypes = new map<string, int>;
    ref map<string, int> ActiveTypes = new map<string, int>;
}

modded class eAIShot
{
    bool m_PredateProbeMotionAtFire;
}

modded class PlayerBase
{
    bool m_PredateProbeShooter;
    EntityAI m_PredateProbeSpectator;
    bool m_PredateProbeTarget;
    bool m_PredateProbeProtect = true;
    bool m_PredateProbeCeaseFire;
    int m_PredateProbeHits;
    int m_PredateProbeMovingHits;
    int m_PredateProbePhysicalCallbacks;
    int m_PredateProbePhysicalPredictions;
    eAIShot m_PredateProbeLastHitRecord;
    ref array<float> m_PredateProbeShotTimes = new array<float>;
    ref array<float> m_PredateProbeFireErrors = new array<float>;
    ref array<string> m_PredateProbeShotPaths = new array<string>;
    ref array<float> m_PredateProbeObservedSpeeds = new array<float>;
    ref array<float> m_PredateProbeLeadLengths = new array<float>;

    override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        if (GetGame().IsServer() && m_PredateProbeTarget && damageType == DT_FIRE_ARM && source && m_eAI_DamageHandler && !m_eAI_DamageHandler.m_ProcessDamage)
        {
            eAIBase physicalShooter = eAIBase.Cast(source.GetHierarchyRootPlayer());
            if (physicalShooter && physicalShooter.m_PredateProbeShooter) m_PredateProbePhysicalCallbacks++;
        }
        // Let Expansion perform its native shot matching/redirection first.
        // A protected fixture must not suppress that reconciliation machinery.
        bool accepted = super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (!accepted) return false;
        if (GetGame().IsServer() && m_PredateProbeTarget && damageType == DT_FIRE_ARM && source)
        {
            Weapon_Base weapon = Weapon_Base.Cast(source);
            eAIBase shooter = eAIBase.Cast(source.GetHierarchyRootPlayer());
            if (!shooter) shooter = eAIBase.Cast(source);
            if (!weapon && shooter) weapon = Weapon_Base.Cast(shooter.GetHumanInventory().GetEntityInHands());
            if (weapon && shooter && shooter.m_PredateProbeShooter)
            {
                int now = GetGame().GetTime();
                eAIShot matching;
                foreach (eAIShot shot : shooter.m_eAI_FiredShots)
                {
                    if (!shot || shot.m_Weapon != weapon || shot.m_HitObjectRoot != this || shot.m_ProcessedTime <= 0) continue;
                    if (now - shot.m_ProcessedTime <= 10) { matching = shot; break; }
                }
                if (matching && m_PredateProbeLastHitRecord != matching)
                {
                    m_PredateProbeHits++;
                    if (matching.m_PredateProbeMotionAtFire) m_PredateProbeMovingHits++;
                    m_PredateProbeLastHitRecord = matching;
                }
                // A native firearm damage callback reached this body. Protect
                // only this fixture body so repeated trials can measure hits.
                return !m_PredateProbeProtect;
            }
        }
        return accepted;
    }
}

modded class Weapon_Base
{
    void PredateProbeNativeEntities(eAIBase ai)
    {
        if (!ai || !ai.m_PredateProbeShooter) return;
        PredateProbeEntityInventory inventory = new PredateProbeEntityInventory;
        int count = PhysicsWorld.GetNumDynamicBodies(ai);
        inventory.DynamicCount = count;
        for (int i = 0; i < count; i++)
        {
            IEntity dynamicEntity = PhysicsWorld.GetDynamicBody(ai, i);
            if (!dynamicEntity) continue;
            Object dynamicObject = Object.Cast(dynamicEntity);
            string dynamicType = dynamicEntity.Type().ToString();
            if (dynamicObject) dynamicType = dynamicObject.GetType();
            int dynamicCount;
            inventory.DynamicTypes.Find(dynamicType, dynamicCount);
            inventory.DynamicTypes.Set(dynamicType, dynamicCount + 1);
        }
        int active = GetNumActiveEntities(ai);
        inventory.ActiveCount = active;
        for (int j = 0; j < active && j < 50000; j++)
        {
            IEntity activeEntity = GetActiveEntity(ai, j);
            if (!activeEntity) continue;
            Object activeObject = Object.Cast(activeEntity);
            string activeType = activeEntity.Type().ToString();
            if (activeObject) activeType = activeObject.GetType();
            int activeCount;
            inventory.ActiveTypes.Find(activeType, activeCount);
            inventory.ActiveTypes.Set(activeType, activeCount + 1);
        }
        string error;
        JsonFileLoader<PredateProbeEntityInventory>.SaveFile("$profile:PredateProbeEntities.json", inventory, error);
    }

    override bool eAI_Fire(int muzzleIndex, eAIBase ai)
    {
        float miss;
        int pendingBefore;
        if (ai && ai.m_PredateProbeShooter)
        {
            vector origin = ai.GetBonePositionWS(ai.GetBoneIndexByName("neck"));
            vector toAim = ai.GetAimPosition() - origin;
            vector direction = ai.GetWeaponAimDirection().Normalized();
            float alignment = vector.Dot(toAim.Normalized(), direction);
            miss = toAim.Length() * Math.Sqrt(Math.Max(0, 1 - alignment * alignment));
            pendingBefore = ai.m_eAI_FiredShots.Count();
        }
        // Observation only: the native firearm action always completes.
        bool fired = super.eAI_Fire(muzzleIndex, ai);
        if (fired && ai && ai.m_PredateProbeShooter)
        {
            ai.m_PredateProbeFireErrors.Insert(miss);
            string path = "No object retained in native shot path";
            if (ai.m_eAI_FiredShots.Count() > pendingBefore)
            {
                eAIShot shot = ai.m_eAI_FiredShots[ai.m_eAI_FiredShots.Count() - 1];
                if (shot) path = shot.GetInfo();
            }
            ai.m_PredateProbeShotPaths.Insert(path);
            if (ai.m_PredateProbeFireErrors.Count() == 1)
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateProbeNativeEntities, 20, false, ai);
        }
        return fired;
    }

    override void EEFired(int muzzleType, int mode, string ammoType)
    {
        super.EEFired(muzzleType, mode, ammoType);
        if (!GetGame().IsServer()) return;
        PlayerBase owner = PlayerBase.Cast(GetHierarchyRootPlayer());
        if (!owner || !owner.m_PredateProbeShooter) return;
        float firedAt = GetGame().GetTickTime();
        owner.m_PredateProbeShotTimes.Insert(firedAt);
        while (owner.m_PredateProbeShotTimes.Count() > 256) owner.m_PredateProbeShotTimes.Remove(0);
    }
}
