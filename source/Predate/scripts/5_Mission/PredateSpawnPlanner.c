class PredateSpawnPlanner
{
    static bool PlayerTerrain(vector point)
    {
        if (!PredateInformationMath.FiniteVector(point) || !PredateRoutePlanner.Land(point)) return false;
        return GetGame().SurfaceGetNormal(point[0], point[2])[1] >= 0.75;
    }

    static bool SafeOperatorSite(PredateSession session, vector point, array<vector> occupied, float spacing, out vector site)
    {
        if (!PlayerTerrain(point)) return false;
        vector grounded = session.PredateGround(point);
        if (!PredateRoutePlanner.Snap(grounded, 12, site) || !PlayerTerrain(site)) return false;
        foreach (vector other : occupied)
            if (PredateHunterBrain.Distance(site, other) < spacing) return false;
        return true;
    }

    static bool OperatorSite(PredateSession session, vector center, array<vector> occupied, float spacing, out vector site)
    {
        for (int attempt = 0; attempt < 40; attempt++)
        {
            float bearing = Math.RandomFloat(0, 360);
            float distance = Math.RandomFloat(450, 2400);
            vector point = center + PredateInformationMath.BearingDirection(bearing) * distance;
            if (SafeOperatorSite(session, point, occupied, spacing, site)) return true;
        }
        if (SafeOperatorSite(session, center, occupied, spacing, site)) return true;
        for (float radius = spacing + 100; radius <= 2200; radius += 350)
            for (int sector = 0; sector < 8; sector++)
                if (SafeOperatorSite(session, center + PredateInformationMath.BearingDirection(sector * 45) * radius, occupied, spacing, site)) return true;
        return false;
    }

    // Draw several irregular points per sector and reserve the actual snapped
    // positions. Random ranking keeps distance from becoming a visible lattice.
    static bool RegionalSites(PredateSession session, array<vector> occupied, int count, array<vector> output)
    {
        for (int attempt = 0; attempt < 3; attempt++)
            if (TryRegionalSites(session, occupied, count, output)) return true;
        return false;
    }

    static bool TryRegionalSites(PredateSession session, array<vector> occupied, int count, array<vector> output)
    {
        array<vector> candidates = {};
        array<float> distances = {};
        array<vector> selected = {};
        float worldSize = GetGame().GetWorld().GetWorldSize();
        for (float x = 850; x < worldSize - 450; x += 1750)
        {
            for (float z = 850; z < worldSize - 450; z += 1750)
            {
                for (int attempt = 0; attempt < 5; attempt++)
                {
                    float east = Math.Clamp(x + Math.RandomFloat(-800, 800), 50, worldSize - 50);
                    float north = Math.Clamp(z + Math.RandomFloat(-800, 800), 50, worldSize - 50);
                    vector site;
                    if (!SafeOperatorSite(session, Vector(east, 0, north), occupied, PredateDeployment.MINIMUM_DISTANCE, site)) continue;
                    float nearest = worldSize * 2;
                    foreach (vector other : occupied)
                        nearest = Math.Min(nearest, PredateHunterBrain.Distance(site, other));
                    candidates.Insert(site);
                    distances.Insert(nearest);
                }
            }
        }
        while (output.Count() + selected.Count() < count)
        {
            int best = -1;
            float bestScore = -1;
            for (int index = 0; index < candidates.Count(); index++)
            {
                if (distances[index] < PredateDeployment.MINIMUM_DISTANCE) continue;
                float score = distances[index] + Math.RandomFloat(0, 1450);
                if (score <= bestScore) continue;
                best = index;
                bestScore = score;
            }
            if (best < 0) return false;
            vector reserved = candidates[best];
            candidates.RemoveOrdered(best);
            distances.RemoveOrdered(best);
            for (int remaining = 0; remaining < candidates.Count(); remaining++)
                distances[remaining] = Math.Min(distances[remaining], PredateHunterBrain.Distance(candidates[remaining], reserved));
            selected.Insert(reserved);
        }
        occupied.InsertAll(selected);
        output.InsertAll(selected);
        return true;
    }

    static bool ReplacementSite(PredateSession session, vector preferred, array<vector> occupied, out vector point)
    {
        if (OperatorSite(session, preferred, occupied, PredateDeployment.MINIMUM_DISTANCE, point)) return true;
        array<vector> alternatives = {};
        if (!RegionalSites(session, occupied, 1, alternatives)) return false;
        point = alternatives[0];
        return true;
    }
}
