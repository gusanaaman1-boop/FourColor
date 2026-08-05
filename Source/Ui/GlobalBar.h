// The global strip, mockup layout: [meter] INPUT | GLOBAL DRIVE | GLOBAL TONE
// + round glowing AUTO LEVEL | MIX | OUTPUT [meter], with hairline separators.
// Meters are real block peaks from the processor's atomics.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Knob.h"

namespace fourcolor
{
    class FourColorProcessor;
}

namespace fourcolor::ui
{
    class GlobalBar : public juce::Component, private juce::Timer
    {
    public:
        GlobalBar (FourColorProcessor& processor);
        ~GlobalBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class RoundToggle;

        void timerCallback() override;

        FourColorProcessor& proc;

        std::unique_ptr<Knob> input, globalDrive, globalTone, mix, output;
        std::unique_ptr<RoundToggle> autoLevelButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoLevelAttachment;

        float displayedIn = 0.0f, displayedOut = 0.0f;
        juce::Rectangle<float> inMeter, outMeter;
        std::vector<int> separatorX;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalBar)
    };
}
