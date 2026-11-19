modded class eAIBase
{
    bool m_PredatePatrolReady;

    bool PredateCanPatrolReady(Entity held)
    {
        if (!m_PredateCombatEnabled || !Weapon_Base.Cast(held) || held.IsDamageDestroyed()) return false;
        if (!IsAlive() || IsUnconscious() || IsRestrained() || !CanRaiseWeapon() || IsInVehicle()) return false;
        if (m_eAI_IsFightingFSM || m_eAI_CurrentThreatToSelfActive >= 0.2 || m_eAI_IsInDangerByArea) return false;
        if (!m_FSM || (!m_FSM.IsInState("Idle") && !m_FSM.IsInState("TraversingWaypoints"))) return false;
        if (GetWeaponManager().IsRunning() || PredateOwnActionPriority()) return false;
        if (!m_eAI_CommandMove || m_eAI_CommandMove.IsBlocked() || m_eAI_JumpClimb || GetMovementSpeedLimit() > 2) return false;
        return GetCurrentCommandID() == DayZPlayerConstants.COMMANDID_MOVE;
    }

    override void eAI_HandleWeapons(float pDt, Entity pInHands, HumanInputController pInputs, out bool pExitIronSights)
    {
        if (PredateCanPatrolReady(pInHands))
        {
            PredateReadyAnimations.Apply();
            if (!IsRaised())
            {
                RaiseWeapon(true);
                m_PredatePatrolReady = true;
            }
        }
        else if (m_PredatePatrolReady)
        {
            // Combat decisions take ownership once the Operator has a threat.
            if (!m_eAI_IsFightingFSM && m_eAI_CurrentThreatToSelfActive < 0.2 && !GetWeaponManager().IsRunning())
                RaiseWeapon(false);
            m_PredatePatrolReady = false;
        }
        super.eAI_HandleWeapons(pDt, pInHands, pInputs, pExitIronSights);
    }

    override bool eAI_CanClimbOn(IEntity parent, SHumanCommandClimbResult climbRes)
    {
        if (!m_PredatePatrolReady || !IsRaised() || m_eAI_IsFightingFSM || m_eAI_CurrentThreatToSelfActive >= 0.2)
            return super.eAI_CanClimbOn(parent, climbRes);
        // Expansion treats a raised rifle as a reason to reject some climbs.
        // Let it check the obstacle at the normal patrol posture.
        RaiseWeapon(false);
        bool allowed = super.eAI_CanClimbOn(parent, climbRes);
        if (allowed) m_PredatePatrolReady = false;
        else RaiseWeapon(true);
        return allowed;
    }
}
