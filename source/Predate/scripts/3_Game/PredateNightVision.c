#ifndef SERVER
modded class PPERequester_CameraNV
{
    static bool s_PredateWhitePhosphor;
    protected bool m_PredateWhitePhosphor;
    protected bool m_PredatePoweredImage;

    override protected void SetNVMode(int mode)
    {
        m_PredateWhitePhosphor = false;
        m_PredatePoweredImage = false;
        super.SetNVMode(mode);
        PredateNightVisionDimming.SetEnabled(m_PredatePoweredImage);
        PredateWhitePhosphor.SetEnabled(m_PredateWhitePhosphor);
    }

    override protected void OnStop(Param par = null)
    {
        PredateNightVisionDimming.SetEnabled(false);
        PredateWhitePhosphor.SetEnabled(false);
        super.OnStop(par);
    }

    override protected void ApplyOpticsProfile(float exposure, float noiseMult)
    {
        super.ApplyOpticsProfile(exposure, noiseMult);
        m_PredatePoweredImage = true;
    }

    override protected void ApplyGlassesProfile(float exposure, float noiseMult)
    {
        super.ApplyGlassesProfile(exposure, noiseMult);
        m_PredatePoweredImage = true;
        m_PredateWhitePhosphor = s_PredateWhitePhosphor;
    }

    bool PredateCanSeeInfrared()
    {
        switch (m_CurrentMode)
        {
            case NV_DEFAULT_GLASSES:
            case NV_DAYTIME_GLASSES:
            case NV_UNDERGROUND_GLASSES:
            case NV_SANDSTORM_GLASSES:
            case NV_UNDERGROUND_SANDSTORM_GLASSES:
            case NV_DEFAULT_OPTICS:
            case NV_UNDERGROUND_OPTICS:
            case NV_SANDSTORM_OPTICS:
            case NV_UNDERGROUND_SANDSTORM_OPTICS:
                return PPERequesterBank.IsRequesterRunning(PPERequesterBank.REQ_CAMERANV);
        }
        return false;
    }
}

class PredateWhitePhosphor extends PPERequester_GameplayBase
{
    static int s_RequesterId = PPERequesterBank.INVALID;

    static void SetEnabled(bool enabled)
    {
        PPERequesterBase requester = PPERequesterBank.GetRequester(s_RequesterId);
        if (!requester || enabled == requester.IsRequesterRunning()) return;
        if (enabled) requester.Start();
        else requester.Stop();
    }

    override protected void OnStart(Param par = null)
    {
        super.OnStart(par);
        // Override both the scene colour and the phosphor tint applied by goggle mods.
        SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR,
            {1.0, 1.0, 1.0, 0.0}, PPEGlow.L_23_NVG + 1, PPOperators.SET);
        SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION, false,
            0.0, PPEGlow.L_23_NVG + 1, PPOperators.SET);
        // Apply this after the shared night-vision brightness multiplier.
        SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_BRIGHTNESS, true,
            0.675, PPEGlow.L_23_NVG + 2, PPOperators.MULTIPLICATIVE);
    }
}

class PredateNightVisionDimming extends PPERequester_GameplayBase
{
    static int s_RequesterId = PPERequesterBank.INVALID;

    static void SetEnabled(bool enabled)
    {
        PPERequesterBase requester = PPERequesterBank.GetRequester(s_RequesterId);
        if (!requester || enabled == requester.IsRequesterRunning()) return;
        if (enabled) requester.Start();
        else requester.Stop();
    }

    override protected void OnStart(Param par = null)
    {
        super.OnStart(par);
        SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_BRIGHTNESS, true, 0.70, PPEGlow.L_23_NVG + 1, PPOperators.MULTIPLICATIVE);
    }
}

class PredateGoggleScopeBlur extends PPERequester_GameplayBase
{
    static int s_RequesterId = PPERequesterBank.INVALID;

    static void SetEnabled(bool enabled)
    {
        PPERequesterBase requester = PPERequesterBank.GetRequester(s_RequesterId);
        if (!requester || enabled == requester.IsRequesterRunning()) return;
        if (enabled) requester.Start();
        else requester.Stop();
    }

    override protected void OnStart(Param par = null)
    {
        super.OnStart(par);
        SetTargetValueFloat(PostProcessEffectType.GaussFilter, PPEGaussFilter.PARAM_INTENSITY, false, 0.64, PPEGaussFilter.L_0_ADS + 1, PPOperators.SET);
    }
}

modded class PPERequesterRegistrations
{
    override protected void RegisterAdditionalRequesters()
    {
        super.RegisterAdditionalRequesters();
        PredateGoggleScopeBlur.s_RequesterId = PPERequesterBank.RegisterRequester(PredateGoggleScopeBlur);
        PredateNightVisionDimming.s_RequesterId = PPERequesterBank.RegisterRequester(PredateNightVisionDimming);
        PredateWhitePhosphor.s_RequesterId = PPERequesterBank.RegisterRequester(PredateWhitePhosphor);
    }
}
#endif
