class PredatePanoramicNVG extends NVGoggles
{
    static bool Active(PlayerBase player)
    {
        if (!player || !player.IsAlive() || player.IsUnconscious()) return false;
        EntityAI mount = player.FindAttachmentBySlotName("Headgear");
        PredatePanoramicNVG goggles;
        if (mount) goggles = PredatePanoramicNVG.Cast(mount.FindAttachmentBySlotName("NVG"));
        if (!goggles)
        {
            mount = player.FindAttachmentBySlotName("Eyewear");
            if (mount) goggles = PredatePanoramicNVG.Cast(mount.FindAttachmentBySlotName("NVG"));
        }
        return goggles && goggles.m_IsLowered && goggles.IsWorking();
    }
}

#ifndef SERVER
modded class DayZPlayerCameraBase
{
    override void SetNVPostprocess(int NVtype)
    {
        PlayerBase player = PlayerBase.Cast(m_pPlayer);
        bool panoramic = player == GetGame().GetPlayer() && PredatePanoramicNVG.Active(player);
        PPERequester_CameraNV.s_PredatePanoramic = panoramic;
        super.SetNVPostprocess(NVtype);
        if (!panoramic) return;
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
