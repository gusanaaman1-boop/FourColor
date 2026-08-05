// Top strip: product name, preset selector (factory programs), A/B compare,
// Quality and global Bypass. Undo/Redo is deliberately absent at 70%: the
// APVTS is not wired through an UndoManager yet, and a dead button is worse
// than no button.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Theme.h"

namespace fourcolor
{
    class FourColorProcessor;
}

namespace fourcolor::ui
{
    class TopBar : public juce::Component
    {
    public:
        TopBar (FourColorProcessor& processor);

        void refreshPresetList();
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        FourColorProcessor& proc;

        juce::ComboBox presetBox;
        juce::TextButton abButton { "A" };
        juce::TextButton copyButton { "COPY" };
        juce::ComboBox qualityBox;
        juce::TextButton bypassButton { "BYPASS" };

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
