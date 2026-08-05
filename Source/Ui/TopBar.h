// Top strip, mockup layout: spaced wordmark | ◀ preset name* ▶ | A / B | undo
// | QUALITY combo | power (bypass). The asterisk marks a modified preset; the
// preset name opens a category menu.

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
    class TopBar : public juce::Component, private juce::Timer
    {
    public:
        TopBar (FourColorProcessor& processor);
        ~TopBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class PowerButton;
        class IconButton;

        void timerCallback() override;
        void showPresetMenu();

        FourColorProcessor& proc;

        std::unique_ptr<juce::ArrowButton> prevButton, nextButton;
        juce::TextButton presetNameButton;
        juce::TextButton aButton { "A" }, bButton { "B" };
        std::unique_ptr<IconButton> undoButton;
        juce::ComboBox qualityBox;
        std::unique_ptr<PowerButton> powerButton;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

        juce::Rectangle<int> qualityLabelArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
