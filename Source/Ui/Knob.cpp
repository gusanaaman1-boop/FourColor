#include "Knob.h"

namespace fourcolor::ui
{
    Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId,
                const juce::String& captionText, juce::Colour accent, Size size,
                const juce::String& tooltip)
        : state (apvts), caption (captionText), accentColour (accent), knobSize (size)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        //  The value is painted by this component, so the text is plain and
        //  frameless - the reference shows no boxes around numbers.
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setVelocityModeParameters (1.0, 1, 0.08, true, juce::ModifierKeys::ctrlModifier);
        slider.setMouseDragSensitivity (170);
        if (tooltip.isNotEmpty())
            slider.setTooltip (tooltip);

        slider.getProperties().set ("arcThickness",
                                    size == Size::large  ? metric::arcLarge
                                  : size == Size::medium ? metric::arcMedium
                                                         : metric::arcSmall);
        setAccent (accent);

        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = [this] { repaint(); if (onDragStateChanged) onDragStateChanged (true); };
        slider.onDragEnd   = [this] { repaint(); if (onDragStateChanged) onDragStateChanged (false); };

        addAndMakeVisible (slider);
        rebind (parameterId);
    }

    void Knob::rebind (const juce::String& parameterId)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, parameterId, slider);

        //  Double-click returns to the parameter's own default.
        if (auto* p = state.getParameter (parameterId))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        repaint();
    }

    void Knob::setAccent (juce::Colour accent, juce::Colour arcEnd)
    {
        accentColour = accent;
        slider.getProperties().set ("arcColour", (int) accent.getARGB());
        slider.getProperties().set ("arcColourEnd",
                                    (int) (arcEnd.isTransparent() ? accent : arcEnd).getARGB());
        repaint();
    }

    void Knob::setGlow (float alpha)  { slider.getProperties().set ("glowAlpha", alpha); }
    void Knob::setSparks (bool s)     { slider.getProperties().set ("sparks", s); }
    void Knob::setSpreadArcs (bool s) { slider.getProperties().set ("spreadArcs", s); }

    void Knob::setEnergy (float level01)
    {
        const float clamped = juce::jlimit (0.0f, 1.0f, level01);
        const float previous = (float) (double) slider.getProperties().getWithDefault ("energy", 0.0);

        if (std::abs (previous - clamped) > 0.02f)
        {
            slider.getProperties().set ("energy", clamped);
            if ((bool) slider.getProperties().getWithDefault ("sparks", false)
                || (bool) slider.getProperties().getWithDefault ("spreadArcs", false))
                slider.repaint();
        }
    }

    void Knob::setInteractionPreview (bool hover, bool drag)
    {
        previewDrag = drag;
        slider.getProperties().set ("forceHover", hover);
        slider.getProperties().set ("forceDrag", drag);
        slider.repaint();
        repaint();
    }

    void Knob::setSideCaptions (const juce::String& left, const juce::String& right)
    {
        leftCaption = left;
        rightCaption = right;
        repaint();
    }

    float Knob::captionHeight() const { return knobSize == Size::small ? 13.0f : 15.0f; }
    float Knob::valueHeight() const   { return knobSize == Size::small ? 15.0f : 16.0f; }

    void Knob::resized()
    {
        auto area = getLocalBounds();
        area.removeFromTop ((int) captionHeight());
        area.removeFromBottom ((int) valueHeight());

        //  Keep the rotary square and centred in what is left, but cap the
        //  diameter: a 1400x900 window should gain breathing room, not
        //  cartoon-sized knobs.
        const float scale = juce::jlimit (0.86f, 1.30f,
                                          (float) juce::jmax (getParentWidth(), 900) / 980.0f);
        const int cap = juce::roundToInt ((knobSize == Size::large  ? 116.0f
                                         : knobSize == Size::medium ? 86.0f
                                                                    : 58.0f) * scale);
        const int d = juce::jmin (area.getWidth(), area.getHeight(), cap);
        slider.setBounds (juce::Rectangle<int> (d, d).withCentre (area.getCentre()));
    }

    void Knob::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();

        //  Caption above.
        g.setFont (captionFont (knobSize == Size::small ? 10.5f : 11.5f));
        g.setColour (tokens::textSecondary);
        g.drawText (caption, area.removeFromTop (captionHeight()), juce::Justification::centred);

        //  Value row: flanking captions at the edges, value in the middle third.
        auto valueRow = getLocalBounds().toFloat().removeFromBottom (valueHeight());

        if (leftCaption.isNotEmpty() || rightCaption.isNotEmpty())
        {
            //  No extra tracking here: these sit in a tight row and must not
            //  truncate (BRIGHT is the widest of them).
            g.setFont (uiFont (9.5f));
            g.setColour (tokens::textMuted);
            const float sideW = valueRow.getWidth() * 0.36f;
            g.drawText (leftCaption, valueRow.withWidth (sideW),
                        juce::Justification::centredLeft);
            g.drawText (rightCaption, valueRow.withLeft (valueRow.getRight() - sideW),
                        juce::Justification::centredRight);
            valueRow = valueRow.withSizeKeepingCentre (valueRow.getWidth() * 0.30f,
                                                       valueRow.getHeight());
        }

        //  While dragging, the number is framed in the control's own colour.
        if (slider.isMouseButtonDown() || previewDrag)
        {
            const float fw = juce::jmin (valueRow.getWidth(), 72.0f);
            g.setColour (accentColour.withAlpha (0.8f));
            g.drawRoundedRectangle (valueRow.withSizeKeepingCentre (fw, valueRow.getHeight() - 1.0f),
                                    3.0f, 1.0f);
        }

        g.setFont (valueFont (11.5f));
        g.setColour (tokens::textPrimary);
        g.drawText (slider.getTextFromValue (slider.getValue()), valueRow,
                    juce::Justification::centred, false);
    }
}
