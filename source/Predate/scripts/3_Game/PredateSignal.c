// Shared schedule and sampling rules for human and AI receivers.
class PredateSignatureActivity
{
    float Energy;

    void RecordShots(int count)
    {
        Energy = Math.Clamp(Energy + Math.Clamp(count, 0, 32) * 0.25, 0, 1);
    }

    void RecordExplosions(int count)
    {
        Energy = Math.Clamp(Energy + Math.Clamp(count, 0, 8) * 0.5, 0, 1);
    }

    void RecordEngineUse(float seconds)
    {
        if (!PredateInformationMath.Finite(seconds) || seconds <= 0) return;
        Energy = Math.Clamp(Energy + Math.Min(seconds, 5) * 0.02, 0, 1);
    }

    void Advance(float seconds)
    {
        Energy = Math.Clamp(Energy - Math.Max(0, seconds) * 0.005, 0, 1);
    }
}

class PredateSignalTrack
{
    string OwnerId;
    string SubjectId;
    bool Selected;
    bool EverLocked;
    float NextPulseAt;
    int NextSampleSerial = 1;
    vector BroadcastCenter;
    float BroadcastRadius;
    float BroadcastTime;
    int PlayerPulseFormat;
    ref PredateObservation LastSample;
    ref array<ref PredateObservation> History = new array<ref PredateObservation>;

    void NormalizeLoaded()
    {
        if (History && History.Count() > 1 && History.Count() <= 4)
        {
            array<ref PredateObservation> chronological = new array<ref PredateObservation>;
            foreach (PredateObservation historical : History)
            {
                if (!historical) return; // Validation rejects malformed history.
                int position;
                while (position < chronological.Count() && chronological[position].ObservedAt <= historical.ObservedAt) position++;
                if (position == chronological.Count()) chronological.Insert(historical);
                else chronological.InsertAt(historical, position);
            }
            History = chronological;
        }
        // The engine writes a null script reference as {} and loads it as a
        // default object. Only that exact empty representation means absent.
        PredateObservation sample = LastSample;
        if (!sample) return;
        if (sample.MapCenter != vector.Zero || sample.MapRadius != 0) return;
        if (sample.Serial == 0 && sample.EventKey == "" && sample.ObserverId == "" && sample.SubjectId == "" && sample.Source == 0 && sample.ObservedAt == 0 && sample.ExpiresAt == 0 && sample.Confidence == 0 && sample.Magnitude == 0 && !sample.HasPosition && !sample.HasBearing && sample.Estimate == vector.Zero && sample.Origin == vector.Zero && sample.Radius == 0 && sample.Bearing == 0 && sample.BearingError == 0)
            LastSample = null;
    }

    void SkipUnselected(float now, float interval)
    {
        if (interval <= 0 || !EverLocked || Selected || now < NextPulseAt) return;
        NextPulseAt += (Math.Floor((now - NextPulseAt) / interval) + 1) * interval;
    }

    void SetSelected(bool selected, float now, float interval)
    {
        if (selected && !EverLocked)
        {
            EverLocked = true;
            NextPulseAt = now;
        }
        else if (selected && !Selected) SkipUnselected(now, interval);
        Selected = selected;
    }

    bool Due(float now) { return Selected && EverLocked && now >= NextPulseAt; }

    void SetPlayerTracking(bool selected)
    {
        Selected = selected;
        if (selected) EverLocked = true;
    }

    void UseBearingSectors(float now)
    {
        if (PlayerPulseFormat == 1) return;
        PlayerPulseFormat = 1;
        BroadcastCenter = vector.Zero;
        BroadcastRadius = 0;
        BroadcastTime = 0;
        NextPulseAt = now;
        LastSample = null;
        History.Clear();
    }

    bool Consume(float now, float interval, PredateObservation observation)
    {
        if (interval <= 0 || !Due(now)) return false;
        if (observation && (!observation.Valid(OwnerId, now) || observation.SubjectId != SubjectId || observation.Source != PredateObservationSource.SIGNAL || observation.ObservedAt != now)) return false;
        NextPulseAt += (Math.Floor((now - NextPulseAt) / interval) + 1) * interval;
        RecordSample(observation);
        return true;
    }

    void RecordSample(PredateObservation observation)
    {
        NextSampleSerial++;
        LastSample = null;
        if (!observation) return; // Close approach consumes the scheduled slot.
        LastSample = observation.Copy();
        PredateObservation historical = observation.Copy();
        History.Insert(historical);
        while (History.Count() > 4) History.RemoveOrdered(0);
    }

    bool Validate(float now)
    {
        if (OwnerId == "" || SubjectId == "" || OwnerId == SubjectId || NextSampleSerial < 1) return false;
        if (!PredateInformationMath.Finite(NextPulseAt) || NextPulseAt < 0 || !History || History.Count() > 4) return false;
        if (Selected && !EverLocked) return false;
        if (PlayerPulseFormat < 0 || PlayerPulseFormat > 1) return false;
        if (PlayerPulseFormat == 1 && (OwnerId != "player-01" || BroadcastRadius != 0)) return false;
        if (BroadcastRadius != 0)
        {
            if (OwnerId != "player-01" || !PredatePulseArea.Valid(BroadcastCenter, BroadcastRadius)) return false;
            if (!PredateInformationMath.Finite(BroadcastTime) || BroadcastTime < 0 || BroadcastTime > now) return false;
        }
        if (LastSample && (!LastSample.Valid(OwnerId, now) || LastSample.SubjectId != SubjectId || LastSample.Source != PredateObservationSource.SIGNAL)) return false;
        foreach (PredateObservation observation : History)
            if (!observation || !observation.Valid(OwnerId, now) || observation.SubjectId != SubjectId || observation.Source != PredateObservationSource.SIGNAL) return false;
        return true;
    }
}

class PredatePulseArea
{
    string OperatorId;
    vector Center;
    float Radius;
    float Age;

    static bool Valid(vector center, float radius)
    {
        return PredateInformationMath.FiniteVector(center) && center[1] == 0 && PredateInformationMath.Finite(radius) && radius > 0;
    }

    // All positions in a cell share one center. Repeated pulses in that cell
    // cannot be averaged into a finer fix. The circle covers the whole cell.
    static vector CoarseCenter(vector sample, float radius)
    {
        float side = radius * 1.25;
        return Vector((Math.Floor(sample[0] / side) + 0.5) * side, 0, (Math.Floor(sample[2] / side) + 0.5) * side);
    }
}

class PredateBearingPulse
{
    static const int DEGREES = 15;
    static const float INTERVAL = 90;
    static const float REACH = 5000;
    string OperatorId;
    int Start;

    static bool Valid(int start) { return start >= 0 && start < 360 && start % DEGREES == 0; }

    static int Sector(vector origin, vector target)
    {
        float heading = PredateMath.CameraHeading(target - origin);
        return Math.Floor(heading / DEGREES) * DEGREES;
    }

    static string RangeText(int start)
    {
        return PredateMath.BearingDigits(start) + "–" + PredateMath.BearingDigits(start + DEGREES) + "°";
    }
}

class PredateSignalRules
{
    static bool WithinFinalApproach(vector observer, vector subject, float radius)
    {
        vector delta = subject - observer;
        delta[1] = 0;
        return delta.LengthSq() < radius * radius;
    }

    static PredateObservation PlayerReading(PredateSignalTrack track, vector origin, vector targetTruth, float now, PredateConfig config)
    {
        if (!PredateInformationMath.FiniteVector(origin) || !PredateInformationMath.FiniteVector(targetTruth)) return null;
        vector difference = targetTruth - origin;
        difference[1] = 0;
        if (difference.LengthSq() > PredateBearingPulse.REACH * PredateBearingPulse.REACH) return null;
        PredateObservation observation = Reading(track, origin, targetTruth, now, PredateBearingPulse.INTERVAL, config, 0, 0);
        if (!observation) return null;
        observation.Bearing = PredateBearingPulse.Sector(origin, targetTruth);
        observation.BearingError = PredateBearingPulse.DEGREES / 2.0;
        observation.MapCenter = vector.Zero;
        observation.MapRadius = 0;
        return observation;
    }

    // Bearings remain private to each receiver. Only the human receives the
    // coarse map area, sampled once at the same instant as the bearing.
    static PredateObservation Sample(PredateSignalTrack track, vector origin, vector targetTruth, float now, PredateConfig config, float energy, float randomUnit)
    {
        if (!track || !track.Due(now)) return null;
        return Reading(track, origin, targetTruth, now, config.PulseInterval, config, energy, randomUnit);
    }

    static PredateObservation Reading(PredateSignalTrack track, vector origin, vector targetTruth, float now, float interval, PredateConfig config, float energy, float randomUnit)
    {
        if (!PredateInformationMath.FiniteVector(origin) || !PredateInformationMath.FiniteVector(targetTruth)) return null;
        if (WithinFinalApproach(origin, targetTruth, config.FinalApproachRadius)) return null;
        float strength = Math.Clamp(energy, 0, 1);
        float error = Math.Max(0, config.BearingErrorDegrees * (1 - 0.5 * strength));
        int center = PredateMath.Bearing(origin, targetTruth);
        int jitter = Math.Round(Math.Clamp(randomUnit, -1, 1) * error);
        int bearing = (center + jitter + 360) % 360;
        PredateObservation observation = new PredateObservation;
        observation.EventKey = "signal-" + track.OwnerId + "-" + track.NextSampleSerial;
        observation.ObserverId = track.OwnerId;
        observation.SubjectId = track.SubjectId;
        observation.Source = PredateObservationSource.SIGNAL;
        observation.ObservedAt = now;
        observation.ExpiresAt = now + interval * 2;
        observation.Confidence = 0.65 + 0.2 * strength;
        observation.HasBearing = true;
        observation.Origin = origin;
        observation.Bearing = bearing;
        // Include the half-degree rounding of the underlying true bearing.
        observation.BearingError = Math.Ceil(error) + 0.5;
        if (track.OwnerId == "player-01" && config.FinalApproachRadius > 0)
        {
            observation.MapRadius = config.FinalApproachRadius;
            observation.MapCenter = PredatePulseArea.CoarseCenter(targetTruth, observation.MapRadius);
        }
        return observation;
    }
}
