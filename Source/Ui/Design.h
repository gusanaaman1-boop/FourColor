// FOUR COLOR design tokens - the single source of colour, metric and type
// values for the whole interface. No component may hard-code a colour.
//
// Values are taken from the reference specification.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace fourcolor::ui
{
    namespace tokens
    {
        // --- surfaces ---------------------------------------------------------
        const juce::Colour backgroundDeep { 0xff090b0e };
        const juce::Colour backgroundTop  { 0xff0c0f13 };
        const juce::Colour panelBase      { 0xff11151a };
        const juce::Colour panelRaised    { 0xff151920 };
        const juce::Colour panelHover     { 0xff191e25 };
        const juce::Colour panelPressed   { 0xff0e1116 };
        const juce::Colour controlInner   { 0xff171b21 };
        const juce::Colour controlOuter   { 0xff222832 };
        const juce::Colour analyzerBack   { 0xff0a0d10 };
        const juce::Colour globalBack     { 0xff0d1014 };

        const juce::Colour borderNormal   { 0xff292f38 };
        const juce::Colour borderSoft     { 0xff20262e };
        const juce::Colour borderHover    { 0xff3a424d };
        const juce::Colour borderFocus    { 0xff3b424d };

        const juce::Colour gridMajor      = juce::Colour (0xff343a43).withAlpha (0.32f);
        const juce::Colour gridMinor      = juce::Colour (0xff2b3139).withAlpha (0.18f);

        // --- text -------------------------------------------------------------
        const juce::Colour textPrimary   { 0xffeceef1 };
        const juce::Colour textSecondary { 0xffa0a6af };
        const juce::Colour textMuted     { 0xff69717c };
        const juce::Colour textDisabled  { 0xff464d57 };

        // --- band / engine colours -------------------------------------------
        const juce::Colour bandLow    { 0xffe66a45 };   // WARM   copper-coral
        const juce::Colour bandLowMid { 0xffc79a45 };   // IRON   smoked gold
        const juce::Colour bandHiMid  { 0xffe84d78 };   // BITE   magenta
        const juce::Colour bandHigh   { 0xff55d6c2 };   // FUZZ   teal
        const juce::Colour periwinkle { 0xff7489d8 };   // auxiliary only

        const juce::Colour band[4] = { bandLow, bandLowMid, bandHiMid, bandHigh };

        // --- functional accents ----------------------------------------------
        const juce::Colour neutralArc   { 0xffd9dce1 };   // band mix
        const juce::Colour neutralArcII { 0xffaeb4bc };   // level / global trims
        const juce::Colour amberRing    { 0xfff2b64c };   // auto level
        const juce::Colour pointer      { 0xfff2f3f5 };
        const juce::Colour arcInactive  { 0xff252b34 };
        const juce::Colour knobBorder   { 0xff303640 };
        const juce::Colour knobHiBorder { 0xff444c58 };

        // --- meters ------------------------------------------------------------
        const juce::Colour meterLow  { 0xff55c9a8 };
        const juce::Colour meterMid  { 0xffe0b44f };
        const juce::Colour meterHigh { 0xffe66a45 };
        const juce::Colour meterPeak { 0xffe84d78 };
    }

    namespace metric
    {
        //  Window fractions from the specification (§4).
        constexpr float topBarBottom     = 0.080f;
        //  The band cards are gone: their S/M/B moved into the headers, so the
        //  analyzer absorbs the space they used. It is the first thing in the
        //  visual priority order and it was the smallest thing on screen.
        constexpr float headersTop       = 0.080f;
        constexpr float headersBottom    = 0.128f;
        constexpr float analyzerTop      = 0.130f;
        constexpr float analyzerBottom   = 0.525f;
        constexpr float panelTop         = 0.535f;
        constexpr float panelBottom      = 0.805f;
        constexpr float globalTop        = 0.815f;

        constexpr float sideMargin       = 14.0f;   // at 980 px wide
        constexpr float cardGap          = 7.0f;
        constexpr float corner           = 7.0f;
        constexpr float cornerSmall      = 5.0f;

        //  Width of the analyzer's dB scale. The band headers align to the
        //  same plot area, so both must use one number.
        constexpr float leftAxis         = 32.0f;

        //  Knob arc geometry (270 degrees, 225 -> 495).
        constexpr float arcStart = 3.92699f;   // 225 deg
        constexpr float arcEnd   = 8.63938f;   // 495 deg

        constexpr float arcLarge  = 5.0f;
        constexpr float arcMedium = 4.0f;
        constexpr float arcSmall  = 3.0f;
    }

    //  Scale factor relative to the 980x620 reference, so paddings and fonts
    //  track the window without hard-coding pixel positions.
    inline float scaleFor (juce::Component& c)
    {
        return juce::jlimit (0.86f, 1.9f, (float) c.getWidth() / 980.0f);
    }

    inline juce::Font uiFont (float height, bool bold = false)
    {
        //  Inter is not bundled; the platform's clean geometric sans is used
        //  with the same tracking discipline.
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    //  Uppercase caption voice: gentle tracking, never decorative.
    inline juce::Font captionFont (float height, bool bold = false)
    {
        return uiFont (height, bold).withExtraKerningFactor (0.14f);
    }

    inline juce::Font valueFont (float height)
    {
        return uiFont (height);
    }

    // --- shared panel painting -------------------------------------------------
    namespace paint
    {
        //  Drop shadow under a rounded panel, drawn as stacked translucent
        //  outlines (cheap and stable at any DPI).
        inline void dropShadow (juce::Graphics& g, juce::Rectangle<float> r, float corner,
                                float radius, float alpha, float yOffset = 2.0f)
        {
            for (float i = radius; i >= 1.0f; i -= 1.0f)
            {
                const float t = i / radius;
                g.setColour (juce::Colours::black.withAlpha (alpha * (1.0f - t) * 0.5f));
                g.drawRoundedRectangle (r.translated (0.0f, yOffset).expanded (i), corner + i, 1.0f);
            }
        }

        //  Outer glow in a colour, used sparingly for the selected band.
        inline void glow (juce::Graphics& g, juce::Rectangle<float> r, float corner,
                          juce::Colour c, float radius, float alpha)
        {
            for (float i = radius; i >= 1.0f; i -= 1.5f)
            {
                const float t = i / radius;
                g.setColour (c.withAlpha (alpha * (1.0f - t)));
                g.drawRoundedRectangle (r.expanded (i), corner + i, 1.4f);
            }
        }

        //  Panel: shadow, fill, 1 px border and the faint inner top highlight.
        inline void panel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill,
                           juce::Colour border, float corner = metric::corner,
                           bool withShadow = true)
        {
            if (withShadow)
                dropShadow (g, r, corner, 5.0f, 0.32f, 3.0f);

            g.setColour (fill);
            g.fillRoundedRectangle (r, corner);

            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.drawLine (r.getX() + corner, r.getY() + 1.0f,
                        r.getRight() - corner, r.getY() + 1.0f, 1.0f);

            g.setColour (border);
            g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        }
    }
}
