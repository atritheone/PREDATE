enum PredateWeaponControl
{
    WHITE_LIGHT = 5,
    LASER_POWER,
    LASER_MODE,
    INFRARED_LIGHT
}

modded class ItemBase
{
    bool PredateSupportsWeaponControl(int control)
    {
        return control == PredateWeaponControl.WHITE_LIGHT && IsLightSource() && GetCompEM();
    }

    void PredateUseWeaponControl(PlayerBase player, int control)
    {
        ComponentEnergyManager energy = GetCompEM();
        if (energy.IsSwitchedOn()) energy.SwitchOff();
        else if (energy.CanWork()) energy.SwitchOn();
        else
        {
            player.PredateWeaponModuleMessage("Weapon module needs a working battery.");
            return;
        }
        if (energy.IsSwitchedOn()) player.PredateWeaponModuleMessage("White Light On");
        else player.PredateWeaponModuleMessage("White Light Off");
    }
}

class PredatePEQWhiteInput : NoIndicationActionInputBase
{
    void PredatePEQWhiteInput(PlayerBase player) { SetInput("UAPredatePEQWhite"); m_InputType = ActionInputType.AIT_SINGLE; }
}

class PredatePEQLaserInput : NoIndicationActionInputBase
{
    void PredatePEQLaserInput(PlayerBase player) { SetInput("UAPredatePEQLaser"); m_InputType = ActionInputType.AIT_SINGLE; }
}

class PredatePEQModeInput : NoIndicationActionInputBase
{
    void PredatePEQModeInput(PlayerBase player) { SetInput("UAPredatePEQMode"); m_InputType = ActionInputType.AIT_SINGLE; }
}

class PredatePEQInfraredInput : NoIndicationActionInputBase
{
    void PredatePEQInfraredInput(PlayerBase player) { SetInput("UAPredatePEQInfrared"); m_InputType = ActionInputType.AIT_SINGLE; }
}

class ActionPredatePEQ : ActionPredateWeaponModule
{
    protected int PredateControlKey() { return 0; }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        return UseAttachments(item, PredateControlKey(), null);
    }

    override void OnStartServer(ActionData action_data)
    {
        if (action_data.m_Player.GetItemInHands() != action_data.m_MainItem) return;
        UseAttachments(action_data.m_MainItem, PredateControlKey(), action_data.m_Player);
    }

    protected bool UseAttachments(ItemBase weapon, int control, PlayerBase player)
    {
        if (!Weapon_Base.Cast(weapon)) return false;
        GameInventory inventory = weapon.GetInventory();
        int count = inventory.AttachmentCount();
        bool found;
        for (int index = 0; index < count; index++)
        {
            ItemBase attachment = ItemBase.Cast(inventory.GetAttachmentFromIndex(index));
            if (!attachment || attachment.IsRuined() || !attachment.PredateSupportsWeaponControl(control)) continue;
            if (!player) return true;
            attachment.PredateUseWeaponControl(player, control);
            found = true;
        }
        return found;
    }
}

class ActionPredatePEQWhite : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQWhiteInput; }
    override protected int PredateControlKey() { return PredateWeaponControl.WHITE_LIGHT; }
}

class ActionPredatePEQLaser : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQLaserInput; }
    override protected int PredateControlKey() { return PredateWeaponControl.LASER_POWER; }
}

class ActionPredatePEQMode : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQModeInput; }
    override protected int PredateControlKey() { return PredateWeaponControl.LASER_MODE; }
}

class ActionPredatePEQInfrared : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQInfraredInput; }
    override protected int PredateControlKey() { return PredateWeaponControl.INFRARED_LIGHT; }
}
