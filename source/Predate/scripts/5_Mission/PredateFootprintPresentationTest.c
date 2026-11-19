class PredateFootprintPresentationTest
{
    int Checks;
    int Failures;
    int m_Phase;
    bool Projected;

    void Check(bool passed)
    {
        Checks++;
        if (!passed) Failures++;
    }

    bool Update(PredateState state, TextWidget hint, ImageWidget marker)
    {
        if (!state || !hint || !marker) return true;
        if (m_Phase == 0 && state.FootprintClue == 0)
        {
            Check(!hint.IsVisible() && !marker.IsVisible());
            m_Phase = 1;
        }
        else if (m_Phase == 1 && state.FootprintClue == 2)
        {
            if (marker.IsVisible())
            {
                float width, height;
                marker.GetScreenSize(width, height);
                Projected = width >= 1 && height >= 1 && !hint.IsVisible();
            }
            Check(Projected || (hint.IsVisible() && PredateReceiverValidation.Fits(hint)));
            m_Phase = 2;
        }
        else if (m_Phase == 2 && state.FootprintClue == 0)
        {
            Check(!hint.IsVisible() && !marker.IsVisible());
            string error;
            JsonFileLoader<PredateFootprintPresentationTest>.SaveFile("$profile:FootprintPresentation.json", this, error);
            return false;
        }
        return true;
    }
}
