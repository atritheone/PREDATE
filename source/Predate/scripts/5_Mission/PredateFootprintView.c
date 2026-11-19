// A local view of a ground mark earned by the player. Projection has no
// access to the creator, live actors, the footprint ledger, or exact age.
class PredateFootprintView
{
    ImageWidget m_Image;

    void PredateFootprintView(Widget hud)
    {
        if (!hud) return;
        m_Image = ImageWidget.Cast(hud.FindAnyWidget("FootprintMark"));
        if (!m_Image) return;
        m_Image.LoadImageFile(0, "Predate/gui/bootprint_ca.paa");
        m_Image.SetImage(0);
    }

    bool Update(PlayerBase player, PredateState state, bool visible)
    {
        if (!m_Image) return false;
        m_Image.Show(false);
        if (!visible || !state || state.FootprintClue == 0) return false;
        vector point = state.FootprintGround + "0 0.04 0";
        vector center = GetGame().GetScreenPosRelative(point);
        if (!OnScreen(center)) return false;

        vector camera = GetGame().GetCurrentCameraPosition();
        if (vector.Distance(camera, point) > PredateFootprintLedger.VIEW_DISTANCE + 2) return false;
        vector contact, normal;
        int component;
        if (DayZPhysics.RaycastRV(camera, point, contact, normal, component, null, null, player, false, false, ObjIntersectView, 0)) return false;

        vector forward = PredateInformationMath.BearingDirection(state.FootprintHeading);
        vector sideways = Vector(forward[2], 0, -forward[0]);
        vector toe = Project(point + forward * 0.22);
        vector heel = Project(point - forward * 0.22);
        vector left = Project(point - sideways * 0.22);
        vector right = Project(point + sideways * 0.22);
        if (!OnScreen(toe) || !OnScreen(heel) || !OnScreen(left) || !OnScreen(right)) return false;

        float viewportWidth, viewportHeight;
        m_Image.GetParent().GetScreenSize(viewportWidth, viewportHeight);
        if (viewportWidth <= 0 || viewportHeight <= 0) return false;
        vector length = Vector((toe[0] - heel[0]) * viewportWidth, (toe[1] - heel[1]) * viewportHeight, 0);
        vector across = Vector((right[0] - left[0]) * viewportWidth, (right[1] - left[1]) * viewportHeight, 0);
        float width = across.Length() / viewportWidth;
        float height = length.Length() / viewportHeight;
        if (width < 0.001 || height < 0.001 || width > 0.25 || height > 0.25) return false;

        m_Image.SetPos(center[0] - width * 0.5, center[1] - height * 0.5);
        m_Image.SetSize(width, height);
        m_Image.SetRotation(0, 0, Math.Atan2(length[0], -length[1]) * Math.RAD2DEG);
        float opacity = 0.6;
        if (state.FootprintClue == 1) opacity = 0.35;
        m_Image.SetAlpha(opacity);
        m_Image.Show(true);
        return true;
    }

    static vector Project(vector point)
    {
        point[1] = GetGame().SurfaceY(point[0], point[2]) + 0.04;
        return GetGame().GetScreenPosRelative(point);
    }

    static bool OnScreen(vector point)
    {
        return point[2] > 0 && point[0] > 0 && point[0] < 1 && point[1] > 0 && point[1] < 1;
    }
}
