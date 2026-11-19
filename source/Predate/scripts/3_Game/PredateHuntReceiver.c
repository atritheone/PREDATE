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
    string FinalSearchKey;

    PredateSignalTrack Find(string subject)
    {
        foreach (PredateSignalTrack track : Tracks)
            if (track.SubjectId == subject) return track;
        return null;
    }

    void Retire(string subject)
    {
        PredateSignalTrack track = Find(subject);
        if (track) Tracks.RemoveItem(track);
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
        return id != OwnerId && (id == "player-01" || PredateProtocol.OperatorBit(id) != 0);
    }

    static bool ValidTime(float value)
    {
        return PredateInformationMath.Finite(value) && value >= 0;
    }
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
        float range = (sample.DistanceBand - 0.5) * PredateBearingPulse.BAND_METRES;
        float center = PredateMath.WrapHeading(sample.Bearing + PredateBearingPulse.DEGREES / 2.0);
        return sample.Origin + PredateInformationMath.BearingDirection(center) * range;
    }

    static float SectorRadius(PredateObservation sample, float now)
    {
        float outer = sample.DistanceBand * PredateBearingPulse.BAND_METRES;
        return PredateBearingPulse.BAND_METRES / 2.0 + outer * Math.Sin(sample.BearingError * Math.DEG2RAD) + (now - sample.ObservedAt) * 1.5;
    }

    static void Offer(map<string, ref PredateHuntLead> leads, string id, PredateObservation evidence, vector own, float now)
    {
        PredateHuntLead previous;
        if (leads.Find(id, previous) && previous.Evidence.ObservedAt > evidence.ObservedAt) return;
        PredateHuntLead lead = new PredateHuntLead;
        lead.Id = id;
        lead.Evidence = evidence;
        lead.Distance = PredateHunterBrain.Distance(own, SectorCenter(evidence));
        lead.Uncertainty = SectorRadius(evidence, now);
        leads.Set(id, lead);
    }

    static bool Closer(PredateHuntLead candidate, PredateHuntLead other, string current)
    {
        if (!other) return true;
        PredateObservation a = candidate.Evidence;
        PredateObservation b = other.Evidence;
        if (a.ObservedAt == b.ObservedAt && a.DistanceBand == b.DistanceBand && PredateBearingPulse.Overlap(a.Bearing, b.Bearing))
        {
            if (a.OverlapLane != b.OverlapLane) return a.OverlapLane > b.OverlapLane;
        }
        if (Math.AbsFloat(candidate.Distance - other.Distance) > 25)
        {
            if (candidate.Distance + candidate.Uncertainty < other.Distance - other.Uncertainty) return true;
            if (other.Distance + other.Uncertainty < candidate.Distance - candidate.Uncertainty) return false;
            // Fresh, distinct distance bands give a useful ranking even where
            // their bearing wedges touch. Ambiguous estimates favour a held lock.
            if (a.ObservedAt == b.ObservedAt && a.DistanceBand != b.DistanceBand)
                return candidate.Distance < other.Distance;
        }
        if (candidate.Id == current || other.Id == current) return candidate.Id == current;
        if (a.ObservedAt != b.ObservedAt) return a.ObservedAt > b.ObservedAt;
        if (candidate.Uncertainty != other.Uncertainty) return candidate.Uncertainty < other.Uncertainty;
        return candidate.Distance < other.Distance;
    }

    static PredateHuntLead Update(PredateHuntReceiver receiver, PredateHunterState hunter, vector own, float now, bool interrupted)
    {
        float elapsed = now - receiver.LastDecisionAt;
        receiver.LastDecisionAt = now;
        if (interrupted)
        {
            receiver.ProgressAt = Math.Min(now, receiver.ProgressAt + Math.Max(0, elapsed));
            return null;
        }
        map<string, ref PredateHuntLead> leads = new map<string, ref PredateHuntLead>;
        foreach (PredateSignalTrack track : receiver.Tracks)
        {
            PredateObservation sample = track.LastSample;
            if (!sample && track.Reception == PredatePulseReception.CLOSE && track.SubjectId == receiver.TargetId && track.History.Count() > 0)
            {
                PredateObservation last = track.History[track.History.Count() - 1];
                if (last.DistanceBand <= 2) sample = last;
            }
            if (sample && !sample.Expired(now)) Offer(leads, track.SubjectId, sample, own, now);
        }
        PredateHuntLead current;
        leads.Find(receiver.TargetId, current);
        bool searchActive = (hunter.Mode == "search" || hunter.Mode == "investigate" || hunter.Mode == "approach") && hunter.HasGoal && hunter.SearchUntil > now;
        if (current && (current.Distance + 50 < receiver.BestDistance || current.Uncertainty + 50 < receiver.BestUncertainty))
        {
            receiver.ProgressAt = now;
            receiver.BestDistance = current.Distance;
            receiver.BestUncertainty = current.Uncertainty;
            receiver.Replanned = false;
        }
        float stalledFor = now - receiver.ProgressAt;
        bool stalled = receiver.TargetId != "" && (stalledFor >= 2 * PredateBearingPulse.INTERVAL || hunter.Reason == "Bounded search exhausted; no new knowledge");
        PredateHuntLead chosen;
        foreach (string subject, PredateHuntLead lead : leads)
        {
            if (stalled && subject == receiver.TargetId) continue;
            if (subject == receiver.DeferredId && now < receiver.DeferredUntil) continue;
            if (Closer(lead, chosen, receiver.TargetId)) chosen = lead;
        }
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
            receiver.FinalSearchKey = "";
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
