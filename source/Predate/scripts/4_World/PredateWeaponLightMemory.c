#ifdef SERVER
modded class ItemBase
{
    protected bool m_PredateStoredLightOn;
    protected bool m_PredateLightStowed;
    protected int m_PredateLoadedWeaponModuleState = -1;

    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);
        if (!GetCompEM() || !PredateSupportsWeaponControl(PredateWeaponControl.WHITE_LIGHT) || !Weapon_Base.Cast(GetHierarchyParent())) return;
        Man holder = GetHierarchyRootPlayer();
        if (!holder || eAIBase.Cast(holder)) return;
        CF_ModStorage data;
        if (storage.Find("Predate", data)) data.Write(PredatePackWeaponModuleState());
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage)) return false;
        CF_ModStorage data;
        int state;
        if (storage.Find("Predate", data) && data.Read(state)) m_PredateLoadedWeaponModuleState = state;
        return true;
    }

    protected int PredatePackWeaponModuleState()
    {
        // The low bit is power intent. Device-specific settings use the remaining bits.
        bool powered = m_PredateStoredLightOn;
        if (!m_PredateLightStowed) powered = GetCompEM().IsSwitchedOn();
        if (powered) return 1;
        return 0;
    }

    void PredateRestoreLoadedWeaponModule(PlayerBase player)
    {
        if (m_PredateLoadedWeaponModuleState < 0) return;
        Weapon_Base weapon = Weapon_Base.Cast(GetHierarchyParent());
        if (weapon && GetCompEM()) PredateApplyWeaponModuleState(m_PredateLoadedWeaponModuleState, player.GetItemInHands() == weapon);
        m_PredateLoadedWeaponModuleState = -1;
    }

    void PredateTransferWeaponModule(map<string, int> states, Weapon_Base weapon, bool drawn, bool restore)
    {
        if (!GetCompEM() || !PredateSupportsWeaponControl(PredateWeaponControl.WHITE_LIGHT)) return;
        InventoryLocation location = new InventoryLocation;
        if (!GetInventory().GetCurrentInventoryLocation(location)) return;
        string key = weapon.GetType() + "/" + location.GetSlot() + "/" + GetType();
        if (restore)
        {
            int state;
            if (!IsRuined() && states.Find(key, state)) PredateApplyWeaponModuleState(state, drawn);
        }
        else states.Set(key, PredatePackWeaponModuleState());
    }

    protected void PredateApplyWeaponModuleState(int state, bool drawn)
    {
        ComponentEnergyManager energy = GetCompEM();
        m_PredateLightStowed = !drawn;
        m_PredateStoredLightOn = (state & 1) != 0;
        if (drawn && m_PredateStoredLightOn && energy.CanWork())
        {
            if (!energy.IsSwitchedOn()) energy.SwitchOn();
        }
        else if (energy.IsSwitchedOn()) energy.SwitchOff();
        SetSynchDirty();
    }

    void PredateSetWeaponLightDrawn(bool drawn)
    {
        ComponentEnergyManager energy = GetCompEM();
        bool stowing = !drawn;
        if (!energy || m_PredateLightStowed == stowing) return;
        m_PredateLightStowed = stowing;
        if (!drawn)
        {
            m_PredateStoredLightOn = energy.IsSwitchedOn();
            if (m_PredateStoredLightOn) energy.SwitchOff();
        }
        else if (m_PredateStoredLightOn && energy.CanWork())
        {
            energy.SwitchOn();
        }
    }
}

modded class PlayerBase
{
    void PredateTransferWeaponModules(map<string, int> states, bool restore)
    {
        if (!restore) states.Clear();
        array<EntityAI> inventory = {};
        GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
        {
            ItemBase attachment = ItemBase.Cast(entity);
            if (!attachment) continue;
            Weapon_Base weapon = Weapon_Base.Cast(attachment.GetHierarchyParent());
            if (weapon) attachment.PredateTransferWeaponModule(states, weapon, GetItemInHands() == weapon, restore);
        }
    }

    override void OnSelectPlayer()
    {
        super.OnSelectPlayer();
        if (!IsAlive() || eAIBase.Cast(this)) return;
        // The native store has attached the weapons and their batteries by now.
        array<EntityAI> inventory = {};
        GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
        {
            ItemBase item = ItemBase.Cast(entity);
            if (item) item.PredateRestoreLoadedWeaponModule(this);
        }
    }

    override void EEItemOutOfHands(EntityAI item)
    {
        PredateUpdateStowedWeaponLights(item, false);
        super.EEItemOutOfHands(item);
    }

    override void EEItemIntoHands(EntityAI item)
    {
        super.EEItemIntoHands(item);
        PredateUpdateStowedWeaponLights(item, true);
    }

    protected void PredateUpdateStowedWeaponLights(EntityAI item, bool drawn)
    {
        if (eAIBase.Cast(this) || !Weapon_Base.Cast(item)) return;
        GameInventory inventory = item.GetInventory();
        for (int index = 0; index < inventory.AttachmentCount(); index++)
        {
            ItemBase attachment = ItemBase.Cast(inventory.GetAttachmentFromIndex(index));
            if (attachment && attachment.PredateSupportsWeaponControl(PredateWeaponControl.WHITE_LIGHT)) attachment.PredateSetWeaponLightDrawn(drawn);
        }
    }
}
#endif
