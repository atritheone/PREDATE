class PredatePEQSample
{
    PlayerBase Holder;
    vector Start;
    vector Direction;
    vector End;
    float Reach;
    float ConeCosine;
    bool Laser;
    bool Infrared;
    bool HasSplash;
    protected bool m_Traced;

    void PredatePEQSample(PlayerBase holder, vector start, vector direction, float reach, float coneAngle, bool laser, bool infrared)
    {
        Holder = holder;
        Start = start;
        Direction = direction;
        Reach = reach;
        ConeCosine = Math.Cos(coneAngle * Math.DEG2RAD);
        Laser = laser;
        Infrared = infrared;
        End = Start + Direction * Reach;
    }

    protected void Trace()
    {
        if (m_Traced) return;
        m_Traced = true;
        vector contact, normal;
        int component;
        if (DayZPhysics.RaycastRV(Start, End, contact, normal, component, null, null, Holder, false, false, ObjIntersectView, 0))
        {
            End = contact - Direction * 0.10;
            HasSplash = true;
        }
    }

    bool Illuminates(PlayerBase target, vector point)
    {
        vector offset = point - Start;
        float distanceSquared = offset.LengthSq();
        if (target == Holder || distanceSquared < 0.01 || distanceSquared > Reach * Reach || vector.Dot(offset.Normalized(), Direction) < ConeCosine) return false;
        vector contact, normal;
        int component;
        set<Object> hits = new set<Object>;
        if (!DayZPhysics.RaycastRV(Start, point, contact, normal, component, hits, null, Holder, false, false, ObjIntersectView, 0)) return true;
        if (!hits.Count()) return false;
        EntityAI first = EntityAI.Cast(hits[0]);
        return first && first.GetHierarchyRoot() == target;
    }

    bool FindClue(eAIBase observer, vector eyes, bool dark, out vector area, out float radius)
    {
        float viewRange = 120;
        if (Laser && !Infrared && !dark) viewRange = 65;
        float possibleRange = Reach + viewRange + 0.1;
        if (vector.DistanceSq(eyes, Start) > possibleRange * possibleRange) return false;
        Trace();
        if (HasSplash && PredatePEQVision.SeesPoint(observer, eyes, End, viewRange))
        {
            area = End;
            radius = 26;
            if (!Laser) return true;

            // A visible upstream stretch can guide the search back along the beam.
            vector upstream = End - Direction * Math.Min(20, vector.Distance(Start, End) * 0.5);
            if (vector.DistanceSq(upstream, Start) > 25 && PredatePEQVision.SeesPoint(observer, eyes, upstream, viewRange))
                area = upstream;
            return true;
        }
        if (!Laser) return false;

        float length = vector.Dot(End - Start, Direction);
        if (length < 10) return false;
        float along = Math.Clamp(vector.Dot(eyes - Start, Direction), 5, length - 1);
        vector sample = Start + Direction * along;
        if (!PredatePEQVision.SeesPoint(observer, eyes, sample, viewRange)) return false;
        area = sample;
        radius = 34;
        return true;
    }
}

class PredatePEQVision
{
    static bool HasNightVision(PlayerBase person)
    {
        EntityAI mount = person.FindAttachmentBySlotName("Eyewear");
        if (WorkingGoggles(mount)) return true;
        return WorkingGoggles(person.FindAttachmentBySlotName("Headgear"));
    }

    protected static bool WorkingGoggles(EntityAI mount)
    {
        if (!mount) return false;
        NVGoggles goggles = NVGoggles.Cast(mount.FindAttachmentBySlotName("NVG"));
        return goggles && goggles.m_IsLowered && goggles.IsWorking();
    }

    static void Collect(PlayerBase holder, array<ref PredatePEQSample> samples)
    {
        if (!holder.IsAlive() || holder.IsUnconscious() || holder.GetIsSimulationDisabled() || holder.Expansion_HasAdminToolInvisibility()) return;
        Weapon_Base rifle = Weapon_Base.Cast(holder.GetEntityInHands());
        if (!rifle) return;
        ItemBase module = ItemBase.Cast(rifle.FindAttachmentBySlotName("weaponFlashlight"));
        if (!module || !module.IsKindOf("MM_ANPEQ15") || !module.GetCompEM() || !module.GetCompEM().IsWorking()) return;
        if (!module.MemoryPointExists("beamStart") || !module.MemoryPointExists("beamEnd")) return;
        // The Workshop pair is version-locked and optional. Read its synchronized
        // mode flags without requiring that addon in hunts that do not use it.
        bool laser, infrared, white, irLamp, tactical;
        EnScript.GetClassVar(module, "m_MM_LaserModeSynch", 0, laser);
        EnScript.GetClassVar(module, "m_PredateLaserIR", 0, infrared);
        EnScript.GetClassVar(module, "m_MM_FlashModeSynch", 0, white);
        EnScript.GetClassVar(module, "m_PredateIRLight", 0, irLamp);
        EnScript.GetClassVar(module, "m_MM_TacticalModeSynch", 0, tactical);
        if (tactical && !holder.IsFireWeaponRaised())
        {
            laser = false;
            white = false;
        }
        if (!laser && !white && !irLamp) return;

        vector start = module.ModelToWorld(module.GetMemoryPointPos("beamStart"));
        vector heading = module.ModelToWorld(module.GetMemoryPointPos("beamEnd")) - start;
        if (!PredateInformationMath.FiniteVector(start) || !PredateInformationMath.FiniteVector(heading) || heading.LengthSq() < 0.000001) return;
        heading.Normalize();
        start += heading * 0.08;
        if (laser) samples.Insert(new PredatePEQSample(holder, start, heading, 120, 0, true, infrared));
        if (white) samples.Insert(new PredatePEQSample(holder, start, heading, 48, 25, false, false));
        if (irLamp) samples.Insert(new PredatePEQSample(holder, start, heading, 32, 18, false, true));
    }

    static bool SeesPoint(eAIBase observer, vector eyes, vector point, float range)
    {
        vector sight = point - eyes;
        float distance = sight.Length();
        if (distance < 0.1 || distance > range) return false;
        vector look = observer.GetLookDirection();
        if (vector.Dot(sight.Normalized(), look.Normalized()) < 0.5) return false;
        float bearing = PredateMath.RelativeBearing(PredateMath.CameraHeading(sight), PredateMath.CameraHeading(look));
        if (Math.AbsFloat(bearing) > observer.eAI_CalculateFOVHalfAngleH(distance)) return false;
        vector contact, normal;
        int component;
        return !DayZPhysics.RaycastRV(eyes, point, contact, normal, component, null, null, observer, false, false, ObjIntersectView, 0);
    }
}

modded class PlayerBase
{
    float m_PredatePEQWhiteLitUntil;
    float m_PredatePEQInfraredLitUntil;
}

#ifndef SERVER
modded class ScriptedLightBase
{
    override bool eAI_IsVisible(bool isNight = false)
    {
        // Expansion's generic light flag has no IR filter. PEQ laser and IR
        // illumination are evaluated by the server-side observer instead.
        if (ClassName() == "MM_LaserBeamSegmentLight" || ClassName() == "MM_LaserDotLight") return false;
        EntityAI source;
        if (IsInherited(PredateModuleSpot) && Class.CastTo(source, GetParent()) && source.IsKindOf("MM_ANPEQ15")) return false;
        return super.eAI_IsVisible(isNight);
    }
}
#endif
