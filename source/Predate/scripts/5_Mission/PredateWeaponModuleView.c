#ifndef SERVER
modded class MissionGameplay
{
    Widget m_PredateWeaponModuleView;
    ImageWidget m_PredateLaserDot;
    CanvasWidget m_PredateLaserBeam;
    protected bool m_PredateLaserImageLoaded;

    override void OnInit()
    {
        super.OnInit();
        m_PredateWeaponModuleView = GetGame().GetWorkspace().CreateWidgets("Predate/gui/weapon_module.layout");
        if (!m_PredateWeaponModuleView) return;
        m_PredateWeaponModuleView.Show(false);
        m_PredateLaserDot = ImageWidget.Cast(m_PredateWeaponModuleView.FindAnyWidget("LaserDot"));
        if (m_PredateLaserDot && m_PredateLaserDot.LoadImageFile(0, "Predate/gui/laser_dot_ca.paa"))
        {
            m_PredateLaserDot.SetImage(0);
            m_PredateLaserImageLoaded = true;
        }
        m_PredateLaserBeam = CanvasWidget.Cast(m_PredateWeaponModuleView.FindAnyWidget("LaserBeam"));
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!m_PredateWeaponModuleView) return;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        bool show = player && player.IsAlive() && !player.IsUnconscious() && !GetDayZGame().IsLoading() && !GetGame().GetUIManager().GetMenu();
        m_PredateWeaponModuleView.Show(show);
        if (!show) return;
        PredateUpdateLaserView(player, PredateWeaponModule.OnWeapon(player.GetItemInHands()));
    }

    protected void PredateUpdateLaserView(PlayerBase player, PredateWeaponModule module)
    {
        if (m_PredateLaserDot) m_PredateLaserDot.Show(false);
        if (m_PredateLaserBeam)
        {
            m_PredateLaserBeam.Clear();
            m_PredateLaserBeam.Show(false);
        }
        int color;
        if (!module || !module.LaserVisualColor(color)) return;
        float width, height;
        m_PredateWeaponModuleView.GetScreenSize(width, height);
        if (width <= 0 || height <= 0) return;

        float scale = Math.Clamp(height / 1080, 0.8, 2.0);
        vector mark;
        bool hit = module.LaserScreenMark(mark);
        if (hit && m_PredateLaserImageLoaded)
        {
            float pixels = scale * 16;
            m_PredateLaserDot.SetSize(pixels / width, pixels / height);
            m_PredateLaserDot.SetPos(mark[0] - pixels / (2 * width), mark[1] - pixels / (2 * height));
            m_PredateLaserDot.SetColor(color);
            m_PredateLaserDot.Show(true);
        }

        DayZPlayerCamera camera = player.GetCurrentPlayerCamera();
        // The optic and iron-sight views can keep the weapon model in the foreground.
        if (!m_PredateLaserBeam || !camera || camera.GetCameraName() != "DayZPlayerCamera1stPerson" || player.IsInOptics() || player.IsInIronsights()) return;

        // The first-person weapon model can place the rail outside the viewport.
        vector origin = "0.62 0.77 0";
        vector emitter, direction;
        if (module.LaserRay(emitter, direction))
        {
            vector projected = GetGame().GetScreenPosRelative(emitter);
            if (projected[2] > 0 && projected[0] > 0 && projected[0] < 1 && projected[1] > 0.53 && projected[1] < 1)
                origin = projected;
        }
        // A miss shows the beam near the rifle without placing a floating impact dot.
        if (!hit) mark = vector.Lerp(origin, "0.5 0.5 0", 0.8);
        vector middle = vector.Lerp(origin, mark, 0.55);
        int rgb = color & 0x00FFFFFF;
        m_PredateLaserBeam.DrawLine(origin[0] * width, origin[1] * height, middle[0] * width, middle[1] * height, 2 * scale, 0xB0000000 | rgb);
        m_PredateLaserBeam.DrawLine(middle[0] * width, middle[1] * height, mark[0] * width, mark[1] * height, scale, 0x70000000 | rgb);
        m_PredateLaserBeam.Show(true);
    }

    override void OnMissionFinish()
    {
        if (m_PredateWeaponModuleView) m_PredateWeaponModuleView.Unlink();
        m_PredateWeaponModuleView = null;
        m_PredateLaserDot = null;
        m_PredateLaserBeam = null;
        super.OnMissionFinish();
    }
}
#endif
