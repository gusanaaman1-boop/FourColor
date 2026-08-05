// The central view: four coloured band regions on a log frequency axis, the
// REAL LR4 magnitude curves of the current crossover (computed analytically
// from the same cutoffs the DSP uses - nothing is faked), three draggable
// crossover handles, band selection by click, and compact per-band info with
// S/M/B directly on each region.
//
// An FFT analyzer is deliberately absent at the 70% stage: the display shows
// the crossover's true response, not a fake spectrum.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Theme.h"

namespace fourcolor::ui
{
    class CrossoverDisplay : public juce::Component, private juce::Timer
    {
    public:
        CrossoverDisplay (juce::AudioProcessorValueTreeState& apvts);
        ~CrossoverDisplay() override;

        std::function<void (int band)> onBandSelected;

        void setSelectedBand (int band);
        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        float xForFrequency (float hz) const;
        float frequencyForX (float x) const;
        int handleAt (juce::Point<float> pos) const;
        juce::Rectangle<float> plotArea() const;
        float currentCut (int i) const;

        juce::AudioProcessorValueTreeState& state;
        std::unique_ptr<juce::ParameterAttachment> cutAttachments[3];
        float cutValues[3] = { 120.0f, 700.0f, 4500.0f };

        //  Small S/M/B buttons per band, attached straight to the parameters.
        struct BandButtons
        {
            juce::TextButton solo { "S" }, mute { "M" }, bypass { "B" };
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aSolo, aMute, aBypass;
        };
        BandButtons bandButtons[numBands];

        int selectedBand = 0;
        int draggingHandle = -1;
        int hoverHandle = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrossoverDisplay)
    };
}
