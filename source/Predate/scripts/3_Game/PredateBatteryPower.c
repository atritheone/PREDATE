modded class ComponentEnergyManager
{
    bool PredatePermanentBattery()
    {
        if (!m_ThisEntityAI || m_ThisEntityAI.IsRuined()) return false;
        return m_ThisEntityAI.IsKindOf("Battery9V") || m_ThisEntityAI.IsKindOf("CarBattery") || m_ThisEntityAI.IsKindOf("TruckBattery");
    }

    override float GetEnergy()
    {
        if (PredatePermanentBattery()) return GetEnergyMax();
        return super.GetEnergy();
    }

    override void SetEnergy(float new_energy)
    {
        if (PredatePermanentBattery()) new_energy = GetEnergyMax();
        super.SetEnergy(new_energy);
    }

    override float AddEnergy(float added_energy)
    {
        if (added_energy < 0 && PredatePermanentBattery()) return 0;
        return super.AddEnergy(added_energy);
    }
}
