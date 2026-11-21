modded class PlayerBase
{
    static const int PREDATE_WEAPON_SHORTCUT_RPC = 918620;

    int m_PredateQuickbarUsedAt = -100000;
    bool m_PredateQuickbarHeld;
    ref array<Weapon_Base> m_PredatePendingWeaponShortcuts = {null, null};
    int m_PredateNextWeaponShortcutSync;

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
        PredateMarkQuickbarUse();
        super.OnQuickBarSingleUse(slotClicked);
    }

    override void OnQuickBarContinuousUseStart(int slotClicked)
    {
        PredateMarkQuickbarUse();
        if (GetGame().GetPlayer() == this) m_PredateQuickbarHeld = true;
        super.OnQuickBarContinuousUseStart(slotClicked);
    }

    override void OnQuickBarContinuousUseEnd(int slotClicked)
    {
        PredateMarkQuickbarUse();
        if (GetGame().GetPlayer() == this) m_PredateQuickbarHeld = false;
        super.OnQuickBarContinuousUseEnd(slotClicked);
    }
}
