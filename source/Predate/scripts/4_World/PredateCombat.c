modded class eAIShot
{
    bool m_PredatePhysicalMotionOnly;
    void eAIShot(Weapon_Base weapon, int muzzleIndex, vector origin, vector dir, Object hitObject, vector hitPosition, int component)
    {
        eAIBase owner = eAIBase.Cast(weapon.GetHierarchyRootPlayer());
        if (!owner || !owner.m_PredateProbeShooter || !owner.GetTarget()) return;
        eAITargetInformationState observed = owner.GetTarget();
        m_PredateProbeMotionAtFire = observed.m_PredateObservedVelocity.LengthSq() > 1;
    }
}

modded class eAIDamageHandler
{
    override bool CheckCandidate(notnull eAIShot candidate, eAIBase ai, vector modelPos, vector dir, float travelTimeRemaining, string dmgZone)
    {
        // A motion hypothesis is not a hitscan result. It may match a real
        // engine collision with that body; it must never redirect another
        // object's hit or manufacture damage from a predicted future position.
        if (candidate.m_PredatePhysicalMotionOnly) return false;
        return super.CheckCandidate(candidate, ai, modelPos, dir, travelTimeRemaining, dmgZone);
    }
}

class PredateFirePolicy
{
    static const float DELIBERATE_RANGE = 20;
    static const float LONG_RANGE = 450;
    static const float LONG_RANGE_SETTLE_TIME = 1.1;

    static bool AimAcceptable(vector toTarget, vector aimDirection)
    {
        float distance = toTarget.Length();
        if (distance < 1 || distance > PredateConfig.DEFAULT_THREAT_DISTANCE) return false;
        vector aim = aimDirection.Normalized();
        float alongAim = vector.Dot(toTarget, aim);
        if (alongAim <= 0) return false;
        // Project in metres to keep sub-metre differences at extended range.
        vector miss = toTarget - aim * alongAim;
        float tolerance = 0.30 + distance * 0.0005;
        if (distance > LONG_RANGE) tolerance = 0.5;
        return miss.LengthSq() <= tolerance * tolerance;
    }

    static float ShotInterval(float distance)
    {
        if (distance > LONG_RANGE) return Math.RandomFloat(3.2, 4.5);
        if (distance >= 200) return Math.RandomFloat(1.4, 2.2);
        return Math.RandomFloat(0.7, 1.1);
    }
}

class PredateShotLane
{
    static bool Clear(eAIBase observer, Weapon_Base weapon, vector point, EntityAI target, bool requireTarget = false)
    {
        vector origin = observer.GetBonePositionWS(observer.GetBoneIndexByName("neck"));
        vector direction = point - origin;
        float distance = direction.Length();
        if (distance < 1) return false;
        direction.Normalize();
        Object hitObject;
        vector hitPosition, hitNormal;
        int component;
        bool hit = weapon.Hitscan(origin, direction, distance + 0.3, observer, weapon, hitObject, hitPosition, hitNormal, component);
        if (!hit) return !requireTarget; // The target has yet to reach a predicted lead point.
        if (hitObject == target) return true;
        EntityAI hitEntity = EntityAI.Cast(hitObject);
        if (hitEntity && hitEntity.GetHierarchyRoot() == target) return true;
        return !requireTarget && vector.Distance(origin, hitPosition) >= distance;
    }
}

class PredateMotionAim
{
    static bool Flight(Weapon_Base weapon, vector origin, vector point, out float flight, out float distance, out float damageCoef = 0)
    {
        int muzzle = weapon.GetCurrentMuzzle();
        if (weapon.IsChamberEmpty(muzzle) || weapon.IsChamberFiredOut(muzzle)) return false;
        string ammo;
        float cartridgeHealth;
        weapon.GetCartridgeInfo(muzzle, cartridgeHealth, ammo);
        if (ammo == "") return false;
        float friction, speedCoef, initialSpeed;
        damageCoef = weapon.eAI_CalculateProjectileDamageCoefAtPosition(origin, ammo, point, 1.0, friction, distance, speedCoef, initialSpeed);
        if (initialSpeed <= 0) return false;
        flight = weapon.eAI_CalculateProjectileTravelTime(friction, distance, initialSpeed);
        return PredateInformationMath.Finite(flight) && flight >= 0 && flight <= 2.4;
    }

    static vector Lead(eAIBase observer, Weapon_Base weapon, eAITargetInformationState state, vector offset)
    {
        float age = observer.m_PredateSimulationTime - observer.m_PredateLastVisualAt;
        if (observer.m_PredateLastVisualAt < 0 || age < 0 || age > 0.25 || weapon.ShootsExplosiveAmmo()) return vector.Zero;
        vector velocity = state.m_PredateObservedVelocity;
        if (!PredateInformationMath.FiniteVector(velocity) || velocity.LengthSq() < 0.04 || velocity.LengthSq() > 100) return vector.Zero;
        vector origin = observer.GetBonePositionWS(observer.GetBoneIndexByName("neck"));
        vector point = state.m_LastKnownPosition + offset;
        vector lead;
        for (int iteration = 0; iteration < 2; iteration++)
        {
            float flight, distance;
            if (!Flight(weapon, origin, point + lead, flight, distance)) return vector.Zero;
            lead = velocity * (flight + age);
        }
        return lead;
    }
}

modded class eAIBase
{
    float m_PredateNextShotTime;
    float m_PredateAimSince;
    float m_PredateLastAimCheck;
    EntityAI m_PredateAimTarget;
    int m_PredateStringShots;
    int m_PredateFiredShots;
    int m_PredateFireAttempts;
    string m_PredateFireBlock = "Native FSM has not requested firing";
    EntityAI m_PredateReconsiderTarget;
    float m_PredateBlockedSince = -1;
    float m_PredateReconsiderAt;
    float m_PredateAimPauseUntil;
    float m_PredateNextAimPause;
    EntityAI m_PredateAimPauseTarget;
    vector m_PredateAimPausePosition;

    override void OverrideTargetPosition(eAITarget target, bool isFinal = true)
    {
        if (m_PredateCombatEnabled && target && target.m_LOS && m_PredateAimPauseTarget && target.GetEntity() == m_PredateAimPauseTarget && m_PredateSimulationTime < m_PredateAimPauseUntil && !m_eAI_IsInDangerByArea && !eAI_IsSideStepping() && GetCurrentCommandID() == DayZPlayerConstants.COMMANDID_MOVE)
        {
            OverrideTargetPosition(m_PredateAimPausePosition, true, 0.25, false);
            return;
        }
        super.OverrideTargetPosition(target, isFinal);
    }

    void PredateRequestAimPause(eAITarget target, float distance)
    {
        float now = m_PredateSimulationTime;
        if (m_PredateProbeShooter || m_PredateVisualTarget != target.GetEntity() || now < m_PredateNextAimPause || m_eAI_IsInDangerByArea || eAI_IsSideStepping() || GetCurrentCommandID() != DayZPlayerConstants.COMMANDID_MOVE) return;
        Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
        if (!weapon || !eAI_CanFire(weapon) || !PredateShotLane.Clear(this, weapon, GetAimPosition(), target.GetEntity())) return;

        float pause = 2.1;
        if (distance > PredateFirePolicy.LONG_RANGE) pause = 2.8;
        m_PredateAimPausePosition = GetPosition();
        m_PredateAimPauseTarget = target.GetEntity();
        m_PredateAimPauseUntil = now + pause;
        m_PredateNextAimPause = m_PredateAimPauseUntil + 2.0;
        PredateTraceAI("paused movement to settle aim");
    }

    void PredateDeferredShot(eAITarget target, string reason, bool canChangeView)
    {
        m_PredateFireBlock = reason;
        if (!target.IsPlayer() || m_PredateProbeShooter) return;
        float now = m_PredateSimulationTime;
        if (m_PredateReconsiderTarget != target.GetEntity() || m_PredateBlockedSince < 0)
        {
            m_PredateReconsiderTarget = target.GetEntity();
            m_PredateBlockedSince = now;
        }
        if (now - m_PredateBlockedSince < 2.5) return;
        m_PredateBlockedSince = now;
        m_PredateReconsiderAt = now + 1.5;
        if (canChangeView && GetCurrentCommandID() == DayZPlayerConstants.COMMANDID_MOVE)
        {
            eAI_ForceSideStep(0.8, null, 0, false);
            m_PredateFireBlock = "Checking a different view after repeated shot denial";
        }
        else m_PredateFireBlock = "Watching after repeated shot denial: " + reason;
        PredateTraceAI(m_PredateFireBlock);
    }

    override float eAI_GetVisibility(float distance, eAITarget target)
    {
        float visibility = super.eAI_GetVisibility(distance, target);
        if (!m_PredateCombatEnabled || m_PredateSensorDepth <= 0 || !target || !target.IsPlayer() || !target.m_LOS || distance <= PredateFirePolicy.LONG_RANGE) return visibility;
        PlayerBase person = PlayerBase.Cast(target.GetEntity());
        if (!person) return visibility;
        float sightLimit = eAI_GetVisibilityDistanceLimit(person);
        if (distance > sightLimit) return visibility;
        // The native accuracy curve drops sharply near its sight limit. Keep
        // a small distance penalty while the character is actually in view.
        return Math.Max(visibility, Math.Lerp(1.0, 0.98, distance / sightLimit));
    }

    bool PredateDeliberateFireMode(Weapon_Base weapon, out bool changed)
    {
        // Expansion distinguishes semi-automatic rifles from single-shot
        // break/bolt-action modes. M4A1 has SemiAuto, not Single.
        if (weapon.Expansion_SetFireMode(ExpansionFireMode.SemiAuto, changed)) return true;
        if (weapon.Expansion_SetFireMode(ExpansionFireMode.Single, changed)) return true;
        if (weapon.Expansion_SetFireMode(ExpansionFireMode.FullAuto, changed)) return true;
        return weapon.Expansion_SetFireMode(ExpansionFireMode.Burst, changed);
    }

    string PredateCombatDiagnostic()
    {
        eAITarget target = GetTarget();
        Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
        string result = "attempts=" + m_PredateFireAttempts + "; gate=" + m_PredateFireBlock + "; fighting=" + m_eAI_IsFightingFSM + "; raised=" + IsRaised() + "; speed=" + Expansion_GetMovementSpeed() + "; threat=" + GetThreatToSelf();
        if (target) result += "; LOS=" + target.m_LOS + "; nativeLOS=" + eAI_HasLOS();
        if (weapon) result += "; chambered=" + weapon.Expansion_IsChambered() + "; jammed=" + weapon.IsJammed();
        vector origin = GetBonePositionWS(GetBoneIndexByName("neck"));
        vector direction = vector.Direction(origin, GetAimPosition()).Normalized();
        float alignment = vector.Dot(direction, GetWeaponAimDirection().Normalized());
        float facing = PredateMath.CameraHeading(GetDirection());
        float looking = PredateMath.CameraHeading(GetLookDirection());
        result += "; alignment=" + alignment + "; facing=" + facing + "; looking=" + looking;
        result += "; stance=" + eAI_GetStance() + "; eyes=" + origin + "; visibility=" + m_Expansion_Visibility;
        result += "; daylight=" + m_Expansion_DaylightVisibility + "; fog=" + m_eAI_FogVisibility;
        result += "; overcast=" + m_eAI_OvercastVisibility + "; dynamicFog=" + m_eAI_DynVolFogVisibility;
        return result;
    }

    override void eAI_SelectFireMode(eAITarget target, float distSq, Weapon_Base weapon)
    {
        if (!m_PredateCombatEnabled)
        {
            super.eAI_SelectFireMode(target, distSq, weapon);
            return;
        }
        if (!eAI_CanFire(weapon)) return;
        bool changed;
        if (distSq > PredateFirePolicy.DELIBERATE_RANGE * PredateFirePolicy.DELIBERATE_RANGE)
            PredateDeliberateFireMode(weapon, changed);
        else
            weapon.Expansion_SetFireMode(ExpansionFireMode.FullAuto, changed);
        if (changed) m_eAI_QueuedShots = 0;
    }

    override void TryFireWeapon()
    {
        if (!m_PredateCombatEnabled)
        {
            super.TryFireWeapon();
            return;
        }
        if (m_PredateProbeShooter && m_PredateProbeCeaseFire)
        {
            m_PredateFireBlock = "Validation fixture draining projectiles";
            return;
        }
        m_PredateFireAttempts++;
        eAITarget target = GetTarget();
        if (!target || !target.GetEntity()) { m_PredateFireBlock = "No native target"; return; }
        if (m_PredateKnowledgeEnabled)
        {
            m_PredateNextPerceptionAt = 0;
            if (!EnforceLOS(0.25))
            {
                m_PredateAimSince = 0;
                m_PredateBlockedSince = -1;
                m_PredateFireBlock = "Fresh visibility check failed";
                return;
            }
            if (target.IsNoise() || !target.m_LOS || !eAI_HasLOS()) { m_PredateFireBlock = "No confirmed visual lane"; return; }
            if (m_PredateVisualTarget != target.GetEntity() || m_PredateSeenSince < 0) { m_PredateFireBlock = "No acquired visual target"; return; }
            if (m_PredateSimulationTime - m_PredateSeenSince < 0.6 || m_PredateSimulationTime - m_PredateLastVisualAt > 0.75) { m_PredateFireBlock = "Visual reaction/freshness gate"; return; }
        }
        float distance = Math.Sqrt(target.GetDistanceSq(true));
        if (distance <= PredateFirePolicy.DELIBERATE_RANGE)
        {
            m_PredateAimSince = 0;
            m_PredateFireBlock = "Native close-range fire requested";
            super.TryFireWeapon();
            return;
        }
        float now = GetGame().GetTickTime();
        if (now < m_PredateNextShotTime) { m_PredateFireBlock = "Cadence cooldown"; return; }
        if (target.GetEntity() == m_PredateReconsiderTarget && m_PredateSimulationTime < m_PredateReconsiderAt) return;
        if (!target.m_LOS || !eAI_HasLOS())
        {
            m_PredateAimSince = 0;
            m_PredateAimPauseUntil = 0;
            m_PredateFireBlock = "Visibility gate";
            return;
        }
        if (Expansion_GetMovementSpeed() > 1.1)
        {
            m_PredateAimSince = 0;
            PredateRequestAimPause(target, distance);
            m_PredateFireBlock = "Movement gate";
            return;
        }
        // Reacquire after losing contact, moving quickly, or switching target.
        if (m_PredateAimTarget != target.GetEntity() || now - m_PredateLastAimCheck > 0.75)
            m_PredateAimSince = 0;
        m_PredateAimTarget = target.GetEntity();
        m_PredateLastAimCheck = now;
        if (m_PredateAimSince == 0) m_PredateAimSince = now;
        bool longRange = distance > PredateFirePolicy.LONG_RANGE;
        float settleTime = 0.6;
        if (longRange) settleTime = PredateFirePolicy.LONG_RANGE_SETTLE_TIME;
        if (now - m_PredateAimSince < settleTime) { m_PredateFireBlock = "Aim settling gate"; return; }

        vector origin = GetBonePositionWS(GetBoneIndexByName("neck"));
        vector point = GetAimPosition();
        vector aim = GetWeaponAimDirection();
        if (!PredateFirePolicy.AimAcceptable(point - origin, aim)) { m_PredateFireBlock = "Aim alignment gate"; return; }

        Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
        if (!weapon) { m_PredateFireBlock = "No firearm"; return; }
        if (!eAI_CanFire(weapon)) { m_PredateFireBlock = "Native weapon is busy or not ready"; return; }
        if (!PredateShotLane.Clear(this, weapon, point, target.GetEntity())) { PredateDeferredShot(target, "No clear firearm lane", true); return; }
        if (longRange)
        {
            float flight, flightDistance, damageCoef;
            if (weapon.ShootsExplosiveAmmo() || !PredateMotionAim.Flight(weapon, origin, point, flight, flightDistance, damageCoef) || !PredateInformationMath.Finite(damageCoef) || damageCoef < 0.35)
            {
                PredateDeferredShot(target, "Chambered round unsuitable at this range", false);
                return;
            }
            bool moving = target.m_PredateObservedVelocity.LengthSq() > 0.25;
            if (moving && PredateMotionAim.Lead(this, weapon, target, target.m_PredateObservedAimOffset).LengthSq() < 0.01)
            {
                PredateDeferredShot(target, "No reliable lead on distant moving contact", false);
                return;
            }
            vector aimedPoint = origin + aim.Normalized() * flightDistance;
            if (!PredateShotLane.Clear(this, weapon, aimedPoint, target.GetEntity(), !moving)) { PredateDeferredShot(target, "Aim lane does not offer a clean shot", true); return; }
        }
        bool changed;
        if (!PredateDeliberateFireMode(weapon, changed)) { m_PredateFireBlock = "Supported deliberate fire mode unavailable"; return; }
        m_eAI_QueuedShots = 1;
        m_PredateBlockedSince = -1;
        m_PredateFireBlock = "Native weapon action requested";
        super.TryFireWeapon();
    }

    void PredateOnShotFired()
    {
        if (!m_PredateCombatEnabled) return;
        PredateTraceAI("fired");
        eAITarget target = GetTarget();
        if (!target) return;
        float distance = Math.Sqrt(target.GetDistanceSq(true));
        if (distance <= PredateFirePolicy.DELIBERATE_RANGE) return;
        m_PredateFiredShots++;
        m_PredateStringShots++;
        float interval = PredateFirePolicy.ShotInterval(distance);
        if (m_PredateStringShots >= 3)
        {
            interval = Math.Max(interval, 3.0);
            if (distance > PredateFirePolicy.LONG_RANGE) interval = 6.0;
            m_PredateStringShots = 0;
        }
        m_PredateNextShotTime = GetGame().GetTickTime() + interval;
        m_PredateAimSince = 0;
        m_PredateAimPauseUntil = 0;
        if (PredateServerDiagnostics.TraceOperators && (m_PredateFiredShots <= 10 || m_PredateFiredShots % 20 == 0))
            Print("[Predate] Deliberate shot " + m_PredateFiredShots + "; range=" + Math.Round(distance) + "m; minimum pause=" + interval + "s.");
    }
}

modded class eAIAimingProfile
{
    override void Update()
    {
        super.Update();
        if (!m_Unit.m_PredateCombatEnabled || m_Unit.m_PredateAimSince <= 0 || GetGame().GetTickTime() - m_Unit.m_PredateAimSince < PredateFirePolicy.LONG_RANGE_SETTLE_TIME) return;
        eAITarget target = m_Unit.GetTarget();
        if (!target || !target.m_LOS || target.GetEntity() != m_Unit.m_PredateAimTarget || m_Unit.Expansion_GetMovementSpeed() > 1.1) return;
        vector origin = m_Unit.GetBonePositionWS(m_Unit.GetBoneIndexByName("neck"));
        vector toTarget = m_Unit.GetAimPosition() - origin;
        if (toTarget.LengthSq() <= PredateFirePolicy.LONG_RANGE * PredateFirePolicy.LONG_RANGE) return;
        // Settling reduces the native aim variation. The sampled direction still
        // has to pass the alignment and geometry checks before a shot is taken.
        m_AimDirection = (m_AimDirection + toTarget.Normalized()).Normalized();
    }
}

modded class eAIPlayerTargetInformation
{
    override vector GetAimOffset(eAIBase ai = null)
    {
        vector observed = super.GetAimOffset(ai);
        if (!ai || !ai.m_PredateCombatEnabled || !ai.m_PredateKnowledgeEnabled || ai.m_PredateSensorDepth > 0) return observed;
        eAITargetInformationState state = ai.eAI_GetTargetInformationState(this, false);
        if (!state || !state.m_LOS || !state.m_PredateHasPosition) return observed;
        Weapon_Base weapon = Weapon_Base.Cast(ai.GetHumanInventory().GetEntityInHands());
        if (!weapon) return observed;
        vector chest = observed;
        chest[1] = chest[1] - 0.25;
        // Select an exposed upper chest from cached visual posture. Sensor rays
        // still use the native neck point. If the chest is covered, retain the
        // observed head/neck aim instead. No unseen position is queried here.
        vector selected = observed;
        if (observed[1] > 0.8 && PredateShotLane.Clear(ai, weapon, state.m_LastKnownPosition + chest, GetEntity())) selected = chest;
        // Extrapolate only the most recently visible motion, using this
        // firearm/cartridge's native flight model. This never refreshes hidden
        // motion and never changes an already-fired projectile's trajectory.
        return selected + PredateMotionAim.Lead(ai, weapon, state, selected);
    }
}

modded class Weapon_Base
{
    bool m_PredateDropScope;
    float m_PredateIntendedFlight;
    float m_PredateIntendedDistance;
    float m_PredateTraceDistance;
    vector m_PredateTraceOrigin;

    override bool Hitscan(vector begin_point, vector direction, float distance, eAIBase ai, Object ignore, out Object hitObject, out vector hitPosition, out vector hitNormal, out int contactComponent)
    {
        bool hit = super.Hitscan(begin_point, direction, distance, ai, ignore, hitObject, hitPosition, hitNormal, contactComponent);
        if (m_PredateDropScope)
        {
            if (hitObject)
            {
                float traceDistance = vector.Distance(m_PredateTraceOrigin, hitPosition);
                m_PredateTraceDistance = traceDistance;
            }
            else m_PredateTraceDistance = m_PredateIntendedDistance;
        }
        return hit;
    }

    override float eAI_CalculateProjectileDrop(float travelTime)
    {
        if (m_PredateDropScope && m_PredateIntendedDistance > 1)
        {
            // Native fire constructs its aim angle at the ray's hit distance.
            // Scale the rise at the intended range to that plane. A background
            // tree must not decide the elevation for a nearer moving opponent.
            return super.eAI_CalculateProjectileDrop(m_PredateIntendedFlight) * m_PredateTraceDistance / m_PredateIntendedDistance;
        }
        return super.eAI_CalculateProjectileDrop(travelTime);
    }

    override bool eAI_Fire(int muzzleIndex, eAIBase ai)
    {
        float observedSpeed, leadLength;
        eAIShot motionRecord;
        EntityAI observedTarget;
        int pendingBefore;
        if (ai && ai.m_PredateCombatEnabled && ai.m_PredateKnowledgeEnabled && ai.GetTarget())
        {
            eAITargetInformationState state = ai.GetTarget();
            observedTarget = state.m_Info.GetEntity();
            observedSpeed = state.m_PredateObservedVelocity.Length();
            vector lead = PredateMotionAim.Lead(ai, this, state, state.m_PredateObservedAimOffset);
            leadLength = lead.Length();
            if (leadLength > 0.1)
            {
                vector origin = ai.GetBonePositionWS(ai.GetBoneIndexByName("neck"));
                float flight, distance;
                if (PredateMotionAim.Flight(this, origin, ai.GetAimPosition(), flight, distance))
                {
                    m_PredateIntendedFlight = flight;
                    m_PredateIntendedDistance = distance;
                    m_PredateTraceDistance = distance;
                    m_PredateTraceOrigin = origin;
                    m_PredateDropScope = true;
                    // Construct while this exact cartridge is still chambered.
                    // Add it only after native Fire confirms a real shot.
                    motionRecord = new eAIShot(this, muzzleIndex, origin, ai.GetWeaponAimDirection(), observedTarget, ai.GetAimPosition(), -1);
                    motionRecord.m_PredatePhysicalMotionOnly = true;
                    pendingBefore = ai.m_eAI_FiredShots.Count();
                }
            }
        }
        bool fired = super.eAI_Fire(muzzleIndex, ai);
        m_PredateDropScope = false;
        if (fired && motionRecord)
        {
            bool nativeBodyRecord;
            for (int i = pendingBefore; i < ai.m_eAI_FiredShots.Count(); i++)
            {
                eAIShot nativeRecord = ai.m_eAI_FiredShots[i];
                if (nativeRecord && nativeRecord.m_HitObjectRoot == observedTarget) nativeBodyRecord = true;
            }
            if (!nativeBodyRecord)
            {
                ai.m_eAI_FiredShots.Insert(motionRecord);
                if (ai.m_PredateProbeShooter) ai.m_PredateProbePhysicalPredictions++;
            }
        }
        if (fired && ai && ai.m_PredateProbeShooter)
        {
            ai.m_PredateProbeObservedSpeeds.Insert(observedSpeed);
            ai.m_PredateProbeLeadLengths.Insert(leadLength);
        }
        if (fired && GetGame().IsServer() && ai) ai.PredateOnShotFired();
        return fired;
    }
}
