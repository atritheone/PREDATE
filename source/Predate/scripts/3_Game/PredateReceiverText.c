// Display only the recipient's earned sample and own position. A historical
// bearing is never re-aimed from the player's current location.
class PredateReceiverText
{
    static string CompassReadout(PredateState state)
    {
        if (!state || !state.Available || !state.Locked) return "Map  M";
        if (state.CloseApproach) return "Final Approach - Track by Sight";
        string text = PredateProtocol.DisplayName(state.OperatorId);
        if (state.HasPulse)
        {
            text += "  |  " + PredateBearingPulse.RangeText(state.Bearing, " – ");
            PredateBearingPulse tracked = state.TrackedPulse();
            if (tracked) text += "  |  " + tracked.DistanceText(" – ");
        }
        return text + "  |  Next Pulse " + Math.Ceil(state.NextPulse).ToString() + "s";
    }

    static bool LostContact(PredateState state)
    {
        return state.CanShowHistory() && state.Available && !state.HasPulse && !state.CloseApproach && state.History.Count() > 0;
    }

    static string HistoryLine(PredateReceiverSample sample)
    {
        int east = Math.Round(sample.Origin[0]);
        int north = Math.Round(sample.Origin[2]);
        int seconds = Math.Floor(sample.Age);
        return PredateBearingPulse.RangeText(sample.Bearing) + "   |   E " + east + " / N " + north + "   |   " + seconds + "s";
    }

    static string Origin(PredateState state, vector receiverPosition)
    {
        if (!state || !state.Available || !state.Locked || !state.HasPulse || state.CloseApproach)
            return "Bearings freeze between pulses.";
        if (!state.HasPulseOrigin) return "This older sample has no recorded origin.";
        if (!PredateInformationMath.FiniteVector(state.PulseOrigin) || !PredateInformationMath.FiniteVector(receiverPosition))
            return "Sample origin unavailable.";
        vector delta = receiverPosition - state.PulseOrigin;
        delta[1] = 0;
        int travelled = Math.Round(delta.Length());
        int east = Math.Round(state.PulseOrigin[0]);
        int north = Math.Round(state.PulseOrigin[2]);
        return "Sample origin E " + east + " / N " + north + " | " + travelled + " m from you";
    }
}
