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
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        MM_LaserSight_Base module = MM_LaserSight_Base.PredateOnWeapon(item);
        return module && !module.IsRuined();
    }

    override void OnStartServer(ActionData action_data)
    {
        if (action_data.m_Player.GetItemInHands() != action_data.m_MainItem) return;
        MM_LaserSight_Base module = MM_LaserSight_Base.PredateOnWeapon(action_data.m_MainItem);
        if (module && !module.IsRuined()) module.PredateControlPEQ(action_data.m_Player, PredateControlKey());
    }

    protected int PredateControlKey() { return 0; }
}

class ActionPredatePEQWhite : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQWhiteInput; }
    override protected int PredateControlKey() { return 5; }
}
class ActionPredatePEQLaser : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQLaserInput; }
    override protected int PredateControlKey() { return 6; }
}
class ActionPredatePEQMode : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQModeInput; }
    override protected int PredateControlKey() { return 7; }
}
class ActionPredatePEQInfrared : ActionPredatePEQ
{
    override typename GetInputType() { return PredatePEQInfraredInput; }
    override protected int PredateControlKey() { return 8; }
}

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);
        actions.Insert(ActionPredatePEQWhite);
        actions.Insert(ActionPredatePEQLaser);
        actions.Insert(ActionPredatePEQMode);
        actions.Insert(ActionPredatePEQInfrared);
    }
}

modded class Weapon_Base
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionPredatePEQWhite);
        AddAction(ActionPredatePEQLaser);
        AddAction(ActionPredatePEQMode);
        AddAction(ActionPredatePEQInfrared);
    }
}

modded class PlayerBase
{
    protected bool PredatePEQUsesQuickbarKey(int slot)
    {
        return slot >= 5 && slot <= 8 && MM_LaserSight_Base.PredateOnWeapon(GetItemInHands()) != null;
    }

    override void OnQuickBarSingleUse(int slotClicked)
    {
        if (!PredatePEQUsesQuickbarKey(slotClicked)) super.OnQuickBarSingleUse(slotClicked);
    }

    override void OnQuickBarContinuousUseStart(int slotClicked)
    {
        if (!PredatePEQUsesQuickbarKey(slotClicked)) super.OnQuickBarContinuousUseStart(slotClicked);
    }

    override void OnQuickBarContinuousUseEnd(int slotClicked)
    {
        if (!PredatePEQUsesQuickbarKey(slotClicked) || m_PredateQuickbarHeld) super.OnQuickBarContinuousUseEnd(slotClicked);
    }
}
