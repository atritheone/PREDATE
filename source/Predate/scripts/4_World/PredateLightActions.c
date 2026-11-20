// The automatic ready stance would otherwise block the native light controls.
class PredateLightActions
{
    static int AllowRaisedStances(int stanceMask)
    {
        return stanceMask | DayZPlayerConstants.STANCEMASK_RAISEDERECT | DayZPlayerConstants.STANCEMASK_RAISEDCROUCH | DayZPlayerConstants.STANCEMASK_RAISEDPRONE;
    }
}

class PredateNVGInput : ToggleNVGActionInput
{
    void PredateNVGInput(PlayerBase player)
    {
        SetInput("UAPredateToggleNVG");
        m_InputType = ActionInputType.AIT_SINGLE;
    }
}

modded class ActionToggleNVG
{
    override typename GetInputType()
    {
        return PredateNVGInput;
    }

    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

modded class ActionTurnOnHeadtorch
{
    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

modded class ActionTurnOffHeadtorch
{
    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

modded class ActionTurnOnHelmetFlashlight
{
    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

modded class ActionTurnOffHelmetFlashlight
{
    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

class PredateWeaponLightInput : NoIndicationActionInputBase
{
    void PredateWeaponLightInput(PlayerBase player)
    {
        SetInput("UAPredateWeaponLight");
        m_InputType = ActionInputType.AIT_SINGLE;
    }
}

modded class ActionTurnOnWeaponFlashlight
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        return !PredateWeaponModule.OnWeapon(item) && super.ActionCondition(player, target, item);
    }

    override typename GetInputType()
    {
        return PredateWeaponLightInput;
    }

    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

modded class ActionTurnOffWeaponFlashlight
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        return !PredateWeaponModule.OnWeapon(item) && super.ActionCondition(player, target, item);
    }

    override typename GetInputType()
    {
        return PredateWeaponLightInput;
    }

    override bool CanBeUsedRaised()
    {
        return true;
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }
}

class PredateWeaponLightModeInput : NoIndicationActionInputBase
{
    void PredateWeaponLightModeInput(PlayerBase player)
    {
        SetInput("UAPredateWeaponLightMode");
        m_InputType = ActionInputType.AIT_SINGLE;
    }
}

class ActionPredateWeaponModule : ActionInstantBase
{
    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINonRuined;
        m_ConditionTarget = new CCTNone;
    }

    override bool HasTarget() { return false; }
    override bool CanBeUsedRaised() { return true; }

    override protected int GetStanceMask(PlayerBase player)
    {
        return PredateLightActions.AllowRaisedStances(super.GetStanceMask(player));
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        PredateWeaponModule module = PredateWeaponModule.OnWeapon(item);
        return module && !module.IsRuined();
    }

    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);
        if (action_data.m_Player.GetItemInHands() != action_data.m_MainItem) return;
        PredateWeaponModule module = PredateWeaponModule.OnWeapon(action_data.m_MainItem);
        if (module && !module.IsRuined()) UseModule(module, action_data.m_Player);
    }

    protected void UseModule(PredateWeaponModule module, PlayerBase player)
    {
    }
}

class ActionPredateWeaponLightMode : ActionPredateWeaponModule
{
    void ActionPredateWeaponLightMode() { m_Text = "Cycle weapon module mode"; }
    override typename GetInputType() { return PredateWeaponLightModeInput; }

    override protected void UseModule(PredateWeaponModule module, PlayerBase player)
    {
        module.CycleMode(player);
    }
}

class ActionPredateWeaponLightPower : ActionPredateWeaponModule
{
    void ActionPredateWeaponLightPower() { m_Text = "Toggle weapon module power"; }
    override typename GetInputType() { return PredateWeaponLightInput; }

    override protected void UseModule(PredateWeaponModule module, PlayerBase player)
    {
        module.TogglePower(player);
    }
}

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);
        actions.Insert(ActionPredateWeaponLightMode);
        actions.Insert(ActionPredateWeaponLightPower);
    }
}

modded class Weapon_Base
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionPredateWeaponLightMode);
        AddAction(ActionPredateWeaponLightPower);
    }
}
