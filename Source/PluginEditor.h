#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace fourcolor
{
    //  Phase 1 placeholder: a titled generic parameter panel, so every parameter
    //  is reachable and testable before the real UI lands in Phase 8. Nothing
    //  here pretends to be finished.
    class FourColorEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit FourColorEditor (FourColorProcessor&);
        ~FourColorEditor() override = default;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        FourColorProcessor& processor;
        juce::GenericAudioProcessorEditor generic { processor };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEditor)
    };
}
