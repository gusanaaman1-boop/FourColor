#include "BandStrip.h"

namespace fourcolor::ui
{
    BandStrip::BandStrip (juce::AudioProcessorValueTreeState& apvts)
        : state (apvts)
    {
        header.setFont (uiFont (13.0f, true));
        header.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (header);

        const char* names[] = { "WARM", "IRON", "BITE", "FUZZ" };
        for (int i = 0; i < 4; ++i)
        {
            auto& b = colorButtons[i];
            b.setButtonText (names[i]);
            b.setClickingTogglesState (false);
            b.setTooltip (juce::String ("Colour engine: ") + names[i]);
            b.onClick = [this, i]
            {
                if (auto* p = state.getParameter (param::band (band, param::color)))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
            };
            addAndMakeVisible (b);
        }

        auto makeKnob = [&] (const char* suffix, const char* title, const char* tip)
        {
            return std::make_unique<Knob> (state, param::band (0, suffix), title,
                                           colour::band[0], tip);
        };

        drive    = makeKnob (param::drive, "DRIVE", "How hard this band is pushed into its colour");
        behavior = makeKnob (param::behavior, "BEHAVIOR", "BODY: saturate the sustain. ATTACK: saturate the hit.");
        tone     = makeKnob (param::tone, "TONE", "Dark <-> bright, shaped around the band centre");
        space    = makeKnob (param::space, "SPACE", "Diffuses only what the saturation created");
        bandMix  = makeKnob (param::bandMix, "MIX", "This band: processed vs clean (aligned)");
        level    = makeKnob (param::level, "LEVEL", "Band output level");

        for (auto* k : { drive.get(), behavior.get(), tone.get(), space.get(),
                         bandMix.get(), level.get() })
            addAndMakeVisible (*k);

        setBand (0);
    }

    void BandStrip::rebindColorButtons()
    {
        auto* p = state.getParameter (param::band (band, param::color));
        jassert (p != nullptr);

        colorAttachment = std::make_unique<juce::ParameterAttachment> (
            *p,
            [this] (float newValue)
            {
                const int active = juce::roundToInt (newValue);
                for (int i = 0; i < 4; ++i)
                {
                    colorButtons[i].setToggleState (i == active, juce::dontSendNotification);
                    colorButtons[i].setColour (juce::TextButton::buttonOnColourId,
                                               colour::band[band].withAlpha (0.85f));
                }
            },
            nullptr);
        colorAttachment->sendInitialUpdate();
    }

    void BandStrip::setBand (int bandIndex)
    {
        band = juce::jlimit (0, numBands - 1, bandIndex);

        header.setText (juce::String (bandName (band)) + "  BAND", juce::dontSendNotification);
        header.setColour (juce::Label::textColourId, colour::band[band]);

        drive   ->rebind (param::band (band, param::drive));
        behavior->rebind (param::band (band, param::behavior));
        tone    ->rebind (param::band (band, param::tone));
        space   ->rebind (param::band (band, param::space));
        bandMix ->rebind (param::band (band, param::bandMix));
        level   ->rebind (param::band (band, param::level));

        for (auto* k : { drive.get(), behavior.get(), tone.get(), space.get(),
                         bandMix.get(), level.get() })
            k->getSlider().setColour (juce::Slider::rotarySliderFillColourId, colour::band[band]);

        rebindColorButtons();
        repaint();
    }

    void BandStrip::paint (juce::Graphics& g)
    {
        g.setColour (colour::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
        g.setColour (colour::band[band].withAlpha (0.5f));
        g.fillRect (2.0f, 4.0f, 3.0f, (float) getHeight() - 8.0f);
    }

    void BandStrip::resized()
    {
        auto area = getLocalBounds().reduced (12, 6);

        auto left = area.removeFromLeft (150);
        header.setBounds (left.removeFromTop (20));
        left.removeFromTop (4);
        for (int i = 0; i < 4; ++i)
        {
            colorButtons[i].setBounds (left.removeFromTop (24).reduced (0, 1));
            left.removeFromTop (2);
        }

        area.removeFromLeft (10);

        //  DRIVE gets ~1.6x the width of the others.
        const int unit = area.getWidth() / 13;
        drive   ->setBounds (area.removeFromLeft (unit * 3));
        behavior->setBounds (area.removeFromLeft (unit * 2));
        tone    ->setBounds (area.removeFromLeft (unit * 2));
        space   ->setBounds (area.removeFromLeft (unit * 2));
        bandMix ->setBounds (area.removeFromLeft (unit * 2));
        level   ->setBounds (area);
    }
}
