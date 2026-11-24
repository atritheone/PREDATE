modded class eAIBase
{
    override protected void RR_QueueDeathHit()
    {
        if (!m_PredateCombatEnabled)
        {
            super.RR_QueueDeathHit();
            return;
        }
        if (!GetGame().IsServer() || !m_RRDeathHit || m_RRDeathHitApplied || m_RRDeathHitQueued)
            return;

        // Let the death command finish enabling native physics, then apply the
        // impulse on the first gameplay queue update.
        m_RRDeathHitQueued = true;
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Call(RR_ApplyDeathHit);
    }
}
