// The global strip: Input, Global Drive, Global Tone, Auto Level, Mix, Output,
// plus real input/output peak meters fed from the processor's atomics.

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

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;

        FourColorProcessor& proc;

        std::unique_ptr<Knob> input, globalDrive, globalTone, mix, output;
        juce::TextButton autoLevelButton { "AUTO LEVEL" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoLevelAttachment;

        float displayedIn = 0.0f, displayedOut = 0.0f;
        juce::Rectangle<int> meterArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalBar)
    };
}
