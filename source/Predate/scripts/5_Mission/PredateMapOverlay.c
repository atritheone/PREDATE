class PredateMapOverlay
{
    static const float BEARING_LABEL_OFFSET = 22;
    static const float BEARING_LABEL_WIDTH = 68;
    static const int DIAL_COLOR = 0xF04B4E52;

    MapWidget m_Map;
    CanvasWidget m_Canvas;
    Widget m_Projection;
    ref array<TextWidget> m_Labels = new array<TextWidget>;
    ref array<TextWidget> m_Bearings = new array<TextWidget>;
    ref array<vector> m_LabelPositions = new array<vector>;
    ref array<float> m_ArcRadii = new array<float>;
    float m_LabelHalfWidth;
    float m_LabelHeight;
    float m_Width;
    float m_Height;
    float m_Left;
    float m_Top;
    float m_Scale;
    vector m_PlayerPoint;
    vector m_NativeOrigin;
    float m_NativeScaleX;
    float m_NativeScaleY;

    void PredateMapOverlay(MapWidget terrain, CanvasWidget canvas)
    {
        m_Map = terrain;
        m_Canvas = canvas;
        m_Projection = terrain.FindAnyWidget("MapProjection");
        for (int index = 0; index < 24; index++)
        {
            TextWidget bearing = TextWidget.Cast(terrain.FindAnyWidget("MapBearing" + index));
            int degrees = index * PredateBearingPulse.DEGREES;
            string text = PredateMath.BearingDigits(degrees);
            if (degrees % 45 == 0) text = PredateMath.CompassPoint(degrees) + " " + text;
            if (bearing)
            {
                bearing.SetText(text);
                bearing.SetColor(DIAL_COLOR);
            }
            m_Bearings.Insert(bearing);
        }
        for (int slot = 0; slot < PredateProtocol.MAX_OPERATORS; slot++)
        {
            TextWidget label = TextWidget.Cast(terrain.FindAnyWidget("MapPulse" + slot));
            if (label) label.SetText(PredateProtocol.DisplayName(PredateProtocol.OperatorAt(slot)));
            m_Labels.Insert(label);
            m_LabelPositions.Insert(vector.Zero);
            m_ArcRadii.Insert(0);
        }
    }

    bool UpdateProjection()
    {
        if (!m_Projection) return false;
        m_Canvas.GetScreenSize(m_Width, m_Height);
        m_Canvas.GetScreenPos(m_Left, m_Top);
        // MapToScreen returns positions for exact-position children of the map.
        // Convert that layout space into the physical pixels used by the canvas.
        float originX, originY, unitX, unitY;
        m_Projection.SetPos(0, 0);
        m_Projection.GetScreenPos(originX, originY);
        m_Projection.SetPos(100, 100);
        m_Projection.GetScreenPos(unitX, unitY);
        m_NativeOrigin = Vector(originX - m_Left, originY - m_Top, 0);
        m_NativeScaleX = (unitX - originX) / 100;
        m_NativeScaleY = (unitY - originY) / 100;
        return m_NativeScaleX > 0 && m_NativeScaleY > 0;
    }

    vector LocalPoint(vector worldPoint)
    {
        worldPoint[1] = 0;
        vector nativePoint = m_Map.MapToScreen(worldPoint);
        return m_NativeOrigin + Vector(nativePoint[0] * m_NativeScaleX, nativePoint[1] * m_NativeScaleY, 0);
    }

    vector WorldPoint(vector localPoint)
    {
        vector relative = localPoint - m_NativeOrigin;
        return m_Map.ScreenToMap(Vector(relative[0] / m_NativeScaleX, relative[1] / m_NativeScaleY, 0));
    }

    vector ViewportCenter()
    {
        return Vector(m_Width / 2, m_Height / 2, 0);
    }

    vector RadialPoint(float bearing, float radius)
    {
        float angle = bearing * Math.DEG2RAD;
        return m_PlayerPoint + Vector(radius * Math.Sin(angle), -radius * Math.Cos(angle), 0);
    }

    void DrawArc(float start, float span, float radius, int segments, float width, int color)
    {
        vector last = RadialPoint(start, radius);
        for (int index = 1; index <= segments; index++)
        {
            vector point = RadialPoint(start + span * index / segments, radius);
            m_Canvas.DrawLine(last[0], last[1], point[0], point[1], width, color);
            last = point;
        }
    }

    void DrawDial(float radius)
    {
        DrawArc(0, 360, radius, 96, 3 * m_Scale, 0xEB26292C);
        DrawArc(0, 360, radius, 96, m_Scale, DIAL_COLOR);
        for (int index = 0; index < 24; index++)
        {
            int degrees = index * PredateBearingPulse.DEGREES;
            float tick = 5 * m_Scale;
            if (degrees % 45 == 0) tick = 9 * m_Scale;
            vector outside = RadialPoint(degrees, radius);
            vector inside = RadialPoint(degrees, radius - tick);
            m_Canvas.DrawLine(outside[0], outside[1], inside[0], inside[1], 2 * m_Scale, DIAL_COLOR);
            TextWidget label = m_Bearings[index];
            if (!label) continue;
            vector point = RadialPoint(degrees, radius + BEARING_LABEL_OFFSET * m_Scale);
            PlaceLabel(label, point, BEARING_LABEL_WIDTH * m_Scale, Math.Round(12 * m_Scale));
        }
    }

    void DrawPlayer(PlayerBase player, float scale)
    {
        vector position = player.GetPosition();
        vector facing = LocalPoint(position + player.GetDirection() * 100) - m_PlayerPoint;
        if (facing.LengthSq() < 0.01) facing = "0 -1 0";
        facing.Normalize();
        vector right = Vector(-facing[1], facing[0], 0);
        vector nose = m_PlayerPoint + facing * (10 * scale);
        vector rear = m_PlayerPoint - facing * (6 * scale);
        vector left = rear - right * (6 * scale);
        right = rear + right * (6 * scale);
        DrawTriangle(nose, left, right, 5 * scale, 0xFF10242D);
        DrawTriangle(nose, left, right, 2 * scale, 0xFF69EFFC);
    }

    void DrawTriangle(vector a, vector b, vector c, float width, int color)
    {
        m_Canvas.DrawLine(a[0], a[1], b[0], b[1], width, color);
        m_Canvas.DrawLine(b[0], b[1], c[0], c[1], width, color);
        m_Canvas.DrawLine(c[0], c[1], a[0], a[1], width, color);
    }

    void PlaceLabel(TextWidget label, vector center, float width, int textSize)
    {
        label.SetScreenSize(width, m_LabelHeight);
        label.SetScreenPos(m_Left + center[0] - width / 2, m_Top + center[1] - m_LabelHeight / 2);
        label.SetTextExactSize(textSize);
        label.Show(true);
    }

    void ShowPulseLabel(int slot, vector center, int color)
    {
        TextWidget label = m_Labels[slot];
        if (!label) return;
        if (center[0] < 0 || center[0] > m_Width || center[1] < 0 || center[1] > m_Height) return;
        center[0] = Math.Clamp(center[0], m_LabelHalfWidth, m_Width - m_LabelHalfWidth);
        center[1] = Math.Clamp(center[1], m_LabelHeight / 2, m_Height - m_LabelHeight / 2);
        vector anchor = center;
        for (int offset = 0; offset < 8; offset++)
        {
            center[1] = anchor[1] + Math.Ceil(offset / 2.0) * m_LabelHeight;
            if (offset % 2 == 1) center[1] = anchor[1] - Math.Ceil(offset / 2.0) * m_LabelHeight;
            if (center[1] < m_LabelHeight / 2 || center[1] > m_Height - m_LabelHeight / 2) continue;
            bool overlaps = false;
            for (int index = 0; index < m_Labels.Count(); index++)
            {
                if (!m_Labels[index] || !m_Labels[index].IsVisible()) continue;
                vector other = m_LabelPositions[index];
                if (Math.AbsFloat(center[0] - other[0]) < m_LabelHalfWidth * 1.6 && Math.AbsFloat(center[1] - other[1]) < m_LabelHeight)
                {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) continue;
            m_LabelPositions[slot] = center;
            label.SetColor(color);
            PlaceLabel(label, center, m_LabelHalfWidth * 2, Math.Round(14 * m_Scale));
            return;
        }
    }

    string HitPulse(float x, float y, PredateState state)
    {
        if (!state) return "";
        vector click = Vector(x - m_Left, y - m_Top, 0);
        vector offset = click - m_PlayerPoint;
        float distance = offset.Length();
        float bearing = PredateMath.WrapHeading(Math.Atan2(offset[0], -offset[1]) * Math.RAD2DEG);
        string closest;
        string labelMatch;
        float closestGap = 8 * m_Scale;
        foreach (PredateBearingPulse pulse : state.Pulses)
        {
            int slot = PredateProtocol.s_OperatorIds.Find(pulse.OperatorId);
            TextWidget label = m_Labels[slot];
            vector center = m_LabelPositions[slot];
            if (label && label.IsVisible() && Math.AbsFloat(click[0] - center[0]) <= m_LabelHalfWidth * 0.65 && Math.AbsFloat(click[1] - center[1]) <= m_LabelHeight / 2)
                labelMatch = pulse.OperatorId;
            float gap = Math.AbsFloat(distance - m_ArcRadii[slot]);
            if (m_ArcRadii[slot] > 0 && gap < closestGap && bearing >= pulse.Start && bearing < pulse.Start + PredateBearingPulse.DEGREES)
            {
                closestGap = gap;
                closest = pulse.OperatorId;
            }
        }
        if (closest != "" && closestGap < 3 * m_Scale) return closest;
        if (labelMatch != "") return labelMatch;
        return closest;
    }

    void Draw(PlayerBase player, PredateState state, float scale)
    {
        m_Canvas.Clear();
        foreach (TextWidget label : m_Labels) if (label) label.Show(false);
        foreach (TextWidget bearing : m_Bearings) if (bearing) bearing.Show(false);
        for (int slot = 0; slot < m_ArcRadii.Count(); slot++) m_ArcRadii[slot] = 0;
        if (!UpdateProjection()) return;
        m_Scale = scale;
        m_PlayerPoint = LocalPoint(player.GetPosition());
        m_LabelHalfWidth = 50 * scale;
        m_LabelHeight = 25 * scale;
        float radius = Math.Min(m_Width, m_Height) * 0.72;
        float horizontalRoom = Math.Min(m_PlayerPoint[0], m_Width - m_PlayerPoint[0]);
        float verticalRoom = Math.Min(m_PlayerPoint[1], m_Height - m_PlayerPoint[1]);
        horizontalRoom -= (BEARING_LABEL_OFFSET + BEARING_LABEL_WIDTH / 2) * scale;
        verticalRoom -= BEARING_LABEL_OFFSET * scale + m_LabelHeight / 2;
        float room = Math.Max(60 * scale, Math.Min(horizontalRoom, verticalRoom));
        radius = Math.Min(radius, room);
        DrawDial(radius);
        if (state)
        {
            array<int> lanes = {};
            for (int lane = 0; lane < 24; lane++) lanes.Insert(0);
            foreach (PredateBearingPulse pulse : state.Pulses)
            {
                int sector = pulse.Start / PredateBearingPulse.DEGREES;
                int operatorSlot = PredateProtocol.s_OperatorIds.Find(pulse.OperatorId);
                float arcRadius = radius - (8 + lanes[sector] * 29) * scale;
                lanes[sector] = lanes[sector] + 1;
                if (arcRadius < 20 * scale) continue;
                m_ArcRadii[operatorSlot] = arcRadius;
                int color = 0xFFFFB853;
                float width = 4 * scale;
                if (state.Locked && pulse.OperatorId == state.OperatorId)
                {
                    color = 0xFFFFF2B5;
                    width = 6 * scale;
                }
                DrawArc(pulse.Start, PredateBearingPulse.DEGREES, arcRadius, 8, width + 3 * scale, 0xE52C1B0C);
                DrawArc(pulse.Start, PredateBearingPulse.DEGREES, arcRadius, 8, width, color);
                ShowPulseLabel(operatorSlot, RadialPoint(pulse.Start + PredateBearingPulse.DEGREES / 2.0, arcRadius - 14 * scale), color);
            }
        }
        DrawPlayer(player, scale);
    }
}
