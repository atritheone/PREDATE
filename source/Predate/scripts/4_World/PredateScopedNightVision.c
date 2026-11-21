#ifndef SERVER
class PredateScopedNightVision
{
    static int ActiveGoggleType(PlayerBase player)
    {
        array<int> active = player.GetNVTypesArray();
        if (active)
        {
            if (active.Find(NVTypes.NV_GOGGLES) >= 0) return NVTypes.NV_GOGGLES;
            if (active.Find(NVTypes.NV_GOGGLES_2D) >= 0) return NVTypes.NV_GOGGLES_2D;
        }
        return NVTypes.NONE;
    }

    static void UpdateBlur(PlayerBase player)
    {
        bool blurred;
        if (player && player.IsAlive() && !player.IsUnconscious() && player.IsInOptics() && ActiveGoggleType(player) != NVTypes.NONE)
        {
            DayZPlayerCameraOptics camera = DayZPlayerCameraOptics.Cast(player.GetCurrentPlayerCamera());
            blurred = IsWeaponScope(player, camera);
        }
        PredateGoggleScopeBlur.SetEnabled(blurred);
    }

    static bool IsWeaponScope(PlayerBase player, DayZPlayerCameraOptics camera)
    {
        if (!camera) return false;
        Weapon_Base rifle = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
        if (!rifle) return false;
        ItemOptics sight = rifle.GetAttachedOptics();
        return sight && !sight.IsNVOptic() && camera.GetCurrentSightEntity() == sight;
    }

    static bool UsesNightScope(PlayerBase player)
    {
        if (!player.IsInOptics()) return false;
        DayZPlayerCameraOptics camera = DayZPlayerCameraOptics.Cast(player.GetCurrentPlayerCamera());
        if (!camera) return false;
        Weapon_Base weapon = Weapon_Base.Cast(player.GetEntityInHands());
        if (!weapon) return false;
        ItemOptics sight = weapon.GetAttachedOptics();
        return sight && sight.IsNVOptic() && camera.GetCurrentSightEntity() == sight;
    }

    static void PlayGoggleHandoff(PlayerBase player, bool gogglesOn)
    {
        string sound = "FOG_NVG_OFF_SoundSet";
        if (gogglesOn) sound = "FOG_NVG_ON_SoundSet";
        if (!GetGame().ConfigIsExisting("CfgSoundSets " + sound))
        {
            sound = "Flashlight_TurnOff_SoundSet";
            if (gogglesOn) sound = "Flashlight_TurnOn_SoundSet";
        }
        EffectSound effect = SEffectManager.PlaySound(sound, player.GetPosition());
        if (effect) effect.SetSoundAutodestroy(true);
    }
}

modded class DayZPlayerCameraOptics
{
    override void SetCameraPP(bool state, DayZPlayerCamera launchedFrom)
    {
        super.SetCameraPP(state, launchedFrom);
        if (!state || launchedFrom != this || !m_opticsUsed || m_opticsUsed.IsNVOptic()) return;
        PlayerBase player = PlayerBase.Cast(m_pPlayer);
        if (!player || player != GetGame().GetPlayer() || player.GetCurrentPlayerCamera() != this) return;
        int goggles = PredateScopedNightVision.ActiveGoggleType(player);
        if (goggles == NVTypes.NONE) return;

        // The scope has applied its mask and lens effects. Restore the powered headset effect.
        m_ResolvedNVType = ResolveNVType(goggles, player);
        SetNVPostprocess(m_ResolvedNVType);
        Mission mission = GetGame().GetMission();
        if (mission && mission.GetEffectWidgets())
            mission.GetEffectWidgets().RemoveActiveEffects({EffectWidgetsTypes.NVG_OCCLUDER});

        // DayZ's scope mask keeps the sight picture sharp. The native camera
        // rebuilds it when the goggles are raised or the sight changes.
        if (!m_opticsUsed.AllowsDOF() && PredateScopedNightVision.IsWeaponScope(player, this))
            GetGame().ResetPPMask();
    }
}
#endif
