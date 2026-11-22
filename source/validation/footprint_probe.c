class PredateFootprintProbeReport
{
    int Checks;
    int Failures;
    int NaturalMarks;
    float SimulationSeconds;
    int PlayerTrail;

    void Check(bool outcome, string label)
    {
        Checks++;
        if (outcome) return;
        Failures++;
        Print("[PredateFootprintProbe] FAIL: " + label);
    }
}

class PredateFootprintValidationMission : MissionServer
{
    ref PredateFootprintProbeReport m_Report = new PredateFootprintProbeReport;
    int m_Phase;
    float m_Next;
    PlayerBase m_Observer;
    vector m_Origin;
    vector m_Ground;
    int m_Surface;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Phase == 4 || !m_PredateSession.m_ActorsActive || !m_PredateSession.m_PeerRoster) return;
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        PredateFootprintLedger ledger = m_PredateSession.m_WorldState.Footprints;
        if (now < m_Next) return;
        if (m_Phase == 0 && now >= 12)
        {
            m_Report.NaturalMarks = ledger.Marks.Count();
            m_Report.Check(m_Report.NaturalMarks > 0 && ledger.Validate(now), "native operator movement records valid marks");
            FindObserver();
            m_Report.Check(m_Observer && m_Observer.m_PredateFootprintLookReceivedAt > 0, "server receives the actual player's gaze");
            if (!m_Observer) { Finish(now); return; }
            m_Origin = m_Observer.GetPosition();
            vector eyes = m_Observer.GetBonePositionWS(m_Observer.GetBoneIndexByName("head"));
            vector gaze = m_Observer.m_PredateFootprintLook;
            vector facing = gaze;
            facing[1] = 0;
            facing.Normalize();
            for (float distance = 5; distance <= 7.5; distance += 0.25)
            {
                vector ahead = m_PredateSession.PredateGround(m_Origin + facing * distance);
                m_Surface = PredateFootprintSensor.SurfaceAt(ahead, m_Ground);
                if (m_Surface == PredateFootprintSurface.HARD || !PredateFootprintSensor.Visible(eyes, gaze, m_Ground, m_Observer)) continue;
                if (!PredateFootprintSensor.FacingGround(eyes, gaze, m_Ground, true)) continue;
                m_Report.PlayerTrail = ledger.Record(m_Ground, m_Surface, now, "lynx-01", PredateMath.Bearing(m_Origin, m_Ground), 0, true);
                if (m_Report.PlayerTrail != 0) break;
            }
            m_Report.Check(m_Report.PlayerTrail > 0, "visible directionally accurate track can be placed ahead of player");
            if (!m_Report.PlayerTrail) { Finish(now); return; }
            vector lateral = Vector(facing[2], 0, -facing[0]);
            vector own = m_PredateSession.PredateGround(m_Ground + facing * 0.45 + lateral * 0.15);
            ledger.Record(own, m_Surface, now, "player-01");
            m_Next = now + 2.5;
            m_Phase = 1;
        }
        else if (m_Phase == 1)
        {
            PredateFootprintTrail trail = ledger.m_TrailIndex.Get(m_Report.PlayerTrail);
            m_Report.Check(trail && trail.Readers.Find("player-01") >= 0, "actual player gaze reads the foreign trail");
            m_Report.Check(m_Observer.PredateFootprintSensorForActor().m_Own != null, "own print visible beside the foreign one");
            m_Observer.SetPosition(m_Origin + "0 0 12");
            m_Next = now + 1.5;
            m_Phase = 2;
        }
        else if (m_Phase == 2)
        {
            ledger.Record(m_Ground, m_Surface, now, "lynx-01", 45, m_Report.PlayerTrail, true);
            m_Observer.SetPosition(m_Origin);
            m_Next = now + 5;
            m_Phase = 3;
        }
        else if (m_Phase == 3)
        {
            PredateFootprintTrail seen = ledger.m_TrailIndex.Get(m_Report.PlayerTrail);
            m_Report.Check(seen && seen.Readers.Find("player-01") >= 0, "record of reading survives a return to the same trail");
            Finish(now);
        }
    }

    void FindObserver()
    {
        array<Man> players = {};
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            m_Observer = player;
            return;
        }
    }

    void Finish(float now)
    {
        m_Phase = 4;
        m_Report.SimulationSeconds = now;
        m_PredateSession.SetPaused(true);
        m_PredateSession.PredateSave();
        m_Report.Check(m_PredateSession.m_HuntError == "", "footprint ledger and read receipts save with the operation");
        string error;
        JsonFileLoader<PredateFootprintProbeReport>.SaveFile("$profile:FootprintsTrial.json", m_Report, error);
        Print("[PredateFootprintProbe] COMPLETE: checks=" + m_Report.Checks + "; failures=" + m_Report.Failures);
    }
}
