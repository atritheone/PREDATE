class PredateCompass
{
    static const int PULSE_COLOR = 0xFFFFB253;
    Widget m_Root;
    Widget m_Scroll;
    TextWidget m_Heading;
    TextWidget m_Marker;
    Widget m_Sector;
    Widget m_FinalApproachOutline;
    Widget m_HuntedIndicator;
    ref array<Widget> m_Ticks = new array<Widget>;
    ref array<int> m_LabelColors = new array<int>;
    ref array<int> m_MarkColors = new array<int>;
    float m_BaseWidth;
    float m_BaseHeight;
    float m_BaseTop;
    int m_BackdropColor;
    int m_HeadingColor;
    float m_ViewportWidth;
    float m_ViewportHeight;
    ref PredatePresentationOptions m_AppliedOptions;
    bool m_TextFitDone;
    bool m_Ready;

    void PredateCompass(Widget hud)
    {
        if (!hud) return;
        m_Root = hud.FindAnyWidget("Compass");
        m_Heading = TextWidget.Cast(hud.FindAnyWidget("Heading"));
        m_Marker = TextWidget.Cast(hud.FindAnyWidget("PulseMarker"));
        m_Sector = hud.FindAnyWidget("PulseSector");
        m_FinalApproachOutline = hud.FindAnyWidget("FinalApproachOutline");
        m_HuntedIndicator = hud.FindAnyWidget("HuntedIndicator");
        Widget huntedLeft = hud.FindAnyWidget("HuntedLeft");
        Widget huntedRight = hud.FindAnyWidget("HuntedRight");
        m_Scroll = hud.FindAnyWidget("CompassScroll");
        for (int i = 0; i < 144; i++) m_Ticks.Insert(hud.FindAnyWidget("Tick" + i));
        m_Ready = true;
        if (!m_Root) m_Ready = false;
        if (!m_Heading) m_Ready = false;
        if (!m_Marker) m_Ready = false;
        if (!m_Sector) m_Ready = false;
        if (!m_FinalApproachOutline) m_Ready = false;
        if (!m_HuntedIndicator || !huntedLeft || !huntedRight) m_Ready = false;
        if (!m_Scroll) m_Ready = false;
        foreach (Widget tick : m_Ticks) { if (!tick) m_Ready = false; }
        if (m_Ready)
        {
            m_Sector.SetColor(PULSE_COLOR);
            m_Marker.SetColor(PULSE_COLOR);
            huntedLeft.SetColor(PredateHuntFeed.HUNTED);
            huntedRight.SetColor(PredateHuntFeed.HUNTED);
            float baseWidth, baseHeight, baseLeft, baseTop;
            m_Root.GetSize(baseWidth, baseHeight);
            m_Root.GetPos(baseLeft, baseTop);
            m_BaseWidth = baseWidth;
            m_BaseHeight = baseHeight;
            m_BaseTop = baseTop;
            int headingColor = m_Heading.GetColor();
            m_HeadingColor = headingColor;
            Widget backdrop = hud.FindAnyWidget("CompassBackdrop");
            if (backdrop)
            {
                int backdropColor = backdrop.GetColor();
                m_BackdropColor = backdropColor;
            }
            for (int markIndex = 0; markIndex < 144; markIndex++)
            {
                TextWidget label = TextWidget.Cast(hud.FindAnyWidget("CompassLabel" + markIndex));
                Widget mark = hud.FindAnyWidget("Mark" + markIndex);
                int labelColor;
                int markColor;
                if (label) labelColor = label.GetColor();
                if (mark) markColor = mark.GetColor();
                m_LabelColors.Insert(labelColor);
                m_MarkColors.Insert(markColor);
            }
            ApplyPresentation(PredatePresentationOptions.Current());
        }
        if (m_Ready) Print("[Predate] Compass initialized: continuous scale, camera heading and sampled pulse marker.");
        else Print("[Predate] COMPASS ERROR: missing layout widgets.");
    }

    void ApplyPresentation(PredatePresentationOptions options)
    {
        if (!m_Ready || !options || !options.Valid()) return;
        m_AppliedOptions = options;
        m_TextFitDone = false;
        Widget hud = m_Root.GetParent();
        float viewportWidth, viewportHeight;
        hud.GetScreenSize(viewportWidth, viewportHeight);
        m_ViewportWidth = viewportWidth;
        m_ViewportHeight = viewportHeight;
        float scale = options.CompassPercent / 100.0;
        // Preserve the compact 1440p design above that resolution. Text uses
        // physical pixels and a distance-field font, avoiding a second engine
        // UI enlargement pass over a small bitmap font.
        float designScale = Math.Min(viewportHeight / 1440.0, 1.0);
        float width = 716.8 * designScale * scale / viewportWidth;
        int textSize = Math.Max(10, Math.Round(18 * designScale * scale));
        float height = 86.4 * designScale * scale / viewportHeight;
        m_Root.SetSize(width, height);
        m_Root.SetPos(0.5 - width * 0.5, m_BaseTop);
        m_Heading.SetTextExactSize(textSize);
        if (options.HighContrast) m_Heading.SetColor(0xFFFFFFFF);
        else m_Heading.SetColor(m_HeadingColor);
        m_Marker.SetTextExactSize(textSize);
        TextWidget receiver = TextWidget.Cast(hud.FindAnyWidget("Receiver"));
        if (receiver) receiver.SetTextExactSize(textSize);
        Widget backdrop = hud.FindAnyWidget("CompassBackdrop");
        if (backdrop)
        {
            if (options.HighContrast) backdrop.SetColor(0xD9080F18);
            else backdrop.SetColor(m_BackdropColor);
        }
        for (int i = 0; i < 144; i++)
        {
            TextWidget label = TextWidget.Cast(hud.FindAnyWidget("CompassLabel" + i));
            if (label)
            {
                label.SetTextExactSize(textSize);
                if (options.HighContrast) label.SetColor(0xFFFFFFFF);
                else label.SetColor(m_LabelColors[i]);
            }
            Widget mark = hud.FindAnyWidget("Mark" + i);
            if (mark)
            {
                if (options.HighContrast) mark.SetColor(0xFFE9F3FF);
                else mark.SetColor(m_MarkColors[i]);
            }
        }
        m_Root.Update();
    }

    void FitMeasuredText()
    {
        if (m_TextFitDone || m_ViewportHeight <= 0) return;
        TextWidget receiver = TextWidget.Cast(m_Root.GetParent().FindAnyWidget("Receiver"));
        if (!receiver) return;
        int receiverWidth, receiverHeight, headingWidth, headingHeight;
        receiver.GetTextSize(receiverWidth, receiverHeight);
        m_Heading.GetTextSize(headingWidth, headingHeight);
        if (receiverHeight <= 0 || headingHeight <= 0) return;
        float required = (Math.Max(receiverHeight, headingHeight) + 0.5) / (m_ViewportHeight * 0.23);
        float width, height;
        m_Root.GetSize(width, height);
        if (required > height) m_Root.SetSize(width, required);
        m_TextFitDone = true;
    }

    void Update(PlayerBase player, PredateState state, float timeslice)
    {
        if (!m_Ready) return;
        m_HuntedIndicator.Show(state && state.Intercepted);
        vector direction = GetGame().GetCurrentCameraDirection();
        if (Math.AbsFloat(direction[0]) + Math.AbsFloat(direction[2]) < 0.001)
            direction = player.GetDirection();
        Render(PredateMath.CameraHeading(direction), state, timeslice);
    }

    void Render(float heading, PredateState state, float timeslice)
    {
        if (!m_Ready) return;
        float viewportWidth, viewportHeight;
        m_Root.GetParent().GetScreenSize(viewportWidth, viewportHeight);
        if (Math.AbsFloat(viewportWidth - m_ViewportWidth) > 0.5 || Math.AbsFloat(viewportHeight - m_ViewportHeight) > 0.5)
            ApplyPresentation(m_AppliedOptions);
        heading = PredateMath.WrapHeading(heading);
        int rounded = Math.Round(heading);
        rounded = rounded % 360;
        m_Heading.SetText(PredateMath.CompassPoint(heading) + "  " + PredateMath.BearingText(rounded));
        FitMeasuredText();

        // Keep glyphs and ticks fixed in their own layout. Move one continuous
        // tape with immediate invalidation instead of moving/hiding each label.
        float tapeWidth, tapeHeight;
        m_Scroll.GetParent().GetScreenSize(tapeWidth, tapeHeight);
        float scrollX = -0.5 - heading / 180.0;
        if (tapeWidth > 0) scrollX = Math.Round(scrollX * tapeWidth) / tapeWidth;
        m_Scroll.SetPos(scrollX, 0, true);
        m_Scroll.Update();

        bool hasTrack = state && state.Available && state.Locked;
        bool finalApproach = hasTrack && state.CloseApproach;
        bool showSample = hasTrack && state.HasPulse && !finalApproach;
        m_FinalApproachOutline.Show(finalApproach);
        m_Marker.Show(false);
        if (m_Sector) m_Sector.Show(false);
        if (showSample)
        {
            int color = PULSE_COLOR;
            PredateBearingPulse pulse = state.TrackedPulse();
            if (pulse) color = pulse.Color();
            m_Marker.SetColor(color);
            if (m_Sector) m_Sector.SetColor(color);
            float halfSector = PredateBearingPulse.DEGREES / 2.0;
            float relative = PredateMath.RelativeBearing(state.Bearing + halfSector, heading);
            float center = Math.Clamp(0.5 + relative / 180.0, 0.03, 0.97);
            m_Marker.SetPos(center - 0.03, 0.05);
            if (Math.AbsFloat(relative) >= 90 + halfSector)
            {
                if (relative < 0) m_Marker.SetText("<");
                else m_Marker.SetText(">");
                m_Marker.Show(true);
            }
            else if (m_Sector)
            {
                float left = Math.Clamp(0.5 + (relative - halfSector) / 180.0, 0, 1);
                float right = Math.Clamp(0.5 + (relative + halfSector) / 180.0, 0, 1);
                m_Sector.SetPos(left, 0.04);
                m_Sector.SetSize(right - left, 0.11);
                m_Sector.Show(true);
            }
        }
        m_Root.Update();
    }

    void SelfTest()
    {
        if (!m_Ready) return;
        bool passed = Math.AbsFloat(PredateMath.RelativeBearing(1, 359) - 2) < 0.001;
        passed = passed && Math.AbsFloat(PredateMath.RelativeBearing(359, 1) + 2) < 0.001;
        passed = passed && PredateMath.CompassPoint(359) == "N";
        passed = passed && PredateMath.CompassPoint(90) == "E";
        PredateState sample = new PredateState;
        sample.Available = true;
        sample.Locked = true;
        sample.HasPulse = true;
        sample.Bearing = 90;
        sample.Age = 1;
        Render(97.5, sample, 0);
        passed = passed && !m_FinalApproachOutline.IsVisible();
        float x, y;
        m_Marker.GetPos(x, y);
        passed = passed && Math.AbsFloat(x - 0.47) < 0.001;
        Render(0, sample, 0);
        m_Marker.GetPos(x, y);
        passed = passed && x > 0.9;
        passed = passed && sample.Bearing == 90;
        sample.CloseApproach = true;
        Render(90, sample, 0);
        passed = passed && !m_Marker.IsVisible() && !m_Sector.IsVisible() && m_FinalApproachOutline.IsVisible();
        Render(0, null, 0);
        passed = passed && !m_FinalApproachOutline.IsVisible();
        if (passed) Print("[Predate] COMPASS SELFTEST PASS: north wrap, headings, marker rotation, frozen sample and close-range cutoff.");
        else Print("[Predate] COMPASS SELFTEST FAIL: heading or marker projection mismatch.");
    }
}
