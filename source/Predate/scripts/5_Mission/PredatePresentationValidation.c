// Runs only with an explicit diagnostic flag and a marker in a test profile.
class PredatePresentationReport
{
    int Schema = 1;
    int Checks;
    int Failures;
    int ConfiguredPercent;
    bool HighContrast;
    float ViewportWidth;
    float ViewportHeight;
    float CompassWidth;
    float CompassHeight;
    float RenderedDegrees;
    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredatePresentationTest] FAIL: " + label);
    }
}

class PredatePresentationValidation
{
    ref PredatePresentationReport m_Report = new PredatePresentationReport;
    ref PredateState m_Sample = new PredateState;
    float m_Heading;
    float m_NextMeasurement;
    bool m_Started;
    float m_Warmup;

    bool Fits(TextWidget text)
    {
        if (!text) return false;
        int textWidth, textHeight;
        float width, height;
        text.GetTextSize(textWidth, textHeight);
        text.GetScreenSize(width, height);
        bool fits = textWidth > 0 && textHeight > 0 && textWidth <= width + 1 && textHeight <= height + 1;
        if (!fits) Print("[PredatePresentationTest] METRICS: name=" + text.GetName() + "; glyphs=" + textWidth + "x" + textHeight + "; box=" + width + "x" + height);
        return fits;
    }

    bool Update(Widget hud, PredateCompass compass, float timeslice)
    {
        if (!compass || !compass.m_Ready) return true;
        m_Warmup += Math.Max(0, timeslice);
        if (m_Warmup < 1) return true;
        if (!m_Started)
        {
            m_Started = true;
            PredatePresentationOptions options = PredatePresentationOptions.Current();
            m_Report.ConfiguredPercent = options.CompassPercent;
            m_Report.HighContrast = options.HighContrast;
            float viewportWidth, viewportHeight, width, height, x, y;
            hud.GetScreenSize(viewportWidth, viewportHeight);
            compass.m_Root.GetScreenSize(width, height);
            compass.m_Root.GetPos(x, y);
            float relativeWidth, relativeHeight;
            compass.m_Root.GetSize(relativeWidth, relativeHeight);
            m_Report.ViewportWidth = viewportWidth;
            m_Report.ViewportHeight = viewportHeight;
            m_Report.CompassWidth = width;
            m_Report.CompassHeight = height;
            float expected = 716.8 * Math.Min(viewportHeight / 1440.0, 1.0) * options.CompassPercent / 100.0 / viewportWidth;
            m_Report.Check(FileExist("$profile:PredatePresentation.json") && options.Valid(), "the native client loaded its valid display preference file");
            m_Report.Check(Math.AbsFloat(relativeWidth - expected) < 0.0001, "native compass width follows the selected size");
            m_Report.Check(Math.AbsFloat(x + relativeWidth * 0.5 - 0.5) < 0.0001, "the resized compass stays horizontally centred");
            m_Report.Check(viewportWidth > 0 && viewportHeight > 0, "the actual native viewport is measured");
            Widget backdrop = hud.FindAnyWidget("CompassBackdrop");
            TextWidget label = TextWidget.Cast(hud.FindAnyWidget("CompassLabel0"));
            bool colors;
            if (backdrop && label)
            {
                if (options.HighContrast) colors = backdrop.GetColor() == 0xD9080F18 && label.GetColor() == 0xFFFFFFFF;
                else colors = backdrop.GetColor() == compass.m_BackdropColor && label.GetColor() == compass.m_LabelColors[0];
            }
            m_Report.Check(colors, "native display colors follow the chosen contrast");
            m_Report.Check(Fits(TextWidget.Cast(hud.FindAnyWidget("Receiver"))), "receiver glyphs fit their actual row");
            // The 10 px font floor needs extra row height at low resolutions.
            float heightCap = Math.Max(72, 144 * Math.Min(viewportHeight / 1440.0, 1) * options.CompassPercent / 100.0);
            m_Report.Check(height <= heightCap, "the compact compass stays within its height budget");
            m_Sample.Available = true;
            m_Sample.Locked = true;
            m_Sample.HasPulse = true;
            m_Sample.Bearing = 345;
            m_Sample.Age = 1;
            compass.Render(352.5, m_Sample, 0);
            float sectorX, sectorY, sectorWidth, sectorHeight;
            compass.m_Sector.GetPos(sectorX, sectorY);
            compass.m_Sector.GetSize(sectorWidth, sectorHeight);
            m_Report.Check(compass.m_Sector.IsVisible() && Math.AbsFloat(sectorX - 11.0 / 24.0) < 0.001 && Math.AbsFloat(sectorWidth - 1.0 / 12.0) < 0.001, "15-degree pulse renders at the correct width across north wrap");
            m_Report.Check(compass.m_Sector.GetColor() == PredateCompass.PULSE_COLOR && !compass.m_Marker.IsVisible() && !hud.FindAnyWidget("PulseFlash"), "one uniform orange sector is visible when a pulse arrives");
        }
        m_Heading += Math.Max(0, timeslice) * 90;
        compass.Render(m_Heading, m_Sample, timeslice);
        if (m_Heading >= m_NextMeasurement)
        {
            m_Report.Check(Fits(compass.m_Heading), "heading glyphs fit while the tape moves");
            int centre = Math.Floor(36 + m_Heading / 5);
            int index = centre - (centre % 3) + 3;
            if (((index * 5 - 180) % 45) == 0) index += 3;
            TextWidget numeric = TextWidget.Cast(hud.FindAnyWidget("CompassLabel" + index));
            m_Report.Check(Fits(numeric), "three-digit tape labels fit their actual layout cells");
            m_NextMeasurement += 45;
        }
        if (m_Heading < 360) return true;
        m_Report.RenderedDegrees = m_Heading;
        m_Report.Check(compass.m_Ticks.Count() == 144 && compass.m_Root.IsVisible(), "a full rotation retains the stable compass widget set");
        string error;
        JsonFileLoader<PredatePresentationReport>.SaveFile("$profile:PresentationProbe.json", m_Report, error);
        Print("[PredatePresentationTest] COMPLETE: checks=" + m_Report.Checks + "; failures=" + m_Report.Failures);
        return false;
    }
}
