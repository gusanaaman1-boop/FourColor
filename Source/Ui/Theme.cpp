#include "Theme.h"

namespace fourcolor::ui
{
    namespace
    {
        float prop (juce::Slider& s, const char* key, float fallback)
        {
            const auto& v = s.getProperties();
            return v.contains (key) ? (float) (double) v[key] : fallback;
        }

        bool propBool (juce::Slider& s, const char* key)
        {
            const auto& v = s.getProperties();
            return v.contains (key) && (bool) v[key];
        }

        juce::Colour propColour (juce::Slider& s, const char* key, juce::Colour fallback)
        {
            const auto& v = s.getProperties();
            return v.contains (key) ? juce::Colour ((juce::uint32) (int) v[key]) : fallback;
        }
    }

    Laf::Laf()
    {
        setColour (juce::Slider::textBoxTextColourId, tokens::textPrimary);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, tokens::textPrimary);
        setColour (juce::ComboBox::backgroundColourId, tokens::globalBack);
        setColour (juce::ComboBox::textColourId, tokens::textPrimary);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, tokens::textSecondary);
        setColour (juce::PopupMenu::backgroundColourId, tokens::panelBase);
        setColour (juce::PopupMenu::textColourId, tokens::textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, tokens::borderHover);
        setColour (juce::PopupMenu::highlightedTextColourId, tokens::textPrimary);
        setColour (juce::TextButton::buttonColourId, tokens::panelBase);
        setColour (juce::TextButton::textColourOffId, tokens::textSecondary);
        setColour (juce::TextButton::textColourOnId, tokens::textPrimary);
        setColour (juce::TooltipWindow::backgroundColourId, tokens::panelRaised);
        setColour (juce::TooltipWindow::textColourId, tokens::textPrimary);
        setColour (juce::TooltipWindow::outlineColourId, tokens::borderNormal);
    }

    void Laf::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                float sliderPos, float, float, juce::Slider& slider)
    {
        const auto full = juce::Rectangle<int> (x, y, w, h).toFloat();
        const float outerR = juce::jmin (full.getWidth(), full.getHeight()) * 0.5f - 2.0f;
        const auto centre = full.getCentre();

        const float thickness = prop (slider, "arcThickness", metric::arcMedium);
        const auto arcCol     = propColour (slider, "arcColour", tokens::neutralArc);
        const auto arcColEnd  = propColour (slider, "arcColourEnd", arcCol);
        const float glowAlpha = prop (slider, "glowAlpha", 0.0f);
        const float energy    = juce::jlimit (0.0f, 1.0f, prop (slider, "energy", 0.0f));
        const bool sparks     = propBool (slider, "sparks");
        const bool spread     = propBool (slider, "spreadArcs");

        //  "forceHover"/"forceDrag" let the screenshot tool render the real
        //  hover and drag styling without faking mouse events - the drawing
        //  path stays identical to a live interaction.
        const bool hovering = (slider.isMouseOverOrDragging() || propBool (slider, "forceHover"))
                              && slider.isEnabled();
        const bool dragging = slider.isMouseButtonDown() || propBool (slider, "forceDrag");

        const float start = metric::arcStart, end = metric::arcEnd;
        const float angle = start + sliderPos * (end - start);
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float fillFrom = bipolar ? (start + end) * 0.5f : start;

        const float arcR = outerR - thickness * 0.5f;

        //  --- SPACE / SPREAD: outer arcs that move outward with the value -----
        if (spread)
        {
            const float reach = 3.0f + 6.0f * sliderPos;   // 3..9 px
            for (int i = 1; i <= 2; ++i)
            {
                const float r = outerR + reach * (float) i * 0.5f;
                juce::Path p;
                p.addCentredArc (centre.x, centre.y, r, r, 0.0f,
                                 start + 0.35f, end - 0.35f, true);
                g.setColour (arcCol.withAlpha (0.10f + 0.22f * sliderPos));
                g.strokePath (p, juce::PathStrokeType (1.2f));
            }
        }

        //  --- DRIVE: harmonic sparks, deterministic per index, driven by the
        //      value and by the live audio energy (static when silent) --------
        if (sparks && sliderPos > 0.02f)
        {
            juce::Random rng (0x5EED);
            const int count = 34;
            for (int i = 0; i < count; ++i)
            {
                const float jitter = rng.nextFloat();
                const float a = start + (end - start) * ((float) i / (count - 1));
                if (a > angle)
                    continue;

                const float len = (2.0f + 7.0f * sliderPos * (0.45f + 0.55f * jitter))
                                * (0.55f + 0.45f * energy);
                const float r0 = outerR + 1.5f;
                const float c = std::cos (a), s = std::sin (a);

                //  Copper near the body, magenta at the tips.
                const float t = (float) i / (count - 1);
                g.setColour (arcCol.interpolatedWith (arcColEnd, t)
                                   .withAlpha (0.18f * (0.4f + 0.6f * energy)));
                g.drawLine (centre.x + r0 * s, centre.y - r0 * c,
                            centre.x + (r0 + len) * s, centre.y - (r0 + len) * c,
                            jitter > 0.6f ? 1.4f : 1.0f);
            }
        }

        //  --- outer glow ------------------------------------------------------
        if (glowAlpha > 0.0f || (hovering && ! sparks))
        {
            const float a = juce::jmax (glowAlpha * sliderPos, hovering ? 0.05f : 0.0f);
            for (float i = 6.0f; i >= 1.0f; i -= 1.5f)
            {
                g.setColour (arcCol.withAlpha (a * (1.0f - i / 7.0f)));
                juce::Path p;
                p.addCentredArc (centre.x, centre.y, arcR + i, arcR + i, 0.0f, start, end, true);
                g.strokePath (p, juce::PathStrokeType (thickness));
            }
        }

        //  --- inactive track --------------------------------------------------
        {
            juce::Path track;
            track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
            g.setColour (tokens::arcInactive);
            g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        //  --- active arc (optionally a gradient along the sweep) --------------
        if (std::abs (angle - fillFrom) > 0.001f)
        {
            const float brighten = dragging ? 0.18f : (hovering ? 0.10f : 0.0f);

            if (arcColEnd != arcCol)
            {
                //  Segment the sweep so the colour can travel along it.
                constexpr int segments = 24;
                for (int i = 0; i < segments; ++i)
                {
                    const float t0 = (float) i / segments, t1 = (float) (i + 1) / segments;
                    const float a0 = fillFrom + (angle - fillFrom) * t0;
                    const float a1 = fillFrom + (angle - fillFrom) * t1;
                    juce::Path seg;
                    seg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                                       juce::jmin (a0, a1), juce::jmax (a0, a1), true);
                    g.setColour (arcCol.interpolatedWith (arcColEnd, t1).brighter (brighten));
                    g.strokePath (seg, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
                }
            }
            else
            {
                juce::Path arc;
                arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                                   juce::jmin (fillFrom, angle), juce::jmax (fillFrom, angle), true);
                g.setColour (arcCol.brighter (brighten));
                g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            }
        }

        //  --- body ------------------------------------------------------------
        const float bodyR = arcR - thickness * 0.5f - 3.5f;
        const auto bodyCentre = centre.translated (0.0f, dragging ? 1.0f : 0.0f);
        const auto bodyRect = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f)
                                  .withCentre (bodyCentre);

        //  Outer shadow (softer while pressed, as if the knob moved down).
        for (float i = (dragging ? 4.0f : 6.0f); i >= 1.0f; i -= 1.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f * (1.0f - i / 7.0f) * 0.4f));
            g.fillEllipse (bodyRect.expanded (i).translated (0.0f, 2.0f));
        }

        juce::ColourGradient body (juce::Colour (0xff1d2229),
                                   bodyRect.getCentreX(), bodyRect.getY(),
                                   juce::Colour (0xff12161b),
                                   bodyRect.getCentreX(), bodyRect.getBottom(), false);
        g.setGradientFill (body);
        g.fillEllipse (bodyRect);

        //  Inner shadow at the top edge.
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (bodyRect.reduced (0.5f).translated (0.0f, 1.0f), 1.6f);

        //  Top highlight, 4% only.
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawLine (bodyRect.getCentreX() - bodyR * 0.45f, bodyRect.getY() + bodyR * 0.16f,
                    bodyRect.getCentreX() + bodyR * 0.45f, bodyRect.getY() + bodyR * 0.16f, 1.4f);

        g.setColour (hovering ? tokens::knobHiBorder : tokens::knobBorder);
        g.drawEllipse (bodyRect.reduced (0.5f), 1.0f);

        //  --- pointer: 45% -> 75% of the radius --------------------------------
        {
            const float r0 = bodyR * 0.45f, r1 = bodyR * 0.78f;
            const float c = std::cos (angle), s = std::sin (angle);
            g.setColour (tokens::pointer);
            g.drawLine (bodyCentre.x + r0 * s, bodyCentre.y - r0 * c,
                        bodyCentre.x + r1 * s, bodyCentre.y - r1 * c,
                        juce::jmax (2.0f, bodyR * 0.11f));
        }
    }

    void Laf::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                float sliderPos, float, float,
                                juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
            return;
        }

        //  BODY <-> ATTACK: copper on the left, magenta on the right, dark and
        //  quiet through the centre.
        const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
        const float trackY = area.getCentreY();
        const bool dragging = slider.isMouseButtonDown() || propBool (slider, "forceDrag");

        //  Tick marks above the track.
        g.setColour (tokens::borderNormal);
        for (int i = 0; i <= 24; ++i)
        {
            const float tx = area.getX() + area.getWidth() * (float) i / 24.0f;
            const bool major = i % 6 == 0;
            g.fillRect (tx - 0.5f, trackY - (major ? 15.0f : 12.0f), 1.0f, major ? 6.0f : 4.0f);
        }

        auto track = juce::Rectangle<float> (area.getX(), trackY - 2.0f, area.getWidth(), 4.0f);
        const float mid = area.getCentreX();

        g.setColour (tokens::arcInactive);
        g.fillRoundedRectangle (track, 2.0f);

        //  Left half fades copper towards the centre; right half magenta.
        {
            juce::ColourGradient left (tokens::bandLow.withAlpha (0.95f), track.getX(), trackY,
                                       tokens::bandLow.withAlpha (0.12f), mid, trackY, false);
            g.setGradientFill (left);
            g.fillRoundedRectangle (track.withRight (mid), 2.0f);

            juce::ColourGradient right (tokens::bandHiMid.withAlpha (0.12f), mid, trackY,
                                        tokens::bandHiMid.withAlpha (0.95f), track.getRight(), trackY,
                                        false);
            g.setGradientFill (right);
            g.fillRoundedRectangle (track.withLeft (mid), 2.0f);
        }

        //  Thumb, ~24 px, with a glow towards the side it is leaning to.
        const float r = 12.0f;
        const auto thumbCentre = juce::Point<float> (sliderPos, trackY);
        const auto lean = sliderPos >= mid ? tokens::bandHiMid : tokens::bandLow;

        if (dragging)
            for (float i = 7.0f; i >= 1.0f; i -= 1.5f)
            {
                g.setColour (lean.withAlpha (0.16f * (1.0f - i / 8.0f)));
                g.fillEllipse (juce::Rectangle<float> ((r + i) * 2.0f, (r + i) * 2.0f)
                                   .withCentre (thumbCentre));
            }

        for (float i = 5.0f; i >= 1.0f; i -= 1.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f * (1.0f - i / 6.0f) * 0.5f));
            g.fillEllipse (juce::Rectangle<float> ((r + i) * 2.0f, (r + i) * 2.0f)
                               .withCentre (thumbCentre.translated (0.0f, 3.0f)));
        }

        g.setColour (tokens::controlInner);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (thumbCentre));
        g.setColour (slider.isMouseOverOrDragging() || dragging ? tokens::textPrimary
                                                                : juce::Colour (0xffaeb4bc));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (thumbCentre)
                           .reduced (1.0f), 2.0f);
    }

    void Laf::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                    const juce::Colour&, bool highlighted, bool down)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = metric::cornerSmall - 1.0f;

        auto base = tokens::panelBase;
        auto border = tokens::borderNormal;

        if (button.getToggleState())
        {
            const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
            base = accent.withAlpha (0.20f);
            border = accent.withAlpha (0.85f);
        }

        if (down) base = base.overlaidWith (juce::Colours::black.withAlpha (0.10f));
        else if (highlighted) { base = base.overlaidWith (juce::Colours::white.withAlpha (0.03f));
                                border = border.brighter (0.15f); }

        g.setColour (base);
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (border);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    void Laf::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                            int, int, int, int, juce::ComboBox& box)
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

        g.setColour (isButtonDown ? tokens::panelPressed
                                  : box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, metric::cornerSmall);
        g.setColour (box.isMouseOver() ? tokens::borderHover : tokens::borderNormal);
        g.drawRoundedRectangle (bounds, metric::cornerSmall, 1.0f);

        juce::Path arrow;
        const float ax = (float) width - 15.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        g.setColour (tokens::textSecondary);
        g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    void Laf::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (tokens::panelBase);
        g.fillRoundedRectangle (r, metric::cornerSmall);
        g.setColour (juce::Colour (0xff343b45));
        g.drawRoundedRectangle (r.reduced (0.5f), metric::cornerSmall, 1.0f);
    }

    juce::Label* Laf::createSliderTextBox (juce::Slider& slider)
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::textColourId, tokens::textPrimary);
        label->setJustificationType (juce::Justification::centred);
        return label;
    }
}
