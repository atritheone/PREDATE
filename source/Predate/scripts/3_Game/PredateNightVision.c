#ifndef SERVER
modded class PPERequester_CameraNV
{
    static bool s_PredatePanoramic;
    protected bool m_PredateDesaturated;

    override protected void SetNVMode(int mode)
    {
        if (m_PredateDesaturated)
        {
            SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION);
            m_PredateDesaturated = false;
        }
        super.SetNVMode(mode);
    }

    override protected void ApplyGlassesProfile(float exposure, float noiseMult)
    {
        super.ApplyGlassesProfile(exposure, noiseMult);
        if (!s_PredatePanoramic) return;
        SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR,
            {0.77, 0.89, 1.0, 0.0}, PPEGlow.L_23_NVG, PPOperators.MULTIPLICATIVE);
        SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION, false,
            0.12, PPEGlow.L_22_SANDSTORM_NVG, PPOperators.LOWEST);
        m_PredateDesaturated = true;
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
#endif
