class PredateFootprintSensor
{
    bool m_Started;
    vector m_LastPosition;
    vector m_PreviousMark;
    float m_NextMarkAt;
    float m_NextScanAt;
    ref array<ref PredateFootprint> m_Nearby = new array<ref PredateFootprint>;

    static int SurfaceAt(vector feet, out vector point)
    {
        point = feet;
        point[1] = GetGame().SurfaceY(feet[0], feet[2]);
        if (Math.AbsFloat(point[1] - feet[1]) > 0.45 || !PredateRoutePlanner.Land(point))
            return PredateFootprintSurface.HARD;

        string surface;
        GetGame().SurfaceGetType3D(feet[0], feet[1] + 0.2, feet[2], surface);
        string material = surface;
        material.ToLower();
        if (material.Contains("snow")) return PredateFootprintSurface.SNOW;
        if (material.Contains("sand")) return PredateFootprintSurface.SAND;
        if (GetGame().IsSurfaceDigable(surface)) return PredateFootprintSurface.EARTH;
        return PredateFootprintSurface.HARD;
    }

    void Stamp(PlayerBase actor, string owner, float now, PredateFootprintLedger ledger)
    {
        vector position = actor.GetPosition();
        float displacement = PredateHunterBrain.Distance(m_LastPosition, position);
        m_LastPosition = position;
        if (!m_Started || displacement > 15 || actor.IsInTransport() || actor.IsSwimming() || actor.GetIsSimulationDisabled())
        {
            m_Started = true;
            m_PreviousMark = position;
            return;
        }
        if (now < m_NextMarkAt || PredateHunterBrain.Distance(position, m_PreviousMark) < 4) return;
        int heading = PredateMath.Bearing(m_PreviousMark, position);
        m_PreviousMark = position;
        m_NextMarkAt = now + 0.8;
        vector ground;
        int surface = SurfaceAt(position, ground);
        ledger.Record(ground, surface, now, owner, heading);
    }

    static bool Visible(vector eyes, vector direction, vector ground, Object observer)
    {
        vector toward = ground - eyes;
        toward[1] = 0;
        if (toward.LengthSq() > 0.25)
        {
            toward.Normalize();
            if (vector.Dot(toward, direction) < 0.5) return false;
        }
        vector hit, normal;
        int component;
        return !DayZPhysics.RaycastRV(eyes, ground + "0 0.1 0", hit, normal, component, null, null, observer, false, false, ObjIntersectView, 0);
    }

    void Scan(eAIBase actor, float now, PredateFootprintLedger ledger)
    {
        if (now < m_NextScanAt || !actor.m_PredateMemory) return;
        m_NextScanAt = now + 1;
        PredateFootprint observed;
        Observe(actor, actor.m_PredateOperatorId, now, ledger, observed, actor.m_PredateMemory);
    }

    PredateObservation Observe(PlayerBase actor, string owner, float now, PredateFootprintLedger ledger, out PredateFootprint observed, PredateObservationMemory memory = null)
    {
        if (actor.GetIsSimulationDisabled() || actor.IsUnconscious()) return null;
        vector position = actor.GetPosition();
        vector eyes = actor.GetBonePositionWS(actor.GetBoneIndexByName("head"));
        if (!PredateInformationMath.FiniteVector(eyes) || eyes == vector.Zero) return null;
        vector facing = actor.GetDirection();
        facing[1] = 0;
        facing.Normalize();
        ledger.Nearby(position, now, m_Nearby);
        int checked;
        for (int index = m_Nearby.Count() - 1; index >= 0 && checked < 3; index--)
        {
            PredateFootprint mark = m_Nearby[index];
            if (mark.Creator == owner) continue;
            checked++;
            if (!Visible(eyes, facing, mark.Position, actor)) continue;
            PredateObservation clue = PredateFootprintLedger.Observe(mark, owner, position, now, true);
            if (clue && (!memory || memory.Add(clue, now)))
            {
                observed = mark;
                return clue;
            }
        }
        return null;
    }
}

modded class PlayerBase
{
    ref PredateFootprintSensor m_PredateFootprintSensor;

    PredateFootprintSensor PredateFootprintSensorForActor()
    {
        if (!m_PredateFootprintSensor) m_PredateFootprintSensor = new PredateFootprintSensor;
        return m_PredateFootprintSensor;
    }
}
