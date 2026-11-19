class PredateHunterTests
{
    int Checks;
    int Failures;

    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateHunterTest] FAIL: " + label);
    }

    void Run()
    {
        PredateHunterState state = new PredateHunterState;
        PredateObservationMemory memory = new PredateObservationMemory;
        memory.OwnerId = "raven-01";
        PredateSignalTrack track = PredateSignalService.Track("raven-01", "player-01");
        vector own = "5000 0 5000";
        PredateHunterBrain.Step(state, own, 0, memory, track, false, false);
        Check(!state.HasGoal && state.Mode == "hold", "no evidence provides no invented destination");
        PredateConfig config = new PredateConfig;
        config.BearingErrorDegrees = 0;
        track.SetSelected(true, 1, 60);
        PredateObservation signal = PredateSignalRules.Sample(track, own, "5000 0 5500", 1, config, 0, 0);
        track.Consume(1, 60, signal);
        PredateHunterBrain.Step(state, own, 1, memory, track, false, false);
        Check(state.Mode == "observe" && state.HasGoal && state.Goal == "5080 0 5060", "first ray creates lateral baseline instead of a target dot");
        Check(state.EvidenceKey == signal.EventKey && state.GoalRevision == 1, "route records its actual evidence and revision");
        PredateHunterState independent = new PredateHunterState;
        PredateHunterBrain.Step(independent, own, 1, memory, track, false, false);
        Check(independent.Goal == state.Goal && independent.Mode == state.Mode, "identical private evidence produces identical intent");
        PredateHunterBrain.Step(state, own, 2, memory, track, false, false);
        Check(state.GoalRevision == 1, "ordinary ticks cannot refresh an evidence-derived route");
        PredateHunterBrain.Step(state, state.Goal, 3, memory, track, false, false);
        Check(!state.HasGoal && state.Mode == "hold", "baseline arrival waits for next real pulse");
        signal = PredateSignalRules.Sample(track, "5080 0 5060", "5000 0 5500", 61, config, 0, 0);
        track.Consume(61, 60, signal);
        PredateHunterBrain.Step(state, "5080 0 5060", 61, memory, track, false, false);
        Check(state.HasGoal && state.Mode == "approach" && PredateHunterBrain.Distance(state.SearchCenter, "5000 0 5500") < 20, "independent historical rays create an estimated search area");
        Check(state.SearchRadius >= 90 && state.SearchRadius <= 500 && PredateHunterBrain.Distance(state.Goal, state.SearchCenter) > 20, "travel uncertainty retained and approach stops short of center");
        PredateHunterState area = new PredateHunterState;
        area.SearchCenter = "5000 0 5500";
        area.SearchRadius = 80;
        area.SearchObservedAt = 61;
        area.SearchUntil = 280;
        PredateHunterBrain.Goal(area, own, "5000 0 5460", 61, "approach", "two historical rays", "earlier-fix");
        PredateSignalTrack parallel = PredateSignalService.Track("raven-01", "player-01");
        parallel.SetSelected(true, 61, 60);
        PredateObservation ray = PredateSignalRules.Sample(parallel, "5000 0 5200", "5000 0 5500", 61, config, 0, 0);
        parallel.Consume(61, 60, ray);
        ray = PredateSignalRules.Sample(parallel, "5000 0 5300", "5000 0 5500", 121, config, 0, 0);
        parallel.Consume(121, 60, ray);
        PredateHunterBrain.Step(area, own, 121, memory, parallel, false, false);
        Check(area.Mode == "approach" && area.GoalRevision == 1, "compatible parallel ray retains useful uncertain area and route");
        Check(area.SearchRadiusAt(121) == 170, "derived area uncertainty grows between independent observations");
        ray = PredateSignalRules.Sample(parallel, "5000 0 5300", "4500 0 5300", 181, config, 0, 0);
        parallel.Consume(181, 60, ray);
        PredateHunterBrain.Step(area, own, 181, memory, parallel, false, false);
        Check(area.Mode == "observe" && area.GoalRevision == 2, "contradictory new ray can invalidate the previous approach");
        PredateHunterState evasion = new PredateHunterState;
        PredateSignalTrack warnedTrack = PredateSignalService.Track("raven-01", "player-01");
        warnedTrack.SetSelected(true, 1, 60);
        ray = PredateSignalRules.Sample(warnedTrack, own, "5000 0 5500", 1, config, 0, 0);
        warnedTrack.Consume(1, 60, ray);
        PredateHunterBrain.Step(evasion, own, 1, memory, warnedTrack, false, false, true);
        Check(evasion.Mode == "evade" && evasion.HasGoal && evasion.EvidenceKey == "", "warning changes the route without supplying a hunter position");
        PredateHunterState noSignal = new PredateHunterState;
        PredateHunterBrain.Step(noSignal, own, 1, memory, null, false, false, true);
        int warningRoute = noSignal.GoalRevision;
        PredateHunterBrain.Step(noSignal, own, 3, memory, null, false, false, true);
        Check(noSignal.Mode == "evade" && noSignal.HasGoal && noSignal.GoalRevision == warningRoute, "warning also prompts one reposition during a quiet receiver window");
        PredateHunterBrain.Step(state, own, 62, memory, track, true, false);
        Check(!state.HasGoal && state.Mode == "combat", "lawful combat yields strategic movement to native tactics");
        PredateHunterBrain.Step(state, own, 63, memory, track, false, true);
        Check(!state.HasGoal && state.Mode == "recover", "own medical/action priority yields movement");
        PredateHunterBrain.Step(state, own, 200, memory, track, false, false);
        Check(!state.HasGoal, "expired signals cannot restart hunting after tactical interruption");
        PredateHunterState blocked = new PredateHunterState;
        PredateSignalTrack emptyTrack = PredateSignalService.Track("raven-01", "player-01");
        blocked.SearchUntil = 120;
        PredateHunterBrain.Goal(blocked, own, "5100 0 5000", 0, "observe", "fixture route", "fixture");
        PredateHunterBrain.Step(blocked, own, 21, memory, emptyTrack, false, false);
        Check(blocked.RouteFailures == 1 && blocked.GoalRevision == 2, "measured twenty-second stall requests one local route alternative");
        Check(PredateHunterBrain.Distance(own, blocked.Goal) < 30, "detour is normalized and remains local");
        PredateHunterBrain.Step(blocked, own, 42, memory, emptyTrack, false, false);
        PredateHunterBrain.Step(blocked, own, 63, memory, emptyTrack, false, false);
        Check(blocked.RouteFailures == 3 && !blocked.HasGoal && blocked.Mode == "reassess", "repeated blockage safely holds without teleportation");
        PredateHunterState search = new PredateHunterState;
        search.SearchCenter = own;
        search.SearchRadius = 50;
        search.SearchUntil = 120;
        PredateHunterBrain.Goal(search, own, own, 0, "investigate", "fixture observation", "sound-1");
        for (int i = 0; i < 5; i++)
        {
            vector position = search.Goal;
            PredateHunterBrain.Step(search, position, i + 1, memory, emptyTrack, false, false);
        }
        Check(!search.HasGoal && search.SearchStep == 4, "remembered area search is bounded");
        PredateObservation sound = new PredateObservation;
        sound.ObserverId = "raven-01";
        sound.EventKey = "sound-2";
        sound.Source = PredateObservationSource.AUDIBLE;
        sound.ObservedAt = 210;
        sound.ExpiresAt = 240;
        sound.HasPosition = true;
        sound.Estimate = "5100 0 5100";
        sound.Radius = 20;
        sound.Confidence = 0.5;
        memory.Add(sound, 210);
        PredateHunterBrain.Step(search, own, 210, memory, track, false, false);
        Check(search.Mode == "investigate" && search.EvidenceKey == "sound-2" && sound.SubjectId == "", "unidentified sound prompts area investigation without identity attribution");
        PredateHunterBrain.Step(search, own, 241, memory, track, false, false);
        Check(!search.HasGoal, "observation expiry cancels unfinished travel/search");
        string error;
        Check(JsonFileLoader<PredateHunterState>.SaveFile("$profile:HunterRoundTrip.json", search, error), "hunter native save");
        PredateHunterState restored = new PredateHunterState;
        Check(JsonFileLoader<PredateHunterState>.LoadFile("$profile:HunterRoundTrip.json", restored, error) && restored.Validate(241), "hunter native reload validates");
        Check(restored.GoalRevision == search.GoalRevision && restored.LastContactKey == search.LastContactKey && restored.NextDecisionAt == search.NextDecisionAt && restored.SearchStep == search.SearchStep, "reload retains intent, evidence, decisions and search progress");
        restored.OwnerId = "other-operator";
        Check(!restored.Validate(241), "another operator cannot inherit saved hunter state");
        Print("[PredateHunterTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }
}
