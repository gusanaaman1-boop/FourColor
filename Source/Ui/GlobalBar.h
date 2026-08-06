// The global strip, reduced to the three controls that belong on the front
// panel: AUTO LEVEL, MASTER MIX and OUTPUT.
//
// Input Trim moved beside the input meter and Output Trim beside the output
// meter, where each sits with the reading it moves. GLOBAL DRIVE and GLOBAL
// TONE moved into the MASTER drawer in the top bar - they are set once and
// left, and they were taking the same visual weight as controls that are
// touched constantly.
//
// No parameter was removed and no ID changed; only where they are drawn.

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
    class GlobalBar : public juce::Component
    {
    public:
        GlobalBar (FourColorProcessor& processor);
        ~GlobalBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class RoundToggle;


        FourColorProcessor& proc;

        std::unique_ptr<Knob> mix, output;
        std::unique_ptr<RoundToggle> autoLevelButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoLevelAttachment;

        std::vector<int> separatorX;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalBar)
    };
}
