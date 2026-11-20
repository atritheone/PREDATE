modded class PlayerBase
{
    static const int PREDATE_PRIMARY_SHORTCUT_RPC = 918620;

    int m_PredateQuickbarUsedAt = -100000;
    bool m_PredateQuickbarHeld;
    Weapon_Base m_PredatePendingPrimaryShortcut;
    int m_PredateNextPrimaryShortcutSync;

    void PredateAssignPrimarySlot(Weapon_Base weapon)
    {
        // Save the shortcut server-side and retry delivery until the new
        // character has received the weapon in its inventory.
        m_QuickBarBase.OnSetEntityNoSync(weapon, 0, true);
        m_PredatePendingPrimaryShortcut = weapon;
        m_PredateNextPrimaryShortcutSync = 0;
    }

    void PredateSyncPrimarySlot()
    {
        Weapon_Base weapon = m_PredatePendingPrimaryShortcut;
        if (!weapon || GetGame().GetTime() < m_PredateNextPrimaryShortcutSync) return;
        if (GetQuickBarEntity(0) != weapon)
        {
            m_PredatePendingPrimaryShortcut = null;
            return;
        }
        m_PredateNextPrimaryShortcutSync = GetGame().GetTime() + 500;
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(weapon);
        rpc.Send(this, PREDATE_PRIMARY_SHORTCUT_RPC, true, GetIdentity());
    }

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);
        if (rpc_type != PREDATE_PRIMARY_SHORTCUT_RPC) return;
        Weapon_Base weapon;
        if (!ctx.Read(weapon) || !weapon) return;
        if (GetGame().IsServer())
        {
            if (sender && GetIdentity() && sender.GetId() == GetIdentity().GetId() && weapon == m_PredatePendingPrimaryShortcut)
                m_PredatePendingPrimaryShortcut = null;
            return;
        }
        if (GetGame().GetPlayer() != this || !IsPlayerLoaded() || weapon.GetHierarchyRootPlayer() != this) return;
        if (GetQuickBarEntity(0) != weapon) m_QuickBarBase.OnSetEntityNoSync(weapon, 0, true);
        if (GetQuickBarEntity(0) != weapon) return;
        ScriptRPC acknowledgement = new ScriptRPC;
        acknowledgement.Write(weapon);
        acknowledgement.Send(this, PREDATE_PRIMARY_SHORTCUT_RPC, true);
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
