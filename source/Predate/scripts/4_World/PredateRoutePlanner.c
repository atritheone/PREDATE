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

    static float LegCost(vector from, vector to)
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
            cover += Cover(point);
        }
        return distance * 0.1 + maximumGrade * 60 - cover * 0.6;
    }

    static bool Plan(vector ownPosition, vector goal, out vector intermediate)
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
            float first = LegCost(ownPosition, candidate);
            float second = LegCost(candidate, goal);
            float cost = first + second;
            if (cost >= bestCost) continue;
            bestCost = cost;
            intermediate = candidate;
        }
        return bestCost < 1000000;
    }
}
