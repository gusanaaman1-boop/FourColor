// A labelled rotary bound to one APVTS parameter: caption above, value below,
// tooltip, double-click reset (JUCE built-in) and ctrl/cmd-drag fine control.
//
// Style is expressed through the Slider's property set, which the shared knob
// renderer in Theme.cpp reads - see its header comment.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Design.h"

namespace fourcolor::ui
{
    class Knob : public juce::Component
    {
    public:
        enum class Size { small, medium, large };

        Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId,
              const juce::String& caption, juce::Colour accent,
              Size size = Size::medium, const juce::String& tooltip = {});

        void resized() override;
        void paint (juce::Graphics&) override;

        //  Rebind to another parameter (used when the selected band changes).
        void rebind (const juce::String& parameterId);

        void setAccent (juce::Colour accent, juce::Colour arcEnd = {});
        void setGlow (float alpha);
        void setSparks (bool shouldShow);
        void setSpreadArcs (bool shouldShow);
        void setEnergy (float level01);

        //  Captions flanking the value row, e.g. DARK / BRIGHT.
        void setSideCaptions (const juce::String& left, const juce::String& right);

        //  QA affordance used by the screenshot tool: renders the real hover /
        //  drag styling without synthesising mouse events. It changes nothing
        //  else - the same drawing path serves a live interaction.
        void setInteractionPreview (bool hover, bool drag);

        //  Fired when the user starts/stops dragging, so the owning panel can
        //  dim its siblings.
        std::function<void (bool isDragging)> onDragStateChanged;

        juce::Slider& getSlider() noexcept { return slider; }
        bool isDragging() const noexcept   { return slider.isMouseButtonDown(); }

    private:
        float captionHeight() const;
        float valueHeight() const;

        juce::AudioProcessorValueTreeState& state;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        juce::String caption, leftCaption, rightCaption;
        juce::Colour accentColour;
        Size knobSize;
        bool previewDrag = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };
}
