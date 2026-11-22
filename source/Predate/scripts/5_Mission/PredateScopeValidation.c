#ifndef SERVER
class PredateScopeReport
{
    bool ScopeOpened;
    bool NightVisionApplied;
    bool NightVisionCleared;
    bool NightVisionBrightnessScaled;
    bool NightVisionBrightnessRestored;
    bool GoggleScopeBlurApplied;
    bool GoggleScopeBlurCleared;
    bool GoggleScopeBlurClearedOnExit;
    bool PoweredPanoramicGoggles;
    bool PanoramicMaskRestored;
    bool PoweredScope;
    bool InfraredVisible;
    bool InfraredHidden;
    string GoggleCamera;
    bool GogglePower;
    bool GoggleMask;
    bool WorkshopOpticsModel;
    bool WorkshopGogglesRestored;
}

class PredateScopeValidation
{
    ref PredateScopeReport m_Report = new PredateScopeReport;
    int m_Stage;
    float m_Elapsed;
    ItemOptics m_PoweredOptic;
    NVGoggles m_PanoramicGoggles;
    PredateWeaponModule m_PoweredModule;

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
            if (!optic || !player.IsWeaponRaiseCompleted())
            {
                if (m_Elapsed < 12) return true;
                return Finish();
            }
            EntityAI mount = player.FindAttachmentBySlotName("Eyewear");
            if (mount) m_PanoramicGoggles = NVGoggles.Cast(mount.FindAttachmentBySlotName("NVG"));
            if (!m_PanoramicGoggles)
            {
                mount = player.FindAttachmentBySlotName("Headgear");
                if (mount) m_PanoramicGoggles = NVGoggles.Cast(mount.FindAttachmentBySlotName("NVG"));
            }
            m_PoweredModule = PredateWeaponModule.OnWeapon(rifle);
            if (m_PanoramicGoggles && m_PoweredModule && optic.IsNVOptic())
            {
                if (!m_PoweredModule.GetCompEM().IsWorking() && m_Elapsed < 12) return true;
                m_PoweredOptic = optic;
                m_PanoramicGoggles.RotateGoggles(false);
            }
            else player.SwitchOptics(optic, true);
            m_Stage = 1;
            m_Elapsed = 0;
            return true;
        }
        if (m_Elapsed < 0.7) return true;
        if (m_PoweredOptic) return UpdatePoweredKit(player);
        DayZPlayerCamera camera = player.GetCurrentPlayerCamera();
        bool scoped = camera && camera.GetCameraName() == "DayZPlayerCameraOptics";
        bool nightVision = PPERequesterBank.IsRequesterRunning(PPERequesterBank.REQ_CAMERANV);
        if (m_Stage == 1)
        {
            m_Report.ScopeOpened = scoped && !nightVision && CurrentScopeBlur() < 0.01;
            Print("[PredateScopeTest] Scope opened=" + scoped + "; headset effect before toggle=" + nightVision + "; goggle blur=" + CurrentScopeBlur());
            player.AddActiveNV(NVTypes.NV_GOGGLES);
        }
        else if (m_Stage == 2)
        {
            m_Report.NightVisionApplied = scoped && nightVision;
            m_Report.NightVisionBrightnessScaled = HasDimming();
            float scopeBlur = CurrentScopeBlur();
            m_Report.GoggleScopeBlurApplied = scoped && scopeBlur > 0.6;
            Print("[PredateScopeTest] Scope held=" + scoped + "; headset effect after toggle=" + nightVision + "; goggle blur=" + scopeBlur);
            player.RemoveActiveNV(NVTypes.NV_GOGGLES);
        }
        else if (m_Stage == 3)
        {
            m_Report.NightVisionCleared = scoped && !nightVision;
            m_Report.NightVisionBrightnessRestored = !PPERequesterBank.IsRequesterRunning(PredateNightVisionDimming.s_RequesterId);
            m_Report.GoggleScopeBlurCleared = scoped && CurrentScopeBlur() < 0.01;
            Print("[PredateScopeTest] Goggle blur after toggle off=" + CurrentScopeBlur());
            player.AddActiveNV(NVTypes.NV_GOGGLES);
        }
        else if (m_Stage == 4)
        {
            player.ExitSights();
        }
        else
        {
            m_Report.GoggleScopeBlurClearedOnExit = !scoped && CurrentScopeBlur() < 0.01;
            player.RemoveActiveNV(NVTypes.NV_GOGGLES);
            return Finish();
        }
        m_Stage++;
        m_Elapsed = 0;
        return true;
    }

    bool HasDimming()
    {
        PPERequesterBase requester = PPERequesterBank.GetRequester(PredateNightVisionDimming.s_RequesterId);
        if (!requester || !requester.IsRequesterRunning()) return false;
        PPERequestParamDataFloat brightness = PPERequestParamDataFloat.Cast(requester.GetRequestData(PostProcessEffectType.Glow, PPEGlow.PARAM_BRIGHTNESS, PPEConstants.VAR_TYPE_FLOAT));
        return brightness && brightness.IsDataActive() && Math.AbsFloat(brightness.m_FloatCurrent - 0.70) < 0.001;
    }

    float CurrentScopeBlur()
    {
        PPERequesterBase requester = PPERequesterBank.GetRequester(PredateGoggleScopeBlur.s_RequesterId);
        PPERequestParamDataFloat blur = PPERequestParamDataFloat.Cast(requester.GetRequestData(PostProcessEffectType.GaussFilter, PPEGaussFilter.PARAM_INTENSITY, PPEConstants.VAR_TYPE_FLOAT));
        if (!blur || !blur.IsDataActive()) return 0;
        return blur.m_FloatCurrent;
    }

    bool UpdatePoweredKit(PlayerBase player)
    {
        MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
        PPERequester_CameraNV nightVision = PPERequester_CameraNV.Cast(PPERequesterBank.GetRequester(PPERequesterBank.REQ_CAMERANV));
        bool seesInfrared = nightVision && nightVision.PredateCanSeeInfrared();
        bool mask = mission && mission.PredatePanoramicMaskShown();
        DayZPlayerCamera camera = player.GetCurrentPlayerCamera();
        bool scoped = camera && camera.GetCameraName() == "DayZPlayerCameraOptics";
        bool gogglesWorking = m_PanoramicGoggles.m_IsLowered && m_PanoramicGoggles.IsWorking();
        switch (m_Stage)
        {
            case 1:
                if (camera) m_Report.GoggleCamera = camera.GetCameraName();
                m_Report.GogglePower = gogglesWorking && seesInfrared;
                m_Report.GoggleMask = mask;
                m_Report.PoweredPanoramicGoggles = m_Report.GogglePower && mask && seesInfrared;
                m_Report.NightVisionBrightnessScaled = HasDimming();
                string gogglesType = m_PanoramicGoggles.GetType();
                m_Report.WorkshopOpticsModel = (gogglesType == "FOG_GPNVG_Tan" || gogglesType == "FOG_GPNVG_Black") && m_PanoramicGoggles.IsUsingOptics2DModel();
                m_Report.InfraredVisible = m_PoweredModule && !m_PoweredModule.IsLightSource() && m_PoweredModule.PredateEmitterVisible();
                MakeScreenshot("$profile:PredatePanoramic.dds");
                player.SwitchOptics(m_PoweredOptic, true);
                break;
            case 2:
                m_Report.ScopeOpened = scoped;
                m_Report.PoweredScope = scoped && !mask && seesInfrared && m_PoweredOptic.IsWorking();
                m_Report.NightVisionApplied = m_Report.PoweredScope;
                m_Report.NightVisionBrightnessScaled = m_Report.NightVisionBrightnessScaled && HasDimming();
                player.ExitSights();
                break;
            case 3:
                m_Report.PanoramicMaskRestored = !scoped && mask && seesInfrared;
                m_Report.WorkshopGogglesRestored = !scoped && gogglesWorking && seesInfrared && m_Report.WorkshopOpticsModel;
                m_PanoramicGoggles.RotateGoggles(true);
                break;
            case 4:
                m_Report.NightVisionCleared = !mask && !seesInfrared;
                m_Report.NightVisionBrightnessRestored = !PPERequesterBank.IsRequesterRunning(PredateNightVisionDimming.s_RequesterId);
                m_Report.InfraredHidden = m_PoweredModule && m_PoweredModule.GetCompEM().IsWorking() && !m_PoweredModule.PredateEmitterVisible();
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
