// An Operator's private view. The only positions here are its own sample
// origins and locations it could observe or infer from those samples.
class PredateHuntReceiver
{
    string OwnerId;
    float NextPulseAt;
    ref array<ref PredateSignalTrack> Tracks = new array<ref PredateSignalTrack>;
    string TargetId;
    string DeferredId;
    float DeferredUntil;
    float ProgressAt;
    float BestDistance;
    float BestUncertainty;
    float LastDecisionAt;
    bool Replanned;
    [NonSerialized()]
    protected ref map<string, PredateSignalTrack> m_TrackLookup;
    [NonSerialized()]
    protected ref map<string, ref PredateHuntSectorGeometry> m_Sectors;

    protected void IndexTracks()
    {
        if (m_TrackLookup && m_TrackLookup.Count() == Tracks.Count()) return;
        if (!m_TrackLookup) m_TrackLookup = new map<string, PredateSignalTrack>;
        else m_TrackLookup.Clear();
        foreach (PredateSignalTrack track : Tracks)
            m_TrackLookup.Insert(track.SubjectId, track);
    }

    PredateSignalTrack Find(string subject)
    {
        if (subject == "" || !Tracks.Count()) return null;
        IndexTracks();
        return m_TrackLookup.Get(subject);
    }

    void AddTrack(PredateSignalTrack track)
    {
        IndexTracks();
        Tracks.Insert(track);
        m_TrackLookup.Insert(track.SubjectId, track);
    }

    PredateHuntSectorGeometry SectorGeometry(PredateObservation reading)
    {
        if (!m_Sectors) m_Sectors = new map<string, ref PredateHuntSectorGeometry>;
        PredateHuntSectorGeometry sector = m_Sectors.Get(reading.SubjectId);
        if (!sector)
        {
            sector = new PredateHuntSectorGeometry;
            m_Sectors.Insert(reading.SubjectId, sector);
        }
        // A recorded observation is immutable. New readings replace its reference.
        if (sector.Reading != reading)
        {
            sector.Reading = reading;
            sector.Center = PredateHuntSelection.SectorCenter(reading);
            sector.BaseRadius = PredateHuntSelection.SectorRadius(reading, reading.ObservedAt);
        }
        return sector;
    }

    void Retire(string subject)
    {
        PredateSignalTrack track = Find(subject);
        if (track)
        {
            Tracks.RemoveItem(track);
            m_TrackLookup.Remove(subject);
        }
        if (m_Sectors) m_Sectors.Remove(subject);
        if (TargetId == subject) TargetId = "";
        if (DeferredId == subject) DeferredId = "";
    }

    bool Valid(float now, string owner)
    {
        if (OwnerId == "") return Tracks && Tracks.Count() == 0 && TargetId == "";
        if (OwnerId != owner || !Tracks || Tracks.Count() > PredateProtocol.MAX_OPERATORS) return false;
        if (!ValidTime(NextPulseAt) || !ValidTime(DeferredUntil) || !ValidTime(ProgressAt) || !ValidTime(LastDecisionAt)) return false;
        if (ProgressAt > now || LastDecisionAt > now || !ValidTime(BestDistance) || !ValidTime(BestUncertainty)) return false;
        if (TargetId != "" && !KnownSubject(TargetId)) return false;
        if (DeferredId != "" && !KnownSubject(DeferredId)) return false;
        array<string> seen = {};
        foreach (PredateSignalTrack track : Tracks)
        {
            if (!track || !KnownSubject(track.SubjectId) || track.OwnerId != owner || seen.Find(track.SubjectId) >= 0) return false;
            track.NormalizeLoaded();
            if (!track.Validate(now)) return false;
            foreach (PredateObservation reading : track.History)
                if (reading.DistanceBand == 0) return false;
            if (track.LastSample && track.LastSample.DistanceBand == 0) return false;
            seen.Insert(track.SubjectId);
        }
        return true;
    }

    bool KnownSubject(string id)
    {
        return id != OwnerId && (id == "player-01" || PredateProtocol.OperatorSlot(id) >= 0);
    }

    static bool ValidTime(float value)
    {
        return PredateInformationMath.Finite(value) && value >= 0;
    }
}

class PredateHuntSectorGeometry
{
    PredateObservation Reading;
    vector Center;
    float BaseRadius;
}

class PredateHuntLead
{
    string Id;
    PredateObservation Evidence;
    float Distance;
    float Uncertainty;
}

class PredateHuntSelection
{
    static vector SectorCenter(PredateObservation sample)
    {
        float range = (sample.DistanceBand - 0.5) * sample.DistanceBandMetres();
        float center = PredateMath.WrapHeading(sample.Bearing + PredateBearingPulse.DEGREES / 2.0);
        return sample.Origin + PredateInformationMath.BearingDirection(center) * range;
    }

    static float SectorRadius(PredateObservation sample, float now)
    {
        int width = sample.DistanceBandMetres();
        float outer = sample.DistanceBand * width;
        return width / 2.0 + outer * Math.Sin(sample.BearingError * Math.DEG2RAD) + (now - sample.ObservedAt) * 1.5;
    }

    static void Score(PredateHuntReceiver receiver, PredateHuntLead lead, string id, PredateObservation evidence, vector own, float now)
    {
        PredateHuntSectorGeometry sector = receiver.SectorGeometry(evidence);
        lead.Id = id;
        lead.Evidence = evidence;
        lead.Distance = PredateHunterBrain.Distance(own, sector.Center);
        lead.Uncertainty = sector.BaseRadius + (now - evidence.ObservedAt) * 1.5;
    }

    static bool Closer(PredateHuntLead candidate, PredateHuntLead other, string current)
    {
        if (!other) return true;
        PredateObservation a = candidate.Evidence;
        PredateObservation b = other.Evidence;
        bool sameBand = a.DistanceBand == b.DistanceBand && a.DistanceBandMetres() == b.DistanceBandMetres();
        if (a.ObservedAt == b.ObservedAt && sameBand && PredateBearingPulse.Overlap(a.Bearing, b.Bearing))
        {
            if (a.OverlapLane != b.OverlapLane) return a.OverlapLane > b.OverlapLane;
        }
        if (Math.AbsFloat(candidate.Distance - other.Distance) > 25)
        {
            if (candidate.Distance + candidate.Uncertainty < other.Distance - other.Uncertainty) return true;
            if (other.Distance + other.Uncertainty < candidate.Distance - candidate.Uncertainty) return false;
            // Fresh, distinct distance bands give a useful ranking even where
            // their bearing wedges touch. Ambiguous estimates favour a held lock.
            if (a.ObservedAt == b.ObservedAt && !sameBand)
                return candidate.Distance < other.Distance;
        }
        if (candidate.Id == current || other.Id == current) return candidate.Id == current;
        if (a.ObservedAt != b.ObservedAt) return a.ObservedAt > b.ObservedAt;
        if (candidate.Uncertainty != other.Uncertainty) return candidate.Uncertainty < other.Uncertainty;
        return candidate.Distance < other.Distance;
    }

    static PredateHuntLead Update(PredateHuntReceiver receiver, PredateHunterState hunter, vector own, float now, bool interrupted, bool finalApproach = false)
    {
        float elapsed = now - receiver.LastDecisionAt;
        receiver.LastDecisionAt = now;
        if (interrupted)
        {
            receiver.ProgressAt = Math.Min(now, receiver.ProgressAt + Math.Max(0, elapsed));
            return null;
        }
        if (finalApproach && !hunter.FinalApproach) receiver.ProgressAt = now;
        bool searchExhausted = (!finalApproach || hunter.FinalApproach) && hunter.Reason == "Bounded search exhausted; no new knowledge";
        if (finalApproach && !searchExhausted && now - receiver.ProgressAt < 2 * PredateBearingPulse.INTERVAL)
            return null;
        PredateHuntLead current;
        PredateSignalTrack tracked = receiver.Find(receiver.TargetId);
        if (!finalApproach && tracked && tracked.LastSample && !tracked.LastSample.Expired(now))
        {
            current = new PredateHuntLead;
            Score(receiver, current, tracked.SubjectId, tracked.LastSample, own, now);
        }
        bool searchActive = (hunter.Mode == "search" || hunter.Mode == "investigate" || hunter.Mode == "approach") && hunter.HasGoal && hunter.SearchUntil > now;
        if (current && (current.Distance + 50 < receiver.BestDistance || current.Uncertainty + 50 < receiver.BestUncertainty))
        {
            receiver.ProgressAt = now;
            receiver.BestDistance = current.Distance;
            receiver.BestUncertainty = current.Uncertainty;
            receiver.Replanned = false;
        }
        float stalledFor = now - receiver.ProgressAt;
        bool stalled = receiver.TargetId != "" && (stalledFor >= 2 * PredateBearingPulse.INTERVAL || searchExhausted);
        PredateHuntLead chosen;
        PredateHuntLead candidate;
        foreach (PredateSignalTrack track : receiver.Tracks)
        {
            string subject = track.SubjectId;
            if ((stalled || finalApproach) && subject == receiver.TargetId) continue;
            if (subject == receiver.DeferredId && now < receiver.DeferredUntil) continue;
            PredateObservation sample = track.LastSample;
            if (!sample || sample.Expired(now)) continue;
            if (!candidate) candidate = new PredateHuntLead;
            if (current && sample == current.Evidence)
            {
                candidate.Id = current.Id;
                candidate.Evidence = sample;
                candidate.Distance = current.Distance;
                candidate.Uncertainty = current.Uncertainty;
            }
            else Score(receiver, candidate, subject, sample, own, now);
            if (Closer(candidate, chosen, receiver.TargetId))
            {
                PredateHuntLead previous = chosen;
                chosen = candidate;
                candidate = previous;
            }
        }
        if (!chosen && finalApproach) return null;
        if (!chosen && (searchActive || current))
        {
            // Keep working a credible area when the receiver offers no alternate.
            chosen = current;
            if (!chosen) return null;
        }
        string selected;
        if (chosen) selected = chosen.Id;
        if (receiver.TargetId != selected)
        {
            if (stalled)
            {
                receiver.DeferredId = receiver.TargetId;
                receiver.DeferredUntil = now + PredateBearingPulse.INTERVAL;
            }
            receiver.TargetId = selected;
            receiver.ProgressAt = now;
            receiver.Replanned = false;
            hunter.FinalApproach = false;
            if (chosen)
            {
                receiver.BestDistance = chosen.Distance;
                receiver.BestUncertainty = chosen.Uncertainty;
            }
            PredateHunterBrain.Hold(hunter, "hold", "Chosen contact changed");
            hunter.LastSignalKey = "";
            hunter.LastContactKey = "";
        }
        else if (!receiver.Replanned && chosen && stalledFor >= PredateBearingPulse.INTERVAL)
        {
            receiver.Replanned = true;
            PredateHunterBrain.Hold(hunter, "hold", "Try a new approach to the selected contact");
            hunter.LastSignalKey = "";
            hunter.LastContactKey = "";
        }
        return chosen;
    }
}
