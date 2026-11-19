
void main() {}
class PredateProbeState
{
    bool RuntimeLoaded;
    bool Server;
    bool Multiplayer;
    bool LocalPlayer;
    bool HasIdentity;
    bool BotAlive;
    float BotTravel;
    int AICommand;
    bool SimulationDisabled;
    bool EnginePaused;
    bool PauseMenu;
    vector BotPosition;
    vector BotBoneRelative;
    int NativeClock;
    float Seconds;
}
class PredateOfflineProbe : MissionGameplay
{
    PlayerBase m_Human;
    eAIBase m_Bot;
    ref eAIGroup m_Group;
    vector m_Start;
    ref PredateProbeState m_Probe = new PredateProbeState;
    float m_Timer;
    override void OnInit()
    {
        super.OnInit();
        eAIBase.s_eAI_FTO = 0;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PublishNativeProbe, 100, true);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CreateActors, 4000, false);
    }
    void CreateActors()
    {
        vector position = "4500 0 6500";
        position[1] = GetGame().SurfaceY(position[0], position[2]);
        m_Human = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", position, 0, "NONE"));
        if (m_Human)
        {
            GetGame().SelectPlayer(null, m_Human);
            m_Human.SetAllowDamage(false);
        }
        position = "4780 0 6710";
        position[1] = GetGame().SurfaceY(position[0], position[2]);
        m_Start = position;
        m_Bot = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", position, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        if (m_Bot)
        {
            m_Group = eAIGroup.CreateGroup(new eAIFactionRaiders);
            m_Bot.SetGroup(m_Group);
            m_Bot.SetMovementSpeedLimits(2, 3);
            m_Bot.eAI_SetThreatDistanceLimit(50);
            m_Bot.m_PredateRouteActive = true;
            m_Bot.DisableSimulation(false);
            position = position + Vector(100, 0, 0);
            position[1] = GetGame().SurfaceY(position[0], position[2]);
            if (m_Group)
            {
                m_Group.AddWaypoint(position);
                m_Group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);
            }
        }
        m_Probe.RuntimeLoaded = true;
        Print("[PredateProbe] Singleplayer actors created.");
    }

    void PublishNativeProbe()
    {
        m_Probe.EnginePaused = IsPaused();
        m_Probe.PauseMenu = GetGame().GetUIManager().IsMenuOpen(MENU_INGAME);
        int clock = GetGame().GetTime();
        m_Probe.NativeClock = clock;
        if (m_Bot)
        {
            vector position = m_Bot.GetPosition();
            m_Probe.BotPosition = position;
            int bone = m_Bot.GetBoneIndexByName("spine3");
            if (bone >= 0)
            {
                vector boneWorld = m_Bot.GetBonePositionWS(bone);
                vector boneModel = m_Bot.WorldToModel(boneWorld);
                m_Probe.BotBoneRelative = boneModel;
            }
        }
        string error;
        JsonFileLoader<PredateProbeState>.SaveFile("$profile:OfflineProbe.json", m_Probe, error);
    }


/*PREDATE_HOLD_METHODS*/
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!IsPaused() && m_Bot && m_Bot.GetIsSimulationDisabled()) m_Bot.DisableSimulation(false);
        if (IsPaused()) return;
        m_Timer += timeslice;
        if (m_Timer < 1) return;
        m_Probe.Seconds += m_Timer;
        m_Timer = 0;
        m_Probe.Server = GetGame().IsServer();
        m_Probe.Multiplayer = GetGame().IsMultiplayer();
        m_Probe.LocalPlayer = GetGame().GetPlayer() != null;
        if (m_Human) m_Probe.HasIdentity = m_Human.GetIdentity() != null;
        if (m_Bot)
        {
            m_Probe.BotAlive = m_Bot.IsAlive();
            m_Probe.AICommand = m_Bot.GetCurrentCommandID();
            m_Probe.SimulationDisabled = m_Bot.GetIsSimulationDisabled();
            m_Probe.BotTravel = vector.Distance(m_Start, m_Bot.GetPosition());
        }
        string error;
        JsonFileLoader<PredateProbeState>.SaveFile("$profile:OfflineProbe.json", m_Probe, error);
        if (m_Probe.Seconds > 35) GetGame().RequestExit(0);
    }
}
Mission CreateCustomMission(string path) { return new PredateOfflineProbe; }
