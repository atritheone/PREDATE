class PredateFootprintTests
{
    int Checks;
    int Failures;

    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateFootprintTest] FAIL: " + label);
    }

    void Run()
    {
        PredateFootprintLedger ledger = new PredateFootprintLedger;
        vector site = "4500 0 6500";
        Check(ledger.Record(site, PredateFootprintSurface.HARD, 0) == 0, "hard ground leaves no mark");
        int serial = ledger.Record(site, PredateFootprintSurface.EARTH, 5);
        ref array<ref PredateFootprint> nearby = new array<ref PredateFootprint>;
        ledger.Nearby(site + "13 0 0", 5, nearby);
        Check(nearby.Count() == 0, "nearby scan excludes distant marks");
        ledger.Nearby(site, 4, nearby);
        Check(nearby.Count() == 0, "nearby scan excludes future timestamps");
        ledger.Nearby(site, 5, nearby);
        bool found = nearby.Count() == 1 && nearby[0].Serial == serial;
        Check(found, "nearby scan finds a new mark on valid terrain");
        if (!found) { PrintResult(); return; }
        PredateFootprint mark = nearby[0];
        Check(!PredateFootprintLedger.Observe(mark, "raven-01", site, 5, false), "occluded mark cannot enter observation memory");
        PredateObservation first = PredateFootprintLedger.Observe(mark, "raven-01", site, 5, true);
        bool anonymous = first && first.Valid("raven-01", 5) && first.SubjectId == "" && first.HasPosition && !first.HasBearing;
        Check(anonymous, "visible mark yields an anonymous observation");
        PredateObservation older = PredateFootprintLedger.Observe(mark, "lynx-01", site, 90, true);
        Check(first && older && first.ObserverId != older.ObserverId && older.Confidence < first.Confidence && older.Radius > first.Radius && PredateFootprintLedger.Notice(older) == "Enemy Tracks", "each observer gets its own clue and old marks use the Enemy Tracks notice");
        ledger.Nearby(site, 125, nearby);
        Check(nearby.Count() == 0, "expired mark cannot be rediscovered");
        ledger.Prune(125);
        int trail = ledger.Record(site, PredateFootprintSurface.SAND, 125, "player-01", 46, 0, true);
        ledger.Record(site + "0 0 4", PredateFootprintSurface.EARTH, 126, "player-01", 45, trail, true);
        ledger.Nearby(site, 126, nearby);
        PredateObservation headed = PredateFootprintLedger.Observe(nearby[0], "raven-01", site, 126, true);
        Check(headed && headed.Valid("raven-01", 126) && headed.Bearing == 46 && PredateFootprintLedger.Notice(headed) == "Recent Tracks · Headed 46", "the notice and the observer retain the exact track heading");
        Check(ledger.Read(nearby[0], "raven-01", 126) && !ledger.Read(nearby[1], "raven-01", 126) && ledger.Read(nearby[1], "lynx-01", 126) && !ledger.Read(nearby[0], "player-01", 126), "only the first foreign mark in a passage is read per observer");

        PredateObservationMemory memory = new PredateObservationMemory;
        memory.OwnerId = "raven-01";
        memory.Add(headed, 126);
        PredateHunterState hunter = new PredateHunterState;
        hunter.OwnerId = "raven-01";
        PredateHunterBrain.Step(hunter, site - "0 0 4", 126, memory, null, false, false);
        Check(hunter.HasGoal && hunter.Goal[0] > site[0] && hunter.Goal[2] > site[2], "reading northeast tracks biases the AI search northeast");

        ledger = new PredateFootprintLedger;
        for (int markIndex = 0; markIndex <= PredateFootprintLedger.CELL_LIMIT; markIndex++)
            ledger.Record(site, PredateFootprintSurface.SNOW, 125);
        ledger.Nearby(site, 125, nearby);
        Check(nearby.Count() == PredateFootprintLedger.CELL_LIMIT && ledger.Validate(125), "local limits bound storage without erasing other areas");
        ledger.Prune(305);
        ledger.Nearby(site, 305, nearby);
        Check(nearby.Count() == 0, "all snow marks age out at their configured lifetime");
        PrintResult();
    }

    void PrintResult()
    {
        Print("[PredateFootprintTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }
}
