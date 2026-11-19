modded class Environment
{
    // Keep weather collection for AI visibility, but never apply thermal stress.
    void PredateNeutralHeatComfort()
    {
        m_HeatComfort = 0;
        SetTargetHeatComfort(0);
        if (!m_Player) return;
        m_Player.GetStatHeatComfort().Set(0);
        m_Player.GetStatHeatBuffer().Set(0);
        m_Player.GetThermalBiasHandler().Set(0);
    }

    override protected void ProcessHeatComfort()
    {
        PredateNeutralHeatComfort();
    }

    override protected void SetHeatcomfortDirectly()
    {
        PredateNeutralHeatComfort();
    }
}
