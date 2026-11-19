modded class PlayerBase
{
    int m_PredateQuickbarUsedAt = -100000;
    bool m_PredateQuickbarHeld;

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
