// Native scheduled ticks use engine uptime even while gameplay is held. Shift
// their timestamps by the held interval so Resume cannot apply catch-up time.
modded class PlayerBase
{
    bool m_PredateClockHeld;
    int m_PredateClockHoldBegan;

    void PredateBeginClockHold()
    {
        if (m_PredateClockHeld) return;
        int now = GetGame().GetTime();
        m_PredateClockHoldBegan = now;
        m_PredateClockHeld = true;
    }

    void PredateEndClockHold()
    {
        if (!m_PredateClockHeld) return;
        int now = GetGame().GetTime();
        int milliseconds = Math.Max(0, now - m_PredateClockHoldBegan);
        float seconds = milliseconds * 0.001;
        if (m_LastTick >= 0) m_LastTick += milliseconds;
        if (m_LastPostFrameTickTime > 0) m_LastPostFrameTickTime += milliseconds;
        if (m_LastShockHitTime > 0) m_LastShockHitTime += milliseconds;
        if (m_eAI_LastHitTime > 0) m_eAI_LastHitTime += seconds;
        if (m_eAI_LastNoiseTime > 0) m_eAI_LastNoiseTime += seconds;
        // Expansion aggression timestamps are whole UTC seconds, not uptime ms.
        int wholeSeconds = Math.Round(seconds);
        if (m_eAI_LastAggressionTime > 0) m_eAI_LastAggressionTime += wholeSeconds;
        if (m_eAI_LastAggressionTimeout > 0) m_eAI_LastAggressionTimeout += wholeSeconds;
        m_PredateClockHeld = false;
    }
}

modded class eAIBase
{
    override void PredateEndClockHold()
    {
        if (!m_PredateClockHeld) return;
        int now = GetGame().GetTime();
        int milliseconds = Math.Max(0, now - m_PredateClockHoldBegan);
        float seconds = milliseconds * 0.001;
        super.PredateEndClockHold();
        if (m_eAI_LastUpdateTime > 0) m_eAI_LastUpdateTime += milliseconds;
        if (m_eAI_LastFireTime > 0) m_eAI_LastFireTime += milliseconds;
        if (m_eAI_LastEvadeTime > 0) m_eAI_LastEvadeTime += milliseconds;
        if (m_eAI_MeleeTime > 0) m_eAI_MeleeTime += milliseconds;
        if (m_eAI_LastDoorInteractionTime > 0) m_eAI_LastDoorInteractionTime += milliseconds;
        if (m_eAI_MinTimeTillNextFire > 0) m_eAI_MinTimeTillNextFire += milliseconds;
        if (m_PredateNextShotTime > 0) m_PredateNextShotTime += seconds;
        if (m_PredateAimSince > 0) m_PredateAimSince += seconds;
        if (m_PredateLastAimCheck > 0) m_PredateLastAimCheck += seconds;
        foreach (eAIShot shot : m_eAI_FiredShots)
        {
            if (!shot) continue;
            shot.m_Time += milliseconds;
            if (shot.m_ProcessedTime > 0) shot.m_ProcessedTime += milliseconds;
        }
        foreach (eAITargetInformation information, eAITarget contact : m_eAI_TargetInformationStates)
        {
            if (!contact) continue;
            if (contact.m_ThreatLevelUpdateTimestamp > 0) contact.m_ThreatLevelUpdateTimestamp += milliseconds;
            if (contact.m_SearchPositionUpdateTimestamp > 0) contact.m_SearchPositionUpdateTimestamp += milliseconds;
            if (contact.m_IsTracked) contact.m_FoundAtTime += milliseconds;
        }
    }
}
