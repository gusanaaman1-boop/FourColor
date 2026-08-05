#include "GlobalBar.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    GlobalBar::GlobalBar (FourColorProcessor& processor)
        : proc (processor)
    {
        auto& apvts = proc.apvts;
        const auto neutral = colour::accent.withAlpha (0.8f);

        input       = std::make_unique<Knob> (apvts, param::input, "INPUT", neutral, "Input trim");
        globalDrive = std::make_unique<Knob> (apvts, param::globalDrive, "GLOBAL DRIVE", neutral,
                                              "Scales all four band drives around their settings");
        globalTone  = std::make_unique<Knob> (apvts, param::globalTone, "GLOBAL TONE", neutral,
                                              "Tilt around 800 Hz");
        mix         = std::make_unique<Knob> (apvts, param::mix, "MIX", neutral,
                                              "Latency-aligned dry/wet");
        output      = std::make_unique<Knob> (apvts, param::output, "OUTPUT", neutral, "Output trim");

        for (auto* k : { input.get(), globalDrive.get(), globalTone.get(), mix.get(), output.get() })
            addAndMakeVisible (*k);

        autoLevelButton.setClickingTogglesState (true);
        autoLevelButton.setColour (juce::TextButton::buttonOnColourId, neutral.withAlpha (0.55f));
        autoLevelButton.setTooltip ("Slow internal loudness match (max +/-12 dB)");
        addAndMakeVisible (autoLevelButton);
        autoLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, param::autoLevel, autoLevelButton);

        startTimerHz (30);
    }

    void GlobalBar::timerCallback()
    {
        //  Peak meters with UI-side decay; the atomics hold block peaks.
        const float in  = proc.readAndResetInputPeak();
        const float out = proc.readAndResetOutputPeak();

        displayedIn  = juce::jmax (in, displayedIn * 0.85f);
        displayedOut = juce::jmax (out, displayedOut * 0.85f);
        repaint (meterArea);
    }

    void GlobalBar::paint (juce::Graphics& g)
    {
        g.setColour (colour::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

        //  IN / OUT meters, -60..0 dB.
        auto drawMeter = [&g] (juce::Rectangle<int> r, float peak, const char* label)
        {
            g.setColour (colour::background);
            g.fillRoundedRectangle (r.toFloat(), 2.0f);

            const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
            const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

            auto fill = r.toFloat().withTrimmedTop ((1.0f - t) * (float) r.getHeight());
            g.setColour (db > -3.0f ? juce::Colour (0xffd05545)
                                    : db > -12.0f ? juce::Colour (0xffcfa03f)
                                                  : juce::Colour (0xff56a89a));
            g.fillRoundedRectangle (fill, 2.0f);

            g.setColour (colour::textDim);
            g.setFont (uiFont (9.0f));
            g.drawText (label, r.withY (r.getBottom() + 1).withHeight (10).expanded (8, 0),
                        juce::Justification::centred);
        };

        if (! meterArea.isEmpty())
        {
            auto meters = meterArea.reduced (0, 12);
            auto inR  = meters.removeFromLeft (10);
            meters.removeFromLeft (6);
            auto outR = meters.removeFromLeft (10);
            drawMeter (inR, displayedIn, "IN");
            drawMeter (outR, displayedOut, "OUT");
        }
    }

    void GlobalBar::resized()
    {
        auto area = getLocalBounds().reduced (12, 6);

        meterArea = area.removeFromRight (40);
        area.removeFromRight (8);

        const int unit = juce::jmax (60, area.getWidth() / 6);
        input      ->setBounds (area.removeFromLeft (unit));
        globalDrive->setBounds (area.removeFromLeft (unit));
        globalTone ->setBounds (area.removeFromLeft (unit));

        auto alArea = area.removeFromLeft (unit);
        autoLevelButton.setBounds (alArea.withSizeKeepingCentre (juce::jmin (unit - 8, 96), 24));

        mix   ->setBounds (area.removeFromLeft (unit));
        output->setBounds (area);
    }
}
