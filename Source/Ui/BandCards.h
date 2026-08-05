// The four band cards under the display: "BAND n" + live frequency range,
// S/M/B, and a segmented meter showing the band's REAL output level whose
// track doubles as a small draggable BAND LEVEL control (the round thumb).
// Clicking a card selects that band; the selected card is outlined in its
// colour, as in the mockup.

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
    class BandCards : public juce::Component, private juce::Timer
    {
    public:
        BandCards (FourColorProcessor& processor);

        std::function<void (int band)> onBandSelected;
        void setSelectedBand (int band);

        //  The display owns the live crossover values; we mirror them for the
        //  range captions.
        void setCutFrequencies (float f1, float f2, float f3);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        juce::Rectangle<int> cardBounds (int b) const;
        juce::Rectangle<float> meterBounds (int b) const;

        FourColorProcessor& proc;

        struct Card
        {
            juce::TextButton solo { "S" }, mute { "M" }, bypass { "B" };
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aSolo, aMute, aBypass;
            std::unique_ptr<juce::ParameterAttachment> levelAttachment;
            float levelDb = 0.0f;      // mirrored parameter value
            float meterPeak = 0.0f;    // displayed (decaying) band output peak
        };
        Card cards[numBands];

        float cutHz[3] = { 120.0f, 700.0f, 4500.0f };
        int selectedBand = 0;
        int draggingLevelBand = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandCards)
    };
}
