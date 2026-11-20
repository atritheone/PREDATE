modded class PlayerBase
{
    private bool m_PredateStartingUnconscious;

    override void OnUnconsciousStart()
    {
        m_PredateStartingUnconscious = true;
        super.OnUnconsciousStart();
        m_PredateStartingUnconscious = false;
    }

    override bool CanDropEntity(notnull EntityAI item)
    {
        if (m_PredateStartingUnconscious && item == GetEntityInHands() && (item.IsWeapon() || item.IsMeleeWeapon()))
            return false;
        return super.CanDropEntity(item);
    }
}

modded class eAIBase
{
    private bool m_PredateAIStartingUnconscious;

    override void OnUnconsciousStart()
    {
        m_PredateAIStartingUnconscious = true;
        super.OnUnconsciousStart();
        m_PredateAIStartingUnconscious = false;
    }

    override bool eAI_DropItemInHandsImpl(bool overrideThreat = false, bool switchOff = true, bool remember = true)
    {
        // Expansion drops the held item before calling PlayerBase.OnUnconsciousStart.
        if (m_PredateAIStartingUnconscious)
        {
            ItemBase held = GetItemInHands();
            if (held && (held.IsWeapon() || held.IsMeleeWeapon()))
                return false;
        }
        return super.eAI_DropItemInHandsImpl(overrideThreat, switchOff, remember);
    }
}
