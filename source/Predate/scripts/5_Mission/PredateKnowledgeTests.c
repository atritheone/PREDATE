// Native Enforce tests. Runs only in explicit development/validation missions.
class PredateKnowledgeTests
{
    int Failures;
    int Checks;

    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateKnowledgeTest] FAIL: " + label);
    }

    PredateObservation Visual(string eventKey, float time, vector estimate)
    {
        PredateObservation observation = new PredateObservation;
        observation.EventKey = eventKey;
        observation.ObserverId = "raven-01";
        observation.SubjectId = "contact-a";
        observation.Source = PredateObservationSource.VISUAL;
        observation.ObservedAt = time;
        observation.ExpiresAt = time + 120;
        observation.Confidence = 0.9;
        observation.HasPosition = true;
        observation.Estimate = estimate;
        observation.Radius = 2;
        return observation;
    }

    PredateObservation Signal(string eventKey, vector origin, float bearing)
    {
        PredateObservation observation = new PredateObservation;
        observation.EventKey = eventKey;
        observation.ObserverId = "raven-01";
        observation.SubjectId = "contact-a";
        observation.Source = PredateObservationSource.SIGNAL;
        observation.ObservedAt = 10;
        observation.ExpiresAt = 130;
        observation.Confidence = 0.7;
        observation.HasBearing = true;
        observation.Origin = origin;
        observation.Bearing = bearing;
        observation.BearingError = 3;
        return observation;
    }

    void Run()
    {
        PredateObservationMemory memory = new PredateObservationMemory;
        memory.OwnerId = "raven-01";
        PredateObservation observation = Visual("visual-1", 10, "50 0 60");
        Check(memory.Add(observation, 10), "lawful visual evidence accepted");
        Check(!memory.Add(observation, 11), "replaying an event cannot refresh memory");
        observation.Estimate = "900 0 900";
        PredateObservation known = memory.Latest("contact-a", PredateObservationSource.VISUAL, 11);
        Check(known && known.Estimate == "50 0 60", "producer cannot mutate stored evidence");
        known.Estimate = "800 0 800";
        known = memory.Latest("contact-a", PredateObservationSource.VISUAL, 11);
        Check(known.Estimate == "50 0 60", "consumer receives a private copy");
        Check(known.RadiusAt(30) > known.RadiusAt(20), "uncertainty grows without new observations");
        Check(known.ConfidenceAt(30) < known.ConfidenceAt(20), "confidence decays without new observations");
        Check(!memory.Latest("unseen-contact", PredateObservationSource.VISUAL, 11), "unknown identity has no inferred location");
        PredateObservationMemory other = new PredateObservationMemory;
        other.OwnerId = "rook-02";
        Check(!other.Add(known, 11) && other.Entries.Count() == 0, "independent operators do not share observations");
        observation = Visual("older-visual", 9, "1 0 1");
        Check(!memory.Add(observation, 11), "older observation cannot replace a newer fix");
        observation = Visual("future-visual", 12, "1 0 1");
        Check(!memory.Add(observation, 11), "future observations rejected");
        observation = Visual("noise-1", 11, "20 0 20");
        observation.Source = PredateObservationSource.AUDIBLE;
        Check(!memory.Add(observation, 11), "a sound cannot assert a known shooter identity");
        observation.SubjectId = "";
        Check(memory.Add(observation, 11), "unidentified sound can be investigated");
        Check(!memory.Latest("contact-a", PredateObservationSource.AUDIBLE, 11), "unidentified noise does not become selected target evidence");
        PredateObservation a = Signal("signal-1", "0 0 0", 45);
        PredateObservation b = Signal("signal-2", "100 0 0", 315);
        Check(memory.Add(a, 11) && memory.Add(b, 11), "bearing history retains distinct observer origins");
        vector intersection;
        Check(PredateInformationMath.IntersectBearings(a, b, 1000, intersection) && vector.Distance(intersection, "50 0 50") < 0.01, "intersecting forward bearings give an estimated area");
        b.Bearing = 45;
        Check(!PredateInformationMath.IntersectBearings(a, b, 1000, intersection), "parallel bearings do not invent a fix");
        b.Bearing = 135;
        Check(!PredateInformationMath.IntersectBearings(a, b, 1000, intersection), "intersections behind the observer rejected");
        b.Bearing = 315;
        b.SubjectId = "someone-else";
        Check(!PredateInformationMath.IntersectBearings(a, b, 1000, intersection), "different signatures cannot be triangulated together");
        b.SubjectId = "contact-a";
        b.Origin = "0 100 0";
        Check(!PredateInformationMath.IntersectBearings(a, b, 1000, intersection), "vertical displacement is not a lateral baseline");
        string error;
        Check(JsonFileLoader<PredateObservationMemory>.SaveFile("$profile:KnowledgeRoundTrip.json", memory, error), "memory writes using native serializer");
        PredateObservationMemory loaded = new PredateObservationMemory;
        Check(JsonFileLoader<PredateObservationMemory>.LoadFile("$profile:KnowledgeRoundTrip.json", loaded, error) && loaded.Validate(11), "native JSON round trip validates");
        known = loaded.Latest("contact-a", PredateObservationSource.VISUAL, 11);
        Check(known.Estimate == "50 0 60" && known.ObservedAt == 10 && known.RadiusAt(30) == 32, "load preserves location, age and expanding uncertainty");
        loaded.Schema = 2;
        Check(!loaded.Validate(11), "future memory schema rejected");
        memory.Prune(130);
        Check(memory.Entries.Count() == 1, "expired evidence pruned independently");
        Check(!memory.Latest("contact-a", PredateObservationSource.VISUAL, 130), "expired visual contact is forgotten");
        memory.Prune(131);
        for (int i = 0; i < 80; i++)
        {
            observation = Visual("bounded-" + i, 140, "10 0 10");
            observation.SubjectId = "contact-" + i;
            memory.Add(observation, 140);
        }
        Check(memory.Entries.Count() == 32 && memory.Validate(140), "history remains bounded and valid");
        Print("[PredateKnowledgeTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }
}
