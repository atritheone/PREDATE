// Server-side model. The engine adapter must establish visibility before
// converting a mark into a player's clue or a bot's private observation.
enum PredateFootprintSurface
{
    HARD = 0,
    EARTH = 1,
    SAND = 2,
    SNOW = 3
}

class PredateFootprint
{
    int Serial;
    string Creator;
    vector Position;
    int Heading;
    float CreatedAt;
    float ExpiresAt;

    bool Expired(float now) { return now >= ExpiresAt; }
}

class PredateFootprintLedger
{
    static const int MAX_MARKS = 128;
    static const float VIEW_DISTANCE = 12;
    int NextSerial = 1;
    ref array<ref PredateFootprint> Marks = new array<ref PredateFootprint>;

    static float Lifetime(int surface)
    {
        if (surface == PredateFootprintSurface.EARTH) return 120;
        if (surface == PredateFootprintSurface.SAND) return 90;
        if (surface == PredateFootprintSurface.SNOW) return 180;
        return 0;
    }

    void Prune(float now)
    {
        for (int index = Marks.Count() - 1; index >= 0; index--)
            if (Marks[index].Expired(now)) Marks.RemoveOrdered(index);
    }

    bool Validate(float now)
    {
        if (!Marks || Marks.Count() > MAX_MARKS || NextSerial < 1) return false;
        int previousSerial;
        float previousTime;
        foreach (PredateFootprint mark : Marks)
        {
            if (!mark || mark.Serial <= previousSerial || mark.Serial >= NextSerial) return false;
            if (mark.Creator != "" && mark.Creator != "player-01" && PredateProtocol.OperatorSlot(mark.Creator) < 0) return false;
            if (mark.Heading < 0 || mark.Heading >= 360) return false;
            if (!PredateInformationMath.FiniteVector(mark.Position) || !PredateInformationMath.Finite(mark.CreatedAt) || !PredateInformationMath.Finite(mark.ExpiresAt)) return false;
            if (mark.CreatedAt < previousTime || mark.CreatedAt > now || mark.ExpiresAt <= mark.CreatedAt || mark.ExpiresAt - mark.CreatedAt > 181) return false;
            previousSerial = mark.Serial;
            previousTime = mark.CreatedAt;
        }
        return true;
    }

    int Record(vector site, int surface, float now, string creator = "", int heading = 0)
    {
        float lifetime = Lifetime(surface);
        if (lifetime <= 0 || !PredateInformationMath.FiniteVector(site) || !PredateInformationMath.Finite(now) || now < 0 || heading < 0 || heading >= 360) return 0;
        if (Marks.Count() > 0 && now < Marks[Marks.Count() - 1].CreatedAt) return 0;
        Prune(now);
        PredateFootprint footprint = new PredateFootprint;
        footprint.Serial = NextSerial++;
        footprint.Creator = creator;
        footprint.Position = site;
        footprint.Heading = heading;
        footprint.CreatedAt = now;
        footprint.ExpiresAt = now + lifetime;
        Marks.Insert(footprint);
        if (Marks.Count() > MAX_MARKS) Marks.RemoveOrdered(0);
        return footprint.Serial;
    }

    // Only used inside the server's nearby visibility scan.
    void Nearby(vector viewer, float now, array<ref PredateFootprint> output)
    {
        output.Clear();
        foreach (PredateFootprint mark : Marks)
            if (Reachable(mark, viewer, now)) output.Insert(mark);
    }

    static bool Reachable(PredateFootprint mark, vector viewer, float now)
    {
        if (!mark || !PredateInformationMath.FiniteVector(viewer) || !PredateInformationMath.Finite(now)) return false;
        vector delta = viewer - mark.Position;
        return now >= mark.CreatedAt && !mark.Expired(now) && delta.LengthSq() <= VIEW_DISTANCE * VIEW_DISTANCE;
    }

    static PredateObservation Observe(PredateFootprint mark, string observer, vector viewer, float now, bool visible)
    {
        if (!visible || observer == "" || !Reachable(mark, viewer, now) || mark.Creator == observer) return null;
        float ageFraction = (now - mark.CreatedAt) / (mark.ExpiresAt - mark.CreatedAt);
        PredateObservation result = new PredateObservation;
        result.EventKey = "footprint-" + mark.Serial;
        result.ObserverId = observer;
        result.Source = PredateObservationSource.FOOTPRINT;
        result.ObservedAt = now;
        result.ExpiresAt = Math.Min(mark.ExpiresAt, now + 30);
        result.Confidence = 0.6 - 0.4 * ageFraction;
        result.HasPosition = true;
        result.Estimate = mark.Position;
        result.Radius = 8 + 12 * ageFraction;
        return result;
    }
}
