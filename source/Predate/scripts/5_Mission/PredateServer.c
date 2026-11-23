// Keep engine mission state in the native class; PREDATE owns a separate session.
modded class MissionServer
{
    ref PredateSession m_PredateSession;
    string m_PredateRespawningIdentity;

    override void OnInit()
    {
        super.OnInit();
        m_PredateSession = new PredateSession;
        m_PredateSession.Init();
        PredatePlayerLifeEvents.Death.Insert(PredateRecordPlayerDeath);
        PredateOperatorDeaths.Defeated.Insert(PredateRecordOperatorDeath);
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
        PredateOperatorDeaths.Defeated.Remove(PredateRecordOperatorDeath);
        PredateServerPauseDriver.Poll.Remove(PredatePollPausedServer);
        PredateServerPauseDriver.Held = false;
        if (m_PredateSession) m_PredateSession.Finish();
        super.OnMissionFinish();
    }

    void PredateRecordPlayerDeath(PlayerBase player)
    {
        PredateDeployment.ObserveDeath(m_PredateSession, player);
    }

    void PredateRecordOperatorDeath(int slot, PlayerBase credited)
    {
        if (!m_PredateSession || !m_PredateSession.m_HuntReady || !m_PredateSession.m_WorldState) return;
        if (!PredateSignalService.RetireDefeatedSignal(m_PredateSession, slot, true)) return;
        m_PredateSession.PredateRequestCheckpoint();
        if (m_PredateSession.m_WorldState.PlayerAwaitingRedeploy) return;

        array<Man> players = {};
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || player == credited || !player.GetIdentity() || !player.IsAlive()) continue;
            ScriptRPC rpc = new ScriptRPC;
            rpc.Write(slot);
            rpc.Send(player, PredateProtocol.SIGNAL_LOST, true, player.GetIdentity());
        }
    }

    override PlayerBase CreateCharacter(PlayerIdentity identity, vector pos, ParamsReadContext ctx, string characterName)
    {
        vector start = m_PredateSession.PredateGround(m_PredateSession.m_HuntConfig.PlayerStart);
        bool nativeInsertion;
        if (identity && m_PredateRespawningIdentity == identity.GetId() && m_PredateSession.m_WorldState.PlayerAwaitingRedeploy)
        {
            vector site;
            nativeInsertion = PredateDeployment.StartPosition(m_PredateSession, null, site);
            if (nativeInsertion) start = site;
        }
        PredateWorldState state = m_PredateSession.m_WorldState;
        string model = state.PlayerCharacterModel;
        TStringArray models = GetGame().ListAvailableCharacters();
        if (models.Find(model) < 0)
        {
            // Older saves can reach redeploy without a saved model.
            if (state.PlayerAwaitingRedeploy)
                model = GetGame().GetMenuDefaultCharacterData().GetCharacterType();
            else
                model = characterName;
            if (models.Find(model) < 0) model = "SurvivorM_Mirek";
        }
        Entity entity = GetGame().CreatePlayer(identity, model, start, 0, "NONE");
        Class.CastTo(m_player, entity);
        if (!m_player) return null;
        state.PlayerCharacterModel = m_player.GetType();
        m_player.m_PredateNativeInsertion = nativeInsertion;
        GetGame().SelectPlayer(identity, m_player);
        if (m_PredateSession.m_Paused) m_PredateSession.HoldPauseActor(m_player);
        return m_player;
    }

    override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
    {
        PlayerBase player = super.OnClientNewEvent(identity, pos, ctx);
        if (identity && m_PredateRespawningIdentity == identity.GetId()) m_PredateRespawningIdentity = "";
        if (player && m_PredateSession.m_WorldState.PlayerAwaitingRedeploy)
        {
            PredateDeployment.StagePlayer(player);
            if (player.m_PredateNativeInsertion) PredateDeployment.Redeploy(m_PredateSession, player);
        }
        return player;
    }

    override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
    {
        if (m_PredateSession.m_WorldState.PlayerAwaitingRedeploy)
        {
            // OnClientNewEvent finishes equipment only for an explicit respawn.
            player.RemoveAllItems();
            return;
        }
        m_PredateSession.PredateEquip(player);
    }

    override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
    {
        PredateDeployment.ObserveDeath(m_PredateSession, player);
        if (identity && player && !player.IsAlive() && m_PredateSession.m_WorldState.PlayerAwaitingRedeploy)
            m_PredateRespawningIdentity = identity.GetId();
        super.OnClientRespawnEvent(identity, player);
    }

    override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
    {
        if (!authFailed) PredateDeployment.ObserveDeath(m_PredateSession, player);
        if (identity && m_PredateRespawningIdentity == identity.GetId()) m_PredateRespawningIdentity = "";
        super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
    }
}
