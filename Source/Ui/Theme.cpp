#include "Theme.h"

namespace fourcolor::ui
{
    Laf::Laf()
    {
        setColour (juce::Slider::textBoxTextColourId, colour::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, colour::text);
        setColour (juce::ComboBox::backgroundColourId, colour::panel);
        setColour (juce::ComboBox::textColourId, colour::text);
        setColour (juce::ComboBox::outlineColourId, colour::panelLine);
        setColour (juce::ComboBox::arrowColourId, colour::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colour::panel);
        setColour (juce::PopupMenu::textColourId, colour::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::panelLine);
        setColour (juce::TextButton::buttonColourId, colour::panel);
        setColour (juce::TextButton::textColourOffId, colour::textDim);
        setColour (juce::TextButton::textColourOnId, colour::text);
        setColour (juce::TooltipWindow::backgroundColourId, colour::panel);
        setColour (juce::TooltipWindow::textColourId, colour::text);
    }

    void Laf::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                float sliderPos, float startAngle, float endAngle,
                                juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float arcThickness = juce::jmax (2.0f, radius * 0.14f);
        const float arcRadius = radius - arcThickness * 0.5f;

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        //  Track.
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (colour::panelLine);
        g.strokePath (track, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        //  Value arc. Bipolar sliders (min < 0 < max) fill from 12 o'clock.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float fillFrom = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                           fillFrom, angle, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        //  Body.
        const float bodyRadius = arcRadius - arcThickness * 1.2f;
        g.setColour (colour::panel.brighter (0.08f));
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2, bodyRadius * 2);
        g.setColour (colour::panelLine);
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2, bodyRadius * 2, 1.0f);

        //  Pointer.
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -bodyRadius + 2.0f, 3.0f, bodyRadius * 0.45f, 1.5f);
        g.setColour (colour::text);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void Laf::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                    const juce::Colour&, bool highlighted, bool down)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 4.0f;

        auto base = colour::panel;
        if (button.getToggleState())
        {
            //  Toggled buttons take their accent from the component ID set by
            //  the owner (band colour or neutral accent).
            base = button.findColour (juce::TextButton::buttonOnColourId);
        }

        if (down) base = base.brighter (0.15f);
        else if (highlighted) base = base.brighter (0.07f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (colour::panelLine);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
}
