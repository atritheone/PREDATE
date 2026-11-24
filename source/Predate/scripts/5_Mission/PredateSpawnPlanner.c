class PredateSpawnRegion
{
    ref array<vector> Sites = {};
}

class PredateSpawnPlanner
{
    static bool PlayerTerrain(vector point)
    {
        if (!PredateInformationMath.FiniteVector(point) || !PredateRoutePlanner.Land(point)) return false;
        return GetGame().SurfaceGetNormal(point[0], point[2])[1] >= 0.75;
    }

    static bool ExactPlayerSite(vector point)
    {
        if (!PlayerTerrain(point)) return false;
        point[1] = GetGame().SurfaceY(point[0], point[2]);
        vector sampled;
        if (!PredateRoutePlanner.Snap(point, 8, sampled) || Math.AbsFloat(sampled[1] - point[1]) > 0.8) return false;
        array<Object> excluded = {};
        return !GetGame().IsBoxColliding(point + "0 1.05 0", vector.Zero, "0.65 1.7 0.65", excluded);
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

    static bool InitialNearPlayer(PredateSession session, vector player, array<vector> occupied, float maximum, out vector site)
    {
        float minimum = PredateDeployment.MINIMUM_DISTANCE;
        for (int attempt = 0; attempt < 90; attempt++)
        {
            float distance = Math.Sqrt(Math.RandomFloat(minimum * minimum, maximum * maximum));
            vector point = player + PredateInformationMath.BearingDirection(Math.RandomFloat(0, 360)) * distance;
            if (SafeOperatorSite(session, point, occupied, minimum, site) && PredateHunterBrain.Distance(site, player) <= maximum)
                return true;
        }
        // Sample a denser ring where the available land is narrow, such as the coast.
        float offset = Math.RandomFloat(0, 360);
        for (float radius = minimum + 100; radius <= maximum; radius += 330)
            for (int bearing = 0; bearing < 360; bearing += 10)
                if (SafeOperatorSite(session, player + PredateInformationMath.BearingDirection(offset + bearing) * radius, occupied, minimum, site))
                    return true;
        return false;
    }

    static bool ReserveInitialSite(PredateSpawnRegion region, array<vector> occupied, float spacing, out vector site)
    {
        while (region.Sites.Count() > 0)
        {
            int index = Math.RandomInt(0, region.Sites.Count());
            vector candidate = region.Sites[index];
            region.Sites.Remove(index);
            bool clear = true;
            foreach (vector other : occupied)
            {
                if (PredateHunterBrain.Distance(candidate, other) >= spacing) continue;
                clear = false;
                break;
            }
            if (!clear) continue;
            site = candidate;
            occupied.Insert(site);
            return true;
        }
        return false;
    }

    static bool InitialSites(PredateSession session, vector player, array<vector> occupied, int count, array<vector> output)
    {
        float spacing = PredateDeployment.MINIMUM_DISTANCE;
        // Keep the player's insertion and all later respawns at the normal
        // separation. A dense initial roster needs more room for local variation.
        if (session.m_HuntConfig.OpponentCount > 30) spacing = 1250;
        if (session.m_HuntConfig.OpponentCount > 65) spacing = 1000;
        bool compactRoster = GetGame().GetWorld().GetWorldSize() < 14000 && session.m_HuntConfig.OpponentCount > 30;
        if (compactRoster) spacing *= 0.84;
        for (int attempt = 0; attempt < 3; attempt++)
        {
            array<vector> reserved = {};
            reserved.InsertAll(occupied);
            array<vector> sites = {};
            // Progressively open more slots between AI on smaller terrains.
            // Initial player separation and later respawns use MINIMUM_DISTANCE.
            float attemptSpacing = spacing;
            if (compactRoster) attemptSpacing -= attempt * 70;
            if (!TryInitialSites(session, player, reserved, attemptSpacing, count, sites)) continue;
            output.InsertAll(sites);
            return true;
        }
        return false;
    }

    static bool InitialRoster(PredateSession session, array<vector> sites)
    {
        vector player = session.m_HuntConfig.PlayerStart;
        array<vector> occupied = {player};
        vector insertion;
        if (!InitialNearPlayer(session, player, occupied, 3200, insertion)) return false;
        occupied.Insert(insertion);
        sites.Insert(insertion);
        int count = session.m_HuntConfig.OpponentCount;
        int nearby = Math.RandomInt(1, Math.Min(count, 2 + count / 25) + 1);
        while (sites.Count() < nearby && InitialNearPlayer(session, player, occupied, 4400, insertion))
        {
            occupied.Insert(insertion);
            sites.Insert(insertion);
        }
        int remaining = count - sites.Count();
        if (remaining > 0 && !InitialSites(session, player, occupied, remaining, sites)) return false;
        // The nearby slot can belong to any Operator.
        for (int index = sites.Count() - 1; index > 0; index--)
        {
            int other = Math.RandomInt(0, index + 1);
            vector held = sites[index];
            sites[index] = sites[other];
            sites[other] = held;
        }
        return true;
    }

    static bool TryInitialSites(PredateSession session, vector player, array<vector> occupied, float spacing, int count, array<vector> sites)
    {
        array<ref PredateSpawnRegion> regions = {};
        array<vector> playerOnly = {player};
        float worldSize = GetGame().GetWorld().GetWorldSize() - 60;
        float width = 2600;
        for (float east = 60; east < worldSize; east += width)
        {
            for (float north = 60; north < worldSize; north += width)
            {
                PredateSpawnRegion region = new PredateSpawnRegion;
                for (int sample = 0; sample < 26; sample++)
                {
                    float x = Math.RandomFloat(east, Math.Min(east + width, worldSize));
                    float z = Math.RandomFloat(north, Math.Min(north + width, worldSize));
                    vector site;
                    if (SafeOperatorSite(session, Vector(x, 0, z), playerOnly, PredateDeployment.MINIMUM_DISTANCE, site))
                        region.Sites.Insert(site);
                }
                if (region.Sites.Count() > 0) regions.Insert(region);
            }
        }

        // Sample different regions first, then let populated regions receive
        // extra Operators in a random order. There is no maximum-distance score
        // to pull every insertion back into a grid.
        array<ref PredateSpawnRegion> covered = {};
        while (regions.Count() > 0 && sites.Count() < count)
        {
            int regionIndex = Math.RandomInt(0, regions.Count());
            PredateSpawnRegion next = regions[regionIndex];
            regions.Remove(regionIndex);
            vector insertion;
            if (!ReserveInitialSite(next, occupied, spacing, insertion)) continue;
            sites.Insert(insertion);
            if (next.Sites.Count() > 0) covered.Insert(next);
        }
        while (covered.Count() > 0 && sites.Count() < count)
        {
            int index = Math.RandomInt(0, covered.Count());
            vector additionalSite;
            if (ReserveInitialSite(covered[index], occupied, spacing, additionalSite)) sites.Insert(additionalSite);
            if (covered[index].Sites.Count() == 0) covered.Remove(index);
        }
        return sites.Count() == count;
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
