// Player life changes are independent of the operation and its Raven record.
class PredateDeployment
{
    static void ObserveDeath(PredateSession session, PlayerBase player)
    {
        if (!session || !session.m_HuntReady || session.m_HuntError != "" || !player || player.m_PredateDeathRecorded || !player.GetIdentity()) return;
        if (player.IsAlive() && player.GetHealth("", "Health") > 0) return;
        player.m_PredateDeathRecorded = true;
        PredateWorldState state = session.m_WorldState;
        if (state.PlayerAwaitingRedeploy) return;
        state.PlayerAwaitingRedeploy = true;
        state.PlayerLastDeathAt = state.SimulationSeconds;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(session.PredateSave);
        Print("[Predate] Player life " + state.PlayerLife + " ended. Waiting for redeployment.");
    }

    static void StagePlayer(PlayerBase player)
    {
        if (player.m_PredateStagedForDeployment) return;
        player.m_PredateStagedForDeployment = true;
        player.RemoveAllItems();
        player.SetAllowDamage(false);
        player.DisableSimulation(true);
    }

    static bool ClearOfOperators(PredateSession session, vector position, float minimumDistance)
    {
        eAIBase raven = session.m_HuntBot;
        if (raven && raven.IsAlive() && PredateHunterBrain.Distance(position, raven.GetPosition()) < minimumDistance) return false;
        return !session.m_PeerRoster || session.m_PeerRoster.ClearOfActors(position, minimumDistance);
    }

    static const float MINIMUM_DISTANCE = 1500;

    static void OperatorOccupancy(PredateSession session, PlayerBase player, array<vector> occupied)
    {
        occupied.Insert(player.GetPosition());
        eAIBase raven = session.m_HuntBot;
        if (raven && raven.IsAlive()) occupied.Insert(raven.GetPosition());
        if (session.m_PeerRoster) session.m_PeerRoster.LivingPositions(occupied);
    }

    static bool StartPosition(PredateSession session, bool customSpawn, vector point, out vector site)
    {
        vector configured = session.m_HuntConfig.PlayerStart;
        if (customSpawn) configured = point;
        if (PredateSpawnPlanner.PlayerTerrain(configured) && ClearOfOperators(session, configured, MINIMUM_DISTANCE))
        {
            site = session.PredateGround(configured);
            return true;
        }
        if (customSpawn) return false;
        // Check increasingly distant insertion points without moving any opponent.
        for (float radius = MINIMUM_DISTANCE; radius <= 2200; radius += 300)
        {
            for (int index = 0; index < 8; index++)
            {
                vector offset = PredateInformationMath.BearingDirection(index * 45) * radius;
                vector candidate = configured + offset;
                if (!PredateSpawnPlanner.PlayerTerrain(candidate) || !ClearOfOperators(session, candidate, MINIMUM_DISTANCE)) continue;
                site = session.PredateGround(candidate);
                return true;
            }
        }
        return false;
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
        if (!state.PlayerAwaitingRedeploy || !player.IsAlive() || !player.m_PredateStagedForDeployment || session.m_HuntError != "") return;

        vector insertion;
        player.m_PredateSpawnRejected = !StartPosition(session, player.m_PredateCustomRedeploy, player.m_PredateRedeployPoint, insertion);
        if (player.m_PredateSpawnRejected) return;
        player.SetPosition(insertion);
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
        RetireOldContact(state.Hunter, state.Observations, session.m_HuntBot, playerId, state.SimulationSeconds, true);
        if (session.m_PeerRoster) session.m_PeerRoster.RetirePlayerContact(playerId, state.SimulationSeconds);

        player.m_PredateStagedForDeployment = false;
        player.DisableSimulation(false);
        player.SetAllowDamage(true);
        state.PlayerLife++;
        state.PlayerLifeStartedAt = state.SimulationSeconds;
        state.PlayerSupplies = new PredateSupplyPlan;
        state.PlayerAwaitingRedeploy = false;
        player.Save();
        session.PredateSave();
        Print("[Predate] Player redeployed into life " + state.PlayerLife + "; operator state retained.");
    }
}
