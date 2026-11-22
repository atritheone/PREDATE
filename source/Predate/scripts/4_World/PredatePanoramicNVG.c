class PredatePanoramicNVG extends NVGoggles
{
    static bool Active(PlayerBase player)
    {
        return PredatePanoramicNVG.Cast(ActiveGoggles(player)) != null;
    }

    static NVGoggles ActiveGoggles(PlayerBase player)
    {
        if (!player || !player.IsAlive() || player.IsUnconscious()) return null;
        NVGoggles goggles = WorkingOn(player.FindAttachmentBySlotName("Headgear"));
        if (goggles) return goggles;
        return WorkingOn(player.FindAttachmentBySlotName("Eyewear"));
    }

    static NVGoggles WorkingOn(EntityAI mount)
    {
        if (!mount) return null;
        NVGoggles goggles = NVGoggles.Cast(mount.FindAttachmentBySlotName("NVG"));
        if (goggles && goggles.m_IsLowered && goggles.IsWorking()) return goggles;
        return null;
    }

    static bool IsWhitePhosphor(NVGoggles goggles)
    {
        if (!goggles) return false;
        if (PredatePanoramicNVG.Cast(goggles) || goggles.IsKindOf("FOG_GPNVG_Base") || goggles.IsKindOf("FOG_GPNVG_GSGM_Base")) return true;
        return goggles.IsKindOf("FOG_ANVIS9_Base") || goggles.IsKindOf("FOG_FPano_Base") || goggles.IsKindOf("FOG_PVS31A_Base") || goggles.IsKindOf("FOG_UAPNVG_Base");
    }
}

#ifndef SERVER
modded class DayZPlayerCameraBase
{
    override void SetNVPostprocess(int NVtype)
    {
        PlayerBase player = PlayerBase.Cast(m_pPlayer);
        NVGoggles goggles;
        if (player == GetGame().GetPlayer()) goggles = PredatePanoramicNVG.ActiveGoggles(player);
        PPERequester_CameraNV.s_PredateWhitePhosphor = PredatePanoramicNVG.IsWhitePhosphor(goggles);
        super.SetNVPostprocess(NVtype);
        if (!PredatePanoramicNVG.Cast(goggles)) return;
        Mission mission = GetGame().GetMission();
        if (mission && mission.GetEffectWidgets())
            mission.GetEffectWidgets().RemoveActiveEffects({EffectWidgetsTypes.NVG_OCCLUDER});
    }
}

modded class DayZPlayerCamera1stPerson
{
    override void OnUpdate(float pDt, out DayZPlayerCameraResult pOutResult)
    {
        super.OnUpdate(pDt, pOutResult);
        if (PredatePanoramicNVG.Active(PlayerBase.Cast(m_pPlayer)))
            pOutResult.m_fFovMultiplier *= 1.10;
    }
}

modded class DayZPlayerImplement
{
    override protected array<InventoryItem> OnDrawOptics2D()
    {
        array<InventoryItem> optics = super.OnDrawOptics2D();
        if (!optics) return null;
        Weapon_Base rifle = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
        ItemOptics sight;
        if (rifle) sight = rifle.GetAttachedOptics();
        bool dedicatedSight = sight && sight.IsInOptics() && sight.IsNVOptic();
        for (int index = optics.Count() - 1; index >= 0; index--)
        {
            InventoryItem overlay = optics[index];
            if (PredatePanoramicNVG.Cast(overlay) || (dedicatedSight && NVGoggles.Cast(overlay)))
                optics.RemoveOrdered(index);
        }
        return optics;
    }
}
#endif
