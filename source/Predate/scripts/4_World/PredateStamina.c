modded class StaminaHandler
{
    override protected void ProcessStaminaChanges(float deltaT, bool isServerOrSingleplayer)
    {
        m_Stamina = m_StaminaCap;
        m_StaminaSynced = m_StaminaCapSynced;
        m_StaminaDelta = 0;
        m_StaminaDepletion = 0;
        m_StaminaDepleted = false;
        super.ProcessStaminaChanges(deltaT, isServerOrSingleplayer);
    }

    override void DepleteStaminaEx(EStaminaModifiers modifier, float dT = -1, float coef = 1.0)
    {
        // Discrete actions such as jumping and melee also bypass depletion.
    }
}
