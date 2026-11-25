modded class eAIBase
{
    protected eAIGroup m_PredateCheckpointGroup;

    void PredateTakeSaveOwnership(eAIGroup group)
    {
        m_PredateCheckpointGroup = group;
        // Keep Expansion's persistent-group behavior, including death cleanup.
        // Only PREDATE schedules these groups for storage writes.
        group.m_Persist = true;
        eAIGroup.s_PersistentGroups.RemoveItem(group);
    }

    override void OnCEIterate(float currentTime, float elapsedTime)
    {
        eAIGroup group = GetGroup();
        // Expansion can save persistent groups in this hook even with an
        // empty player list, without consulting its persistent-group list.
        bool managed = group && group == m_PredateCheckpointGroup && group.m_Persist;
        if (managed) group.m_Persist = false;
        super.OnCEIterate(currentTime, elapsedTime);
        if (managed) group.m_Persist = true;
    }
}
