#include "BandStrip.h"

namespace fourcolor::ui
{
    BandStrip::BandStrip (juce::AudioProcessorValueTreeState& apvts)
        : state (apvts)
    {
        //  The COLOR rows are painted by this component (radio dot + name);
        //  the buttons are transparent hit targets on top of the rows.
        const char* names[] = { "WARM", "IRON", "BITE", "FUZZ" };
        for (int i = 0; i < 4; ++i)
        {
            auto& btn = colorButtons[i];
            btn.setButtonText (names[i]);
            btn.setClickingTogglesState (false);
            btn.setTooltip (juce::String ("Colour engine: ") + names[i]);
            btn.onClick = [this, i]
            {
                if (auto* p = state.getParameter (param::band (band, param::color)))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
            };
            btn.setAlpha (0.0f);          // invisible hit target; row drawn in paint()
            addAndMakeVisible (btn);
        }

        auto makeKnob = [&] (const char* suffix, const char* title, const char* tip)
        {
            return std::make_unique<Knob> (state, param::band (0, suffix), title,
                                           colour::band[0], tip);
        };

        drive   = makeKnob (param::drive, "DRIVE", "How hard this band is pushed into its colour");
        tone    = makeKnob (param::tone, "TONE", "Dark <-> bright, shaped around the band centre");
        space   = makeKnob (param::space, "SPACE / SPREAD", "Diffuses only what the saturation created");
        bandMix = makeKnob (param::bandMix, "MIX", "This band: processed vs clean (aligned)");

        for (auto* k : { drive.get(), tone.get(), space.get(), bandMix.get() })
            addAndMakeVisible (*k);

        behaviorSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        behaviorSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        behaviorSlider.setTooltip ("BODY: saturate the sustain. ATTACK: saturate the hit.");
        addAndMakeVisible (behaviorSlider);

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
                activeColor = juce::roundToInt (newValue);
                repaint();
            },
            nullptr);
        colorAttachment->sendInitialUpdate();
    }

    void BandStrip::setBand (int bandIndex)
    {
        band = juce::jlimit (0, numBands - 1, bandIndex);
        const auto accent = colour::band[band];

        drive  ->rebind (param::band (band, param::drive));
        tone   ->rebind (param::band (band, param::tone));
        space  ->rebind (param::band (band, param::space));
        bandMix->rebind (param::band (band, param::bandMix));

        for (auto* k : { drive.get(), tone.get(), space.get(), bandMix.get() })
            k->getSlider().setColour (juce::Slider::rotarySliderFillColourId, accent);

        behaviorAttachment.reset();
        behaviorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, param::band (band, param::behavior), behaviorSlider);
        behaviorSlider.setColour (juce::Slider::rotarySliderFillColourId, accent);

        rebindColorButtons();
        repaint();
    }

    void BandStrip::paint (juce::Graphics& g)
    {
        const auto accent = colour::band[band];

        auto bounds = getLocalBounds().toFloat();
        g.setColour (colour::panel.interpolatedWith (accent, 0.03f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (accent.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.2f);

        //  Section headers.
        g.setFont (labelFont (12.0f, false));
        g.setColour (colour::textDim);
        g.drawText ("COLOR", colorArea.withHeight (16), juce::Justification::centred);
        g.drawText ("BEHAVIOR", behaviorArea.withHeight (16), juce::Justification::centred);

        //  COLOR radio rows (buttons are invisible hit targets on top).
        const char* names[] = { "WARM", "IRON", "BITE", "FUZZ" };
        for (int i = 0; i < 4; ++i)
        {
            const auto r = colorButtons[i].getBounds().toFloat();
            const bool on = i == activeColor;

            if (on)
            {
                g.setColour (accent.withAlpha (0.85f));
                g.fillRoundedRectangle (r, 4.0f);
            }

            g.setColour (on ? juce::Colours::black.withAlpha (0.55f) : colour::panelLine.brighter (0.15f));
            g.drawEllipse (r.getX() + 8.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f, 1.2f);
            if (on)
            {
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.fillEllipse (r.getX() + 10.0f, r.getCentreY() - 1.5f, 3.0f, 3.0f);
            }

            g.setFont (labelFont (12.0f, on));
            g.setColour (on ? juce::Colour (0xff141519) : colour::textDim);
            g.drawText (names[i], r.toNearestInt().withTrimmedLeft (24),
                        juce::Justification::centredLeft);
        }

        //  BODY / 0 / ATTACK captions under the slider.
        const auto sliderBounds = behaviorSlider.getBounds();
        g.setFont (labelFont (11.0f));
        g.setColour (colour::textDim);
        g.drawText ("BODY", sliderBounds.getX() - 2, sliderBounds.getBottom() - 16, 60, 14,
                    juce::Justification::centredLeft);
        g.drawText ("ATTACK", sliderBounds.getRight() - 62, sliderBounds.getBottom() - 16, 60, 14,
                    juce::Justification::centredRight);

        //  DARK / BRIGHT captions for TONE.
        const auto toneBounds = tone->getBounds();
        g.setFont (labelFont (10.5f));
        g.drawText ("DARK", toneBounds.getX() - 12, toneBounds.getBottom() - 26, 44, 13,
                    juce::Justification::centredLeft);
        g.drawText ("BRIGHT", toneBounds.getRight() - 34, toneBounds.getBottom() - 26, 50, 13,
                    juce::Justification::centredLeft);
    }

    void BandStrip::resized()
    {
        auto area = getLocalBounds().reduced (16, 10);

        colorArea = area.removeFromLeft (150);
        auto list = colorArea.withTrimmedTop (18);
        for (int i = 0; i < 4; ++i)
        {
            colorButtons[i].setBounds (list.removeFromTop (26).reduced (0, 1));
            list.removeFromTop (2);
        }

        area.removeFromLeft (14);

        const int unit = area.getWidth() / 13;
        drive->setBounds (area.removeFromLeft (unit * 2));
        area.removeFromLeft (unit / 2);

        behaviorArea = area.removeFromLeft (unit * 4);
        behaviorSlider.setBounds (behaviorArea.withTrimmedTop (22).withTrimmedBottom (16)
                                              .reduced (10, 0));

        area.removeFromLeft (unit / 2);
        tone->setBounds (area.removeFromLeft (unit * 2));
        space->setBounds (area.removeFromLeft (unit * 2));
        bandMix->setBounds (area);
    }
}
