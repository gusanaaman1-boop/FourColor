// Top strip: spaced wordmark with the four band dots | preset browser
// (‹ name* ›) | A / B | undo | QUALITY | power.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Design.h"

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
        class UndoButton;
        class PresetField;

        void timerCallback() override;
        void showPresetMenu();

        FourColorProcessor& proc;

        std::unique_ptr<juce::ArrowButton> prevButton, nextButton;
        std::unique_ptr<PresetField> presetField;
        juce::TextButton aButton { "A" }, bButton { "B" };
        std::unique_ptr<UndoButton> undoButton;
        juce::ComboBox qualityBox;
        std::unique_ptr<PowerButton> powerButton;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

        juce::Rectangle<int> logoArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
