modded class QuickBarBase
{
    static const int PREDATE_SLOT_COUNT = 4;

    override void updateSlotsCount()
    {
        SetSize(PREDATE_SLOT_COUNT);
    }

    override void SetEntityShortcut(EntityAI entity, int index, bool force = false)
    {
        if (index >= PREDATE_SLOT_COUNT) return;
        super.SetEntityShortcut(entity, index, force);
    }
}

modded class PlayerBase
{
    static const int PREDATE_WEAPON_SHORTCUT_RPC = 918620;

    int m_PredateQuickbarUsedAt = -100000;
    bool m_PredateQuickbarHeld;
    ref array<Weapon_Base> m_PredatePendingWeaponShortcuts = {null, null};
    int m_PredateNextWeaponShortcutSync;
    int m_PredateQuickbarSwitchSlot;
    int m_PredateQuickbarSwitchDeadline;
    EntityAI m_PredateQuickbarSwitchItem;
    EntityAI m_PredateQuickbarSwitchHands;

    void PredateAssignWeaponSlot(Weapon_Base weapon, int index)
    {
        // Save the shortcut server-side and retry delivery until the new
        // character has received the weapon in its inventory.
        m_QuickBarBase.OnSetEntityNoSync(weapon, index, true);
        m_PredatePendingWeaponShortcuts[index] = weapon;
        m_PredateNextWeaponShortcutSync = 0;
    }

    void PredateSyncWeaponSlots()
    {
        if ((!m_PredatePendingWeaponShortcuts[0] && !m_PredatePendingWeaponShortcuts[1]) || GetGame().GetTime() < m_PredateNextWeaponShortcutSync) return;
        m_PredateNextWeaponShortcutSync = GetGame().GetTime() + 500;
        foreach (int index, Weapon_Base weapon : m_PredatePendingWeaponShortcuts)
        {
            if (!weapon) continue;
            if (GetQuickBarEntity(index) != weapon)
            {
                m_PredatePendingWeaponShortcuts[index] = null;
                continue;
            }
            ScriptRPC rpc = new ScriptRPC;
            rpc.Write(index);
            rpc.Write(weapon);
            rpc.Send(this, PREDATE_WEAPON_SHORTCUT_RPC, true, GetIdentity());
        }
    }

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);
        if (rpc_type != PREDATE_WEAPON_SHORTCUT_RPC) return;
        int index;
        Weapon_Base weapon;
        if (!ctx.Read(index) || index < 0 || index >= m_PredatePendingWeaponShortcuts.Count() || !ctx.Read(weapon) || !weapon) return;
        if (GetGame().IsServer())
        {
            if (sender && GetIdentity() && sender.GetId() == GetIdentity().GetId() && weapon == m_PredatePendingWeaponShortcuts[index])
                m_PredatePendingWeaponShortcuts[index] = null;
            return;
        }
        if (GetGame().GetPlayer() != this || !IsPlayerLoaded() || weapon.GetHierarchyRootPlayer() != this) return;
        if (GetQuickBarEntity(index) != weapon) m_QuickBarBase.OnSetEntityNoSync(weapon, index, true);
        if (GetQuickBarEntity(index) != weapon) return;
        ScriptRPC acknowledgement = new ScriptRPC;
        acknowledgement.Write(index);
        acknowledgement.Write(weapon);
        acknowledgement.Send(this, PREDATE_WEAPON_SHORTCUT_RPC, true);
    }

    void PredateMarkQuickbarUse()
    {
        if (GetGame().GetPlayer() == this) m_PredateQuickbarUsedAt = GetGame().GetTime();
    }

    override void OnQuickBarSingleUse(int slotClicked)
    {
        if (slotClicked < 1 || slotClicked > QuickBarBase.PREDATE_SLOT_COUNT) return;
        PredateMarkQuickbarUse();
        EntityAI hands = GetEntityInHands();
        if (GetGame().GetPlayer() == this && IsRaised() && GetCommand_Move() && Weapon_Base.Cast(hands))
        {
            EntityAI item = GetQuickBarEntity(slotClicked - 1);
            if (item)
            {
                // DayZ discards quickbar taps while raised. Lower first and
                // let its normal inventory action handle the actual swap.
                m_PredateQuickbarSwitchSlot = slotClicked;
                m_PredateQuickbarSwitchItem = item;
                m_PredateQuickbarSwitchHands = hands;
                m_PredateQuickbarSwitchDeadline = GetGame().GetTime() + 1500;
                return;
            }
        }
        PredateClearQuickbarSwitch();
        super.OnQuickBarSingleUse(slotClicked);
    }

    bool PredateUpdateQuickbarSwitch(bool canSetReady)
    {
        if (!m_PredateQuickbarSwitchSlot) return false;
        bool expired = GetGame().GetTime() >= m_PredateQuickbarSwitchDeadline;
        bool sameShortcut = m_PredateQuickbarSwitchItem && GetQuickBarEntity(m_PredateQuickbarSwitchSlot - 1) == m_PredateQuickbarSwitchItem;
        if (!canSetReady || expired || !sameShortcut || GetEntityInHands() != m_PredateQuickbarSwitchHands)
        {
            PredateClearQuickbarSwitch();
            return false;
        }
        if (!IsRaised() && !IsInOptics() && !IsInIronsights() && GetDayZPlayerInventory().IsIdle() && ScriptInputUserData.CanStoreInputUserData())
            OnQuickBarSingleUse(m_PredateQuickbarSwitchSlot);
        return true;
    }

    void PredateClearQuickbarSwitch()
    {
        m_PredateQuickbarSwitchSlot = 0;
        m_PredateQuickbarSwitchItem = null;
        m_PredateQuickbarSwitchHands = null;
    }

    override void OnQuickBarContinuousUseStart(int slotClicked)
    {
        if (slotClicked < 1 || slotClicked > QuickBarBase.PREDATE_SLOT_COUNT) return;
        PredateMarkQuickbarUse();
        if (GetGame().GetPlayer() == this) m_PredateQuickbarHeld = true;
        super.OnQuickBarContinuousUseStart(slotClicked);
    }

    override void OnQuickBarContinuousUseEnd(int slotClicked)
    {
        if (slotClicked < 1 || slotClicked > QuickBarBase.PREDATE_SLOT_COUNT) return;
        PredateMarkQuickbarUse();
        if (GetGame().GetPlayer() == this) m_PredateQuickbarHeld = false;
        super.OnQuickBarContinuousUseEnd(slotClicked);
    }
}
