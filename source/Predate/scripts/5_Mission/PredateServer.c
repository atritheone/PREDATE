// Keep engine mission state in the native class; PREDATE owns a separate session.
modded class MissionServer
{
    ref PredateSession m_PredateSession;

    override void OnInit()
    {
        super.OnInit();
        m_PredateSession = new PredateSession;
        m_PredateSession.Init();
        PredatePlayerLifeEvents.Death.Insert(PredateRecordPlayerDeath);
        PredateServerPauseDriver.Poll.Insert(PredatePollPausedServer);
    }

    override void OnUpdate(float timeslice)
    {
        if (m_PredateSession && m_PredateSession.m_Paused) return;
        super.OnUpdate(timeslice);
        if (m_PredateSession) m_PredateSession.Update(timeslice);
    }

    override bool IsPaused()
    {
        if (m_PredateSession && m_PredateSession.m_Paused) return true;
        return super.IsPaused();
    }

    void PredatePollPausedServer(float timeslice)
    {
        if (m_PredateSession && m_PredateSession.m_Paused)
            m_PredateSession.Update(timeslice);
    }

    override void OnMissionFinish()
    {
        PredatePlayerLifeEvents.Death.Remove(PredateRecordPlayerDeath);
        PredateServerPauseDriver.Poll.Remove(PredatePollPausedServer);
        PredateServerPauseDriver.Held = false;
        if (m_PredateSession) m_PredateSession.Finish();
        super.OnMissionFinish();
    }

    void PredateRecordPlayerDeath(PlayerBase player)
    {
        PredateDeployment.ObserveDeath(m_PredateSession, player);
    }

    override PlayerBase CreateCharacter(PlayerIdentity identity, vector pos, ParamsReadContext ctx, string characterName)
    {
        vector start = m_PredateSession.PredateGround(m_PredateSession.m_HuntConfig.PlayerStart);
        Entity entity = GetGame().CreatePlayer(identity, characterName, start, 0, "NONE");
        Class.CastTo(m_player, entity);
        GetGame().SelectPlayer(identity, m_player);
        if (m_PredateSession.m_Paused) m_PredateSession.HoldPauseActor(m_player);
        return m_player;
    }

    override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
    {
        if (m_PredateSession.m_WorldState.PlayerAwaitingRedeploy)
        {
            // Native login and native respawn may both create a body. Neither
            // grants a new playable life until the deployment choice arrives.
            player.RemoveAllItems();
            return;
        }
        m_PredateSession.PredateEquip(player);
    }

    override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
    {
        PredateDeployment.ObserveDeath(m_PredateSession, player);
        super.OnClientRespawnEvent(identity, player);
    }

    override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
    {
        if (!authFailed) PredateDeployment.ObserveDeath(m_PredateSession, player);
        super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
    }
}
