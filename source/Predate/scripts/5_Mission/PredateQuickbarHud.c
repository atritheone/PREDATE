modded class IngameHud
{
    float m_PredateQuickbarAlpha;
    ref map<Widget, float> m_PredateQuickbarOriginalAlpha = new map<Widget, float>;
    bool m_PredateQuickbarReported;

    void PredateFadeQuickbarChildren(Widget parent, float alpha)
    {
        Widget child = parent.GetChildren();
        while (child)
        {
            if (!m_PredateQuickbarOriginalAlpha.Contains(child))
                m_PredateQuickbarOriginalAlpha.Insert(child, child.GetAlpha());
            child.SetAlpha(m_PredateQuickbarOriginalAlpha.Get(child) * alpha);
            PredateFadeQuickbarChildren(child, alpha);
            child = child.GetSibling();
        }
    }

    void PredateRestoreQuickbarAlpha()
    {
        foreach (Widget child, float alpha : m_PredateQuickbarOriginalAlpha)
            if (child) child.SetAlpha(alpha);
        m_PredateQuickbarOriginalAlpha.Clear();
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        if (!m_QuickbarWidget || !m_HudVisibility) return;
        if (!m_PredateQuickbarReported)
        {
            Print("[Predate] Auto-hide quickbar initialized: fade in 0.15s, hold 2s, fade out 0.5s.");
            m_PredateQuickbarReported = true;
        }
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        bool inventory = m_HudVisibility.IsContextFlagActive(EHudContextFlags.INVENTORY_OPEN);
        bool menu = m_HudVisibility.IsContextFlagActive(EHudContextFlags.MENU_OPEN);
        bool permitted = player && player.IsAlive() && !player.IsUnconscious() && (!menu || inventory);
        bool active;
        if (permitted)
        {
            active = inventory || player.m_PredateQuickbarHeld || GetGame().GetTime() - player.m_PredateQuickbarUsedAt < 2000;
            if (active && m_HudVisibility.IsContextFlagActive(EHudContextFlags.QUICKBAR_HIDE))
                super.ShowQuickbarPlayer(true);
        }
        else if (player) player.m_PredateQuickbarHeld = false;

        if (m_HudVisibility.IsContextFlagActive(EHudContextFlags.QUICKBAR_DISABLE) || m_HudVisibility.IsContextFlagActive(EHudContextFlags.QUICKBAR_GLOBAL))
            permitted = false;

        if (!permitted) m_PredateQuickbarAlpha = 0;
        else if (active) m_PredateQuickbarAlpha = Math.Min(1, m_PredateQuickbarAlpha + timeslice / 0.15);
        else m_PredateQuickbarAlpha = Math.Max(0, m_PredateQuickbarAlpha - timeslice / 0.5);
        // Generated item previews opt out of their parent's alpha. Fade their
        // widgets too, and hide the whole grid once the fade finishes.
        m_QuickbarWidget.Show(permitted && m_PredateQuickbarAlpha > 0);
        if (m_PredateQuickbarAlpha > 0 && m_PredateQuickbarAlpha < 1)
            PredateFadeQuickbarChildren(m_QuickbarWidget, m_PredateQuickbarAlpha);
        else PredateRestoreQuickbarAlpha();
    }
}
