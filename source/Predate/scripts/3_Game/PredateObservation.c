// Server-private evidence. No entity handles, current health or equipment live here.
enum PredateObservationSource
{
    VISUAL = 1,
    AUDIBLE = 2,
    SIGNAL = 3,
    IMPACT = 4,
    FOOTPRINT = 5
}

class PredateObservation
{
    int Serial;
    string EventKey;
    string ObserverId;
    string SubjectId;
    int Source;
    float ObservedAt;
    float ExpiresAt;
    float Confidence;
    float Magnitude;
    bool HasPosition;
    vector Estimate;
    float Radius;
    bool HasBearing;
    vector Origin;
    float Bearing;
    float BearingError;
    vector MapCenter;
    float MapRadius;
    float PulseDistance;

    PredateObservation Copy()
    {
        PredateObservation copied = new PredateObservation;
        copied.Serial = Serial;
        copied.EventKey = EventKey;
        copied.ObserverId = ObserverId;
        copied.SubjectId = SubjectId;
        copied.Source = Source;
        copied.ObservedAt = ObservedAt;
        copied.ExpiresAt = ExpiresAt;
        copied.Confidence = Confidence;
        copied.Magnitude = Magnitude;
        copied.HasPosition = HasPosition;
        copied.Estimate = Estimate;
        copied.Radius = Radius;
        copied.HasBearing = HasBearing;
        copied.Origin = Origin;
        copied.Bearing = Bearing;
        copied.BearingError = BearingError;
        copied.MapCenter = MapCenter;
        copied.MapRadius = MapRadius;
        copied.PulseDistance = PulseDistance;
        return copied;
    }

    bool Valid(string owner, float now)
    {
        if (ObserverId != owner || owner == "" || EventKey == "") return false;
        if (Source < PredateObservationSource.VISUAL || Source > PredateObservationSource.FOOTPRINT) return false;
        if (!PredateInformationMath.Finite(ObservedAt) || !PredateInformationMath.Finite(ExpiresAt)) return false;
        if (ObservedAt < 0 || ObservedAt > now || ExpiresAt <= ObservedAt) return false;
        if (!PredateInformationMath.Finite(Confidence) || Confidence < 0 || Confidence > 1) return false;
        if (!PredateInformationMath.Finite(Magnitude) || Magnitude < 0) return false;
        if (!HasPosition && !HasBearing) return false;
        if (HasPosition && (!PredateInformationMath.FiniteVector(Estimate) || !PredateInformationMath.Finite(Radius) || Radius < 0)) return false;
        if (HasBearing && (!PredateInformationMath.FiniteVector(Origin) || !PredateInformationMath.Finite(Bearing) || Bearing < 0 || Bearing >= 360 || !PredateInformationMath.Finite(BearingError) || BearingError < 0 || BearingError > 180)) return false;
        if (Source == PredateObservationSource.SIGNAL && (!HasBearing || HasPosition || SubjectId == "")) return false;
        if (MapRadius != 0 && (Source != PredateObservationSource.SIGNAL || owner != "player-01" || !PredatePulseArea.Valid(MapCenter, MapRadius))) return false;
        if (!PredateInformationMath.Finite(PulseDistance) || PulseDistance < 0 || PulseDistance > PredateBearingPulse.REACH) return false;
        if (PulseDistance > 0 && (Source != PredateObservationSource.SIGNAL || owner != "player-01" || MapRadius != 0)) return false;
        // Noise and bullet impact alone never prove who produced them.
        if ((Source == PredateObservationSource.AUDIBLE || Source == PredateObservationSource.IMPACT) && SubjectId != "") return false;
        if (Source == PredateObservationSource.FOOTPRINT && (!HasPosition || HasBearing || SubjectId != "")) return false;
        if (Source == PredateObservationSource.VISUAL && (!HasPosition || SubjectId == "")) return false;
        return true;
    }

    bool Expired(float now) { return now >= ExpiresAt; }

    float ConfidenceAt(float now)
    {
        if (Expired(now)) return 0;
        float age = Math.Max(0, now - ObservedAt);
        return Confidence * (1 - age / (ExpiresAt - ObservedAt));
    }

    float RadiusAt(float now, float movementAllowance = 1.5)
    {
        return Radius + Math.Max(0, now - ObservedAt) * Math.Max(0, movementAllowance);
    }
}

class PredateObservationMemory
{
    int Schema = 1;
    string OwnerId;
    int NextSerial = 1;
    ref array<ref PredateObservation> Entries = new array<ref PredateObservation>;

    bool Add(PredateObservation observation, float now)
    {
        if (!observation || !observation.Valid(OwnerId, now) || observation.Expired(now)) return false;
        foreach (PredateObservation existing : Entries)
        {
            if (existing.EventKey == observation.EventKey) return false;
            if (existing.SubjectId == observation.SubjectId && existing.Source == observation.Source && observation.ObservedAt < existing.ObservedAt) return false;
        }
        Prune(now);
        // Keep the current visual fix; signal origins retain a short history.
        if (observation.Source == PredateObservationSource.VISUAL)
        {
            for (int i = Entries.Count() - 1; i >= 0; i--)
                if (Entries[i].Source == observation.Source && Entries[i].SubjectId == observation.SubjectId) Entries.RemoveOrdered(i);
        }
        PredateObservation stored = observation.Copy();
        stored.Serial = NextSerial;
        NextSerial++;
        Entries.Insert(stored);
        if (Entries.Count() > 32)
        {
            int oldest;
            for (int index = 1; index < Entries.Count(); index++)
                if (Entries[index].Serial < Entries[oldest].Serial) oldest = index;
            Entries.RemoveOrdered(oldest);
        }
        return true;
    }

    void Prune(float now)
    {
        for (int i = Entries.Count() - 1; i >= 0; i--)
            if (Entries[i].Expired(now)) Entries.RemoveOrdered(i);
    }

    PredateObservation Latest(string subject, int source, float now)
    {
        PredateObservation best;
        foreach (PredateObservation observation : Entries)
        {
            if (observation.SubjectId != subject || observation.Source != source || observation.Expired(now)) continue;
            if (!best || observation.Serial > best.Serial) best = observation;
        }
        if (!best) return null;
        return best.Copy();
    }

    bool Validate(float now)
    {
        if (Schema != 1 || OwnerId == "" || !Entries || Entries.Count() > 32 || NextSerial < 1) return false;
        array<string> seen = new array<string>;
        array<int> serials = new array<int>;
        foreach (PredateObservation observation : Entries)
        {
            if (!observation || !observation.Valid(OwnerId, now)) return false;
            if (observation.Serial < 1 || observation.Serial >= NextSerial) return false;
            if (seen.Find(observation.EventKey) >= 0 || serials.Find(observation.Serial) >= 0) return false;
            seen.Insert(observation.EventKey);
            serials.Insert(observation.Serial);
        }
        return true;
    }
}

class PredateInformationMath
{
    static bool Finite(float value) { return value == value && value >= -100000000 && value <= 100000000; }
    static bool FiniteVector(vector value) { return Finite(value[0]) && Finite(value[1]) && Finite(value[2]); }

    static vector BearingDirection(float bearing)
    {
        float radians = bearing * Math.DEG2RAD;
        return Vector(Math.Sin(radians), 0, Math.Cos(radians));
    }

    static bool IntersectBearings(PredateObservation a, PredateObservation b, float maximumDistance, out vector estimate)
    {
        if (!a || !b || !a.HasBearing || !b.HasBearing) return false;
        if (a.Source != PredateObservationSource.SIGNAL || b.Source != PredateObservationSource.SIGNAL) return false;
        if (a.ObserverId != b.ObserverId || a.SubjectId != b.SubjectId || a.SubjectId == "") return false;
        vector baseline = b.Origin - a.Origin;
        baseline[1] = 0;
        if (baseline.LengthSq() < 900) return false;
        vector da = BearingDirection(a.Bearing);
        vector db = BearingDirection(b.Bearing);
        float denominator = da[0] * db[2] - da[2] * db[0];
        if (Math.AbsFloat(denominator) < 0.0872) return false;
        vector delta = b.Origin - a.Origin;
        float alongA = (delta[0] * db[2] - delta[2] * db[0]) / denominator;
        float alongB = (delta[0] * da[2] - delta[2] * da[0]) / denominator;
        if (alongA <= 0 || alongB <= 0 || alongA > maximumDistance || alongB > maximumDistance) return false;
        estimate = a.Origin + da * alongA;
        return FiniteVector(estimate);
    }
}
