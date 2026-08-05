// The selected band's panel. Left to right, with the width proportions from
// the specification: COLOR list 15% | DRIVE 17% | BEHAVIOR 31% | TONE 13% |
// SPACE / SPREAD 13% | divider | MIX + LEVEL 9%.
//
// The hierarchy is deliberate: DRIVE is the largest control, BEHAVIOR owns the
// centre, SPACE and TONE are medium, MIX and LEVEL are small and set apart.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Knob.h"

namespace fourcolor::ui
{
    class BandStrip : public juce::Component
    {
    public:
        BandStrip (juce::AudioProcessorValueTreeState& apvts);

        void setBand (int bandIndex);
        int getBand() const noexcept { return band; }

        //  Live output level of the selected band, for the DRIVE sparks.
        void setEnergy (float level01);

        //  QA affordance for the screenshot tool (see Knob::setInteractionPreview).
        enum class Control { none, drive, behavior, tone, space };
        void setInteractionPreview (Control control, bool hover, bool drag);

        //  Raised while the user drags a control here, so the analyzer can
        //  emphasise the matching region.
        std::function<void (int emphasisKind, int band)> onEmphasisChanged;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        void rebindColorButtons();
        void setDragging (Knob* which, bool isDragging);
        juce::Rectangle<float> colorRowBounds (int index) const;

        juce::AudioProcessorValueTreeState& state;
        int band = 0;
        int activeColor = 0;
        int hoverColorRow = -1;

        std::unique_ptr<juce::ParameterAttachment> colorAttachment;
        std::unique_ptr<Knob> drive, tone, space, bandMix, level;

        juce::Slider behaviorSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> behaviorAttachment;

        juce::Rectangle<int> colorArea, behaviorArea, rightGroup;
        int dividerX = 0;

        Knob* draggingKnob = nullptr;
        bool behaviorDragging = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
    };
}
