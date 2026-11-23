class PredateFirearmAim
{
    static bool IsPlayerFirearm(DayZPlayerImplement player)
    {
        if (!player || player.IsAI() || !IsCLIParam("predateSessionId")) return false;
        Weapon_Base weapon = Weapon_Base.Cast(player.GetEntityInHands());
        return weapon && !weapon.IsKindOf("Archery_Base");
    }
}

modded class DayZPlayerCameraIronsights
{
    override void OnUpdate(float pDt, out DayZPlayerCameraResult pOutResult)
    {
        super.OnUpdate(pDt, pOutResult);
        if (PredateFirearmAim.IsPlayerFirearm(DayZPlayerImplement.Cast(m_pPlayer)))
            pOutResult.m_fShootFromCamera = 1.0;
    }
}

modded class DayZPlayerImplementAiming
{
    protected float m_PredateSharedAimX;
    protected float m_PredateSharedAimY;

    override void OnRaiseBegin(DayZPlayerImplement player)
    {
        super.OnRaiseBegin(player);
        m_PredateSharedAimX = 0;
        m_PredateSharedAimY = 0;
    }

    override bool ProcessAimFilters(float pDt, SDayZPlayerAimingModel pModel, int stance_index)
    {
        bool processed = super.ProcessAimFilters(pDt, pModel, stance_index);
        if (!PredateFirearmAim.IsPlayerFirearm(m_PlayerDpi))
        {
            m_PredateSharedAimX = 0;
            m_PredateSharedAimY = 0;
            return processed;
        }

        // Apply the changing sway once to shared aim, so the engine does not
        // apply it a second time to a shot already aimed with the sight camera.
        float aimX = pModel.m_fAimXHandsOffset;
        float aimY = pModel.m_fAimYHandsOffset;
        pModel.m_fAimXMouseShift += aimX - m_PredateSharedAimX;
        pModel.m_fAimYMouseShift += aimY - m_PredateSharedAimY;
        m_PredateSharedAimX = aimX;
        m_PredateSharedAimY = aimY;
        pModel.m_fAimXHandsOffset = 0;
        pModel.m_fAimYHandsOffset = 0;
        return processed;
    }

    override protected float CalculateWeight(int stance_index, float current_stamina, float camera_sway_modifier, bool holding_breath)
    {
        float weight = super.CalculateWeight(stance_index, current_stamina, camera_sway_modifier, holding_breath);
        if (!PredateFirearmAim.IsPlayerFirearm(m_PlayerDpi)) return weight;
        if (m_PlayerDpi.IsInOptics() || m_PlayerDpi.IsInIronsights()) return weight * 0.5;
        return 0.0;
    }
}
