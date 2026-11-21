class PredateSurvivalRules
{
    static bool SoloIsSurvivalModifier(PredateSession s, int id)
    {
        switch (id)
        {
            case eModifiers.MDF_TEMPERATURE:
            case eModifiers.MDF_HEATBUFFER:
            case eModifiers.MDF_BURNING:
            case eModifiers.MDF_HUNGER:
            case eModifiers.MDF_FEVER:
            case eModifiers.MDF_COMMON_COLD:
            case eModifiers.MDF_THIRST:
            case eModifiers.MDF_STUFFED:
            case eModifiers.MDF_POISONING:
            case eModifiers.MDF_HEMOLYTIC_REACTION:
            case eModifiers.MDF_STOMACH:
            case eModifiers.MDF_INFLUENZA:
            case eModifiers.MDF_CHOLERA:
            case eModifiers.MDF_SALMONELLA:
            case eModifiers.MDF_BRAIN:
            case eModifiers.MDF_VOMITSTUFFED:
            case eModifiers.MDF_WETNESS:
            case eModifiers.MDF_TOXICITY:
            case eModifiers.MDF_WOUND_INFECTION1:
            case eModifiers.MDF_WOUND_INFECTION2:
            case eModifiers.MDF_FATIGUE:
            case eModifiers.MDF_CONTAMINATION1:
            case eModifiers.MDF_CONTAMINATION2:
            case eModifiers.MDF_CONTAMINATION3:
            case eModifiers.MDF_AREAEXPOSURE:
            case eModifiers.MDF_DROWNING:
            case eModifiers.MDF_HEAVYMETAL1:
            case eModifiers.MDF_HEAVYMETAL2:
            case eModifiers.MDF_HEAVYMETAL3:
            case eModifiers.MDF_PNEUMONIA:
            case eModifiers.MDF_SANDSTORM_EXPOSURE_STATIC:
            case eModifiers.MDF_SANDSTORM_EXPOSURE_DYNAMIC:
            case eModifiers.MDF_PARTICLES_BREATH:
            case eModifiers.MDF_PARTICLES_EYES:
            case eModifiers.MDF_HEAT_STROKE1:
            case eModifiers.MDF_HEAT_STROKE2:
            case eModifiers.MDF_HEAT_STROKE3:
            case eModifiers.MDF_HEAT_STROKE4:
            return true;
        }
        return false;
    }

    static void SoloApplyCombatRules(PredateSession s, PlayerBase player)
    {
        if (!player || !player.IsAlive())
            return;

        ModifiersManager manager = player.GetModifiersManager();
        if (manager)
        {
            for (int id = 1; id < eModifiers.COUNT; id++)
            {
                if (!s.SoloIsSurvivalModifier(id))
                    continue;
                ModifierBase modifier = manager.GetModifier(id);
                if (!modifier || (modifier.IsLocked() && !modifier.IsActive() && !modifier.m_ShouldBeActive))
                    continue;
                modifier.Deactivate();
                modifier.m_ShouldBeActive = false;
                modifier.SetLock(true);
            }
        }
        player.RemoveAllAgents();
        player.GetStatEnergy().Set(player.GetStatEnergy().GetMax());
        player.GetStatWater().Set(player.GetStatWater().GetMax());
        player.GetStatHeatComfort().Set(0.0);
        player.GetStatHeatBuffer().Set(0);
        player.GetThermalBiasHandler().Set(0);
        player.GetStatWet().Set(0);
        player.UnsetMixedSoundState(eMixedSoundStates.HUNGRY);
        player.UnsetMixedSoundState(eMixedSoundStates.THIRSTY);
        // Do not set health, blood, shock, stamina, bleeding or weapon damage.
    }
}
