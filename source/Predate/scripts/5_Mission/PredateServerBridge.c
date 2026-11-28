class PredateServerBridge
{
    PredateSession m_Session;
    ref PredateSessionStatus m_Status = new PredateSessionStatus;
    float m_Timer;
    float m_StatusElapsed;
    bool m_Published;
    bool m_Closing;
    int m_StopPublishRetries;
    string m_LastCommand;
    ref array<Man> m_StatusPlayers = {};

    void PredateServerBridge(PredateSession session)
    {
        m_Session = session;
        string sessionId;
        GetGame().CommandlineGetParam("predateSessionId", sessionId);
        m_Status.SessionId = sessionId;
        m_Status.WorldName = GetGame().GetWorldName();
    }

    static int MagazineCount(PlayerBase actor)
    {
        if (!actor) return -1;
        Weapon_Base weapon = Weapon_Base.Cast(actor.GetHumanInventory().GetEntityInHands());
        if (!weapon) return -1;
        Magazine magazine = weapon.GetMagazine(0);
        if (!magazine) return -1;
        return magazine.GetAmmoCount();
    }

    void SaveAll()
    {
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player) continue;
            if (!player.GetIdentity()) continue;
            if (!player.IsAlive()) PredateDeployment.ObserveDeath(m_Session, player);
            else if (!player.m_PredateStagedForDeployment) player.Save();
        }
        m_Session.PredateSave();
    }

    void Stop()
    {
        if (m_Closing) return;
        m_Closing = true;
        m_Session.BeginSuspend();
        if (m_Session.m_Score) m_Session.m_Score.FinishRounds();
        SaveAll();
        m_Status.Phase = "stopping";
        Publish();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExitServer, 250, false);
    }

    void ExitServer()
    {
        if (!m_Published && m_StopPublishRetries < 4)
        {
            m_StopPublishRetries++;
            Publish();
            if (!m_Published)
            {
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExitServer, 250, false);
                return;
            }
        }
        Print("[Predate] Clean session shutdown requested.");
        GetGame().RequestExit(0);
    }

    void ClientQuit(PlayerBase player)
    {
        m_Session.BeginSuspend();
        SaveAll();
        m_Status.Phase = "draining";
        Publish();
        AcknowledgeQuit(player);
    }

    void AcknowledgeQuit(PlayerBase player)
    {
        ScriptRPC acknowledgement = new ScriptRPC;
        acknowledgement.Write(m_Status.SessionId);
        acknowledgement.Send(player, PredateProtocol.SESSION_ACK, true, player.GetIdentity());
    }

    void Publish(bool force = true)
    {
        if (m_Status.SessionId == "") return;
        string previousPhase = m_Status.Phase;
        string previousError = m_Status.Error;
        bool previousReady = m_Status.Ready;
        bool previousPaused = m_Status.Paused;
        bool previousPauseReady = m_Status.PauseReady;
        int previousPlayers = m_Status.Players;
        int previousLife = m_Status.PlayerLife;
        bool previousRedeploy = m_Status.PlayerAwaitingRedeploy;
        bool previousRejected = m_Status.PlayerSpawnRejected;
        m_Status.Paused = m_Session.m_Paused;
        m_Status.PauseReady = m_Session.m_PauseReady;
        if (!m_Closing)
        {
            if (m_Session.m_Paused) m_Status.Phase = "paused";
            else if (m_Status.Phase == "paused") m_Status.Phase = "ready";
        }
        m_Status.Error = m_Session.m_HuntError;
        m_Status.Ready = m_Session.m_HuntReady && m_Status.Error == "" && !m_Closing;
        if (m_Status.Error != "") m_Status.Phase = "error";
        else if (m_Status.Phase == "booting" && m_Status.Ready) m_Status.Phase = "ready";
        m_Status.Players = 0;
        m_Status.PlayerSpawnRejected = false;
        m_StatusPlayers.Clear();
        GetGame().GetPlayers(m_StatusPlayers);
        PlayerBase quitPlayer;
        foreach (Man man : m_StatusPlayers)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player) continue;
            if (!player.GetIdentity()) continue;
            if (player.m_PredateSuicideRequested)
            {
                player.m_PredateSuicideRequested = false;
                if (m_Session.m_Paused && m_Session.m_PauseReady && !m_Session.m_Suspending && !player.m_PredateSessionQuitRequested && m_Session.m_HuntReady && m_Session.m_HuntError == "" && !m_Session.m_WorldState.PlayerAwaitingRedeploy && player.IsAlive() && player.IsPlayerLoaded())
                {
                    bool allowedDamage = player.GetAllowDamage();
                    // Accept this deliberate life change while gameplay stays held.
                    player.SetAllowDamage(true);
                    player.m_PredateScoreSuicide = true;
                    player.SetHealth("", "Health", 0);
                    player.SetAllowDamage(allowedDamage);
                }
            }
            m_Status.Players++;
            m_Status.PlayerSpawnRejected = player.m_PredateSpawnRejected;
            if (PredateServerDiagnostics.Enabled)
            {
                m_Status.PlayerHealth = player.GetHealth("", "Health");
                m_Status.PlayerBlood = player.GetHealth("", "Blood");
                m_Status.PlayerPosition = player.GetPosition();
                Weapon_Base weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
                m_Status.PlayerFirearmClass = "";
                if (weapon) m_Status.PlayerFirearmClass = weapon.GetType();
                m_Status.PlayerMagazine = MagazineCount(player);
                m_Status.PlayerDamageAllowed = player.GetAllowDamage();
                m_Status.PlayerSimulationDisabled = player.GetIsSimulationDisabled();
            }
            if (player.m_PredateSessionQuitRequested)
            {
                player.m_PredateSessionQuitRequested = false;
                quitPlayer = player;
            }
        }
        if (quitPlayer && !m_Closing)
        {
            ClientQuit(quitPlayer);
            return;
        }
        if (quitPlayer) AcknowledgeQuit(quitPlayer);
        if (m_Session.m_HuntProgress && PredateServerDiagnostics.Enabled)
        {
            m_Status.Selected = m_Session.m_HuntProgress.Selected;
            m_Status.Defeated = m_Session.m_HuntProgress.Defeated;
            m_Status.PulseRemaining = m_Session.m_HuntProgress.PulseRemaining;
            m_Status.PulseAge = m_Session.m_HuntProgress.PulseAge;
        }
        eAIBase bot = m_Session.m_HuntBot;
        if (bot && PredateServerDiagnostics.Enabled)
        {
            bool botDisabled = bot.GetIsSimulationDisabled();
            m_Status.BotSimulationDisabled = botDisabled;
            m_Status.BotHealth = bot.GetHealth("", "Health");
            m_Status.BotBlood = bot.GetHealth("", "Blood");
            m_Status.BotShock = bot.GetHealth("", "Shock");
            m_Status.BotPosition = bot.GetPosition();
            m_Status.BotMagazine = MagazineCount(bot);
        }
        if (m_Session.m_WorldState)
        {
            m_Status.SimulationSeconds = m_Session.m_WorldState.SimulationSeconds;
            if (PredateServerDiagnostics.Enabled) m_Status.MemoryCount = m_Session.m_WorldState.Contacts.Count();
            m_Status.PlayerLife = m_Session.m_WorldState.PlayerLife;
            m_Status.PlayerAwaitingRedeploy = m_Session.m_WorldState.PlayerAwaitingRedeploy;
        }
        bool changed = previousPhase != m_Status.Phase || previousError != m_Status.Error || previousReady != m_Status.Ready;
        changed = changed || previousPaused != m_Status.Paused || previousPauseReady != m_Status.PauseReady;
        changed = changed || previousPlayers != m_Status.Players || previousLife != m_Status.PlayerLife;
        changed = changed || previousRedeploy != m_Status.PlayerAwaitingRedeploy || previousRejected != m_Status.PlayerSpawnRejected;
        if (!force && m_Published && !changed && !PredateServerDiagnostics.Enabled && m_StatusElapsed < 1) return;
        string error;
        m_Published = JsonFileLoader<PredateSessionStatus>.SaveFile("$profile:PredateSessionStatus.json", m_Status, error);
        if (m_Published) m_StatusElapsed = 0;
    }

    void Update(float seconds)
    {
        if (m_Status.SessionId == "") return;
        m_Timer += seconds;
        m_StatusElapsed += seconds;
        if (m_Timer < 0.25) return;
        m_Timer = 0;
        bool acknowledged;
        string contents = PredateCommandFile.ReadChanged("$profile:PredateControl.json", m_LastCommand);
        if (contents != "")
        {
            m_LastCommand = contents;
            PredateControl command = new PredateControl;
            string error;
            if (JsonFileLoader<PredateControl>.LoadData(contents, command, error))
            {
                if (command.Schema == 1 && command.SessionId == m_Status.SessionId && command.Sequence > m_Status.Acknowledged)
                {
                    m_Status.Acknowledged = command.Sequence;
                    acknowledged = true;
                    if (command.Action == "stop")
                    {
                        Stop();
                        return;
                    }
                    else if (command.Action == "checkpoint") SaveAll();
                    else if (command.Action == "pause") m_Session.SetPaused(true);
                    else if (command.Action == "resume") m_Session.SetPaused(false);
                }
            }
        }
        Publish(acknowledged);
    }
}
