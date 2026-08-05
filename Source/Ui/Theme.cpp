#include "Theme.h"

namespace fourcolor::ui
{
    Laf::Laf()
    {
        setColour (juce::Slider::textBoxTextColourId, colour::textDim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, colour::text);
        setColour (juce::ComboBox::backgroundColourId, colour::panel);
        setColour (juce::ComboBox::textColourId, colour::text);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, colour::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colour::panelHi);
        setColour (juce::PopupMenu::textColourId, colour::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::panelLine);
        setColour (juce::TextButton::buttonColourId, colour::panel);
        setColour (juce::TextButton::textColourOffId, colour::textDim);
        setColour (juce::TextButton::textColourOnId, colour::text);
        setColour (juce::TooltipWindow::backgroundColourId, colour::panelHi);
        setColour (juce::TooltipWindow::textColourId, colour::text);
    }

    void Laf::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                float sliderPos, float startAngle, float endAngle,
                                juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        //  Tick dots around the body: lit up to the value (from 12 o'clock for
        //  bipolar parameters).
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float centreAngle = (startAngle + endAngle) * 0.5f;
        constexpr int numDots = 13;
        const float dotRadius = radius - 1.5f;

        for (int i = 0; i < numDots; ++i)
        {
            const float t = (float) i / (numDots - 1);
            const float a = startAngle + t * (endAngle - startAngle);

            bool lit = bipolar ? (angle >= centreAngle ? (a >= centreAngle && a <= angle)
                                                       : (a <= centreAngle && a >= angle))
                               : a <= angle;

            const float dotSize = lit ? 3.0f : 2.0f;
            g.setColour (lit ? accent : colour::panelLine);
            const float dx = centre.x + dotRadius * std::sin (a);
            const float dy = centre.y - dotRadius * std::cos (a);
            g.fillEllipse (dx - dotSize * 0.5f, dy - dotSize * 0.5f, dotSize, dotSize);
        }

        //  Body: soft-gradient dark puck.
        const float bodyRadius = radius - 8.0f;
        {
            juce::ColourGradient grad (colour::knobBody.brighter (0.25f),
                                       centre.x - bodyRadius * 0.5f, centre.y - bodyRadius * 0.7f,
                                       colour::knobBody.darker (0.35f),
                                       centre.x + bodyRadius * 0.4f, centre.y + bodyRadius,
                                       false);
            g.setGradientFill (grad);
            g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                           bodyRadius * 2.0f, bodyRadius * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                           bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);
        }

        //  Pointer.
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.6f, -bodyRadius + 4.0f, 3.2f, bodyRadius * 0.42f, 1.6f);
        g.setColour (colour::text);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
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

        //  The BODY <-> ATTACK style: gradient track fading towards the right,
        //  small ticks above, round thumb.
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
        const float trackY = area.getCentreY();

        //  Ticks.
        g.setColour (colour::panelLine);
        for (int i = 0; i <= 20; ++i)
        {
            const float tx = area.getX() + area.getWidth() * (float) i / 20.0f;
            const bool major = i % 5 == 0;
            g.fillRect (tx - 0.5f, trackY - (major ? 14.0f : 11.0f), 1.0f, major ? 5.0f : 3.0f);
        }

        //  Track.
        auto track = juce::Rectangle<float> (area.getX(), trackY - 2.5f, area.getWidth(), 5.0f);
        juce::ColourGradient grad (accent.withAlpha (0.95f), track.getX(), trackY,
                                   accent.withAlpha (0.10f), track.getRight(), trackY, false);
        g.setColour (colour::panelLine.withAlpha (0.6f));
        g.fillRoundedRectangle (track, 2.5f);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (track, 2.5f);

        //  Thumb.
        const float r = 13.0f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (sliderPos - r, trackY - r + 1.5f, r * 2.0f, r * 2.0f);
        juce::ColourGradient thumb (juce::Colour (0xffd7d7db), sliderPos - r * 0.4f, trackY - r * 0.6f,
                                    juce::Colour (0xff8f8f96), sliderPos + r * 0.4f, trackY + r, false);
        g.setGradientFill (thumb);
        g.fillEllipse (sliderPos - r, trackY - r, r * 2.0f, r * 2.0f);
    }

    void Laf::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                    const juce::Colour&, bool highlighted, bool down)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 4.0f;

        auto base = colour::panelHi;
        if (button.getToggleState())
            base = button.findColour (juce::TextButton::buttonOnColourId);

        if (down) base = base.brighter (0.15f);
        else if (highlighted) base = base.brighter (0.07f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (button.getToggleState()
                         ? base.brighter (0.35f) : colour::panelLine);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    juce::Label* Laf::createSliderTextBox (juce::Slider& slider)
    {
        //  The mockup shows plain dim value text under the knobs - no box.
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::textColourId, colour::textDim);
        label->setFont (uiFont (12.0f));
        return label;
    }

    void Laf::drawComboBox (juce::Graphics& g, int width, int height, bool,
                            int, int, int, int, juce::ComboBox& box)
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (colour::panelLine);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) width - 15.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 3.5f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.0f);
        arrow.lineTo (ax + 3.5f, ay - 2.0f);
        g.setColour (colour::textDim);
        g.strokePath (arrow, juce::PathStrokeType (1.4f));
    }
}
