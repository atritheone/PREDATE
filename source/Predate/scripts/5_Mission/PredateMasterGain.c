#ifndef SERVER
modded class MissionGameplay
{
    protected static const float PREDATE_MASTER_GAIN = 3.375;
    protected bool m_PredateManageMasterGain;
    protected bool m_PredateResumeGainAfterOptions;
    protected ref GameOptions m_PredateGainOptions;
    protected NumericOptionsAccess m_PredateMasterVolume;

    override void OnMissionStart()
    {
        super.OnMissionStart();
        string sessionId;
        m_PredateManageMasterGain = GetGame().CommandlineGetParam("predateSessionId", sessionId);
        if (m_PredateManageMasterGain) PredateApplyMasterGain();
    }

    override void OnMissionFinish()
    {
        m_PredateManageMasterGain = false;
        m_PredateResumeGainAfterOptions = false;
        PredateRevertMasterGain();
        super.OnMissionFinish();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!m_PredateResumeGainAfterOptions || GetGame().GetUIManager().IsMenuOpen(MENU_OPTIONS)) return;

        m_PredateResumeGainAfterOptions = false;
        PredateApplyMasterGain();
    }

    void PredateRestoreMasterSlider()
    {
        if (!m_PredateManageMasterGain) return;
        PredateRevertMasterGain();
        m_PredateResumeGainAfterOptions = true;
    }

    protected void PredateApplyMasterGain()
    {
        if (m_PredateMasterVolume) return;
        m_PredateGainOptions = new GameOptions();
        m_PredateMasterVolume = NumericOptionsAccess.Cast(m_PredateGainOptions.GetOptionByType(AT_OPTIONS_MASTER_VOLUME));
        if (!m_PredateMasterVolume) return;

        // The native master controls the mixed output. Test applies it without saving a boosted slider value.
        m_PredateMasterVolume.WriteValue(m_PredateMasterVolume.ReadValue() * PREDATE_MASTER_GAIN);
        m_PredateMasterVolume.Test();
    }

    protected void PredateRevertMasterGain()
    {
        if (m_PredateMasterVolume) m_PredateMasterVolume.Revert();
        m_PredateMasterVolume = null;
        m_PredateGainOptions = null;
    }
}

modded class OptionsMenu
{
    override Widget Init()
    {
        MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
        if (mission) mission.PredateRestoreMasterSlider();
        Widget root = super.Init();
        if (!mission || !IsCLIParam("predateSessionId") || !root || !m_Tabber) return root;

        for (int index = 0; index < m_Tabber.GetTabCount(); index++)
        {
            Widget panel = m_Tabber.GetTab(index);
            if (!panel || !panel.FindAnyWidget("expansion_tab_content")) continue;
            Widget control = root.FindAnyWidget("Tab_Control_" + index);
            if (control)
            {
                control.Show(false);
                control.Enable(false);
            }
            panel.Show(false);
            panel.Enable(false);
            m_Tabber.AlignTabbers();
            break;
        }
        return root;
    }
}
#endif
