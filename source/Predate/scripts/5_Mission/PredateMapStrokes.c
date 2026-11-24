class PredateMapStroke
{
    ImageWidget Image;
    int Band = -1;
    int Color;
    int Priority;
    float Bearing;
    vector Center;
    vector Size;
}

// Filtered 15-degree arc masks preserve the exact radius and bearing of a
// pulse without the aliased edges and visible joins of canvas line segments.
class PredateMapStrokes
{
    static ref array<int> s_SourceRadii = {20, 24, 29, 35, 41, 50, 60, 72, 86, 103, 124, 149, 178, 214, 257, 308, 370, 444, 532, 639, 767, 920, 1104, 1325, 1590};
    Widget m_Parent;
    ref array<ref PredateMapStroke> m_Strokes = {};
    int m_Used;
    int m_Visible;
    float m_Left;
    float m_Top;
    float m_Width;
    float m_Height;

    void PredateMapStrokes(Widget parent)
    {
        m_Parent = parent;
    }

    void Begin(float left, float top, float width, float height)
    {
        m_Used = 0;
        m_Left = left;
        m_Top = top;
        m_Width = width;
        m_Height = height;
    }

    void Finish()
    {
        for (int index = m_Used; index < m_Visible; index++)
            m_Strokes[index].Image.Show(false);
        m_Visible = m_Used;
    }

    static int NearestBand(float desiredRadius)
    {
        int low;
        int high = s_SourceRadii.Count() - 1;
        while (low < high)
        {
            int middle = (low + high) / 2;
            float boundary = (s_SourceRadii[middle] + s_SourceRadii[middle + 1]) * 0.5;
            if (desiredRadius <= boundary) high = middle;
            else low = middle + 1;
        }
        return low;
    }

    void DrawArc(vector origin, float start, float radius, float width, int color, int priority)
    {
        if (radius < 0.5 || width <= 0) return;
        float bearing = start + PredateBearingPulse.DEGREES / 2.0;
        float angle = bearing * Math.DEG2RAD;
        vector center = origin + Vector(radius * Math.Sin(angle), -radius * Math.Cos(angle), 0);

        // The source art has a four-unit stroke. Choose the closest
        // radius-to-stroke ratio, then scale to the exact screen radius.
        int band = NearestBand(4.0 * radius / width);
        float sourceRadius = s_SourceRadii[band];
        float scale = radius / sourceRadius;
        // Leave enough room around the midpoint for the complete stroke.
        // Square bounds keep the rotated art inside the widget.
        float quarterAngle = PredateBearingPulse.DEGREES * Math.DEG2RAD / 4;
        float extent = (Math.Ceil(2 * sourceRadius * Math.Sin(quarterAngle)) + 4) * scale;
        if (center[0] + extent < 0 || center[0] - extent > m_Width || center[1] + extent < 0 || center[1] - extent > m_Height) return;

        int index = m_Used;
        if (index == m_Strokes.Count())
        {
            ImageWidget image = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidgets("Predate/gui/map_stroke.layout", m_Parent));
            if (!image) return;
            PredateMapStroke created = new PredateMapStroke;
            created.Image = image;
            m_Strokes.Insert(created);
        }
        PredateMapStroke stroke = m_Strokes[index];
        if (stroke.Band != band)
        {
            if (!stroke.Image.LoadImageFile(0, "set:PredateMapArcs image:Arc" + band) || !stroke.Image.SetImage(0)) return;
            stroke.Band = band;
        }
        m_Used++;
        vector size = Vector(2 * extent, 2 * extent, 0);
        center += Vector(m_Left, m_Top, 0);
        bool geometryChanged = center != stroke.Center || size != stroke.Size || bearing != stroke.Bearing;
        bool reordered = priority != stroke.Priority;
        if (geometryChanged)
        {
            stroke.Image.SetScreenSize(size[0], size[1], false);
            stroke.Image.SetScreenPos(center[0] - extent, center[1] - extent, false);
            stroke.Image.SetRotation(0, 0, bearing, false);
            stroke.Center = center;
            stroke.Size = size;
            stroke.Bearing = bearing;
        }
        if (reordered)
        {
            stroke.Image.SetSort(priority, false);
            stroke.Priority = priority;
        }
        if (color != stroke.Color)
        {
            stroke.Image.SetColor(color);
            stroke.Color = color;
        }
        if (index >= m_Visible) stroke.Image.Show(true);
        else if (geometryChanged || reordered) stroke.Image.Update();
    }
}
