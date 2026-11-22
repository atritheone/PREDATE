class PredateFootprintSensor
{
    static const float INSPECTION_SECONDS = 0.45;
    bool m_Started;
    vector m_LastPosition;
    vector m_PreviousMark;
    int m_Trail;
    float m_LastMarkAt;
    float m_NextMarkAt;
    float m_NextScanAt;
    vector m_PreviousTravel;
    int m_Focus;
    float m_FocusAt;
    ref PredateFootprint m_Foreign;
    ref PredateFootprint m_Own;
    bool m_InspectingOwn;
    ref array<ref PredateFootprint> m_Nearby = {};

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
        if (!actor.IsAlive() || actor.IsUnconscious() || actor.IsInTransport() || actor.IsSwimming() || actor.GetIsSimulationDisabled())
        {
            m_Started = false;
            return;
        }
        vector position = actor.GetPosition();
        vector travel = position - m_LastPosition;
        travel[1] = 0;
        float displacement = travel.Length();
        m_LastPosition = position;
        if (!m_Started || displacement > 15)
        {
            if (!m_Started) m_Trail = ledger.ResumeTrail(owner, position, now);
            else m_Trail = 0;
            m_Started = true;
            m_LastMarkAt = now;
            m_PreviousMark = position;
            m_PreviousTravel = vector.Zero;
            return;
        }
        if (displacement < 0.12) return;
        travel.Normalize();
        bool straight = m_PreviousTravel == vector.Zero || vector.Dot(m_PreviousTravel, travel) >= 0.65;
        m_PreviousTravel = travel;
        if (now < m_NextMarkAt || PredateHunterBrain.Distance(position, m_PreviousMark) < 7) return;
        m_PreviousMark = position;
        m_NextMarkAt = now + 1.25;
        vector ground;
        int surface = SurfaceAt(position, ground);
        if (surface == PredateFootprintSurface.HARD) return;
        if (now - m_LastMarkAt > 25) m_Trail = 0;
        int trail = ledger.Record(ground, surface, now, owner, PredateMath.Bearing(vector.Zero, travel), m_Trail, straight);
        if (trail == 0) return;
        m_Trail = trail;
        m_LastMarkAt = now;
    }

    static bool FacingGround(vector eyes, vector direction, vector ground, bool inspect)
    {
        vector toward = ground + "0 0.04 0" - eyes;
        if (!inspect)
        {
            toward[1] = 0;
            direction[1] = 0;
        }
        if (toward.LengthSq() < 0.01) return true;
        toward.Normalize();
        direction.Normalize();
        if (inspect) return vector.Dot(toward, direction) >= 0.96;
        return vector.Dot(toward, direction) >= 0.5;
    }

    static bool Visible(vector eyes, vector direction, vector ground, Object observer)
    {
        if (!FacingGround(eyes, direction, ground, false)) return false;
        vector hit, normal;
        int component;
        return !DayZPhysics.RaycastRV(eyes, ground + "0 0.06 0", hit, normal, component, null, null, observer, false, false, ObjIntersectView, 0);
    }

    PredateObservation Scan(PlayerBase actor, string owner, float now, PredateFootprintLedger ledger, bool human)
    {
        if (now < m_NextScanAt) return null;
        if (human) m_NextScanAt = now + 0.24;
        else m_NextScanAt = now + 0.49;
        m_Foreign = null;
        m_Own = null;
        m_InspectingOwn = false;
        if (!actor.IsAlive() || actor.IsUnconscious() || actor.GetIsSimulationDisabled())
        {
            m_Focus = 0;
            return null;
        }

        vector position = actor.GetPosition();
        ledger.Nearby(position, now, m_Nearby);
        if (m_Nearby.Count() == 0)
        {
            m_Focus = 0;
            return null;
        }

        vector eyes = actor.GetBonePositionWS(actor.GetBoneIndexByName("head"));
        vector look = actor.GetDirection();
        bool hasGaze;
        if (human)
        {
            hasGaze = actor.m_PredateFootprintLookReceivedAt > 0 && GetGame().GetTime() - actor.m_PredateFootprintLookReceivedAt < 900;
            if (hasGaze) look = actor.m_PredateFootprintLook;
        }
        else
        {
            eAIBase ai = eAIBase.Cast(actor);
            if (!ai) return null;
            look = ai.GetLookDirection();
            hasGaze = true;
        }
        PredateFootprint focused;
        float focusDistance = 100;
        int foreignChecks, ownChecks;
        for (int index = m_Nearby.Count() - 1; index >= 0; index--)
        {
            PredateFootprint mark = m_Nearby[index];
            bool own = mark.Creator == owner;
            if (own && (!human || ownChecks >= 2 || m_InspectingOwn)) continue;
            if (!own && foreignChecks >= 3) continue;
            bool inspecting = hasGaze && FacingGround(eyes, look, mark.Position, true);
            if (!inspecting && (!human || (own && m_Own) || (!own && m_Foreign))) continue;
            if (!FacingGround(eyes, look, mark.Position, false)) continue;
            if (own) ownChecks++;
            else foreignChecks++;
            if (!Visible(eyes, look, mark.Position, actor)) continue;
            if (own)
            {
                m_Own = mark;
                if (inspecting) m_InspectingOwn = true;
                continue;
            }
            if (!m_Foreign) m_Foreign = mark;
            if (!inspecting) continue;
            float distance = vector.DistanceSq(position, mark.Position);
            if (distance >= focusDistance) continue;
            focusDistance = distance;
            m_Foreign = mark;
            focused = mark;
        }
        if (!focused)
        {
            m_Focus = 0;
            return null;
        }
        if (m_Focus != focused.Serial)
        {
            m_Focus = focused.Serial;
            m_FocusAt = now;
            return null;
        }
        if (now - m_FocusAt < INSPECTION_SECONDS) return null;
        PredateObservation clue = PredateFootprintLedger.Observe(focused, owner, position, now, true);
        if (!clue || !ledger.Read(focused, owner, now)) return null;
        return clue;
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
