class PredateServerBridge
{
    PredateSession m_Session;
    ref PredateSessionStatus m_Status = new PredateSessionStatus;
    float m_Timer;
    bool m_Closing;

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
        SaveAll();
        m_Status.Phase = "stopping";
        Publish();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExitServer, 250, false);
    }

    void ExitServer()
    {
        Print("[Predate] Clean session shutdown requested.");
        GetGame().RequestExit(0);
    }

    void ClientQuit(PlayerBase player)
    {
        m_Session.BeginSuspend();
        SaveAll();
        m_Status.Phase = "draining";
        Publish();
        ScriptRPC acknowledgement = new ScriptRPC;
        acknowledgement.Write(m_Status.SessionId);
        acknowledgement.Send(player, PredateProtocol.SESSION_ACK, true, player.GetIdentity());
    }

    void Publish()
    {
        if (m_Status.SessionId == "") return;
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
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
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
                    player.SetHealth("", "Health", 0);
                    player.SetAllowDamage(allowedDamage);
                }
            }
            m_Status.Players++;
            m_Status.PlayerHealth = player.GetHealth("", "Health");
            m_Status.PlayerBlood = player.GetHealth("", "Blood");
            m_Status.PlayerPosition = player.GetPosition();
            m_Status.PlayerSpawnRejected = player.m_PredateSpawnRejected;
            m_Status.PlayerMagazine = MagazineCount(player);
            Weapon_Base heldWeapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
            string firearmClass;
            if (heldWeapon) firearmClass = heldWeapon.GetType();
            m_Status.PlayerFirearmClass = firearmClass;
            bool damageAllowed = player.GetAllowDamage();
            bool playerDisabled = player.GetIsSimulationDisabled();
            m_Status.PlayerDamageAllowed = damageAllowed;
            m_Status.PlayerSimulationDisabled = playerDisabled;
            if (player.m_PredateSessionQuitRequested)
            {
                player.m_PredateSessionQuitRequested = false;
                ClientQuit(player);
            }
        }
        if (m_Session.m_HuntProgress)
        {
            m_Status.Selected = m_Session.m_HuntProgress.Selected;
            m_Status.Defeated = m_Session.m_HuntProgress.Defeated;
            m_Status.PulseRemaining = m_Session.m_HuntProgress.PulseRemaining;
            m_Status.PulseAge = m_Session.m_HuntProgress.PulseAge;
        }
        eAIBase bot = m_Session.m_HuntBot;
        if (bot)
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
            m_Status.MemoryCount = m_Session.m_WorldState.Contacts.Count();
            m_Status.PlayerLife = m_Session.m_WorldState.PlayerLife;
            m_Status.PlayerAwaitingRedeploy = m_Session.m_WorldState.PlayerAwaitingRedeploy;
        }
        string error;
        JsonFileLoader<PredateSessionStatus>.SaveFile("$profile:PredateSessionStatus.json", m_Status, error);
    }

    void Update(float seconds)
    {
        if (m_Status.SessionId == "") return;
        m_Timer += seconds;
        if (m_Timer < 0.25) return;
        m_Timer = 0;
        if (FileExist("$profile:PredateControl.json"))
        {
            PredateControl command = new PredateControl;
            string error;
            if (JsonFileLoader<PredateControl>.LoadFile("$profile:PredateControl.json", command, error))
            {
                if (command.Schema == 1 && command.SessionId == m_Status.SessionId && command.Sequence > m_Status.Acknowledged)
                {
                    m_Status.Acknowledged = command.Sequence;
                    if (command.Action == "stop") Stop();
                    else if (command.Action == "checkpoint") SaveAll();
                    else if (command.Action == "pause") m_Session.SetPaused(true);
                    else if (command.Action == "resume") m_Session.SetPaused(false);
                }
            }
        }
        Publish();
    }
}
