// The global strip: [L/R meter] INPUT | GLOBAL DRIVE | AUTO LEVEL | GLOBAL
// TONE | MIX | OUTPUT [L/R meter], with faint vertical separators between the
// groups. Meters read real block peaks and never take the band colours.

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
        void drawStereoMeter (juce::Graphics&, juce::Rectangle<float>,
                              const float* levels, const float* peaks) const;

        FourColorProcessor& proc;

        std::unique_ptr<Knob> input, globalDrive, globalTone, mix, output;
        std::unique_ptr<RoundToggle> autoLevelButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoLevelAttachment;

        float inLevel[2] { 0.0f, 0.0f }, outLevel[2] { 0.0f, 0.0f };
        float inPeakHold[2] { 0.0f, 0.0f }, outPeakHold[2] { 0.0f, 0.0f };
        int inPeakAge[2] { 0, 0 }, outPeakAge[2] { 0, 0 };

        juce::Rectangle<float> inMeter, outMeter;
        std::vector<int> separatorX;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalBar)
    };
}
