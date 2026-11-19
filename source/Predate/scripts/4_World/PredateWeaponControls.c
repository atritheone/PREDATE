// Reuse the player's Reload binding and DayZ's normal weapon action pipeline.
class PredateReloadInput : NoIndicationActionInputBase
{
    int m_PredatePendingReload;

    void PredateReloadInput(PlayerBase player)
    {
        SetInput("UAReloadMagazine");
        m_InputType = ActionInputType.AIT_SINGLE;
    }

    override void Update()
    {
        m_Active = false;
        m_JustActivate = false;
        if (!m_Enabled || !GetGame().GetInput().HasGameFocus())
        {
            m_PredatePendingReload = 0;
            return;
        }
        UAInput input = m_input.InputP();
        if (input.LocalDoubleClick() || input.LocalHoldBegin() || input.LocalHold())
        {
            m_PredatePendingReload = 0;
            return;
        }
        // Leave a short window for the second tap to cancel the reload.
        if (input.LocalPress() && m_PredatePendingReload > 0) m_PredatePendingReload = 0;
        if (input.LocalClick()) m_PredatePendingReload = GetGame().GetTime() + 400;
        if (m_PredatePendingReload > 0 && GetGame().GetTime() >= m_PredatePendingReload)
        {
            m_PredatePendingReload = 0;
            m_Active = true;
            m_JustActivate = true;
        }
    }
}

class PredateRackInput : WeaponManipulationActionInput
{
    void PredateRackInput(PlayerBase player)
    {
        m_InputType = ActionInputType.AIT_DOUBLECLICK;
    }

    override void Update()
    {
        super.Update();
        if (m_JustActivate && m_Player && m_Player.GetWeaponManager())
            m_Player.GetWeaponManager().SetEjectBulletTryTimestamp();
    }
}

modded class FirearmActionAttachMagazineQuick
{
    override typename GetInputType() { return PredateReloadInput; }
}

modded class FirearmActionLoadBulletQuick
{
    override typename GetInputType() { return PredateReloadInput; }
}

modded class FirearmActionLoadMultiBulletQuick
{
    override typename GetInputType() { return PredateReloadInput; }
}

modded class FirearmActionMechanicManipulate
{
    override typename GetInputType() { return PredateRackInput; }
}

modded class FirearmActionUnjam
{
    override typename GetInputType() { return PredateRackInput; }
}

modded class PlayerBase
{
    Weapon_Base m_PredateCheckingWeapon;
    Weapon_Base m_PredateMagazineMessageWeapon;
    int m_PredateMagazineCheckFinish;
    int m_PredateMagazineMessageUntil;
    string m_PredateMagazineMessage;

    string PredateMagazineEstimate(Weapon_Base weapon)
    {
        int muzzle = weapon.GetCurrentMuzzle();
        int rounds;
        int capacity;
        Magazine magazine = weapon.GetMagazine(muzzle);
        if (magazine)
        {
            rounds = magazine.GetAmmoCount();
            capacity = magazine.GetAmmoMax();
        }
        else if (weapon.HasInternalMagazine(muzzle))
        {
            rounds = weapon.GetInternalMagazineCartridgeCount(muzzle);
            capacity = weapon.GetInternalMagazineMaxCartridgeCount(muzzle);
        }
        else return "No magazine inserted";
        if (rounds <= 0) return "Magazine  empty";
        if (capacity <= 0) return "Magazine  unknown";
        if (rounds >= capacity) return "Magazine  full";
        float fraction = rounds / (capacity * 1.0);
        if (fraction > 0.66) return "Magazine  mostly full";
        if (fraction > 0.33) return "Magazine  about half";
        return "Magazine  low";
    }

    override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
    {
        super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);
        if (GetGame().GetPlayer() != this || GetGame().IsDedicatedServer()) return;
        Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
        bool permitted = IsAlive() && !IsUnconscious() && weapon && weapon.IsIdle() && !GetWeaponManager().IsRunning();
        permitted = permitted && !GetGame().GetUIManager().GetMenu() && GetGame().GetInput().HasGameFocus();
        if (!permitted)
        {
            if (m_PredateMagazineCheckFinish > 0) m_PredateMagazineMessageUntil = 0;
            m_PredateMagazineCheckFinish = 0;
            m_PredateCheckingWeapon = null;
            return;
        }
        int now = GetGame().GetTime();
        if (GetUApi().GetInputByID(UAReloadMagazine).LocalHoldBegin())
        {
            m_PredateCheckingWeapon = weapon;
            m_PredateMagazineMessageWeapon = weapon;
            m_PredateMagazineCheckFinish = now + 1000;
            m_PredateMagazineMessage = "Checking magazine...";
            m_PredateMagazineMessageUntil = now + 1000;
        }
        if (m_PredateMagazineCheckFinish > 0 && weapon != m_PredateCheckingWeapon)
        {
            m_PredateMagazineCheckFinish = 0;
            m_PredateMagazineMessageUntil = 0;
            m_PredateCheckingWeapon = null;
        }
        if (m_PredateMagazineCheckFinish > 0 && now >= m_PredateMagazineCheckFinish)
        {
            m_PredateMagazineMessage = PredateMagazineEstimate(weapon);
            m_PredateMagazineMessageUntil = now + 3000;
            m_PredateMagazineCheckFinish = 0;
            m_PredateCheckingWeapon = null;
            Print("[Predate] Magazine check completed without altering ammunition.");
        }
    }
}
