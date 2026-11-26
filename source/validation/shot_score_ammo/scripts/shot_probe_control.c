class PredateShotProbeControl
{
    static const int ALIGN = 9136129;
    static const int FIRE = 9136130;
    static ref ScriptInvoker Align = new ScriptInvoker;
}

modded class PlayerBase
{
    int m_ShotProbeStage;
    float m_ShotProbeAt;
    int m_ShotProbeServerFire;
    int m_ShotProbeServerShots;
    bool m_ShotProbeBurst;

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        if (GetGame().IsServer() && (rpc_type == PredateShotProbeControl.ALIGN || rpc_type == PredateShotProbeControl.FIRE))
        {
            if (!sender || !GetIdentity() || sender.GetId() != GetIdentity().GetId() || !FileExist("$profile:PredateShotTrace.txt")) return;
            if (rpc_type == PredateShotProbeControl.FIRE)
            {
                if (m_ShotProbeServerShots == 0) m_ShotProbeBurst = FileExist("$profile:PredateShotBurst.txt");
                m_ShotProbeServerFire++;
                return;
            }
            vector origin;
            vector direction;
            if (ctx.Read(origin) && ctx.Read(direction))
                PredateShotProbeControl.Align.Invoke(this, origin, direction);
            return;
        }
        super.OnRPC(sender, rpc_type, ctx);
    }

    override void HandleWeapons(float pDt, Entity pInHands, HumanInputController pInputs, out bool pExitIronSights)
    {
        super.HandleWeapons(pDt, pInHands, pInputs, pExitIronSights);
        if (GetGame().IsServer())
        {
            if (m_ShotProbeServerFire == 0) return;
            Weapon_Base serverWeapon = Weapon_Base.Cast(pInHands);
            if (!serverWeapon || !GetWeaponManager().CanFire(serverWeapon)) return;
            m_ShotProbeServerFire--;
            Print("[PredateShot] requested server fire from synced avatar");
            GetWeaponManager().Fire(serverWeapon);
            m_ShotProbeServerShots++;
            return;
        }
        if (GetGame().GetPlayer() != this || !IsCLIParam("predateShotProbeClient") || !IsRaised()) return;
        bool burst = IsCLIParam("predateShotProbeBurst");
        int endStage = 4;
        if (burst) endStage = 12;
        if (m_ShotProbeStage >= endStage) return;
        Weapon_Base weapon = Weapon_Base.Cast(GetEntityInHands());
        if (!weapon || !GetWeaponManager().CanFire(weapon)) return;
        float now = GetGame().GetTickTime();
        if (m_ShotProbeAt == 0) m_ShotProbeAt = now + 4;
        if (now < m_ShotProbeAt) return;
        if (!burst && m_ShotProbeStage == 1)
        {
            vector muzzle = weapon.ModelToWorld(weapon.GetSelectionPositionMS("usti hlavne"));
            vector chamber = weapon.ModelToWorld(weapon.GetSelectionPositionMS("konec hlavne"));
            ScriptRPC rpc = new ScriptRPC;
            rpc.Write(muzzle);
            rpc.Write(vector.Direction(chamber, muzzle).Normalized());
            rpc.Send(this, PredateShotProbeControl.ALIGN, true);
            Print("[PredateShot] requested alignment from muzzle=" + muzzle);
            m_ShotProbeAt = now + 2;
        }
        else
        {
            Print("[PredateShot] requested fire at stage=" + m_ShotProbeStage);
            GetWeaponManager().Fire(weapon);
            ScriptRPC fire = new ScriptRPC;
            fire.Send(this, PredateShotProbeControl.FIRE, true);
            m_ShotProbeAt = now + 1;
            if (burst) m_ShotProbeAt = now + 0.12;
        }
        m_ShotProbeStage++;
    }
}

modded class WeaponFire
{
    override void OnEntry(WeaponEventBase e)
    {
        if (e) PredateShotTag.Apply(m_weapon);
        super.OnEntry(e);
    }
}

class PredateShotTag
{
    static void Apply(Weapon_Base weapon)
    {
        PlayerBase player = PlayerBase.Cast(weapon.GetHierarchyRootPlayer());
        if (!player || player.IsAI()) return;
        if (!GetGame().IsServer() || !FileExist("$profile:PredateShotProbeAlias.txt")) return;
        int muzzle = weapon.GetCurrentMuzzle();
        float damage;
        string ammo;
        if (!weapon.GetCartridgeInfo(muzzle, damage, ammo) || ammo != "Bullet_556x45") return;
        string tag = "PredateProbe556_Sky";
        if (player.m_ShotProbeServerShots == 1) tag = "PredateProbe556_Body";
        if (player.m_ShotProbeServerShots == 2) tag = "PredateProbe556_Air";
        if (!weapon.PopCartridgeFromChamber(muzzle, damage, ammo)) return;
        bool chambered = weapon.PushCartridgeToChamber(muzzle, damage, tag);
        Print("[PredateShot] tag " + tag + " chambered=" + chambered + " server=" + GetGame().IsServer());
        if (!chambered) weapon.PushCartridgeToChamber(muzzle, damage, ammo);
    }
}

modded class DayZPlayerImplementAiming
{
    override bool ProcessAimFilters(float pDt, SDayZPlayerAimingModel pModel, int stance_index)
    {
        bool result = super.ProcessAimFilters(pDt, pModel, stance_index);
        PlayerBase player = PlayerBase.Cast(m_PlayerDpi);
        if (!player || player.IsAI()) return result;
        bool air;
        if (GetGame().IsServer())
            air = player.m_ShotProbeServerShots == 2 || player.m_ShotProbeBurst;
        else air = IsCLIParam("predateShotProbeClient") && (player.m_ShotProbeStage == 3 || IsCLIParam("predateShotProbeBurst"));
        if (air)
            pModel.m_fAimYMouseShift += Math.Clamp(72 - pModel.m_fCurrentAimY, -6, 6);
        return result;
    }
}

modded class Weapon_Base
{
    override void EEFired(int muzzleType, int mode, string ammoType)
    {
        super.EEFired(muzzleType, mode, ammoType);
        if (!GetGame().IsServer() && IsCLIParam("predateShotProbeClient") && GetHierarchyRootPlayer() == GetGame().GetPlayer())
            Print("[PredateShot] local EEFired " + ammoType);
    }
}
