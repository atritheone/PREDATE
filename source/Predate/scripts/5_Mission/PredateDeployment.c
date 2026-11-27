class PredateInsertionScore
{
    int Operators = -1;
    int Priority;
    int ApproachBand;
    float DistanceFromDeath;
    vector Position;

    bool ImprovesOn(int priority, int operators, int approachBand, float distanceFromDeath)
    {
        int contactTier = Math.Min(2, operators);
        int previousContactTier = Math.Min(2, Operators);
        if (contactTier != previousContactTier) return contactTier > previousContactTier;
        if (priority != Priority) return priority > Priority;
        if (operators != Operators) return operators > Operators;
        if (approachBand != ApproachBand) return approachBand < ApproachBand;
        return operators == 0 && distanceFromDeath < DistanceFromDeath;
    }
}

// Player life changes are independent of the operation and its Raven record.
class PredateDeployment
{
    static const int DEATH_SAVE_DELAY_MS = 350;

    static void RememberCharacter(PredateWorldState state, PlayerBase player)
    {
        if (state && player && state.PlayerCharacterModel == "")
            state.PlayerCharacterModel = player.GetType();
    }

    static void ObserveDeath(PredateSession session, PlayerBase player)
    {
        if (!session || !session.m_HuntReady || session.m_HuntError != "" || !player || player.m_PredateDeathRecorded || !player.GetIdentity()) return;
        if (player.IsAlive() && player.GetHealth("", "Health") > 0) return;
        player.m_PredateDeathRecorded = true;
        player.m_PredateHasDeploymentPosition = false;
        PredateWorldState state = session.m_WorldState;
        RememberCharacter(state, player);
        if (state.PlayerAwaitingRedeploy) return;
#ifdef SERVER
        if (!state.PlayerWeaponModules) state.PlayerWeaponModules = new map<string, int>;
        player.PredateTransferWeaponModules(state.PlayerWeaponModules, false);
#endif
        if (session.m_Score) session.m_Score.EndLife(player);
        state.PlayerAwaitingRedeploy = true;
        state.PlayerLastDeathAt = state.SimulationSeconds;
        state.PlayerDeathPosition = player.GetPosition();
        state.HasPlayerDeathPosition = PredateInformationMath.FiniteVector(state.PlayerDeathPosition);
        // Give the native death notification time to reach the client before saving the roster.
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(session.PredateSave, DEATH_SAVE_DELAY_MS, false);
        Print("[Predate] Player life " + state.PlayerLife + " ended. Waiting for redeployment.");
    }

    static void StagePlayer(PlayerBase player)
    {
        if (player.m_PredateStagedForDeployment) return;
        player.m_PredateStagedForDeployment = true;
        player.RemoveAllItems();
        player.SetAllowDamage(false);
    }

    static const float MINIMUM_DISTANCE = 1500;
    static const float PREFERRED_DEATH_DISTANCE = 5000;
    static const float FALLBACK_DEATH_DISTANCE = 3500;
    static const float REGION_WIDTH = 2600;
    static const int INSERTION_HISTORY_SIZE = PredateOperatorRecord.INSERTION_HISTORY_SIZE;

    static void LivingOperators(PredateSession session, array<vector> occupied)
    {
        eAIBase raven = session.m_HuntBot;
        if (raven && raven.IsAlive()) occupied.Insert(raven.GetPosition());
        if (session.m_PeerRoster) session.m_PeerRoster.LivingPositions(occupied);
    }

    static bool RateSite(vector point, array<vector> occupied, float minimumDistance, float snapAllowance, out int operators, out int approachBand)
    {
        operators = 0;
        approachBand = 0;
        if (occupied.Count() == 0) return true;
        float minRange = minimumDistance - snapAllowance;
        float maxRange = PredateBearingPulse.REACH + snapAllowance;
        float totalDistance;
        foreach (vector position : occupied)
        {
            float east = point[0] - position[0];
            float north = point[2] - position[2];
            float squared = east * east + north * north;
            if (squared < minRange * minRange) return false;
            if (squared > maxRange * maxRange) continue;
            operators++;
            float range = Math.Max(minimumDistance, Math.Sqrt(squared) - snapAllowance);
            totalDistance += range;
        }
        if (operators == 0) return false;
        approachBand = Math.Floor(totalDistance / (operators * 25.0));
        return true;
    }

    static int SitePriority(vector point, int operators, float distanceFromDeath, array<vector> previous, float snapAllowance)
    {
        if (operators == 0) return 0;
        int distanceTier;
        if (distanceFromDeath + snapAllowance >= PREFERRED_DEATH_DISTANCE) distanceTier = 2;
        else if (distanceFromDeath + snapAllowance >= FALLBACK_DEATH_DISTANCE) distanceTier = 1;
        bool newRegion = true;
        foreach (vector position : previous)
        {
            if (PredateHunterBrain.Distance(point, position) + snapAllowance >= REGION_WIDTH) continue;
            newRegion = false;
            break;
        }
        // A fresh area at least 3.5 km out beats revisiting a previous landing area.
        if (distanceTier > 0)
        {
            if (newRegion) return distanceTier + 3;
            return distanceTier + 1;
        }
        if (newRegion) return 1;
        return 0;
    }

    static void ConsiderSite(vector point, vector origin, array<vector> previous, array<vector> occupied, array<Object> exclusions, float minimumDistance, PredateInsertionScore best)
    {
        const float snapRadius = 8;
        int operators, approachBand;
        if (!RateSite(point, occupied, minimumDistance, snapRadius, operators, approachBand)) return;
        float distance = PredateHunterBrain.Distance(point, origin);
        int priority = SitePriority(point, operators, distance, previous, snapRadius);
        if (!best.ImprovesOn(priority, operators, approachBand, distance - snapRadius)) return;
        if (!PredateSpawnPlanner.PlayerTerrain(point)) return;
        float ground = GetGame().SurfaceY(point[0], point[2]);
        vector sampled;
        if (!PredateRoutePlanner.Snap(Vector(point[0], ground, point[2]), snapRadius, sampled)) return;
        if (!RateSite(sampled, occupied, minimumDistance, 0, operators, approachBand)) return;
        distance = PredateHunterBrain.Distance(sampled, origin);
        priority = SitePriority(sampled, operators, distance, previous, 0);
        if (!best.ImprovesOn(priority, operators, approachBand, distance) || !PredateSpawnPlanner.PlayerTerrain(sampled)) return;
        ground = GetGame().SurfaceY(sampled[0], sampled[2]);
        if (Math.AbsFloat(sampled[1] - ground) > 0.8) return;
        sampled[1] = Math.Max(ground, sampled[1]) + 0.15;
        if (GetGame().IsBoxColliding(sampled + "0 0.95 0", vector.Zero, "0.65 1.7 0.65", exclusions)) return;
        best.Operators = operators;
        best.Priority = priority;
        best.ApproachBand = approachBand;
        best.DistanceFromDeath = distance;
        best.Position = sampled;
    }

    static bool RegionalInsertion(vector origin, array<vector> previous, array<vector> occupied, array<Object> exclusions, float minimumDistance, out vector site, out int contacts)
    {
        float limit = GetGame().GetWorld().GetWorldSize() - 10;
        array<ref PredateInsertionScore> regions = {};
        int mostContacts;
        int samplesPerRegion = 20 + occupied.Count() / 5;
        for (float east = 10; east < limit; east += REGION_WIDTH)
        {
            for (float north = 10; north < limit; north += REGION_WIDTH)
            {
                PredateInsertionScore region = new PredateInsertionScore;
                float farEast = Math.Min(east + REGION_WIDTH, limit);
                float farNorth = Math.Min(north + REGION_WIDTH, limit);
                for (int attempt = 0; attempt < samplesPerRegion; attempt++)
                {
                    vector point = Vector(Math.RandomFloat(east, farEast), 0, Math.RandomFloat(north, farNorth));
                    ConsiderSite(point, origin, previous, occupied, exclusions, minimumDistance, region);
                }
                if (region.Operators < 1) continue;
                regions.Insert(region);
                mostContacts = Math.Max(mostContacts, region.Operators);
            }
        }

        // Require multiple contacts when possible, then compare density within the best areas for variety.
        int contactFloor = Math.Min(mostContacts, 2);
        int bestPriority = -1;
        int strongestAreaContacts;
        foreach (PredateInsertionScore area : regions)
        {
            if (area.Operators < contactFloor || area.Priority < bestPriority) continue;
            if (area.Priority > bestPriority)
            {
                bestPriority = area.Priority;
                strongestAreaContacts = 0;
            }
            strongestAreaContacts = Math.Max(strongestAreaContacts, area.Operators);
        }
        int minimumContacts = Math.Max(contactFloor, Math.Ceil(strongestAreaContacts * 0.60));
        int eligibleRegions;
        foreach (PredateInsertionScore candidate : regions)
        {
            if (candidate.Operators < minimumContacts || candidate.Priority != bestPriority) continue;
            eligibleRegions++;
            if (Math.RandomInt(0, eligibleRegions) != 0) continue;
            site = candidate.Position;
            contacts = candidate.Operators;
        }
        return eligibleRegions > 0;
    }

    static bool StartPosition(PredateSession session, PlayerBase staged, out vector site)
    {
        vector origin = session.m_HuntConfig.PlayerStart;
        if (session.m_WorldState.HasPlayerDeathPosition) origin = session.m_WorldState.PlayerDeathPosition;
        array<vector> occupied = {};
        LivingOperators(session, occupied);
        array<Object> exclusions = {};
        if (staged) exclusions.Insert(staged);
        return FindInsertion(origin, session.m_HuntConfig.PlayerStart, session.m_WorldState.RecentPlayerInsertions, occupied, exclusions, site);
    }

    static bool OperatorStartPosition(PredateSession session, PlayerBase player, PredateOperatorRecord record, vector initial, out vector site)
    {
        vector origin = initial;
        if (record.HasDeathPosition) origin = record.DeathPosition;
        array<vector> occupied = {};
        LivingOperators(session, occupied);
        if (player && player.IsAlive()) occupied.Insert(player.GetPosition());
        array<Object> exclusions = {};
        return FindInsertion(origin, initial, record.RecentInsertions, occupied, exclusions, site);
    }

    static bool FindInsertion(vector origin, vector initial, array<vector> previous, array<vector> occupied, array<Object> exclusions, out vector site)
    {
        if (FindInsertionAtSpacing(origin, initial, previous, occupied, exclusions, MINIMUM_DISTANCE, site)) return true;
        // Dense opening rosters already allow closer spacing. Use lower
        // separation only when no site satisfies the usual redeploy limit.
        if (occupied.Count() > 30 && FindInsertionAtSpacing(origin, initial, previous, occupied, exclusions, 1250, site)) return true;
        if (occupied.Count() <= 65) return false;
        if (FindInsertionAtSpacing(origin, initial, previous, occupied, exclusions, 1000, site)) return true;
        return PredateSpawnPlanner.IsSakhal() && FindInsertionAtSpacing(origin, initial, previous, occupied, exclusions, 650, site);
    }

    static bool FindInsertionAtSpacing(vector origin, vector initial, array<vector> previous, array<vector> occupied, array<Object> exclusions, float minimumDistance, out vector site)
    {
        vector regionalSite;
        int regionalContacts;
        bool foundRegion = occupied.Count() > 0 && RegionalInsertion(origin, previous, occupied, exclusions, minimumDistance, regionalSite, regionalContacts);
        if (foundRegion && (regionalContacts >= 2 || occupied.Count() == 1))
        {
            site = regionalSite;
            return true;
        }

        // Look for narrow pockets that the random map samples can miss.
        PredateInsertionScore best = new PredateInsertionScore;
        ConsiderSite(origin, origin, previous, occupied, exclusions, minimumDistance, best);
        ConsiderSite(initial, origin, previous, occupied, exclusions, minimumDistance, best);
        if (PredateSpawnPlanner.IsSakhal())
        {
            foreach (vector reserveSite : PredateSpawnPlanner.s_SakhalReserveSites)
                ConsiderSite(reserveSite, origin, previous, occupied, exclusions, minimumDistance, best);
        }

        // Search around every Operator, including areas far from the death.
        array<float> radii = {minimumDistance + 2, 1550, 1750, 2250, 3000, 4000, PredateBearingPulse.REACH - 2};
        foreach (vector position : occupied)
        {
            float heading = PredateMath.Bearing(position, origin);
            float direct = Math.Clamp(PredateHunterBrain.Distance(position, origin), minimumDistance + 2, PredateBearingPulse.REACH - 2);
            ConsiderSite(position + PredateInformationMath.BearingDirection(heading) * direct, origin, previous, occupied, exclusions, minimumDistance, best);
            foreach (float radius : radii)
            {
                for (int sector = 0; sector < 24; sector++)
                    ConsiderSite(position + PredateInformationMath.BearingDirection(heading + sector * 15) * radius, origin, previous, occupied, exclusions, minimumDistance, best);
            }
        }

        // Fill gaps between the rings, where several receiver ranges overlap.
        if (occupied.Count() > 1)
        {
            float worldSize = GetGame().GetWorld().GetWorldSize();
            for (float east = 250; east < worldSize; east += 500)
                for (float north = 250; north < worldSize; north += 500)
                    ConsiderSite(Vector(east, 0, north), origin, previous, occupied, exclusions, minimumDistance, best);
        }
        else if (occupied.Count() == 0)
        {
            // With no living Operators, find safe terrain close to the death.
            for (float ring = 25; ring <= PredateBearingPulse.REACH; ring += Math.Max(25, ring * 0.17))
            {
                if (best.Operators == 0 && ring >= best.DistanceFromDeath + 8) break;
                for (int index = 0; index < 24; index++)
                    ConsiderSite(origin + PredateInformationMath.BearingDirection(index * 15) * ring, origin, previous, occupied, exclusions, minimumDistance, best);
            }
        }
        if (foundRegion && best.Operators < 2)
        {
            site = regionalSite;
            return true;
        }
        if (best.Operators < 0) return false;

        for (float step = 200; step >= 1; step *= 0.25)
        {
            vector center = best.Position;
            for (int pass = 0; pass < 4; pass++)
            {
                for (int direction = 0; direction < 8; direction++)
                    ConsiderSite(center + PredateInformationMath.BearingDirection(direction * 45) * step, origin, previous, occupied, exclusions, minimumDistance, best);
                if (best.Position == center) break;
                center = best.Position;
            }
        }
        site = best.Position;
        return true;
    }

    static void RetireOldContact(PredateHunterState hunter, PredateObservationMemory memory, eAIBase actor, string playerId, float now, bool tracksOldLife)
    {
        if (actor) actor.m_PredateScoreHuntingPlayer = false;
        bool playerContact;
        if (hunter.Mode == "combat")
        {
            PlayerBase previousTarget;
            if (actor) previousTarget = PlayerBase.Cast(actor.m_PredateVisualTarget);
            if (previousTarget && previousTarget.GetIdentity()) playerContact = previousTarget.GetIdentity().GetId() == playerId;
            else if (!previousTarget) playerContact = tracksOldLife;
        }
        if (hunter.HasGoal && hunter.EvidenceKey != "")
        {
            if (tracksOldLife && hunter.EvidenceKey == hunter.LastSignalKey) playerContact = true;
            foreach (PredateObservation evidence : memory.Entries)
            {
                if (evidence.EventKey == hunter.EvidenceKey && evidence.Source == PredateObservationSource.VISUAL && evidence.SubjectId == playerId)
                    playerContact = true;
            }
        }
        if (!playerContact) return;

        PredateHunterBrain.Hold(hunter, "hold", "Previous player life ended");
        hunter.NextDecisionAt = now;
        hunter.EvidenceKey = "";
        if (actor && actor.m_PredateAppliedGoal != -1) actor.PredateReleaseRoute();
    }

    static void Redeploy(PredateSession session, PlayerBase player)
    {
        PredateWorldState state = session.m_WorldState;
        if (!state.PlayerAwaitingRedeploy || !player.IsAlive() || !player.GetIdentity() || session.m_HuntError != "") return;

        if (!player.m_PredateNativeInsertion)
        {
            vector insertion;
            player.m_PredateSpawnRejected = !StartPosition(session, player, insertion);
            if (player.m_PredateSpawnRejected)
            {
                Print("[Predate] Redeploy delayed: no safe insertion in receiver range.");
                return;
            }
            PredateActorSimulation.SetHeld(player, false);
            player.SetPosition(insertion);
        }
        player.RemoveAllItems();
        session.PredateEquip(player);
        if (session.m_HuntError != "") return;
#ifdef SERVER
        if (state.PlayerWeaponModules) player.PredateTransferWeaponModules(state.PlayerWeaponModules, true);
#endif
        session.m_HuntProgress.PlayerPlacementRevision = 1;

        // Age and resource state survive. Clear only contacts that referred to
        // the previous body, so saved knowledge cannot reacquire the new one.
        string playerId = player.GetIdentity().GetId();
        for (int index = state.Contacts.Count() - 1; index >= 0; index--)
            if (state.Contacts[index].PlayerId == playerId) state.Contacts.Remove(index);
        session.m_MemoryRestored = false;
        PredateSignalService.PlayerRedeployed(session);
        RetireOldContact(state.Hunter, state.Observations, session.m_HuntBot, playerId, state.SimulationSeconds, state.HuntReceiver.TargetId == "player-01");
        state.HuntReceiver.Retire("player-01");
        if (session.m_PeerRoster) session.m_PeerRoster.RetirePlayerContact(playerId, state.SimulationSeconds);

        player.m_PredateStagedForDeployment = false;
        PredateActorSimulation.SetHeld(player, false);
        player.SetAllowDamage(true);
        state.PlayerLife++;
        state.PlayerLifeStartedAt = state.SimulationSeconds;
        state.PlayerSupplies = new PredateSupplyPlan;
        state.PlayerAwaitingRedeploy = false;
        if (session.m_Score) session.m_Score.Life(state.PlayerLife, true);
        player.m_PredateScoringLife = state.PlayerLife;
        state.HasPlayerDeathPosition = false;
        player.m_PredateNativeInsertion = false;
        player.m_PredateSpawnRejected = false;
        player.m_PredateDeploymentPosition = player.GetPosition();
        state.RecentPlayerInsertions.Insert(player.m_PredateDeploymentPosition);
        if (state.RecentPlayerInsertions.Count() > INSERTION_HISTORY_SIZE) state.RecentPlayerInsertions.RemoveOrdered(0);
        player.m_PredateHasDeploymentPosition = true;
        player.Save();
        session.PredateSave();
        Print("[Predate] Player redeployed into life " + state.PlayerLife + "; operator state retained.");
    }
}
