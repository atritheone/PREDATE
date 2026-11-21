modded class ActionTargetsCursor
{
    override protected string GetItemDesc(ActionBase action)
    {
        Object target = m_DisplayInteractTarget;
        if (!target && m_Target) target = m_Target.GetObject();
        if (target && !target.IsAlive())
        {
            // Match the cursor's name source when hovering a ruined attachment.
            if (m_Target && m_Target.GetParent() && !target.DisplayNameRuinAttach())
                target = m_Target.GetParent();
            if (PlayerBase.Cast(target) && !target.IsAlive()) return "OPERATOR";
        }
        return super.GetItemDesc(action);
    }
}
