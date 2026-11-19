// Purpose-built local session control; active only when launched by PREDATE.
modded class DayZGame
{
    ref PredateClientAgent m_PredateClientAgent;
    bool m_PredateSessionQuitAck;

    override void OnUpdate(bool doSim, float timeslice)
    {
        super.OnUpdate(doSim, timeslice);
        if (IsDedicatedServer()) return;
        if (!m_PredateClientAgent)
        {
            string id;
            if (!CommandlineGetParam("predateSessionId", id)) return;
            m_PredateClientAgent = new PredateClientAgent(id);
        }
        m_PredateClientAgent.Update(timeslice);
    }
}

class PredateClientAgent
{
    ref PredateClientStatus m_Status;
    float m_Timer;
    float m_QuitTime;
    bool m_QuitRequested;
    bool m_UserPaused;
    bool m_ServerFrozen;
    bool m_PauseDisconnected;
    bool m_ResumePending;
    bool m_ResumeAuthorized;
    bool m_DeployAfterRespawn;
    bool m_CustomRedeploy;
    vector m_RedeployPoint;

    void PredateClientAgent(string id)
    {
        m_Status = new PredateClientStatus;
        m_Status.SessionId = id;
    }

    void RequestQuit()
    {
        if (m_QuitRequested) return;
        m_QuitRequested = true;
        m_QuitTime = 0;
        m_Status.Phase = "saving";
        if (GetGame().GetGameState() != DayZGameState.IN_GAME)
        {
            GetGame().RequestExit(0);
            return;
        }
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(1);
        rpc.Write(m_Status.SessionId);
        rpc.Send(GetGame().GetPlayer(), PredateProtocol.SESSION_REQUEST, true);
        Print("[Predate] Save and Quit requested.");
    }

    void RequestPause()
    {
        if (m_QuitRequested || m_ResumePending || m_Status.Error != "") return;
        m_UserPaused = true;
    }

    void RequestResume()
    {
        if (m_QuitRequested || m_ResumePending) return;
        m_UserPaused = false;
        m_ResumePending = true;
    }

    bool HandlePausedDisconnect(int errorCode)
    {
        bool expectedTimeout = errorCode == 0x00040004 && (m_UserPaused || m_ServerFrozen || m_ResumePending);
        bool retryableJoin = m_ResumePending && (errorCode == 0x00020006 || errorCode == 0x00040033);
        if (!expectedTimeout && !retryableJoin) return false;
        m_PauseDisconnected = true;
        m_Status.Error = "";
        Print("[Predate] Paused connection released; the world remains held for Resume.");
        return true;
    }

    void ConnectionFailed(int errorCode)
    {
        if (GetDayZGame().m_loading) GetDayZGame().m_loading.PredateFinishJoin();
        m_Status.Error = "DayZ could not join the local world (" + ErrorModuleHandler.GetErrorHex(errorCode) + ").";
        if (errorCode == 0x00040031)
            m_Status.Error = "DayZ could not obtain a Steam login for the local world (0x00040031). Start Steam online, then retry Continue. Steam Offline Mode did not work on this build.";
    }

    void Update(float seconds)
    {
        m_Timer += seconds;
        if (m_QuitRequested)
        {
            m_QuitTime += seconds;
            if (GetDayZGame().m_PredateSessionQuitAck)
            {
                m_Status.Phase = "exiting";
                string exitError;
                JsonFileLoader<PredateClientStatus>.SaveFile("$profile:PredateClientStatus.json", m_Status, exitError);
                Print("[Predate] Save acknowledged; closing the game normally.");
                GetGame().RequestExit(0);
                return;
            }
            if (m_QuitTime > 30)
            {
                m_Status.Error = "Save acknowledgement timed out. Game retained; retry Save and Quit.";
                m_QuitRequested = false;
            }
        }
        if (m_Timer < 0.25) return;
        m_Timer = 0;
        Mission currentMission = GetGame().GetMission();
        m_Status.Paused = m_UserPaused;
        m_Status.PauseDisconnected = m_PauseDisconnected;
        bool pauseMenu = GetGame().GetUIManager().IsMenuOpen(MENU_INGAME);
        m_Status.PauseMenu = pauseMenu;
        int gameState = GetGame().GetGameState();
        m_Status.GameState = gameState;
        if ((m_UserPaused || m_ResumePending) && gameState != DayZGameState.IN_GAME && !GetDayZGame().IsLoading() && !GetDayZGame().IsConnecting() && !pauseMenu)
            GetGame().GetUIManager().EnterScriptedMenu(MENU_INGAME, null);
        if (m_ResumePending && GetGame().GetGameState() == DayZGameState.IN_GAME && GetGame().GetPlayer())
            m_PauseDisconnected = false;
        if (!m_QuitRequested)
        {
            m_Status.Phase = "menu";
            if (GetGame().GetGameState() == DayZGameState.IN_GAME) m_Status.Phase = "connected";
        }
        if (FileExist("$profile:PredateClientCommand.json"))
        {
            PredateClientCommand command = new PredateClientCommand;
            string readError;
            if (JsonFileLoader<PredateClientCommand>.LoadFile("$profile:PredateClientCommand.json", command, readError))
            {
                if (command.Schema == 1 && command.SessionId == m_Status.SessionId && command.Sequence > m_Status.Acknowledged)
                {
                    m_Status.Acknowledged = command.Sequence;
                    m_Status.Error = "";
                    if (command.Action == "quit") RequestQuit();
                    else if (command.Action == "server-closed")
                    {
                        // The server has already failed; an acknowledgement cannot arrive.
                        m_Status.Phase = "exiting";
                        GetGame().RequestExit(0);
                        return;
                    }
                    else if (command.Action == "connect" && !m_QuitRequested)
                    {
                        if (GetGame().GetGameState() != DayZGameState.IN_GAME)
                            GetDayZGame().ConnectFromServerBrowserEx("127.0.0.1", command.Port, command.QueryPort, command.Password);
                    }
                    else if (command.Action == "disconnect") GetDayZGame().DisconnectSessionForce();
                    else if (command.Action == "pause" && currentMission) currentMission.Pause();
                    else if (command.Action == "paused") m_ServerFrozen = true;
                    else if (command.Action == "request-resume") RequestResume();
                    else if (command.Action == "resume" && currentMission)
                    {
                        m_UserPaused = false;
                        m_ServerFrozen = false;
                        m_PauseDisconnected = false;
                        m_ResumePending = false;
                        m_ResumeAuthorized = true;
                        currentMission.Continue();
                        m_ResumeAuthorized = false;
                    }
                }
            }
        }
        string error;
        JsonFileLoader<PredateClientStatus>.SaveFile("$profile:PredateClientStatus.json", m_Status, error);
    }
}

modded class ErrorHandlerModuleScript
{
    protected override void OnErrorThrown(int errorCode, owned string additionalInfo = "")
    {
        DayZGame game = GetDayZGame();
        if (game && game.m_PredateClientAgent && game.m_PredateClientAgent.HandlePausedDisconnect(errorCode)) return;
        super.OnErrorThrown(errorCode, additionalInfo);
        if (!game || !game.m_PredateClientAgent) return;
        ErrorCategory category = GetCategory();
        if (category == ErrorCategory.ConnectErrorClient || category == ErrorCategory.ConnectErrorServer || category == ErrorCategory.ConnectErrorScript || category == ErrorCategory.ClientKicked)
            game.m_PredateClientAgent.ConnectionFailed(errorCode);
    }
}
