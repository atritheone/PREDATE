modded class ExpansionSettings
{
    static const float PREDATE_SNIPER_PRONE_DISTANCE = 500;

    override void ServerInit()
    {
        super.ServerInit();
        ExpansionAISettings settings = GetAI();
        if (!settings || !ApplyPredateTuning(settings)) return;

        // Preserve the file's other values, including casing of climb exclusions.
        ExpansionAISettings saved = new ExpansionAISettings;
        if (ExpansionJsonFileParser<ExpansionAISettings>.Load(EXPANSION_AI_SETTINGS, saved) && ApplyPredateTuning(saved))
            saved.Save();
    }

    protected bool ApplyPredateTuning(ExpansionAISettings settings)
    {
        bool changed = settings.AccuracyMin != PredateConfig.DEFAULT_AI_ACCURACY_MIN || settings.AccuracyMax != PredateConfig.DEFAULT_AI_ACCURACY_MAX;
        changed = changed || settings.SniperProneDistanceThreshold != PREDATE_SNIPER_PRONE_DISTANCE || settings.ShoryukenChance != 0 || settings.MemeLevel != 0;
        if (!changed) return false;

        settings.AccuracyMin = PredateConfig.DEFAULT_AI_ACCURACY_MIN;
        settings.AccuracyMax = PredateConfig.DEFAULT_AI_ACCURACY_MAX;
        settings.SniperProneDistanceThreshold = PREDATE_SNIPER_PRONE_DISTANCE;
        settings.ShoryukenChance = 0;
        settings.MemeLevel = 0;
        return true;
    }
}
