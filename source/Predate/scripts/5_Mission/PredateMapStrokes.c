class PredateMapStroke
{
    ImageWidget Image;
    string Mask;
    int Color;
    int Priority;
    float Bearing;
    vector Center;
    vector Size;
}

// Filtered masks give every stroke coverage at its edges, including the
// continuous dial ring and the square ends of pulse sectors.
class PredateMapStrokes
{
    static ref array<int> s_SourceRadii = {20, 24, 29, 35, 41, 50, 60, 72, 86, 103, 124, 149, 178, 214, 257, 308, 370, 444, 532, 639, 767, 920, 1104, 1325, 1590};
    static ref array<float> s_RingWidths = {2.4, 3.6, 6.0, 10.0};
    static ref array<float> s_LineRatios = {2, 3, 4, 6, 9, 13, 19, 28, 42, 63, 94, 141, 212, 318, 477, 716};
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
        DrawMask("set:PredateMapArcs image:Arc" + band, center, Vector(2 * extent, 2 * extent, 0), bearing, color, priority);
    }

    void DrawRing(vector center, float radius, float width, int color, int priority)
    {
        // The atlas uses 480-pixel radii inside 1024-pixel squares.
        float requested = width * 480 / radius;
        int band = 0;
        while (band + 1 < s_RingWidths.Count() && requested > (s_RingWidths[band] + s_RingWidths[band + 1]) * 0.5)
            band++;
        float size = radius * (1024.0 / 480);
        DrawMask("set:PredateMapLinework image:Ring" + band, center, Vector(size, size, 0), 0, color, priority);
    }

    void DrawLine(vector from, vector to, float width, int color, int priority)
    {
        vector delta = to - from;
        float length = delta.Length();
        if (length < 0.5 || width <= 0) return;
        float angle = Math.Atan2(delta[1], delta[0]) * Math.RAD2DEG;
        float ratio = length / width;
        int band = 0;
        while (band + 1 < s_LineRatios.Count() && ratio > (s_LineRatios[band] + s_LineRatios[band + 1]) * 0.5)
            band++;
        float size = length * (16.0 / 15);
        DrawMask("set:PredateMapLinework image:Line" + band, (from + to) * 0.5, Vector(size, size, 0), angle, color, priority);
    }

    void DrawBox(vector center, float width, float height, int color, int priority)
    {
        DrawMask("set:PredateMapLinework image:Box", center, Vector(width, height, 0), 0, color, priority);
    }

    protected void DrawMask(string mask, vector center, vector size, float bearing, int color, int priority)
    {
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
        if (stroke.Mask != mask)
        {
            if (!stroke.Image.LoadImageFile(0, mask) || !stroke.Image.SetImage(0)) return;
            stroke.Mask = mask;
        }
        m_Used++;
        center += Vector(m_Left, m_Top, 0);
        bool geometryChanged = center != stroke.Center || size != stroke.Size || bearing != stroke.Bearing;
        bool reordered = priority != stroke.Priority;
        if (geometryChanged)
        {
            stroke.Image.SetScreenSize(size[0], size[1], false);
            stroke.Image.SetScreenPos(center[0] - size[0] / 2, center[1] - size[1] / 2, false);
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
        if (geometryChanged || reordered) stroke.Image.Update();
        if (index >= m_Visible) stroke.Image.Show(true);
    }
}
