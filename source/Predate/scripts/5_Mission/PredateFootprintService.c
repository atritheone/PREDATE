class PredateFootprintService
{
    static int PlayerClue(PredateSession session, PlayerBase player, out vector ground = vector.Zero, out int heading = 0)
    {
        if (!session.m_HuntReady || !session.m_HuntConfig.EnableFootprints || !player.IsAlive() || session.m_WorldState.PlayerAwaitingRedeploy) return 0;
        PredateFootprintSensor sensor = player.PredateFootprintSensorForActor();
        PredateFootprint observed;
        PredateObservation clue = sensor.Observe(player, "player-01", session.m_WorldState.SimulationSeconds, session.m_WorldState.Footprints, observed);
        if (!clue) return 0;
        ground = observed.Position;
        heading = observed.Heading;
        if (clue.Confidence >= 0.4) return 2;
        return 1;
    }

    static void Tick(PredateSession session, PlayerBase player)
    {
        if (!session.m_HuntConfig.EnableFootprints) return;
        PredateFootprintLedger ledger = session.m_WorldState.Footprints;
        float now = session.m_WorldState.SimulationSeconds;
        ledger.Prune(now);
        player.PredateFootprintSensorForActor().Stamp(player, "player-01", now, ledger);
        StampAndScan(session.m_HuntBot, now, ledger);
        if (!session.m_PeerRoster) return;
        foreach (PredatePeerBinding peer : session.m_PeerRoster.m_Bindings)
            StampAndScan(peer.Actor, now, ledger);
    }

    static void StampAndScan(eAIBase actor, float now, PredateFootprintLedger ledger)
    {
        if (!actor || !actor.IsAlive()) return;
        PredateFootprintSensor sensor = actor.PredateFootprintSensorForActor();
        sensor.Stamp(actor, actor.m_PredateOperatorId, now, ledger);
        sensor.Scan(actor, now, ledger);
    }
}
