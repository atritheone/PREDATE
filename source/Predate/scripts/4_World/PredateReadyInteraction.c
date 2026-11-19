modded class ActionManagerClient
{
    override protected void FindContextualUserActions(int pCurrentCommandID)
    {
        // Vanilla clears the cursor target whenever the weapon is raised.
        if (m_ActionPossible && m_Player.PredateMayPreviewInteraction() && !GetGame().IsInventoryOpen())
            m_Targets.Update();
        super.FindContextualUserActions(pCurrentCommandID);
    }
}

modded class ContinuousInteractActionInput
{
    override void UpdatePossibleActions(PlayerBase player, ActionTarget target, ItemBase item, int action_condition_mask)
    {
        if (!player.PredateMayPreviewInteraction() || GetUAInput().ID() != UAAction)
        {
            super.UpdatePossibleActions(player, target, item, action_condition_mask);
            return;
        }
        // Preview only the actions on the interact binding. All other native
        // conditions, including reach and the target component, still apply.
        player.m_PredateInteractionPreview = true;
        super.UpdatePossibleActions(player, target, item, action_condition_mask & ~ActionConditionMask.ACM_RAISED);
        player.m_PredateInteractionPreview = false;
    }

    override void Update()
    {
        super.Update();
        if (!m_Player || !m_Enabled || GetUAInput().ID() != UAAction || m_Player.m_PredateInteractionTime <= 0) return;
        if (m_Player.IsRaised())
        {
            // The action drawn by the cursor is a preview until the weapon lowers.
            m_Active = false;
            m_JustActivate = false;
            return;
        }
        if (m_InputType != ActionInputType.AIT_SINGLE || !m_Player.m_PredateInteractionTap) return;
        ActionBase action = GetAction();
        if (!action || !action.CanBeStarted()) return;
        m_Player.m_PredateInteractionTap = false;
        m_Active = true;
        m_JustActivate = true;
    }
}

modded class ActionBase
{
    override protected int GetStanceMask(PlayerBase player)
    {
        int mask = super.GetStanceMask(player);
        if (!player.m_PredateInteractionPreview) return mask;
        if (mask & DayZPlayerConstants.STANCEMASK_ERECT) mask |= DayZPlayerConstants.STANCEMASK_RAISEDERECT;
        if (mask & DayZPlayerConstants.STANCEMASK_CROUCH) mask |= DayZPlayerConstants.STANCEMASK_RAISEDCROUCH;
        if (mask & DayZPlayerConstants.STANCEMASK_PRONE) mask |= DayZPlayerConstants.STANCEMASK_RAISEDPRONE;
        return mask;
    }
}
