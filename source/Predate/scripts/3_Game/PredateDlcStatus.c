class PredateDlcStatus
{
    string Nonce;
    bool Sakhal;
    bool Nasdara;
}

modded class DayZGame
{
    float m_PredateDlcReadyAt;
    bool m_PredateDlcReported;

    override void OnUpdate(bool doSim, float timeslice)
    {
        super.OnUpdate(doSim, timeslice);
        if (IsDedicatedServer() || m_PredateDlcReported || GetGameState() != DayZGameState.MAIN_MENU || IsLoading()) return;
        string nonce;
        if (!CommandlineGetParam("predateDlcProbe", nonce)) return;
        ContentDLC content = GetContentDLCService();
        if (!content || !GetMission() || !GetUIManager().GetMenu()) return;
        if (!m_PredateDlcReadyAt) m_PredateDlcReadyAt = GetTickTime() + 1.5;
        if (GetTickTime() < m_PredateDlcReadyAt) return;
        PredateDlcStatus status = new PredateDlcStatus;
        status.Nonce = nonce;
        status.Sakhal = content.IsDLCInstalled(EDLCId.FROSTLINE);
        status.Nasdara = content.IsDLCInstalled(EDLCId.BADLANDS);
        string error;
        if (JsonFileLoader<PredateDlcStatus>.SaveFile("$profile:PredateDlcStatus.json", status, error))
        {
            m_PredateDlcReported = true;
            RequestExit(0);
        }
    }
}
