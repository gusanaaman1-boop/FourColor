#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "Ui/BandCards.h"
#include "Ui/BandStrip.h"
#include "Ui/CrossoverDisplay.h"
#include "Ui/GlobalBar.h"
#include "Ui/Theme.h"
#include "Ui/TopBar.h"

namespace fourcolor
{
    //  Layout (mockup): TopBar / spectrum display / band cards / selected-band
    //  panel / global bar.
    class FourColorEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit FourColorEditor (FourColorProcessor&);
        ~FourColorEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;

        FourColorProcessor& processor;

        ui::Laf laf;
        ui::TopBar topBar { processor };
        ui::CrossoverDisplay display { processor };
        ui::BandCards bandCards { processor };
        ui::BandStrip bandStrip { processor.apvts };
        ui::GlobalBar globalBar { processor };
        juce::TooltipWindow tooltips { this, 600 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEditor)
    };
}
