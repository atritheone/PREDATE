// Deliberate knowledge boundary for the pinned Expansion implementation.
// The sensor may test truth for visibility. Decision paths receive cached evidence.
modded class eAIBase
{
    bool m_PredateKnowledgeEnabled;
    string m_PredateOperatorId;
    ref PredateObservationMemory m_PredateMemory;
    float m_PredateSimulationTime;
    int m_PredateSensorDepth;
    EntityAI m_PredateVisualTarget;
    float m_PredateSeenSince = -1;
    float m_PredateLastVisualAt = -1;
    float m_PredateNextPerceptionAt;
    int m_PredateNextVisibleChoice;
    eAITarget m_PredateNearestVisible;
    ref array<ref PredateObservation> m_PredatePendingSounds = new array<ref PredateObservation>;

    void PredateBindInformation(bool enabled, string owner, PredateObservationMemory memory, array<ref PredateObservation> pending, float now)
    {
        bool newlyEnabled = enabled && !m_PredateKnowledgeEnabled;
        m_PredateKnowledgeEnabled = enabled;
        m_PredateOperatorId = owner;
        m_PredateMemory = memory;
        m_PredatePendingSounds = pending;
        m_PredateSimulationTime = now;
        if (!newlyEnabled) return;
        m_PredateVisualTarget = null;
        m_PredateSeenSince = -1;
        m_PredateLastVisualAt = -1;
        m_PredateNextPerceptionAt = 0;
        vector ownPosition = GetPosition();
        // Entity replacement must not import live coordinates acquired before
        // the replacement received its PREDATE policy.
        foreach (eAITargetInformation info, eAITarget state : m_eAI_TargetInformationStates)
        {
            if (!info || !state || !info.IsPlayer()) continue;
            state.m_LOS = false;
            state.m_PredateHasPosition = false;
            state.m_PredateObservedVelocity = vector.Zero;
            state.m_PredateObservedMoving = false;
            state.m_PredateObservedAimOffset = "0 1.4 0";
            state.m_PredateObservedUnconscious = false;
            state.m_PredateObservedRaised = false;
            state.m_PredateObservedFighting = false;
            state.m_PredateObservedLit = false;
            state.m_PredateObservedAttackCooldown = 0;
            state.m_LastKnownPosition = ownPosition;
            state.m_SearchPosition = state.m_LastKnownPosition;
            state.m_SearchDirection = vector.Zero;
            state.m_SearchOnLOSLost = false;
            state.m_ThreatLevel = 0.35;
            state.m_ThreatLevelActive = 0.0999;
        }
    }

    override bool PlayerIsEnemy(EntityAI other, bool track = false, out bool isPlayerMoving = false, out bool friendly = false, out bool targeted = false)
    {
        // An authored opposed-combat fixture needs a real streaming client
        // without introducing a third combatant. Ordinary actors never enable
        // both of these server-only probe fields.
        if (m_PredateProbeShooter && m_PredateProbeSpectator && other == m_PredateProbeSpectator)
        {
            isPlayerMoving = false;
            friendly = true;
            targeted = false;
            return false;
        }
        if (!m_PredateKnowledgeEnabled) return super.PlayerIsEnemy(other, track, isPlayerMoving, friendly, targeted);
        PlayerBase person = PlayerBase.Cast(other);
        if (!person) return super.PlayerIsEnemy(other, track, isPlayerMoving, friendly, targeted);
        isPlayerMoving = false;
        friendly = false;
        targeted = false;
        if (person == this || person.GetGroup() == GetGroup())
        {
            friendly = true;
            return false;
        }
        eAITargetInformationState known = eAI_GetTargetInformationState(person.GetTargetInformation(), false);
        if (known)
        {
            targeted = known.m_PredateHasPosition;
            isPlayerMoving = known.m_PredateObservedMoving;
        }
        // Independent operators do not inspect another group's target list or
        // an unseen person's motion, stance, weapon or aggression timestamps.
        return true;
    }

    void PredateAdvanceInformation(float now)
    {
        m_PredateSimulationTime = now;
        if (!m_PredateKnowledgeEnabled || !m_PredateMemory) return;
        for (int i = m_PredatePendingSounds.Count() - 1; i >= 0; i--)
        {
            PredateObservation sound = m_PredatePendingSounds[i];
            if (now < sound.ObservedAt) continue;
            if (m_PredateMemory.Add(sound, now))
            {
                m_eAI_NoiseTargetInfo.SetNoiseParams(null, sound.Estimate, sound.Magnitude, Math.Max(0, sound.ExpiresAt - now), 0.35);
                eAI_AddNoiseTarget(0.35);
            }
            m_PredatePendingSounds.Remove(i);
        }
        m_PredateMemory.Prune(now);
    }

    void PredateRememberVisual(eAITargetInformationState state)
    {
        if (!m_PredateMemory || !state || !state.m_Info) return;
        if (state.m_PredateVisualContactKey != "" && m_PredateSimulationTime - state.m_PredateVisualContactAt < 0.5) return;
        // The sensor can recognize its current visual contact without assigning
        // that silhouette to a radio call sign.
        if (state.m_PredateVisualContactKey == "" || m_PredateSimulationTime - state.m_PredateVisualContactAt > 2)
            state.m_PredateVisualContactKey = "sighting-" + m_PredateMemory.NextSerial;
        state.m_PredateVisualContactAt = m_PredateSimulationTime;
        PredateObservation observation = new PredateObservation;
        observation.EventKey = "visual-" + m_PredateMemory.NextSerial;
        observation.ObserverId = m_PredateOperatorId;
        observation.VisualContactKey = state.m_PredateVisualContactKey;
        observation.Source = PredateObservationSource.VISUAL;
        observation.ObservedAt = m_PredateSimulationTime;
        observation.ExpiresAt = m_PredateSimulationTime + 120;
        observation.Confidence = 0.95;
        observation.HasPosition = true;
        observation.Estimate = state.m_LastKnownPosition;
        observation.Radius = 2;
        m_PredateMemory.Add(observation, m_PredateSimulationTime);
    }

    override bool EnforceLOS(float pDt)
    {
        if (!m_PredateKnowledgeEnabled) return super.EnforceLOS(pDt);
        eAITarget target = GetTarget();
        if (!target) return false;
        if (!m_eAI_TargetChanged && m_PredateSimulationTime < m_PredateNextPerceptionAt) return target.m_LOS;
        m_PredateNextPerceptionAt = m_PredateSimulationTime + 0.25;
        bool changed = m_eAI_TargetChanged;
        m_eAI_TargetChanged = true;
        m_PredateSensorDepth++;
        bool visible = super.EnforceLOS(pDt);
        m_eAI_TargetChanged = changed;
        if (visible && target && target.IsPlayer())
        {
            target.PredateCaptureVisual();
            if (m_PredateVisualTarget != target.GetEntity())
            {
                m_PredateVisualTarget = target.GetEntity();
                m_PredateSeenSince = m_PredateSimulationTime;
            }
            m_PredateLastVisualAt = m_PredateSimulationTime;
        }
        else
        {
            m_PredateVisualTarget = null;
            m_PredateSeenSince = -1;
        }
        m_PredateSensorDepth--;
        return visible;
    }

    override void eAI_PrioritizeTargets()
    {
        super.eAI_PrioritizeTargets();
        if (!m_PredateKnowledgeEnabled || IsUnconscious()) return;
        eAITarget original = GetTarget();
        if (!original || (!original.IsPlayer() && original.IsAcuteDanger())) return;
        int now = GetGame().GetTime();
        if (now >= m_PredateNextVisibleChoice)
        {
            m_PredateNextVisibleChoice = now + 500;
            m_PredateNearestVisible = null;
            float closest = m_eAI_ThreatDistanceLimit;
            vector eyes = GetBonePositionWS(GetBoneIndexByName("Head"));
            foreach (eAITarget candidate : m_eAI_Targets)
            {
                PlayerBase person = PlayerBase.Cast(candidate.GetEntity());
                if (!candidate.IsPlayer() || !person || !person.IsAlive() || !PlayerIsEnemy(person)) continue;
                vector body = person.GetBonePositionWS(person.GetBoneIndexByName("Neck"));
                float distance = vector.Distance(eyes, body);
                if (distance >= closest || !PredateCanSeeContact(candidate, person, eyes, body, distance)) continue;
                closest = distance;
                m_PredateNearestVisible = candidate;
            }
        }
        if (!m_PredateNearestVisible || m_PredateNearestVisible == original) return;
        int index = m_eAI_Targets.Find(m_PredateNearestVisible);
        if (index <= 0) return;
        m_eAI_Targets[index] = original;
        m_eAI_Targets[0] = m_PredateNearestVisible;
        if (m_eAI_NoiseTarget == index) m_eAI_NoiseTarget = 0;
        else if (original.IsNoise()) m_eAI_NoiseTarget = index;
        m_eAI_TargetChanged = true;
        m_eAI_SyncCurrentTarget = true;
    }

    bool PredateCanSeeContact(eAITarget candidate, PlayerBase person, vector eyes, vector body, float distance)
    {
        if (person.Expansion_HasAdminToolInvisibility()) return false;
        vector sight = body - eyes;
        float angle = PredateMath.RelativeBearing(PredateMath.CameraHeading(sight), PredateMath.CameraHeading(GetLookDirection()));
        if (Math.AbsFloat(angle) > eAI_CalculateFOVHalfAngleH(distance)) return false;
        m_PredateSensorDepth++;
        float visibleDistance = eAI_GetVisibilityDistance(distance, candidate);
        m_PredateSensorDepth--;
        if (visibleDistance < distance) return false;
        vector position, normal;
        int component;
        set<Object> hits = new set<Object>;
        if (!DayZPhysics.RaycastRV(eyes, body + sight.Normalized() * 0.4, position, normal, component, hits, null, this, false, false, ObjIntersectView, 0)) return false;
        foreach (Object hit : hits)
        {
            EntityAI entity = EntityAI.Cast(hit);
            return entity && entity.GetHierarchyRoot() == person;
        }
        return false;
    }

    override void eAI_OnNoiseEvent(EntityAI source, vector position, float lifetime, eAINoiseParams params, float strengthMultiplier)
    {
        if (!m_PredateKnowledgeEnabled)
        {
            super.eAI_OnNoiseEvent(source, position, lifetime, params, strengthMultiplier);
            return;
        }
        if (!params || IsUnconscious() || m_eAI_NoiseInvestigationDistanceLimit <= 0) return;
        if (source && source.GetHierarchyRoot() == this) return;
        float strength = params.m_Strength * strengthMultiplier;
        if (strength <= 0) return;
        if (source && position == vector.Zero) position = source.GetPosition();
        float distance = vector.Distance(GetPosition(), position);
        if (distance > strength || distance > m_eAI_NoiseInvestigationDistanceLimit) return;
        float radius = Math.Max(5, distance * 0.1);
        vector estimate = ExpansionMath.GetRandomPointInCircle(position, radius);
        float delay = distance / 343.0 + 0.17;
        float confidence = 0.5;
        if (!m_PredateMemory) return;
        PredateObservation observation = new PredateObservation;
        int eventSerial = m_PredateMemory.NextSerial;
        m_PredateMemory.NextSerial++;
        observation.EventKey = "noise-" + eventSerial;
        observation.ObserverId = m_PredateOperatorId;
        observation.Source = PredateObservationSource.AUDIBLE;
        if (params.m_Type == eAINoiseType.BULLETIMPACT) observation.Source = PredateObservationSource.IMPACT;
        observation.ObservedAt = m_PredateSimulationTime + delay;
        observation.ExpiresAt = observation.ObservedAt + 120;
        observation.Confidence = confidence;
        observation.Magnitude = strength;
        observation.HasPosition = true;
        observation.Estimate = estimate;
        observation.Radius = radius;
        m_PredatePendingSounds.Insert(observation);
        while (m_PredatePendingSounds.Count() > 8) m_PredatePendingSounds.Remove(0);
    }
}

modded class eAITargetInformationState
{
    string m_PredateVisualContactKey;
    float m_PredateVisualContactAt;
    bool m_PredateHasPosition;
    vector m_PredateObservedAimOffset = "0 1.4 0";
    bool m_PredateObservedUnconscious;
    bool m_PredateObservedRaised;
    bool m_PredateObservedFighting;
    bool m_PredateObservedLit;
    bool m_PredateObservedMoving;
    vector m_PredateObservedVelocity;
    float m_PredateObservedAttackCooldown;

    bool PredateRestricted()
    {
        return m_AI && m_AI.m_PredateKnowledgeEnabled && m_Info && m_Info.IsPlayer();
    }

    override void SetInitial(float threat, vector position)
    {
        m_PredateHasPosition = true;
        super.SetInitial(threat, position);
    }

    void PredateCaptureVisual()
    {
        if (!PredateRestricted() || m_AI.m_PredateSensorDepth <= 0 || !m_LOS) return;
        if (m_AI.eAI_GetTargetInformation() == m_Info)
        {
            super.UpdatePosition(true);
            vector offset = m_Info.GetAimOffset(m_AI);
            m_PredateObservedAimOffset = offset;
            bool unconscious = m_Info.IsUnconscious();
            bool raised = m_Info.IsRaised();
            bool fighting = m_Info.IsFighting();
            bool lit = m_Info.IsLit();
            float attackCooldown = m_Info.GetAttackCooldown();
            m_PredateObservedUnconscious = unconscious;
            m_PredateObservedRaised = raised;
            m_PredateObservedFighting = fighting;
            m_PredateObservedLit = lit;
            m_PredateObservedAttackCooldown = attackCooldown;
            PlayerBase person = PlayerBase.Cast(m_Info.GetEntity());
            if (person)
            {
                vector velocity = person.Expansion_GetActualVelocity();
                m_PredateObservedMoving = velocity.LengthSq() > 0.01;
                if (PredateInformationMath.FiniteVector(velocity) && velocity.LengthSq() <= 100)
                    m_PredateObservedVelocity = velocity;
                else m_PredateObservedVelocity = vector.Zero;
            }
            super.UpdateThreat(true);
            m_PredateHasPosition = true;
            m_AI.PredateRememberVisual(this);
        }
    }

    override void UpdatePosition(bool force = false)
    {
        if (!PredateRestricted()) { super.UpdatePosition(force); return; }
        // A cached LOS bit is not permission to read a position that may already
        // be behind cover. Only a fresh successful sensor pass captures truth.
        if (m_LOS) return;
        if (!m_PredateHasPosition)
        {
            // Discovery alone does not grant the candidate's hidden location.
            vector own = m_AI.GetPosition();
            m_LastKnownPosition = own;
            m_SearchPosition = own;
            m_SearchDirection = vector.Zero;
        }
        else if (!force) super.UpdatePosition(false);
    }

    override void UpdateThreat(bool force = false)
    {
        if (!PredateRestricted()) { super.UpdateThreat(force); return; }
        if (m_LOS) return;
        // Do not recalculate from an occluded person's live stance/weapon/health.
        int now = GetGame().GetTime();
        float elapsed = Math.Max(0, now - m_ThreatLevelUpdateTimestamp) * 0.001;
        m_ThreatLevelUpdateTimestamp = now;
        if (m_ThreatLevel <= 0) m_ThreatLevel = 0.35;
        m_ThreatLevelActive = Math.Lerp(m_ThreatLevelActive, 0.0999, Math.Clamp(elapsed / 30.0, 0, 1));
    }
}

modded class eAITarget
{
    override bool CanMeleeIfClose()
    {
        if (PredateRestricted())
        {
            if (!m_LOS) return false;
            Weapon_Base weapon = Weapon_Base.Cast(m_AI.GetHumanInventory().GetEntityInHands());
            if (weapon && (weapon.Expansion_IsChambered() || weapon.Expansion_GetMagazineAmmoCount() > 0)) return false;
            return true;
        }
        return super.CanMeleeIfClose();
    }
    override bool IsUnconscious()
    {
        if (PredateRestricted() && m_AI.m_PredateSensorDepth == 0) return m_PredateObservedUnconscious;
        return super.IsUnconscious();
    }
    override bool IsRaised()
    {
        if (PredateRestricted() && m_AI.m_PredateSensorDepth == 0) return m_PredateObservedRaised;
        return super.IsRaised();
    }
    override bool IsFighting()
    {
        if (PredateRestricted() && m_AI.m_PredateSensorDepth == 0) return m_PredateObservedFighting;
        return super.IsFighting();
    }
    override bool IsLit()
    {
        if (PredateRestricted() && m_AI.m_PredateSensorDepth == 0) return m_PredateObservedLit;
        return super.IsLit();
    }
    override float GetAttackCooldown()
    {
        if (PredateRestricted() && m_AI.m_PredateSensorDepth == 0) return m_PredateObservedAttackCooldown;
        return super.GetAttackCooldown();
    }
    override IEntity GetParent()
    {
        if (PredateRestricted() && m_AI.m_PredateSensorDepth == 0) return null;
        return super.GetParent();
    }
}

modded class eAIEntityTargetInformation
{
    override vector GetPosition(eAIBase ai = null, bool actual = false, eAITargetInformationState state = null)
    {
        if (!ai || !ai.m_PredateKnowledgeEnabled || !IsPlayer() || ai.m_PredateSensorDepth > 0)
            return super.GetPosition(ai, actual, state);
        if (!state) state = ai.eAI_GetTargetInformationState(this, false);
        if (!state) return ai.GetPosition();
        if (actual || state.m_LOS) return state.m_LastKnownPosition;
        state.UpdatePosition(false);
        return state.m_SearchPosition;
    }
}

modded class eAIPlayerTargetInformation
{
    override float CalculateThreat(eAIBase ai = null, eAITargetInformationState state = null)
    {
        if (ai && ai.m_PredateKnowledgeEnabled && ai.m_PredateSensorDepth == 0)
        {
            if (state && state.m_ThreatLevel > 0) return state.m_ThreatLevel;
            return 0.35;
        }
        return super.CalculateThreat(ai, state);
    }

    override vector GetAimOffset(eAIBase ai = null)
    {
        if (ai && ai.m_PredateKnowledgeEnabled && ai.m_PredateSensorDepth == 0)
        {
            eAITargetInformationState state = ai.eAI_GetTargetInformationState(this, false);
            if (state) return state.m_PredateObservedAimOffset;
            return "0 1.4 0";
        }
        return super.GetAimOffset(ai);
    }
}
