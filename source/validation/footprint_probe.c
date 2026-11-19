class PredateFootprintProbeReport
{
    int Checks;
    int Failures;
    int NaturalMarks;
    float SimulationSeconds;
    string EventKey;

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
    bool m_Done;
    float m_ScannedAt;
    int m_HintPhase;
    int m_PlayerMark;
    float m_HintAt;
    PlayerBase m_Observer;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Done || !m_PredateSession.m_ActorsActive || !m_PredateSession.m_PeerRoster) return;
        float now = m_PredateSession.m_WorldState.SimulationSeconds;
        PredateFootprintLedger ledger = m_PredateSession.m_WorldState.Footprints;
        PredatePeerBinding lynx = m_PredateSession.m_PeerRoster.Find("lynx-01");
        if (m_Report.EventKey == "" && now >= 13)
        {
            VerifyNativeMarks(lynx, ledger, now);
            if (m_Report.EventKey == "") Finish(now);
        }
        else if (m_Report.EventKey != "" && m_HintPhase == 0 && now >= m_ScannedAt + 1.3)
        {
            m_Report.Check(lynx.State.Hunter.EvidenceKey == m_Report.EventKey, "native hunter considered the visually earned footprint");
            VerifyPlayerContext(ledger, now);
            if (m_PlayerMark == 0) Finish(now);
        }
        else if (m_HintPhase == 1 && now >= m_HintAt + 2.5)
        {
            m_Report.Check(PredateFootprintService.PlayerClue(m_PredateSession, m_Observer) == 2, "player clue contains only a recent mark category");
            for (int index = ledger.Marks.Count() - 1; index >= 0; index--)
                if (ledger.Marks[index].Serial == m_PlayerMark) ledger.Marks.RemoveOrdered(index);
            m_HintAt = now;
            m_HintPhase = 2;
        }
        else if (m_HintPhase == 2 && now >= m_HintAt + 2.5)
        {
            m_Report.Check(PredateFootprintService.PlayerClue(m_PredateSession, m_Observer) == 0, "missing mark clears the contextual clue");
            Finish(now);
        }
    }

    void VerifyPlayerContext(PredateFootprintLedger ledger, float now)
    {
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase observer = PlayerBase.Cast(man);
            if (!observer || !observer.GetIdentity()) continue;
            m_Observer = observer;
            break;
        }
        m_Report.Check(m_Observer && PredateFootprintService.PlayerClue(m_PredateSession, m_Observer) == 0, "a distant trace does not produce a player hint");
        if (!m_Observer) return;
        vector facing = m_Observer.GetDirection();
        vector ground;
        vector position = m_Observer.GetPosition();
        for (float distance = 4; distance <= 8; distance += 1)
        {
            vector ahead = m_PredateSession.PredateGround(position + facing * distance);
            int surface = PredateFootprintSensor.SurfaceAt(ahead, ground);
            if (surface == PredateFootprintSurface.HARD) continue;
            vector eyes = m_Observer.GetBonePositionWS(m_Observer.GetBoneIndexByName("head"));
            if (!PredateFootprintSensor.Visible(eyes, facing, ground, m_Observer)) continue;
            m_PlayerMark = ledger.Record(ground, surface, now, "lynx-01", PredateMath.Bearing(position, ground));
            if (m_PlayerMark != 0) break;
        }
        m_Report.Check(m_PlayerMark != 0, "test can place a physically visible clue near the player");
        m_HintAt = now;
        m_HintPhase = 1;
    }

    void VerifyNativeMarks(PredatePeerBinding observer, PredateFootprintLedger ledger, float now)
    {
        m_Report.NaturalMarks = ledger.Marks.Count();
        m_Report.Check(ledger.Validate(now) && m_Report.NaturalMarks >= 4, "native movement creates bounded, valid marks");
        bool lynxMarked, rookMarked;
        bool mobileActorsOnly = true;
        foreach (PredateFootprint mark : ledger.Marks)
        {
            if (mark.Creator == "lynx-01") lynxMarked = true;
            if (mark.Creator == "rook-01") rookMarked = true;
            if (mark.Creator == "player-01") mobileActorsOnly = false;
        }
        m_Report.Check(lynxMarked && rookMarked, "marks come from distinct moving rivals");
        m_Report.Check(mobileActorsOnly, "stationary player leaves no marks");

        eAIBase actor = observer.Actor;
        vector position = actor.GetPosition();
        vector facing = actor.GetDirection();
        facing[1] = 0;
        facing.Normalize();
        vector eyes = actor.GetBonePositionWS(actor.GetBoneIndexByName("head"));
        vector ground;
        vector ahead = m_PredateSession.PredateGround(position + facing * 2.0);
        int surface = PredateFootprintSensor.SurfaceAt(ahead, ground);
        m_Report.Check(surface != PredateFootprintSurface.HARD, "native surface at the inspection point accepts a mark");
        vector elevated;
        m_Report.Check(PredateFootprintSensor.SurfaceAt(ahead + "0 3 0", elevated) == PredateFootprintSurface.HARD, "an actor above ground leaves no terrain mark");
        m_Report.Check(PredateFootprintSensor.Visible(eyes, facing, ground, actor), "forward ground can be inspected");
        m_Report.Check(!PredateFootprintSensor.Visible(eyes, facing * -1.0, ground, actor), "observer cannot inspect a mark behind them");
        if (surface == PredateFootprintSurface.HARD) return;

        int serial = ledger.Record(ground, surface, now, "rook-01");
        m_Report.EventKey = "footprint-" + serial;
        m_ScannedAt = now;
        PredateFootprintSensor sensor = actor.PredateFootprintSensorForActor();
        sensor.m_NextScanAt = now;
        sensor.Scan(actor, now, ledger);
        PredateObservation clue = observer.State.Memory.Latest("", PredateObservationSource.FOOTPRINT, now);
        m_Report.Check(clue && clue.EventKey == m_Report.EventKey && clue.ObserverId == "lynx-01" && clue.Estimate == ground && !clue.HasBearing, "real visibility scan adds only the anonymous observed ground clue");
    }

    void Finish(float now)
    {
        m_Done = true;
        m_Report.SimulationSeconds = now;
        m_PredateSession.SetPaused(true);
        m_PredateSession.PredateSave();
        m_Report.Check(m_PredateSession.m_HuntError == "", "footprint ledger saves with the operation");
        string error;
        JsonFileLoader<PredateFootprintProbeReport>.SaveFile("$profile:FootprintsTrial.json", m_Report, error);
        Print("[PredateFootprintProbe] COMPLETE: checks=" + m_Report.Checks + "; failures=" + m_Report.Failures);
    }
}
