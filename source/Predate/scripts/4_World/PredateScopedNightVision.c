#ifndef SERVER
modded class DayZPlayerCameraOptics
{
    override void SetCameraPP(bool state, DayZPlayerCamera launchedFrom)
    {
        super.SetCameraPP(state, launchedFrom);
        if (!state || launchedFrom != this || !m_opticsUsed || m_opticsUsed.IsNVOptic()) return;
        PlayerBase player = PlayerBase.Cast(m_pPlayer);
        if (!player || player != GetGame().GetPlayer() || player.GetCurrentPlayerCamera() != this) return;
        array<int> active = player.GetNVTypesArray();
        if (!active) return;
        int goggles = NVTypes.NONE;
        if (active.Find(NVTypes.NV_GOGGLES) >= 0) goggles = NVTypes.NV_GOGGLES;
        else if (active.Find(NVTypes.NV_GOGGLES_2D) >= 0) goggles = NVTypes.NV_GOGGLES_2D;
        if (goggles == NVTypes.NONE) return;

        // The scope has applied its mask and lens effects. Restore the powered headset effect.
        m_ResolvedNVType = ResolveNVType(goggles, player);
        SetNVPostprocess(m_ResolvedNVType);
        Mission mission = GetGame().GetMission();
        if (mission && mission.GetEffectWidgets())
            mission.GetEffectWidgets().RemoveActiveEffects({EffectWidgetsTypes.NVG_OCCLUDER});
    }
}
#endif
