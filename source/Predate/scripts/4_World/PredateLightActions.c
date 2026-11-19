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
