class PredateMapOverlay
{
    static const float BEARING_LABEL_OFFSET = 22;
    static const float BEARING_LABEL_WIDTH = 68;
    static const int DIAL_COLOR = 0xF04B4E52;
    static const float INNER_CLEARANCE = 76;
    static const float MIN_LANE_SPACING = 18;
    static const float PREFERRED_LANE_SPACING = 29;
    static const int SELECTED_COLOR = 0xFFFFFFFF;
    static const int LABEL_BACKGROUND = 0xFF14222C;
    static const int LABEL_HIGHLIGHT = 0xFF284758;

    MapWidget m_Map;
    CanvasWidget m_Canvas;
    ref array<TextWidget> m_Labels = new array<TextWidget>;
    ref array<TextWidget> m_Bearings = new array<TextWidget>;
    ref array<vector> m_LabelPositions = new array<vector>;
    ref array<vector> m_LabelAnchors = new array<vector>;
    ref array<float> m_LabelHalfWidths = new array<float>;
    ref array<bool> m_LabelLeaders = new array<bool>;
    ref array<float> m_ArcRadii = new array<float>;
    ref array<int> m_ArcStarts = new array<int>;
    ref array<vector> m_ArcCenters = new array<vector>;
    ref array<int> m_ActiveSlots = new array<int>;
    ref array<int> m_NextSlots = new array<int>;
    ref array<int> m_PlacedSlots = new array<int>;
    ref array<string> m_HoverHits = new array<string>;
    PredateBearingPulse m_HoveredPulse;
    float m_LabelScale;
    float m_LabelFit = 1;
    float m_LabelHeight;
    float m_Width;
    float m_Height;
    float m_Left;
    float m_Top;
    float m_Scale;
    vector m_PlayerPoint;
    int m_LabelLockSlot = -1;

    void PredateMapOverlay(MapWidget terrain, CanvasWidget canvas)
    {
        m_Map = terrain;
        m_Canvas = canvas;
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
            m_LabelAnchors.Insert(vector.Zero);
            m_LabelHalfWidths.Insert(0);
            m_LabelLeaders.Insert(false);
            m_ArcRadii.Insert(0);
            m_ArcStarts.Insert(0);
            m_ArcCenters.Insert(vector.Zero);
        }
    }

    bool UpdateProjection()
    {
        m_Canvas.GetScreenSize(m_Width, m_Height);
        m_Canvas.GetScreenPos(m_Left, m_Top);
        return m_Width > 0 && m_Height > 0;
    }

    vector LocalPoint(vector worldPoint)
    {
        worldPoint[1] = 0;
        vector screen = m_Map.MapToScreen(worldPoint);
        // The native map gives absolute screen pixels. The canvas is local to the map pane.
        return Vector(screen[0] - m_Left, screen[1] - m_Top, 0);
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

    void DrawPulse(PredateBearingPulse pulse, float radius, bool selected, bool hovered = false)
    {
        int distanceColor = pulse.Color();
        float width = 4 * m_Scale;
        int outline = 0xE52C1B0C;
        int fill = distanceColor;
        if (selected)
        {
            width = 6 * m_Scale;
            outline = distanceColor;
            fill = SELECTED_COLOR;
            DrawArc(pulse.Start, PredateBearingPulse.DEGREES, radius, 8, width + 6 * m_Scale, 0xE52C1B0C);
        }
        else if (hovered)
        {
            width = 6 * m_Scale;
            outline = 0xFFF5FAFF;
            DrawArc(pulse.Start, PredateBearingPulse.DEGREES, radius, 8, width + 6 * m_Scale, 0xFF14222C);
        }
        DrawArc(pulse.Start, PredateBearingPulse.DEGREES, radius, 8, width + 4 * m_Scale, outline);
        DrawArc(pulse.Start, PredateBearingPulse.DEGREES, radius, 8, width, fill);
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

    vector HistoryPoint(PredateReceiverSample sample, float bearing, float metres)
    {
        return LocalPoint(sample.Origin + PredateInformationMath.BearingDirection(bearing) * metres);
    }

    void DrawHistory(PredateState state)
    {
        if (!state || !state.CanShowHistory()) return;
        for (int index = state.History.Count() - 1; index >= 0; index--)
        {
            PredateReceiverSample sample = state.History[index];
            if (!sample || sample.DistanceBand <= 0) continue;
            float outer = sample.DistanceBand * PredateBearingPulse.BAND_METRES;
            float inner = outer - PredateBearingPulse.BAND_METRES;
            int alpha = Math.Round(Math.Clamp(215 - sample.Age * 0.5, 70, 215));
            int edge = ARGB(alpha, 100, 226, 255);
            int guide = ARGB(alpha / 2, 100, 226, 255);
            vector origin = HistoryPoint(sample, sample.Bearing, 0);
            vector nearPoint = HistoryPoint(sample, sample.Bearing, inner);
            vector farPoint = HistoryPoint(sample, sample.Bearing, outer);
            m_Canvas.DrawLine(origin[0], origin[1], nearPoint[0], nearPoint[1], m_Scale, guide);
            m_Canvas.DrawLine(nearPoint[0], nearPoint[1], farPoint[0], farPoint[1], 2 * m_Scale, edge);
            for (int step = 1; step <= 8; step++)
            {
                float bearing = sample.Bearing + PredateBearingPulse.DEGREES * step / 8.0;
                vector nextNear = HistoryPoint(sample, bearing, inner);
                vector nextFar = HistoryPoint(sample, bearing, outer);
                m_Canvas.DrawLine(nearPoint[0], nearPoint[1], nextNear[0], nextNear[1], 2 * m_Scale, edge);
                m_Canvas.DrawLine(farPoint[0], farPoint[1], nextFar[0], nextFar[1], 2 * m_Scale, edge);
                nearPoint = nextNear;
                farPoint = nextFar;
            }
            m_Canvas.DrawLine(origin[0], origin[1], nearPoint[0], nearPoint[1], m_Scale, guide);
            m_Canvas.DrawLine(nearPoint[0], nearPoint[1], farPoint[0], farPoint[1], 2 * m_Scale, edge);
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

    void PlaceLabel(TextWidget label, vector center, float width, int textSize, float height = 0)
    {
        if (height <= 0) height = 25 * m_Scale;
        label.SetScreenSize(width, height);
        label.SetScreenPos(m_Left + center[0] - width / 2, m_Top + center[1] - height / 2);
        label.SetTextExactSize(textSize);
        label.Show(true);
    }

    void SizePulseLabels(float scale)
    {
        if (scale == m_LabelScale) return;
        m_LabelScale = scale;
        for (int slot = 0; slot < m_Labels.Count(); slot++)
        {
            TextWidget label = m_Labels[slot];
            if (!label) continue;
            label.SetTextExactSize(Math.Round(13 * scale));
            int width, height;
            label.GetTextSize(width, height);
            m_LabelHalfWidths[slot] = Math.Max(14 * scale, width * 0.5 + 5 * scale);
        }
    }

    // Screen-space line clipping, used to keep nameplates and their leaders
    // clear of the strokes that would otherwise look like an attachment.
    static bool LineInBox(vector first, vector last, vector center, float halfWidth, float halfHeight)
    {
        vector start = first - center;
        vector delta = last - first;
        vector extent = Vector(halfWidth, halfHeight, 0);
        float enter;
        float leave = 1;
        for (int axis = 0; axis < 2; axis++)
        {
            if (Math.AbsFloat(delta[axis]) < 0.01)
            {
                if (Math.AbsFloat(start[axis]) > extent[axis]) return false;
                continue;
            }
            float low = (-extent[axis] - start[axis]) / delta[axis];
            float high = (extent[axis] - start[axis]) / delta[axis];
            enter = Math.Max(enter, Math.Min(low, high));
            leave = Math.Min(leave, Math.Max(low, high));
            if (enter > leave) return false;
        }
        return true;
    }

    static vector LabelAxes(vector point, vector axis)
    {
        return Vector(point[0] * axis[0] + point[1] * axis[1], point[1] * axis[0] - point[0] * axis[1], 0);
    }

    bool ArcInBox(int slot, vector center, float halfWidth, float halfHeight, vector axis = "1 0 0")
    {
        float radius = m_ArcRadii[slot];
        float reach = radius * 0.132;
        vector offset = LabelAxes(m_ArcCenters[slot] - center, axis);
        if (Math.AbsFloat(offset[0]) > halfWidth + reach || Math.AbsFloat(offset[1]) > halfHeight + reach) return false;
        int start = m_ArcStarts[slot];
        vector first = LabelAxes(RadialPoint(start, radius) - center, axis);
        for (int segment = 1; segment <= 3; segment++)
        {
            vector last = LabelAxes(RadialPoint(start + segment * 5, radius) - center, axis);
            if (LineInBox(first, last, vector.Zero, halfWidth, halfHeight)) return true;
            first = last;
        }
        return false;
    }

    vector LabelEdge(int slot, vector center)
    {
        vector anchor = m_LabelAnchors[slot];
        float halfWidth = LabelHalfWidth(slot);
        float halfHeight = m_LabelHeight / 2;
        float x = Math.Clamp(anchor[0], center[0] - halfWidth, center[0] + halfWidth);
        float y = Math.Clamp(anchor[1], center[1] - halfHeight, center[1] + halfHeight);
        return Vector(x, y, 0);
    }

    bool LabelClear(int slot, vector center)
    {
        float halfWidth = LabelHalfWidth(slot);
        float halfHeight = m_LabelHeight / 2;
        if (center[0] < halfWidth || center[0] + halfWidth > m_Width || center[1] < halfHeight || center[1] + halfHeight > m_Height) return false;
        vector fromPlayer = center - m_PlayerPoint;
        if (Math.AbsFloat(fromPlayer[0]) < halfWidth + 14 * m_Scale && Math.AbsFloat(fromPlayer[1]) < halfHeight + 16 * m_Scale) return false;
        vector first = m_LabelAnchors[slot];
        vector edge = LabelEdge(slot, center);
        float gap = 3 * m_Scale;
        foreach (int placed : m_PlacedSlots)
        {
            vector other = m_LabelPositions[placed];
            float otherWidth = LabelHalfWidth(placed);
            if (Math.AbsFloat(center[0] - other[0]) < halfWidth + otherWidth + gap && Math.AbsFloat(center[1] - other[1]) < m_LabelHeight + gap) return false;
            if (LineInBox(first, edge, other, otherWidth + gap, halfHeight + gap)) return false;
            if (m_LabelLeaders[placed] && LineInBox(m_LabelAnchors[placed], LabelEdge(placed, other), center, halfWidth + gap, halfHeight + gap)) return false;
        }
        vector leaderCenter = (first + edge) * 0.5;
        vector leaderSpan = edge - first;
        float leaderLength = leaderSpan.Length();
        vector leaderAxis = leaderSpan.Normalized();
        float stroke = 6 * m_Scale;
        foreach (int shown : m_ActiveSlots)
        {
            if (ArcInBox(shown, center, halfWidth + stroke, halfHeight + stroke)) return false;
            if (shown != slot && ArcInBox(shown, leaderCenter, leaderLength * 0.5 + stroke, stroke, leaderAxis)) return false;
        }
        return true;
    }

    void PlacePulseLabel(int slot, array<float> directions)
    {
        if (!m_Labels[slot]) return;
        // Identical strokes can only be separated in the picker.
        foreach (int other : m_ActiveSlots)
            if (other != slot && m_ArcRadii[other] == m_ArcRadii[slot] && m_ArcStarts[other] == m_ArcStarts[slot]) return;
        float middle = m_ArcStarts[slot] + PredateBearingPulse.DEGREES / 2.0;
        array<float> attachmentOffsets = {0, -5, 5};
        for (int reach = 15; reach <= 51; reach += 12)
        {
            foreach (float attachment : attachmentOffsets)
            {
                vector point = RadialPoint(middle + attachment, m_ArcRadii[slot]);
                if (point[0] < 0 || point[0] > m_Width || point[1] < 0 || point[1] > m_Height) continue;
                m_LabelAnchors[slot] = point;
                foreach (float angle : directions)
                {
                    vector direction = RadialPoint(middle + angle, 1) - m_PlayerPoint;
                    float horizontal = LabelHalfWidth(slot) / Math.Max(0.01, Math.AbsFloat(direction[0]));
                    float vertical = m_LabelHeight / 2 / Math.Max(0.01, Math.AbsFloat(direction[1]));
                    vector center = point + direction * (Math.Min(horizontal, vertical) + reach * m_Scale);
                    if (!LabelClear(slot, center)) continue;
                    ShowPulseName(slot, center, true);
                    return;
                }
            }
        }
    }

    void ArrangePulseLabels(int selectedSlot)
    {
        foreach (TextWidget label : m_Labels) if (label) label.Show(false);
        m_PlacedSlots.Clear();
        m_LabelFit = 1;
        m_LabelHeight = 20 * m_Scale;
        array<float> directions = {180, 0, -90, 90, -135, 135, -45, 45, -112.5, 112.5, -67.5, 67.5};
        if (selectedSlot >= 0) PlacePulseLabel(selectedSlot, directions);
        for (int near = m_ActiveSlots.Count() - 1; near >= 0; near--)
            if (m_ActiveSlots[near] != selectedSlot) PlacePulseLabel(m_ActiveSlots[near], directions);
        PlaceRemainingNames();
    }

    float LabelHalfWidth(int slot)
    {
        return m_LabelHalfWidths[slot] * m_LabelFit;
    }

    void ShowPulseName(int slot, vector center, bool showLeader)
    {
        m_LabelPositions[slot] = center;
        m_LabelLeaders[slot] = showLeader;
        m_PlacedSlots.Insert(slot);
        m_Labels[slot].SetColor(SELECTED_COLOR);
        int textSize = Math.Max(1, Math.Floor(13 * m_Scale * m_LabelFit));
        PlaceLabel(m_Labels[slot], center, LabelHalfWidth(slot) * 2, textSize, m_LabelHeight);
    }

    void CollectLabelCells(array<vector> cells, float width, bool keepArcsClear, bool keepArrowClear = true)
    {
        cells.Clear();
        float gap = 3 * m_Scale * m_LabelFit;
        float column = width + gap;
        float row = m_LabelHeight + gap;
        float halfWidth = width * 0.5;
        float halfHeight = m_LabelHeight * 0.5;
        int columns = Math.Floor(m_Width / column);
        int rows = Math.Floor(m_Height / row);
        for (int across = 0; across < columns; across++)
        {
            for (int down = 0; down < rows; down++)
            {
                vector center = Vector((across + 0.5) * column, (down + 0.5) * row, 0);
                vector fromPlayer = center - m_PlayerPoint;
                if (keepArrowClear && Math.AbsFloat(fromPlayer[0]) < halfWidth + 16 * m_Scale && Math.AbsFloat(fromPlayer[1]) < halfHeight + 16 * m_Scale) continue;
                bool clear = true;
                foreach (int placed : m_PlacedSlots)
                {
                    vector offset = center - m_LabelPositions[placed];
                    if (Math.AbsFloat(offset[0]) >= halfWidth + LabelHalfWidth(placed) + gap || Math.AbsFloat(offset[1]) >= m_LabelHeight + gap) continue;
                    clear = false;
                    break;
                }
                if (!clear) continue;
                if (keepArcsClear)
                {
                    foreach (int active : m_ActiveSlots)
                    {
                        if (!ArcInBox(active, center, halfWidth + gap, halfHeight + gap)) continue;
                        clear = false;
                        break;
                    }
                }
                if (clear) cells.Insert(center);
            }
        }
    }

    void PlaceRemainingNames()
    {
        array<int> waiting = {};
        float width;
        foreach (int slot : m_ActiveSlots)
        {
            if (!m_Labels[slot]) continue;
            width = Math.Max(width, LabelHalfWidth(slot) * 2);
            if (m_PlacedSlots.Find(slot) < 0) waiting.Insert(slot);
        }
        if (waiting.Count() == 0) return;
        array<vector> cells = {};
        CollectLabelCells(cells, width, true);
        if (cells.Count() < waiting.Count()) CollectLabelCells(cells, width, false);
        if (cells.Count() < waiting.Count())
        {
            // On smaller viewports reflow the whole map so no name is lost.
            foreach (int previous : m_PlacedSlots) m_Labels[previous].Show(false);
            m_PlacedSlots.Clear();
            waiting.Copy(m_ActiveSlots);
            float desired = waiting.Count() + 8;
            int rows = Math.Max(1, Math.Ceil(Math.Sqrt(desired * m_Height * width / (m_Width * m_LabelHeight))));
            int columns = Math.Ceil(desired / rows);
            float fitWidth = m_Width / (columns * (width + 3 * m_Scale));
            float fitHeight = m_Height / (rows * (m_LabelHeight + 3 * m_Scale));
            m_LabelFit = Math.Min(1, Math.Min(fitWidth, fitHeight) * 0.99);
            m_LabelHeight *= m_LabelFit;
            width *= m_LabelFit;
            CollectLabelCells(cells, width, false);
            if (cells.Count() < waiting.Count()) CollectLabelCells(cells, width, false, false);
        }
        for (int index = waiting.Count() - 1; index >= 0; index--)
        {
            int pending = waiting[index];
            if (!m_Labels[pending] || cells.Count() == 0) continue;
            vector anchor = m_ArcCenters[pending];
            m_LabelAnchors[pending] = anchor;
            int nearest;
            float distance = vector.DistanceSq(cells[0], anchor);
            for (int cell = 1; cell < cells.Count(); cell++)
            {
                float nextDistance = vector.DistanceSq(cells[cell], anchor);
                if (nextDistance >= distance) continue;
                nearest = cell;
                distance = nextDistance;
            }
            vector center = cells[nearest];
            cells.RemoveOrdered(nearest);
            float leaderLimit = 65 * m_Scale;
            bool shortLeader = distance < leaderLimit * leaderLimit && LabelClear(pending, center);
            ShowPulseName(pending, center, shortLeader);
        }
    }

    void DrawPulseLabels(int activeSlot)
    {
        foreach (int slot : m_PlacedSlots)
        {
            vector center = m_LabelPositions[slot];
            vector anchor = m_LabelAnchors[slot];
            vector edge = LabelEdge(slot, center);
            int fill = LABEL_BACKGROUND;
            int leader = 0xFFC9D7E0;
            if (slot == activeSlot)
            {
                fill = LABEL_HIGHLIGHT;
                leader = SELECTED_COLOR;
            }
            if (m_LabelLeaders[slot] || slot == activeSlot)
            {
                m_Canvas.DrawLine(anchor[0], anchor[1], edge[0], edge[1], 4 * m_Scale, LABEL_BACKGROUND);
                m_Canvas.DrawLine(anchor[0], anchor[1], edge[0], edge[1], 1.4 * m_Scale, leader);
            }
            float halfWidth = LabelHalfWidth(slot);
            m_Canvas.DrawLine(center[0] - halfWidth, center[1], center[0] + halfWidth, center[1], m_LabelHeight, fill);
        }
    }

    void HitPulses(float x, float y, PredateState state, array<string> matches)
    {
        matches.Clear();
        if (!state) return;
        vector click = Vector(x - m_Left, y - m_Top, 0);
        vector offset = click - m_PlayerPoint;
        float distance = offset.Length();
        float bearing = PredateMath.WrapHeading(Math.Atan2(offset[0], -offset[1]) * Math.RAD2DEG);
        float closestGap = 9 * m_Scale;
        float closestRadius;
        foreach (int placed : m_PlacedSlots)
        {
            vector center = m_LabelPositions[placed];
            if (Math.AbsFloat(click[0] - center[0]) > LabelHalfWidth(placed) || Math.AbsFloat(click[1] - center[1]) > m_LabelHeight / 2) continue;
            matches.Insert(PredateProtocol.OperatorAt(placed));
            return;
        }
        foreach (PredateBearingPulse pulse : state.Pulses)
        {
            int slot = PredateProtocol.s_OperatorIds.Find(pulse.OperatorId);
            if (slot < 0 || slot >= m_Labels.Count()) continue;
            float radius = m_ArcRadii[slot];
            float gap = Math.AbsFloat(distance - radius);
            if (radius <= 0 || !PredateBearingPulse.Contains(pulse.Start, bearing)) continue;
            if (gap < closestGap)
            {
                closestGap = gap;
                closestRadius = radius;
                matches.Clear();
            }
            if (radius == closestRadius) matches.Insert(pulse.OperatorId);
        }
    }

    void PreviewPulse(float x, float y, PredateState state)
    {
        m_HoveredPulse = null;
        m_HoverHits.Clear();
        if (x < m_Left || x > m_Left + m_Width || y < m_Top || y > m_Top + m_Height) return;
        HitPulses(x, y, state, m_HoverHits);
        if (m_HoverHits.Count() != 1) return;
        foreach (PredateBearingPulse pulse : state.Pulses)
        {
            if (pulse.OperatorId != m_HoverHits[0]) continue;
            m_HoveredPulse = pulse;
            return;
        }
    }

    void Draw(PlayerBase player, PredateState state, float scale, bool allowHover = true)
    {
        m_Canvas.Clear();
        m_NextSlots.Clear();
        m_HoverHits.Clear();
        m_HoveredPulse = null;
        foreach (TextWidget bearing : m_Bearings) if (bearing) bearing.Show(false);
        vector oldViewport = Vector(m_Width, m_Height, 0);
        vector oldOrigin = Vector(m_Left, m_Top, 0);
        if (!UpdateProjection())
        {
            m_ActiveSlots.Clear();
            ArrangePulseLabels(-1);
            return;
        }
        vector playerPoint = LocalPoint(player.GetPosition());
        bool moved = m_Scale != scale || m_PlayerPoint != playerPoint || oldViewport != Vector(m_Width, m_Height, 0) || oldOrigin != Vector(m_Left, m_Top, 0);
        m_Scale = scale;
        m_PlayerPoint = playerPoint;
        DrawHistory(state);
        SizePulseLabels(scale);
        // Set the radius from the viewport. The map clips the dial when the player is off-centre.
        float horizontalRoom = m_Width / 2 - (BEARING_LABEL_OFFSET + BEARING_LABEL_WIDTH / 2) * scale;
        float verticalRoom = m_Height / 2 - (BEARING_LABEL_OFFSET + 12.5) * scale;
        float radius = Math.Max(60 * scale, Math.Min(horizontalRoom, verticalRoom));
        DrawDial(radius);
        if (state && state.Pulses.Count() > 0)
        {
            // The wire order is farthest to nearest. Any overlapping nearer arc
            // takes a lane inside all of the farther arcs it touches.
            array<int> lanes = {};
            int deepest;
            for (int index = 0; index < state.Pulses.Count(); index++)
            {
                int lane;
                for (int previous = 0; previous < index; previous++)
                    if (PredateBearingPulse.Overlap(state.Pulses[index].Start, state.Pulses[previous].Start))
                        lane = Math.Max(lane, lanes[previous] + 1);
                lanes.Insert(lane);
                deepest = Math.Max(deepest, lane);
            }
            float outerArc = radius - 8 * scale;
            float radialRoom = Math.Max(0, outerArc - INNER_CLEARANCE * scale);
            int innerLane = Math.Min(deepest, Math.Floor(radialRoom / (MIN_LANE_SPACING * scale)));
            float spacing = Math.Min(PREFERRED_LANE_SPACING * scale, radialRoom / Math.Max(1, innerLane));
            // A full dial shares its innermost lane. Clicking an overlapping
            // portion offers every contact at that bearing in the pulse picker.
            PredateBearingPulse selectedPulse;
            int selectedSlot = -1;
            bool labelsChanged = moved || state.Pulses.Count() != m_ActiveSlots.Count();
            for (int pulseIndex = 0; pulseIndex < state.Pulses.Count(); pulseIndex++)
            {
                PredateBearingPulse pulse = state.Pulses[pulseIndex];
                int operatorSlot = PredateProtocol.s_OperatorIds.Find(pulse.OperatorId);
                if (operatorSlot < 0 || operatorSlot >= m_ArcRadii.Count()) continue;
                float arcRadius = outerArc - Math.Min(lanes[pulseIndex], innerLane) * spacing;
                if (!labelsChanged && (m_ActiveSlots[pulseIndex] != operatorSlot || m_ArcStarts[operatorSlot] != pulse.Start || m_ArcRadii[operatorSlot] != arcRadius)) labelsChanged = true;
                m_ArcRadii[operatorSlot] = arcRadius;
                m_ArcStarts[operatorSlot] = pulse.Start;
                m_ArcCenters[operatorSlot] = RadialPoint(pulse.Start + PredateBearingPulse.DEGREES / 2.0, arcRadius);
                m_NextSlots.Insert(operatorSlot);
                if (state.Locked && pulse.OperatorId == state.OperatorId)
                {
                    selectedPulse = pulse;
                    selectedSlot = operatorSlot;
                }
                else DrawPulse(pulse, arcRadius, false);
            }
            if (labelsChanged || m_LabelLockSlot != selectedSlot)
            {
                foreach (int former : m_ActiveSlots)
                    if (m_NextSlots.Find(former) < 0) m_ArcRadii[former] = 0;
                m_ActiveSlots.Copy(m_NextSlots);
                m_LabelLockSlot = selectedSlot;
                ArrangePulseLabels(selectedSlot);
            }
            int highlightedSlot = selectedSlot;
            if (allowHover)
            {
                int mouseX, mouseY;
                GetMousePos(mouseX, mouseY);
                PreviewPulse(mouseX, mouseY, state);
                if (m_HoveredPulse)
                {
                    highlightedSlot = PredateProtocol.OperatorSlot(m_HoveredPulse.OperatorId);
                    if (highlightedSlot != selectedSlot)
                        DrawPulse(m_HoveredPulse, m_ArcRadii[highlightedSlot], false, true);
                }
            }
            if (selectedPulse) DrawPulse(selectedPulse, m_ArcRadii[selectedSlot], true);
            DrawPulseLabels(highlightedSlot);
        }
        else if (m_ActiveSlots.Count() > 0)
        {
            foreach (int previousSlot : m_ActiveSlots) m_ArcRadii[previousSlot] = 0;
            m_ActiveSlots.Clear();
            m_LabelLockSlot = -1;
            ArrangePulseLabels(-1);
        }
        DrawPlayer(player, scale);
    }
}
