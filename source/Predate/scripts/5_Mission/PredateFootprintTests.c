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
        Check(first && older && first.ObserverId != older.ObserverId && older.Confidence < first.Confidence && older.Radius > first.Radius, "each observer gets its own clue and old marks carry more uncertainty");
        ledger.Nearby(site, 125, nearby);
        Check(nearby.Count() == 0, "expired mark cannot be rediscovered");
        ledger.Prune(125);
        int oldest = ledger.Record(site, PredateFootprintSurface.SAND, 125);
        for (int markIndex = 0; markIndex < PredateFootprintLedger.MAX_MARKS; markIndex++)
            ledger.Record(site, PredateFootprintSurface.SNOW, 125);
        ledger.Nearby(site, 125, nearby);
        Check(nearby.Count() == PredateFootprintLedger.MAX_MARKS && nearby[0].Serial > oldest && ledger.Validate(125), "footprint storage removes the oldest mark without reordering the remaining marks");
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
