// Multiplayer HUD severity levels do not distinguish 100% from merely high.
// Synchronize visibility only; the normal notifiers still own colours/arrows.
modded class PlayerBase
{
    bool m_PredateHealthHudVisible = true;
    bool m_PredateBloodHudVisible = true;
    bool m_PredateVitalsSampled;
    float m_PredatePreviousHealth;
    float m_PredatePreviousBlood;
    int m_PredateHealthChangedUntil;
    int m_PredateBloodChangedUntil;

    void PlayerBase()
    {
        RegisterNetSyncVariableBool("m_PredateHealthHudVisible");
        RegisterNetSyncVariableBool("m_PredateBloodHudVisible");
    }

    void PredateUpdateVitalHud()
    {
        if (!GetGame().IsServer() || !GetIdentity()) return;
        float health = GetHealth("", "Health");
        float blood = GetHealth("", "Blood");
        int now = GetGame().GetTime();
        if (m_PredateVitalsSampled)
        {
            if (health != m_PredatePreviousHealth) m_PredateHealthChangedUntil = now + 1000;
            if (blood != m_PredatePreviousBlood) m_PredateBloodChangedUntil = now + 1000;
        }
        m_PredateVitalsSampled = true;
        m_PredatePreviousHealth = health;
        m_PredatePreviousBlood = blood;
        bool showHealth = health < GetMaxHealth("", "Health") || now < m_PredateHealthChangedUntil;
        bool showBlood = blood < GetMaxHealth("", "Blood") || now < m_PredateBloodChangedUntil;
        if (showHealth != m_PredateHealthHudVisible || showBlood != m_PredateBloodHudVisible)
        {
            m_PredateHealthHudVisible = showHealth;
            m_PredateBloodHudVisible = showBlood;
            SetSynchDirty();
        }
    }
}
