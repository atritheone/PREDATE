class PredateFootprintService
{
    static int PlayerClue(PredateSession session, PlayerBase player, out vector ground = vector.Zero, out int heading = 0)
    {
        if (!session.m_HuntReady || !session.m_HuntConfig.EnableFootprints || !player.IsAlive() || session.m_WorldState.PlayerAwaitingRedeploy) return 0;
        PredateFootprint mark = player.PredateFootprintSensorForActor().m_Foreign;
        float now = session.m_WorldState.SimulationSeconds;
        if (!PredateFootprintLedger.Reachable(mark, player.GetPosition(), now)) return 0;
        ground = mark.Position;
        heading = mark.Heading;
        if (now - mark.CreatedAt <= (mark.ExpiresAt - mark.CreatedAt) * 0.5) return 2;
        return 1;
    }

    static void Tick(PredateSession session, PlayerBase player)
    {
        if (!session.m_HuntConfig.EnableFootprints) return;
        PredateFootprintLedger ledger = session.m_WorldState.Footprints;
        float now = session.m_WorldState.SimulationSeconds;
        ledger.Prune(now);
        PredateFootprintSensor sensor = player.PredateFootprintSensorForActor();
        sensor.Stamp(player, "player-01", now, ledger);
        if (!session.m_WorldState.PlayerAwaitingRedeploy)
        {
            PredateObservation clue = sensor.Scan(player, "player-01", now, ledger, true);
            if (clue) player.m_PredateFootprintNotice = PredateFootprintLedger.Notice(clue);
        }
        StampAndScan(session.m_HuntBot, now, ledger);
        if (!session.m_PeerRoster) return;
        foreach (PredatePeerBinding peer : session.m_PeerRoster.m_Bindings)
            StampAndScan(peer.Actor, now, ledger);
    }

    static void StampAndScan(eAIBase actor, float now, PredateFootprintLedger ledger)
    {
        if (!actor || !actor.IsAlive() || !actor.m_PredateMemory) return;
        PredateFootprintSensor sensor = actor.PredateFootprintSensorForActor();
        sensor.Stamp(actor, actor.m_PredateOperatorId, now, ledger);
        // Phase the initial scans across roster slots. Scans remain local to each operator.
        if (sensor.m_NextScanAt == 0)
        {
            int slot = PredateProtocol.OperatorSlot(actor.m_PredateOperatorId);
            int phase = slot % 2;
            sensor.m_NextScanAt = now + phase * 0.25;
        }
        PredateObservation clue = sensor.Scan(actor, actor.m_PredateOperatorId, now, ledger, false);
        if (clue) actor.m_PredateMemory.Add(clue, now);
    }
}
