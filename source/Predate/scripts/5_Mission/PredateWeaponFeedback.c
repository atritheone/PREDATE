modded class IngameHud
{
    Widget m_PredateWeaponFeedback;
    TextWidget m_PredateModeFeedback;
    TextWidget m_PredateZeroingFeedback;
    TextWidget m_PredateMagazineFeedback;
    Weapon_Base m_PredateFeedbackWeapon;
    int m_PredateFeedbackMuzzle;
    int m_PredateFeedbackMode;
    float m_PredateFeedbackZeroing;
    int m_PredateFeedbackWarmup;
    int m_PredateModeUntil;
    int m_PredateZeroingUntil;
    PlayerBase m_PredateFeedbackPlayer;
    int m_PredateShownModuleMessage;
    float m_PredateModeAlpha;
    float m_PredateZeroingAlpha;
    float m_PredateMagazineAlpha;

    override void Init(Widget hud_panel_widget)
    {
        super.Init(hud_panel_widget);
        m_PredateWeaponFeedback = GetGame().GetWorkspace().CreateWidgets("Predate/gui/weapon_feedback.layout", hud_panel_widget);
        if (!m_PredateWeaponFeedback) return;
        m_PredateModeFeedback = TextWidget.Cast(m_PredateWeaponFeedback.FindAnyWidget("PredateModeFeedback"));
        m_PredateZeroingFeedback = TextWidget.Cast(m_PredateWeaponFeedback.FindAnyWidget("PredateZeroingFeedback"));
        m_PredateMagazineFeedback = TextWidget.Cast(m_PredateWeaponFeedback.FindAnyWidget("PredateMagazineFeedback"));
        m_PredateWeaponFeedback.Show(false);
        array<string> obsolete = {"WeaponStats", "WeaponMode", "Zeroing"};
        foreach (string name : obsolete)
        {
            Widget oldReadout = hud_panel_widget.FindAnyWidget(name);
            if (oldReadout) { oldReadout.Show(false); oldReadout.SetAlpha(0); }
        }
        Print("[Predate] Transient weapon feedback initialized; permanent weapon/ammo readout hidden.");
    }

    float PredateFeedbackFade(float alpha, bool active, float dt)
    {
        if (active) return Math.Min(1, alpha + dt / 0.08);
        return Math.Max(0, alpha - dt / 0.3);
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        if (!m_PredateWeaponFeedback || !m_PredateModeFeedback || !m_PredateZeroingFeedback || !m_PredateMagazineFeedback) return;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player != m_PredateFeedbackPlayer)
        {
            m_PredateFeedbackPlayer = player;
            m_PredateShownModuleMessage = 0;
        }
        Weapon_Base weapon;
        if (player) weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
        if (!player || !weapon)
        {
            m_PredateFeedbackWeapon = null;
            m_PredateWeaponFeedback.Show(false);
            return;
        }
        int now = GetGame().GetTime();
        int muzzle = weapon.GetCurrentMuzzle();
        int mode = weapon.GetCurrentMode(muzzle);
        float zeroing = weapon.GetCurrentZeroing(muzzle);
        if (weapon != m_PredateFeedbackWeapon)
        {
            m_PredateFeedbackWeapon = weapon;
            m_PredateFeedbackWarmup = now + 1000;
            m_PredateModeUntil = 0;
            m_PredateZeroingUntil = 0;
            m_PredateModeAlpha = 0;
            m_PredateZeroingAlpha = 0;
            m_PredateMagazineAlpha = 0;
        }
        else if (now > m_PredateFeedbackWarmup)
        {
            if (mode != m_PredateFeedbackMode || muzzle != m_PredateFeedbackMuzzle)
            {
                string modeName = Widget.TranslateString(weapon.GetCurrentModeName(muzzle));
                m_PredateModeFeedback.SetText("Fire Mode  " + modeName);
                m_PredateModeUntil = now + 1100;
            }
            if (Math.AbsFloat(zeroing - m_PredateFeedbackZeroing) > 0.1 || muzzle != m_PredateFeedbackMuzzle)
            {
                m_PredateZeroingFeedback.SetText("Zeroing  " + Math.Round(zeroing).ToString() + " m");
                m_PredateZeroingUntil = now + 1100;
            }
        }
        m_PredateFeedbackMode = mode;
        m_PredateFeedbackMuzzle = muzzle;
        m_PredateFeedbackZeroing = zeroing;
        if (player.m_PredateModuleMessageRevision != m_PredateShownModuleMessage)
        {
            m_PredateShownModuleMessage = player.m_PredateModuleMessageRevision;
            int expires = player.m_PredateModuleMessageTime + 3000;
            if (now < expires)
            {
                m_PredateModeFeedback.SetText(player.m_PredateModuleMessage);
                m_PredateModeUntil = expires;
            }
        }
        m_PredateModeAlpha = PredateFeedbackFade(m_PredateModeAlpha, now < m_PredateModeUntil, timeslice);
        m_PredateZeroingAlpha = PredateFeedbackFade(m_PredateZeroingAlpha, now < m_PredateZeroingUntil, timeslice);
        bool magazineMessage = weapon == player.m_PredateMagazineMessageWeapon && now < player.m_PredateMagazineMessageUntil;
        m_PredateMagazineAlpha = PredateFeedbackFade(m_PredateMagazineAlpha, magazineMessage, timeslice);
        m_PredateModeFeedback.SetAlpha(m_PredateModeAlpha);
        m_PredateZeroingFeedback.SetAlpha(m_PredateZeroingAlpha);
        m_PredateMagazineFeedback.SetAlpha(m_PredateMagazineAlpha);
        m_PredateMagazineFeedback.SetText(player.m_PredateMagazineMessage);
        bool permitted = !m_HudVisibility.IsContextFlagActive(IngameHudVisibility.HUD_HIDE_FLAGS);
        m_PredateWeaponFeedback.Show(permitted);
    }
}
