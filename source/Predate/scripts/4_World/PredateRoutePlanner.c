// Static terrain knowledge only. Dynamic actors, vehicles and inventory never
// affect route scores; selecting a route cannot reveal an unseen opponent.
class PredateRoutePlanner
{
    static bool Snap(vector point, float radius, out vector sampled)
    {
        PGFilter filter = ExpansionPathFilters.GetInstance().m_PathFilter_NoClosedDoors_NoJumpClimb;
        return GetGame().GetWorld().GetAIWorld().SampleNavmeshPosition(point, radius, filter, sampled);
    }
    static bool Land(vector point)
    {
        float size = GetGame().GetWorld().GetWorldSize();
        if (point[0] <= 10 || point[2] <= 10 || point[0] >= size - 10 || point[2] >= size - 10) return false;
        return !GetGame().SurfaceIsSea(point[0], point[2]) && !GetGame().SurfaceIsPond(point[0], point[2]);
    }

    static float Cover(vector position)
    {
        array<Object> objects = new array<Object>;
        array<CargoBase> cargo = new array<CargoBase>;
        GetGame().GetObjectsAtPosition3D(position, 18, objects, cargo);
        float cover;
        foreach (Object object : objects)
        {
            if (!object) continue;
            if (object.IsBuilding()) cover += 2;
            else if (object.IsTree() || object.IsBush()) cover += 1;
        }
        return Math.Min(8, cover);
    }

    static float LegCost(vector from, vector to, bool countCover = true)
    {
        float distance = PredateHunterBrain.Distance(from, to);
        if (distance < 1) return 0;
        float previousHeight = GetGame().SurfaceY(from[0], from[2]);
        float maximumGrade;
        float cover;
        for (int i = 1; i <= 4; i++)
        {
            vector point = from + (to - from) * (i * 0.25);
            if (!Land(point)) return 1000000;
            float height = GetGame().SurfaceY(point[0], point[2]);
            float grade = Math.AbsFloat(height - previousHeight) / (distance * 0.25);
            if (grade > 0.75) return 1000000;
            maximumGrade = Math.Max(maximumGrade, grade);
            previousHeight = height;
            point[1] = height;
            if (countCover) cover += Cover(point);
        }
        return distance * 0.1 + maximumGrade * 60 - cover * 0.6;
    }

    static bool Plan(vector ownPosition, vector goal, out vector intermediate, PredateHunterState state)
    {
        if (!Land(goal)) return false;
        vector forward = goal - ownPosition;
        forward[1] = 0;
        forward.Normalize();
        vector side = Vector(forward[2], 0, -forward[0]);
        vector middle = ownPosition + (goal - ownPosition) * 0.5;
        float offset = Math.Min(35, PredateHunterBrain.Distance(ownPosition, goal) * 0.3);
        float bestCost = 1000000;
        for (int i = -1; i <= 1; i++)
        {
            vector candidate = middle + side * (i * offset);
            if (!Land(candidate)) continue;
            candidate[1] = GetGame().SurfaceY(candidate[0], candidate[2]);
            vector sampled;
            if (!Snap(candidate, 8, sampled)) continue;
            candidate = sampled;
            float first = LegCost(ownPosition, candidate, false);
            float second = LegCost(candidate, goal, false);
            float cost = first + second;
            if (cost < 1000000)
            {
                cost -= Cover(candidate) * 1.2;
                if (state && (state.Mode == "search" || state.Mode == "investigate"))
                {
                    vector evidencePoint = state.SearchCenter;
                    evidencePoint[1] = GetGame().SurfaceY(evidencePoint[0], evidencePoint[2]) + 1.25;
                    PhxInteractionLayers terrain = PhxInteractionLayers.TERRAIN | PhxInteractionLayers.BUILDING | PhxInteractionLayers.ROADWAY;
                    if (!DayZPhysics.RayCastBullet(candidate + "0 1.6 0", evidencePoint, terrain, null, null, null, null, null)) cost -= 12;
                }
            }
            if (cost >= bestCost) continue;
            bestCost = cost;
            intermediate = candidate;
        }
        if (bestCost < 1000000) return true;
        return NavmeshDetour(ownPosition, goal, intermediate);
    }

    static bool NavmeshDetour(vector start, vector goal, out vector intermediate)
    {
        // A bridge deck is traversable even when terrain samples beneath it hit water.
        if (PredateHunterBrain.Distance(start, goal) > 1200) return false;
        PGFilter filter = ExpansionPathFilters.GetInstance().m_PathFilter_NoJumpClimb;
        array<vector> path = {};
        if (!GetGame().GetWorld().GetAIWorld().FindPath(start, goal, filter, path)) return false;
        if (path.Count() < 2 || vector.DistanceSq(path[path.Count() - 1], goal) > 16) return false;
        for (int index = 1; index < path.Count() - 1; index++)
        {
            // Ignore the first mesh corners when they are already under this Operator.
            if (vector.DistanceSq(start, path[index]) < 16) continue;
            intermediate = path[index];
            return true;
        }
        intermediate = goal;
        return true;
    }

    static bool PartOfAreaVisible(vector start, vector point, Object observer)
    {
        vector hit, normal;
        int component;
        return !DayZPhysics.RaycastRV(start, point, hit, normal, component, null, null, observer, false, false, ObjIntersectView, 0);
    }

    static void RecordObservedSectors(eAIBase actor, PredateHunterState state, float now)
    {
        vector eyes = actor.GetBonePositionWS(actor.GetBoneIndexByName("Head"));
        vector facing = actor.GetLookDirection();
        if (eyes == vector.Zero || !PredateInformationMath.FiniteVector(eyes)) return;
        float range = Math.Min(actor.m_eAI_ThreatDistanceLimit, actor.m_Expansion_Visibility * 1100);
        for (int sector = 0; sector < 8; sector++)
        {
            int bit = 1 << sector;
            if ((state.CheckedSectors & bit) != 0) continue;
            vector point = PredateHunterBrain.SearchSector(state, sector, now);
            if (!Land(point))
            {
                state.CheckedSectors |= bit;
                continue;
            }
            point[1] = GetGame().SurfaceY(point[0], point[2]) + 1;
            vector direction = point - eyes;
            if (direction.Length() > range || vector.Dot(direction.Normalized(), facing) < 0.7) continue;
            if (PartOfAreaVisible(eyes, point, actor)) state.CheckedSectors |= bit;
        }
    }
}
