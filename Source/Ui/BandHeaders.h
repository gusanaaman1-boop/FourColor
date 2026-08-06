// The band header strip that sits directly above the analyzer plot: one header
// per band, aligned to that band's frequency range, carrying
//
//     LOW  ·  WARM  ·  55%                    (P) (S) (M)
//
// The three buttons are the SAME parameters the old band cards used - no new
// Parameter ID exists for any of this:
//
//     Power  ->  bN_bypass   (Power ON means bypass = false)
//     S      ->  bN_solo
//     M      ->  bN_mute
//
// Power is a VISUAL rename of the old B button, not a new control. That keeps
// host automation, state recall, every existing preset and the clean-bypass
// audio path that has already been proved by the null test.

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
    class BandHeaders : public juce::Component, private juce::Timer
    {
    public:
        BandHeaders (FourColorProcessor& processor);
        ~BandHeaders() override;

        std::function<void (int band)> onBandSelected;

        void setSelectedBand (int band);

        //  The analyzer owns the live crossover values; headers follow them so
        //  each one sits over its own frequency range.
        void setCutFrequencies (float f1, float f2, float f3);

        //  0 = fully off, 1 = fully lit. The analyzer reads the same curve so
        //  the header and the band's colour fade together.
        float getPowerFade (int band) const noexcept { return bands[band].powerFade; }

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        juce::Rectangle<int> headerBounds (int band) const;

        //  A flat round icon button. Deliberately not a TextButton: these are
        //  22 px targets in a 26 px strip and the stock look does not survive
        //  that, but the hit area still has to reach the 24 px minimum.
        class IconToggle;

        FourColorProcessor& proc;

        struct Band
        {
            std::unique_ptr<IconToggle> power, solo, mute;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aPower, aSolo, aMute;
            std::unique_ptr<juce::ParameterAttachment> colorAttachment, driveAttachment;

            int colorIndex = 0;
            float drivePercent = 25.0f;

            //  Power is stored inverted from the parameter: the parameter is
            //  "bypass", the control is "power".
            bool powered = true;
            float powerFade = 1.0f;
        };
        Band bands[numBands];

        float cutHz[3] = { 120.0f, 700.0f, 4500.0f };
        int selectedBand = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandHeaders)
    };
}
