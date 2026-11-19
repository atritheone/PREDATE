// Local display choices do not affect gameplay rules or saved intelligence.
class PredatePresentationOptions
{
    int Schema = 1;
    int CompassPercent = 100;
    bool HighContrast;
    static ref PredatePresentationOptions s_Current;

    bool Valid() { return Schema == 1 && CompassPercent >= 80 && CompassPercent <= 150; }

    static PredatePresentationOptions Current()
    {
        if (s_Current) return s_Current;
        s_Current = new PredatePresentationOptions;
        if (FileExist("$profile:PredatePresentation.json"))
        {
            PredatePresentationOptions saved = new PredatePresentationOptions;
            string error;
            if (JsonFileLoader<PredatePresentationOptions>.LoadFile("$profile:PredatePresentation.json", saved, error) && saved && saved.Valid()) s_Current = saved;
            else Print("[Predate] Invalid local presentation preferences; using the standard compass.");
        }
        return s_Current;
    }
}
