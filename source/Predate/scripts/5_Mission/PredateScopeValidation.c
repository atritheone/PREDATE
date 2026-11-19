#ifndef SERVER
class PredateScopeReport
{
    bool ScopeOpened;
    bool NightVisionApplied;
    bool NightVisionCleared;
}

class PredateScopeValidation
{
    ref PredateScopeReport m_Report = new PredateScopeReport;
    int m_Stage;
    float m_Elapsed;

    bool Update(PlayerBase player, float seconds)
    {
        if (!player || !player.m_PredateState) return true;
        if (GetGame().GetUIManager().GetMenu()) return true;
        m_Elapsed += seconds;
        if (m_Stage == 0)
        {
            Weapon_Base rifle = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
            ItemOptics optic;
            if (rifle) optic = rifle.GetAttachedOptics();
            if (!optic)
            {
                if (m_Elapsed < 12) return true;
                return Finish();
            }
            player.SwitchOptics(optic, true);
            m_Stage = 1;
            m_Elapsed = 0;
            return true;
        }
        if (m_Elapsed < 0.7) return true;
        DayZPlayerCamera camera = player.GetCurrentPlayerCamera();
        bool scoped = camera && camera.GetCameraName() == "DayZPlayerCameraOptics";
        bool nightVision = PPERequesterBank.IsRequesterRunning(PPERequesterBank.REQ_CAMERANV);
        if (m_Stage == 1)
        {
            m_Report.ScopeOpened = scoped && !nightVision;
            Print("[PredateScopeTest] Scope opened=" + scoped + "; headset effect before toggle=" + nightVision);
            player.AddActiveNV(NVTypes.NV_GOGGLES);
        }
        else if (m_Stage == 2)
        {
            m_Report.NightVisionApplied = scoped && nightVision;
            Print("[PredateScopeTest] Scope held=" + scoped + "; headset effect after toggle=" + nightVision);
            player.RemoveActiveNV(NVTypes.NV_GOGGLES);
        }
        else
        {
            m_Report.NightVisionCleared = scoped && !nightVision;
            player.ExitSights();
            return Finish();
        }
        m_Stage++;
        m_Elapsed = 0;
        return true;
    }

    bool Finish()
    {
        string error;
        JsonFileLoader<PredateScopeReport>.SaveFile("$profile:ScopedNightVision.json", m_Report, error);
        return false;
    }
}
#endif
