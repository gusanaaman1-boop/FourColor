// The four band cards: name, engine name, small DRIVE and LEVEL knobs, and
// S / M / B. Clicking anywhere on a card selects that band.
//
// Selection is not signalled by colour alone: the selected card also gains a
// border, a vertical gradient wash and a small outer glow.

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
    class BandCards : public juce::Component, private juce::Timer
    {
    public:
        BandCards (FourColorProcessor& processor);
        ~BandCards() override;

        std::function<void (int band)> onBandSelected;
        void setSelectedBand (int band);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        juce::Rectangle<int> cardBounds (int b) const;
        int cardAt (juce::Point<int> p) const;

        FourColorProcessor& proc;

        struct Card
        {
            std::unique_ptr<Knob> drive, level;
            juce::TextButton solo { "S" }, mute { "M" }, bypass { "B" };
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aSolo, aMute, aBypass;
            std::unique_ptr<juce::ParameterAttachment> colorAttachment;
            int colorIndex = 0;
        };
        Card cards[numBands];

        int selectedBand = 0;
        int hoverCard = -1;
        int pressedCard = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandCards)
    };
}
