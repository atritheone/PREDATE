modded class IngameHud
{
    int m_PredateHealthTendency;
    int m_PredateBloodTendency;
    int m_PredateHealthStatus = 1;
    int m_PredateBloodStatus = 1;
    bool m_PredateHealthShown = true;
    bool m_PredateBloodShown = true;

    override void Init(Widget hud_panel_widget)
    {
        super.Init(hud_panel_widget);
        SetStaminaBarVisibility(false);
        SetPermanentCrossHair(false);
        bool staminaHidden = m_Stamina && !m_Stamina.IsVisible() && m_StaminaBackground && !m_StaminaBackground.IsVisible();
        Print("[Predate] Stamina HUD hidden: " + staminaHidden);
        Widget presence = hud_panel_widget.FindAnyWidget("PresencePanel");
        Widget stance = hud_panel_widget.FindAnyWidget("StancePanel");
        if (presence) presence.Show(false);
        if (stance) stance.Show(false);
        bool noiseHidden = presence && !presence.IsVisible();
        bool stanceHidden = stance && !stance.IsVisible();
        Print("[Predate] HUD hidden: noise=" + noiseHidden + "; stance=" + stanceHidden);
        PredateHideSurvivalNotifier("Hungry");
        PredateHideSurvivalNotifier("Thirsty");
        PredateHideSurvivalNotifier("Temperature");
        HideTemperature();
        Widget divider = hud_panel_widget.FindAnyWidget("NotifierDivider");
        if (divider)
        {
            divider.Show(false);
            divider.SetAlpha(0);
            Print("[Predate] Survival HUD separator hidden.");
        }
        for (int i = 1; i < HeatBufferMdfr.NUMBER_OF_STAGES; i++)
        {
            Widget heatBuffer = hud_panel_widget.FindAnyWidget("HeatBuffer" + i.ToString());
            if (heatBuffer) heatBuffer.Show(false);
        }
    }

    void PredateHideSurvivalNotifier(string name)
    {
        if (!m_Notifiers) return;
        Widget icon = m_Notifiers.FindAnyWidget("Icon" + name);
        if (icon) icon.Show(false);
        for (int i = 1; i <= 3; i++)
        {
            Widget up = m_Notifiers.FindAnyWidget(name + "ArrowUp" + i.ToString());
            Widget down = m_Notifiers.FindAnyWidget(name + "ArrowDown" + i.ToString());
            if (up) up.Show(false);
            if (down) down.Show(false);
        }
        if (icon && !icon.IsVisible())
            Print("[Predate] Survival HUD hidden: " + name);
        else
            Print("[Predate] HUD ERROR: could not hide " + name);
    }

    override void DisplayNotifier(int key, int tendency, int status)
    {
        // Ignore subsequent status updates so their arrows cannot reappear.
        if (key == NTFKEY_HUNGRY || key == NTFKEY_THIRSTY || key == NTFKEY_FEVERISH) return;
        super.DisplayNotifier(key, tendency, status);
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (key == NTFKEY_HEALTHY)
        {
            m_PredateHealthTendency = tendency;
            m_PredateHealthStatus = status;
            if (player) PredateSetVitalVisibility(key, player.m_PredateHealthHudVisible);
        }
        else if (key == NTFKEY_BLOOD)
        {
            m_PredateBloodTendency = tendency;
            m_PredateBloodStatus = status;
            if (player) PredateSetVitalVisibility(key, player.m_PredateBloodHudVisible);
        }
    }

    void PredateSetVitalVisibility(int key, bool visible)
    {
        if (!m_Notifiers) return;
        string name = "Health";
        if (key == NTFKEY_BLOOD) name = "Blood";
        Widget icon = m_Notifiers.FindAnyWidget("Icon" + name);
        if (icon) icon.Show(visible);
        if (visible) return;
        for (int i = 1; i <= 3; i++)
        {
            Widget up = m_Notifiers.FindAnyWidget(name + "ArrowUp" + i.ToString());
            Widget down = m_Notifiers.FindAnyWidget(name + "ArrowDown" + i.ToString());
            if (up) up.Show(false);
            if (down) down.Show(false);
        }
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player || !m_Notifiers) return;
        bool changed = player.m_PredateHealthHudVisible != m_PredateHealthShown || player.m_PredateBloodHudVisible != m_PredateBloodShown;
        if (player.m_PredateHealthHudVisible != m_PredateHealthShown)
        {
            m_PredateHealthShown = player.m_PredateHealthHudVisible;
            super.DisplayNotifier(NTFKEY_HEALTHY, m_PredateHealthTendency, m_PredateHealthStatus);
            PredateSetVitalVisibility(NTFKEY_HEALTHY, m_PredateHealthShown);
        }
        if (player.m_PredateBloodHudVisible != m_PredateBloodShown)
        {
            m_PredateBloodShown = player.m_PredateBloodHudVisible;
            super.DisplayNotifier(NTFKEY_BLOOD, m_PredateBloodTendency, m_PredateBloodStatus);
            PredateSetVitalVisibility(NTFKEY_BLOOD, m_PredateBloodShown);
        }
        if (changed)
            Print("[Predate] Context HUD: health=" + m_PredateHealthShown + "; blood=" + m_PredateBloodShown);
    }

    override void DisplayPresence() {}
    override void DisplayStance(int stance) {}

    override void SetPermanentCrossHair(bool show)
    {
        if (m_PermanentCrossHair) m_PermanentCrossHair.Show(false);
    }

    override void SetStaminaBarVisibility(bool show)
    {
        // The native HUD repeatedly requests visibility as stamina changes.
        if (m_Stamina) m_Stamina.Show(false);
        if (m_StaminaBackground) m_StaminaBackground.Show(false);
        m_IsStaminaVisible = true;
        m_StaminaTimer = 0;
    }

    override void SetTemperature(string temp) {}

    // Do not create the controller that animates and reveals heat-buffer stages.
    override void InitHeatBufferUI(Man player) {}
}

modded class CrossHairSelector
{
    override protected void ShowCrossHair(CrossHair crossHair)
    {
        super.ShowCrossHair(null);
    }
}

modded class IngameHudVisibility
{
    override protected void UpdateVisibility(EHudElement element, bool target, bool ignoreWidgetVisiblityState)
    {
        if (element == EHudElement.RHUD_DIVIDER || element == EHudElement.LHUD_PRESENCE || element == EHudElement.LHUD_STANCE) target = false;
        super.UpdateVisibility(element, target, ignoreWidgetVisiblityState);
    }
}
