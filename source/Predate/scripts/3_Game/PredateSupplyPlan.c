enum PredateSupplyKind
{
    NONE,
    MAGAZINE,
    AMMUNITION,
    QUANTITY,
    SINGLE,
    FUELLED_ITEM
}

class PredateSupplyEntry
{
    string ClassName;
    int Kind;
    float Units;
    int LiquidType;
}

class PredateSupplyPlan
{
    static const float INTERVAL = 1800;
    static const int MAX_ENTRIES = 128;
    float NextAt;
    ref array<ref PredateSupplyEntry> Entries = new array<ref PredateSupplyEntry>;

    static float NextBoundary(float start, float now)
    {
        return start + (Math.Floor(Math.Max(0, now - start) / INTERVAL) + 1) * INTERVAL;
    }

    PredateSupplyEntry Find(string name, int liquidType = 0)
    {
        foreach (PredateSupplyEntry entry : Entries)
            if (entry.ClassName == name && entry.LiquidType == liquidType) return entry;
        return null;
    }

    void Add(string name, int kind, float units, int liquidType = 0)
    {
        if (units <= 0) return;
        PredateSupplyEntry entry = Find(name, liquidType);
        if (!entry)
        {
            if (Entries.Count() >= MAX_ENTRIES) return;
            entry = new PredateSupplyEntry;
            entry.ClassName = name;
            entry.Kind = kind;
            entry.LiquidType = liquidType;
            Entries.Insert(entry);
        }
        entry.Units += units;
    }

    bool Valid(float now)
    {
        if (!PredateInformationMath.Finite(NextAt) || NextAt < 0 || NextAt > now + INTERVAL + 0.01) return false;
        if (!Entries || Entries.Count() > MAX_ENTRIES || (NextAt == 0 && Entries.Count() > 0)) return false;
        foreach (PredateSupplyEntry entry : Entries)
        {
            if (!entry || entry.ClassName == "" || entry.Kind < PredateSupplyKind.MAGAZINE || entry.Kind > PredateSupplyKind.FUELLED_ITEM) return false;
            if (!PredateInformationMath.Finite(entry.Units) || entry.Units <= 0 || entry.Units > 10000000 || entry.LiquidType < 0) return false;
        }
        return true;
    }
}
