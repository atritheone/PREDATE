// Native aiming reads target motion. Permit it only inside a fresh successful
// visual scope; an old positive LOS bit is not permission to inspect hidden motion.
modded class eAIAimingProfile
{
    override void Update()
    {
        eAITarget target = m_Unit.GetTarget();
        if (!m_Unit.m_PredateKnowledgeEnabled || !target || !target.IsPlayer())
        {
            super.Update();
            return;
        }
        if (!target.m_LOS)
        {
            vector forward = m_Unit.GetDirection();
            m_AimDirection = forward;
            return;
        }
        m_Unit.m_PredateNextPerceptionAt = 0;
        if (!m_Unit.EnforceLOS(0.05))
        {
            vector ownForward = m_Unit.GetDirection();
            m_AimDirection = ownForward;
            return;
        }
        m_Unit.m_PredateSensorDepth++;
        super.Update();
        m_Unit.m_PredateSensorDepth--;
    }
}
