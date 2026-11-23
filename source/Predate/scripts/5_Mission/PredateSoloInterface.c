#ifndef SERVER
class PredateSoloInterface
{
    static void MultiplayerInputs(array<int> actions)
    {
        array<string> names = {
            "UAChat", "UAVoiceOverNet", "UAVoiceOverNetToggle", "UAVoiceOverNetMute",
            "UAVoiceLevel", "UAVoiceDistanceUp", "UAVoiceDistanceDown", "UAVoiceModifierHelper"
        };
        foreach (string name : names)
        {
            UAInput input = GetUApi().GetInputByName(name);
            if (input) actions.Insert(input.ID());
        }
    }
}

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();
        if (!IsCLIParam("predateSessionId")) return;
        array<int> actions = {};
        PredateSoloInterface.MultiplayerInputs(actions);
        foreach (int action : actions)
            GetUApi().GetInputByID(action).ForceDisable(true);
        GetGame().GetWorld().DisableTransmitVoN(true);
        GetGame().GetWorld().SetVoiceOn(false);
        GetGame().EnableMicCapture(false);
        if (m_HudRootWidget)
        {
            Widget chatFrame = m_HudRootWidget.FindAnyWidget("ChatFrameWidget");
            if (chatFrame) chatFrame.Show(false);
        }
    }

    override void ShowChat()
    {
        if (!IsCLIParam("predateSessionId")) super.ShowChat();
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        if ((eventTypeId == ChatMessageEventTypeID || eventTypeId == ChatChannelEventTypeID) && IsCLIParam("predateSessionId")) return;
        super.OnEvent(eventTypeId, params);
    }
}

modded class OptionsMenuSounds
{
    void OptionsMenuSounds(Widget parent, Widget details_root, GameOptions options, OptionsMenu menu)
    {
        if (!IsCLIParam("predateSessionId")) return;
        GetGame().EnableMicCapture(false);
        m_WasMicCapturing = false;
        m_AudioLevelTimer.Stop();
        Widget voiceSettings = m_Root.FindAnyWidget("voice_settings_root");
        if (voiceSettings) voiceSettings.Show(false);
    }
}

modded class OptionsMenuGame
{
    void OptionsMenuGame(Widget parent, Widget details_root, GameOptions options, OptionsMenu menu)
    {
        if (!IsCLIParam("predateSessionId")) return;
        Widget serverInfo = m_Root.FindAnyWidget("serverinfo_setting_option");
        if (serverInfo && serverInfo.GetParent()) serverInfo.GetParent().Show(false);
    }
}

modded class InGameMenu
{
    override protected bool SetServerInfo()
    {
        if (IsCLIParam("predateSessionId")) return false;
        return super.SetServerInfo();
    }

    override void SetServerInfoVisibility(bool show)
    {
        super.SetServerInfoVisibility(show && !IsCLIParam("predateSessionId"));
    }
}
#endif
