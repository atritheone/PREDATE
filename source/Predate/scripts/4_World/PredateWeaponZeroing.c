modded class Weapon_Base
{
    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);
        if (rpc_type != PredateProtocol.WEAPON_ZEROING || !GetGame().IsServer() || !sender) return;
        PlayerBase owner = PlayerBase.Cast(GetHierarchyRootPlayer());
        if (!owner || !owner.IsAlive() || !owner.GetIdentity() || owner.GetIdentity().GetId() != sender.GetId() || owner.GetItemInHands() != this) return;
        int muzzle, step;
        string sight;
        if (!ctx.Read(muzzle) || !ctx.Read(step) || !ctx.Read(sight) || step < 0) return;
        if (muzzle >= 0 && muzzle < GetMuzzleCount())
            SetStepZeroing(muzzle, step);
        else if (muzzle == -1)
        {
            ItemOptics optic = GetAttachedOptics();
            if (optic && optic.GetType() == sight) optic.SetStepZeroing(step);
        }
    }
}

#ifndef SERVER
class PredateZeroingMemory
{
    static const string FILE = "$profile:PredateZeroing.json";
    ref map<string, int> Steps = new map<string, int>;

    bool Remember(string key, int step)
    {
        int previous;
        if (step < 0 || (Steps.Find(key, previous) && previous == step)) return false;
        Steps.Set(key, step);
        return true;
    }
}

modded class PlayerBase
{
    protected ref PredateZeroingMemory m_PredateZeroingMemory;
    protected Weapon_Base m_PredateZeroingWeapon;
    protected ItemOptics m_PredateZeroingOptic;

    override void HandleWeapons(float pDt, Entity pInHands, HumanInputController pInputs, out bool pExitIronSights)
    {
        bool controlled = GetGame().GetPlayer() == this && IsAlive();
        Weapon_Base weapon;
        ItemOptics optic;
        bool changingZeroing;
        bool zeroingOptic;
        if (controlled)
        {
            weapon = Weapon_Base.Cast(pInHands);
            if (weapon) optic = weapon.GetAttachedOptics();
            if (weapon && (weapon != m_PredateZeroingWeapon || optic != m_PredateZeroingOptic))
                PredateRestoreZeroing(weapon, optic);
            changingZeroing = weapon && (pInputs.IsZeroingUp() || pInputs.IsZeroingDown());
            zeroingOptic = changingZeroing && optic && (optic.IsInOptics() || optic.IsUsingWeaponIronsightsOverride());
        }
        super.HandleWeapons(pDt, pInHands, pInputs, pExitIronSights);
        if (changingZeroing)
            PredateRememberZeroing(weapon, optic, zeroingOptic);
    }

    protected void PredateRestoreZeroing(Weapon_Base weapon, ItemOptics optic)
    {
        if (!m_PredateZeroingMemory)
        {
            string error;
            if (FileExist(PredateZeroingMemory.FILE))
                JsonFileLoader<PredateZeroingMemory>.LoadFile(PredateZeroingMemory.FILE, m_PredateZeroingMemory, error);
            if (!m_PredateZeroingMemory || !m_PredateZeroingMemory.Steps) m_PredateZeroingMemory = new PredateZeroingMemory;
        }
        int step;
        if (weapon != m_PredateZeroingWeapon)
        {
            for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
            {
                if (m_PredateZeroingMemory.Steps.Find(weapon.GetType() + "/iron/" + muzzle, step) && weapon.SetStepZeroing(muzzle, step))
                    PredateSyncRestoredZeroing(weapon, muzzle, step, "");
            }
        }
        if (optic && m_PredateZeroingMemory.Steps.Find(weapon.GetType() + "/optic/" + optic.GetType(), step) && optic.SetStepZeroing(step))
            PredateSyncRestoredZeroing(weapon, -1, step, optic.GetType());
        m_PredateZeroingWeapon = weapon;
        m_PredateZeroingOptic = optic;
    }

    protected void PredateRememberZeroing(Weapon_Base weapon, ItemOptics optic, bool zeroingOptic)
    {
        bool changed;
        if (zeroingOptic)
            changed = m_PredateZeroingMemory.Remember(weapon.GetType() + "/optic/" + optic.GetType(), optic.GetStepZeroing());
        else
        {
            for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
            {
                if (m_PredateZeroingMemory.Remember(weapon.GetType() + "/iron/" + muzzle, weapon.GetStepZeroing(muzzle)))
                    changed = true;
            }
        }
        if (changed)
        {
            string error;
            JsonFileLoader<PredateZeroingMemory>.SaveFile(PredateZeroingMemory.FILE, m_PredateZeroingMemory, error);
        }
    }

    protected void PredateSyncRestoredZeroing(Weapon_Base weapon, int muzzle, int step, string sight)
    {
        if (!GetGame().IsMultiplayer()) return;
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(muzzle);
        rpc.Write(step);
        rpc.Write(sight);
        rpc.Send(weapon, PredateProtocol.WEAPON_ZEROING, true);
    }
}
#endif
