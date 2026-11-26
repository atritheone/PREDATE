class PredateMapViewport
{
    MapWidget m_Terrain;
    Widget m_Pane;
    float m_WorldSize;
    float m_Scale = 0.22;
    float m_PixelsPerMetre;
    vector m_Center;
    vector m_Origin;
    vector m_Size;
    float m_ScreenHeight;
    bool m_Dirty = true;

    void PredateMapViewport(MapWidget terrain, Widget pane)
    {
        m_Terrain = terrain;
        m_Pane = pane;
        m_WorldSize = GetGame().GetWorld().GetWorldSize();
    }

    void SetCenter(vector center)
    {
        center[0] = Math.Clamp(center[0], 0, m_WorldSize);
        center[1] = 0;
        center[2] = Math.Clamp(center[2], 0, m_WorldSize);
        if (center == m_Center) return;
        m_Center = center;
        m_Dirty = true;
    }

    void SetScale(float scale)
    {
        scale = Math.Clamp(scale, 0.02, 1);
        if (scale == m_Scale) return;
        m_Scale = scale;
        m_Dirty = true;
    }

    vector ToScreen(vector world)
    {
        return m_Origin + m_Size * 0.5 + Vector(world[0] - m_Center[0], m_Center[2] - world[2], 0) * m_PixelsPerMetre;
    }

    vector ToWorld(vector screen)
    {
        if (m_PixelsPerMetre <= 0) return m_Center;
        vector offset = (screen - m_Origin - m_Size * 0.5) * (1 / m_PixelsPerMetre);
        return m_Center + Vector(offset[0], 0, -offset[1]);
    }

    void Update(float screenHeight)
    {
        float left, top, width, height;
        m_Pane.GetScreenPos(left, top);
        m_Pane.GetScreenSize(width, height);
        vector origin = Vector(left, top, 0);
        vector size = Vector(width, height, 0);
        if (!m_Dirty && origin == m_Origin && size == m_Size && screenHeight == m_ScreenHeight) return;
        if (width <= 0 || height <= 0 || screenHeight <= 0 || m_WorldSize <= 0) return;
        m_Origin = origin;
        m_Size = size;
        m_ScreenHeight = screenHeight;
        m_Dirty = false;

        m_Terrain.SetScale(m_Scale);
        vector zero = m_Terrain.MapToScreen(vector.Zero);
        vector east = m_Terrain.MapToScreen(Vector(m_WorldSize, 0, 0));
        m_PixelsPerMetre = (east[0] - zero[0]) / m_WorldSize;
        if (m_PixelsPerMetre <= 0)
        {
            m_Dirty = true;
            return;
        }

        // DayZ projects into a virtual viewport and clamps its camera to the
        // terrain. Translate the terrain widget by the clamped remainder.
        float nativeWidth = m_PixelsPerMetre * m_WorldSize * m_Terrain.GetScale();
        vector nativeHalf = Vector(nativeWidth, screenHeight, 0) * 0.5;
        vector difference = (nativeHalf - size * 0.5) * (1 / m_PixelsPerMetre);
        m_Terrain.SetMapPos(m_Center + Vector(difference[0], 0, -difference[1]));
        vector nativeOrigin = m_Terrain.MapToScreen(m_Terrain.GetMapPos()) - nativeHalf;
        vector terrainOrigin = ToScreen(m_Center) - (m_Terrain.MapToScreen(m_Center) - nativeOrigin);

        // Let the pane show through wherever the projected terrain ends.
        vector northWest = ToScreen(Vector(0, 0, m_WorldSize));
        vector southEast = ToScreen(Vector(m_WorldSize, 0, 0));
        vector end = m_Origin + m_Size;
        float drawWidth = Math.Min(end[0], southEast[0]) - terrainOrigin[0];
        float drawHeight = Math.Min(end[1], southEast[1]) - terrainOrigin[1];
        bool visible = drawWidth > 0 && drawHeight > 0 && northWest[0] < end[0] && northWest[1] < end[1] && southEast[0] > left && southEast[1] > top;
        if (visible)
        {
            m_Terrain.SetScreenPos(terrainOrigin[0], terrainOrigin[1], false);
            m_Terrain.SetScreenSize(drawWidth, drawHeight, false);
            m_Terrain.Update();
        }
        m_Terrain.Show(visible);
    }
}
