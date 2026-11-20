class PredateInsertionScore
{
    int Operators = -1;
    int ApproachBand;
    float DistanceFromDeath;
    vector Position;

    bool ImprovesOn(int operators, int approachBand, float distanceFromDeath)
    {
        if (operators != Operators) return operators > Operators;
        if (approachBand != ApproachBand) return approachBand < ApproachBand;
        return distanceFromDeath < DistanceFromDeath;
    }
}

// Player life changes are independent of the operation and its Raven record.
class PredateDeployment
{
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
        PredateWorldState state = session.m_WorldState;
        RememberCharacter(state, player);
        if (state.PlayerAwaitingRedeploy) return;
        state.PlayerAwaitingRedeploy = true;
        state.PlayerLastDeathAt = state.SimulationSeconds;
        state.PlayerDeathPosition = player.GetPosition();
        state.HasPlayerDeathPosition = PredateInformationMath.FiniteVector(state.PlayerDeathPosition);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(session.PredateSave);
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

    static void LivingOperators(PredateSession session, array<vector> occupied)
    {
        eAIBase raven = session.m_HuntBot;
        if (raven && raven.IsAlive()) occupied.Insert(raven.GetPosition());
        if (session.m_PeerRoster) session.m_PeerRoster.LivingPositions(occupied);
    }

    static void OperatorOccupancy(PredateSession session, PlayerBase player, array<vector> occupied)
    {
        occupied.Insert(player.GetPosition());
        LivingOperators(session, occupied);
    }

    static bool RateSite(vector point, array<vector> occupied, float snapAllowance, out int operators, out int approachBand)
    {
        operators = 0;
        approachBand = 0;
        if (occupied.Count() == 0) return true;
        float minRange = MINIMUM_DISTANCE - snapAllowance;
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
            totalDistance += Math.Max(MINIMUM_DISTANCE, Math.Sqrt(squared) - snapAllowance);
        }
        if (operators == 0) return false;
        // Use 25 m bands of average approach distance so near-equivalent
        // insertions are resolved by proximity to the last life.
        approachBand = Math.Floor(totalDistance / (operators * 25.0));
        return true;
    }

    static void ConsiderSite(vector point, vector origin, array<vector> occupied, array<Object> exclusions, PredateInsertionScore best)
    {
        const float snapRadius = 8;
        int operators, approachBand;
        if (!RateSite(point, occupied, snapRadius, operators, approachBand)) return;
        if (!best.ImprovesOn(operators, approachBand, PredateHunterBrain.Distance(point, origin) - snapRadius)) return;
        if (!PredateSpawnPlanner.PlayerTerrain(point)) return;
        float ground = GetGame().SurfaceY(point[0], point[2]);
        vector sampled;
        if (!PredateRoutePlanner.Snap(Vector(point[0], ground, point[2]), snapRadius, sampled)) return;
        if (!RateSite(sampled, occupied, 0, operators, approachBand)) return;
        float distance = PredateHunterBrain.Distance(sampled, origin);
        if (!best.ImprovesOn(operators, approachBand, distance) || !PredateSpawnPlanner.PlayerTerrain(sampled)) return;
        ground = GetGame().SurfaceY(sampled[0], sampled[2]);
        if (Math.AbsFloat(sampled[1] - ground) > 0.8) return;
        sampled[1] = Math.Max(ground, sampled[1]) + 0.15;
        if (GetGame().IsBoxColliding(sampled + "0 0.95 0", vector.Zero, "0.65 1.7 0.65", exclusions)) return;
        best.Operators = operators;
        best.ApproachBand = approachBand;
        best.DistanceFromDeath = distance;
        best.Position = sampled;
    }

    static bool StartPosition(PredateSession session, PlayerBase staged, out vector site)
    {
        vector origin = session.m_HuntConfig.PlayerStart;
        if (session.m_WorldState.HasPlayerDeathPosition) origin = session.m_WorldState.PlayerDeathPosition;
        array<vector> occupied = {};
        LivingOperators(session, occupied);
        array<Object> exclusions = {};
        if (staged) exclusions.Insert(staged);
        PredateInsertionScore best = new PredateInsertionScore;
        ConsiderSite(origin, origin, occupied, exclusions, best);
        ConsiderSite(session.m_HuntConfig.PlayerStart, origin, occupied, exclusions, best);

        // Search around every Operator, including areas far from the death.
        array<float> radii = {MINIMUM_DISTANCE + 2, 1550, 1750, 2250, 3000, 4000, PredateBearingPulse.REACH - 2};
        foreach (vector position : occupied)
        {
            float heading = PredateMath.Bearing(position, origin);
            float direct = Math.Clamp(PredateHunterBrain.Distance(position, origin), MINIMUM_DISTANCE + 2, PredateBearingPulse.REACH - 2);
            ConsiderSite(position + PredateInformationMath.BearingDirection(heading) * direct, origin, occupied, exclusions, best);
            foreach (float radius : radii)
            {
                for (int sector = 0; sector < 24; sector++)
                    ConsiderSite(position + PredateInformationMath.BearingDirection(heading + sector * 15) * radius, origin, occupied, exclusions, best);
            }
        }

        // Fill gaps between the rings, where several receiver ranges overlap.
        if (occupied.Count() > 1)
        {
            float worldSize = GetGame().GetWorld().GetWorldSize();
            for (float east = 250; east < worldSize; east += 500)
                for (float north = 250; north < worldSize; north += 500)
                    ConsiderSite(Vector(east, 0, north), origin, occupied, exclusions, best);
        }
        else if (occupied.Count() == 0)
        {
            // With no living Operators, find safe terrain close to the death.
            for (float ring = 25; ring <= PredateBearingPulse.REACH; ring += Math.Max(25, ring * 0.17))
            {
                if (best.Operators == 0 && ring >= best.DistanceFromDeath + 8) break;
                for (int index = 0; index < 24; index++)
                    ConsiderSite(origin + PredateInformationMath.BearingDirection(index * 15) * ring, origin, occupied, exclusions, best);
            }
        }
        if (best.Operators < 0) return false;

        for (float step = 200; step >= 1; step *= 0.25)
        {
            vector center = best.Position;
            for (int pass = 0; pass < 4; pass++)
            {
                for (int direction = 0; direction < 8; direction++)
                    ConsiderSite(center + PredateInformationMath.BearingDirection(direction * 45) * step, origin, occupied, exclusions, best);
                if (best.Position == center) break;
                center = best.Position;
            }
        }
        site = best.Position;
        return true;
    }

    static void RetireOldContact(PredateHunterState hunter, PredateObservationMemory memory, eAIBase actor, string playerId, float now, bool tracksOldLife)
    {
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
            player.DisableSimulation(false);
            player.SetPosition(insertion);
        }
        player.RemoveAllItems();
        session.PredateEquip(player);
        if (session.m_HuntError != "") return;
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
        player.DisableSimulation(false);
        player.SetAllowDamage(true);
        state.PlayerLife++;
        state.PlayerLifeStartedAt = state.SimulationSeconds;
        state.PlayerSupplies = new PredateSupplyPlan;
        state.PlayerAwaitingRedeploy = false;
        state.HasPlayerDeathPosition = false;
        player.m_PredateNativeInsertion = false;
        player.m_PredateSpawnRejected = false;
        player.Save();
        session.PredateSave();
        Print("[Predate] Player redeployed into life " + state.PlayerLife + "; operator state retained.");
    }
}
