enum PredateFootprintSurface
{
    HARD,
    EARTH,
    SAND,
    SNOW
}

class PredateFootprint
{
    int Serial;
    int Trail;
    string Creator;
    vector Position;
    int Heading;
    bool DirectionKnown;
    float CreatedAt;
    float ExpiresAt;
}

class PredateFootprintTrail
{
    int Id;
    float ExpiresAt;
    ref array<string> Readers = {};
}

class PredateFootprintLedger
{
    static const int MAX_MARKS = 16384;
    static const int CELL_LIMIT = 24;
    static const float CELL_SIZE = 16;
    static const float VIEW_DISTANCE = 8;
    int NextSerial = 1;
    ref array<ref PredateFootprint> Marks = {};
    ref array<ref PredateFootprintTrail> Trails = {};
    [NonSerialized()]
    ref map<string, ref array<ref PredateFootprint>> m_Cells;
    [NonSerialized()]
    ref map<int, ref PredateFootprintTrail> m_TrailIndex;
    [NonSerialized()]
    ref map<string, ref PredateFootprint> m_LastByCreator;
    [NonSerialized()]
    float m_NextPrune;

    static float Lifetime(int surface)
    {
        if (surface == PredateFootprintSurface.EARTH) return 120;
        if (surface == PredateFootprintSurface.SAND) return 90;
        if (surface == PredateFootprintSurface.SNOW) return 180;
        return 0;
    }

    static string CellKey(int x, int z) { return x.ToString() + ":" + z.ToString(); }

    void IndexMark(PredateFootprint mark)
    {
        string key = CellKey(Math.Floor(mark.Position[0] / CELL_SIZE), Math.Floor(mark.Position[2] / CELL_SIZE));
        array<ref PredateFootprint> cell = m_Cells.Get(key);
        if (!cell)
        {
            cell = new array<ref PredateFootprint>;
            m_Cells.Set(key, cell);
        }
        cell.Insert(mark);
        m_LastByCreator.Set(mark.Creator, mark);
    }

    void BuildIndex()
    {
        m_Cells = new map<string, ref array<ref PredateFootprint>>;
        m_TrailIndex = new map<int, ref PredateFootprintTrail>;
        m_LastByCreator = new map<string, ref PredateFootprint>;
        foreach (PredateFootprint mark : Marks) IndexMark(mark);
        foreach (PredateFootprintTrail trail : Trails) m_TrailIndex.Set(trail.Id, trail);
    }

    void Prune(float now)
    {
        if (now < m_NextPrune && m_Cells) return;
        m_NextPrune = now + 2;
        int kept;
        for (int index = 0; index < Marks.Count(); index++)
        {
            PredateFootprint mark = Marks[index];
            if (now >= mark.ExpiresAt) continue;
            if (kept != index) Marks[kept] = mark;
            kept++;
        }
        bool changed = kept < Marks.Count();
        Marks.Resize(kept);
        for (int trailIndex = Trails.Count() - 1; trailIndex >= 0; trailIndex--)
        {
            if (now < Trails[trailIndex].ExpiresAt) continue;
            Trails.RemoveOrdered(trailIndex);
            changed = true;
        }
        if (changed || !m_Cells) BuildIndex();
    }

    bool Validate(float now)
    {
        if (!Marks || !Trails || Marks.Count() > MAX_MARKS || Trails.Count() > MAX_MARKS || NextSerial < 1) return false;
        int previousTrail;
        foreach (PredateFootprintTrail trail : Trails)
        {
            if (!trail || trail.Id <= previousTrail || trail.Id >= NextSerial || !trail.Readers || trail.Readers.Count() > PredateProtocol.MAX_OPERATORS + 1) return false;
            if (!PredateInformationMath.Finite(trail.ExpiresAt) || trail.ExpiresAt > now + 181) return false;
            previousTrail = trail.Id;
        }
        int previousSerial;
        float previousTime;
        foreach (PredateFootprint mark : Marks)
        {
            if (!mark || mark.Serial <= previousSerial || mark.Serial >= NextSerial || mark.Trail < 0 || mark.Trail > mark.Serial) return false;
            if (mark.Creator != "" && mark.Creator != "player-01" && PredateProtocol.OperatorSlot(mark.Creator) < 0) return false;
            if (mark.Heading < 0 || mark.Heading >= 360) return false;
            if (!PredateInformationMath.FiniteVector(mark.Position) || !PredateInformationMath.Finite(mark.CreatedAt) || !PredateInformationMath.Finite(mark.ExpiresAt)) return false;
            if (mark.CreatedAt < previousTime || mark.CreatedAt > now || mark.ExpiresAt <= mark.CreatedAt || mark.ExpiresAt - mark.CreatedAt > 181) return false;
            previousSerial = mark.Serial;
            previousTime = mark.CreatedAt;
        }
        return true;
    }

    int Record(vector site, int surface, float now, string creator = "", int heading = 0, int trailId = 0, bool directionKnown = false)
    {
        float lifetime = Lifetime(surface);
        if (lifetime <= 0 || !PredateInformationMath.FiniteVector(site) || !PredateInformationMath.Finite(now) || now < 0 || heading < 0 || heading >= 360) return 0;
        if (Marks.Count() > 0 && now < Marks[Marks.Count() - 1].CreatedAt) return 0;
        Prune(now);
        array<ref PredateFootprint> cell = m_Cells.Get(CellKey(Math.Floor(site[0] / CELL_SIZE), Math.Floor(site[2] / CELL_SIZE)));
        if (Marks.Count() >= MAX_MARKS || (cell && cell.Count() >= CELL_LIMIT)) return 0;
        PredateFootprint footprint = new PredateFootprint;
        footprint.Serial = NextSerial++;
        footprint.Creator = creator;
        footprint.Position = site;
        footprint.Heading = heading;
        footprint.DirectionKnown = directionKnown;
        footprint.CreatedAt = now;
        footprint.ExpiresAt = now + lifetime;
        PredateFootprintTrail trail = m_TrailIndex.Get(trailId);
        if (!trail)
        {
            trail = new PredateFootprintTrail;
            trail.Id = footprint.Serial;
            Trails.Insert(trail);
            m_TrailIndex.Set(trail.Id, trail);
        }
        trail.ExpiresAt = Math.Max(trail.ExpiresAt, footprint.ExpiresAt);
        footprint.Trail = trail.Id;
        Marks.Insert(footprint);
        IndexMark(footprint);
        return trail.Id;
    }

    int ResumeTrail(string creator, vector position, float now)
    {
        Prune(now);
        PredateFootprint mark = m_LastByCreator.Get(creator);
        if (!mark || now - mark.CreatedAt > 25 || vector.DistanceSq(mark.Position, position) > 225) return 0;
        return mark.Trail;
    }

    bool Read(PredateFootprint mark, string observer, float now)
    {
        if (!mark || observer == "" || mark.Creator == observer || now >= mark.ExpiresAt) return false;
        int id = mark.Trail;
        if (id == 0) id = mark.Serial;
        if (!m_TrailIndex) BuildIndex();
        PredateFootprintTrail trail = m_TrailIndex.Get(id);
        if (!trail)
        {
            // Pre-upgrade marks were individual trails. Preserve them until they expire.
            trail = new PredateFootprintTrail;
            trail.Id = id;
            trail.ExpiresAt = mark.ExpiresAt;
            int insertAt;
            while (insertAt < Trails.Count() && Trails[insertAt].Id < id) insertAt++;
            Trails.InsertAt(trail, insertAt);
            m_TrailIndex.Set(id, trail);
        }
        if (trail.Readers.Find(observer) >= 0) return false;
        trail.Readers.Insert(observer);
        return true;
    }

    void Nearby(vector viewer, float now, array<ref PredateFootprint> output)
    {
        output.Clear();
        if (!m_Cells) BuildIndex();
        int west = Math.Floor((viewer[0] - VIEW_DISTANCE) / CELL_SIZE);
        int east = Math.Floor((viewer[0] + VIEW_DISTANCE) / CELL_SIZE);
        int south = Math.Floor((viewer[2] - VIEW_DISTANCE) / CELL_SIZE);
        int north = Math.Floor((viewer[2] + VIEW_DISTANCE) / CELL_SIZE);
        for (int x = west; x <= east; x++)
        {
            for (int z = south; z <= north; z++)
            {
                array<ref PredateFootprint> cell = m_Cells.Get(CellKey(x, z));
                if (!cell) continue;
                foreach (PredateFootprint mark : cell)
                    if (Reachable(mark, viewer, now)) output.Insert(mark);
            }
        }
    }

    static bool Reachable(PredateFootprint mark, vector viewer, float now)
    {
        return mark && now >= mark.CreatedAt && now < mark.ExpiresAt && vector.DistanceSq(viewer, mark.Position) <= VIEW_DISTANCE * VIEW_DISTANCE;
    }

    static string Notice(PredateObservation clue)
    {
        string text = "Enemy Tracks";
        if (clue.Confidence >= 0.4) text = "Recent Tracks";
        if (!clue.HasBearing) return text;
        int heading = clue.Bearing;
        return text + " · Headed " + heading.ToString();
    }

    static PredateObservation Observe(PredateFootprint mark, string observer, vector viewer, float now, bool visible)
    {
        if (!visible || observer == "" || !Reachable(mark, viewer, now) || mark.Creator == observer) return null;
        float ageFraction = (now - mark.CreatedAt) / (mark.ExpiresAt - mark.CreatedAt);
        int id = mark.Trail;
        if (id == 0) id = mark.Serial;
        PredateObservation result = new PredateObservation;
        result.EventKey = "footprint-" + id;
        result.ObserverId = observer;
        result.Source = PredateObservationSource.FOOTPRINT;
        result.ObservedAt = now;
        result.ExpiresAt = Math.Min(mark.ExpiresAt, now + 24);
        result.Confidence = 0.6 - 0.4 * ageFraction;
        result.HasPosition = true;
        result.Estimate = mark.Position;
        result.Radius = 8 + 12 * ageFraction;
        result.HasBearing = mark.DirectionKnown;
        result.Origin = mark.Position;
        result.Bearing = mark.Heading;
        return result;
    }
}
