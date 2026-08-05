#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "Ui/Analyzer.h"
#include "Ui/BandCards.h"
#include "Ui/BandStrip.h"
#include "Ui/GlobalBar.h"
#include "Ui/Theme.h"
#include "Ui/TopBar.h"

namespace fourcolor
{
    //  Layout follows the reference proportions (Design.h, metric::*):
    //  top bar 0-8% | analyzer 8-37% | band cards 38-52.5% |
    //  selected band 53.5-80.5% | global strip 81.5-100%.
    class FourColorEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit FourColorEditor (FourColorProcessor&);
        ~FourColorEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        //  QA affordance for the screenshot tool: render the real hover/drag
        //  styling of one selected-band control without synthesising events.
        ui::BandStrip& getBandStrip() noexcept { return bandStrip; }
        ui::Analyzer& getAnalyzer() noexcept   { return analyzer; }

    private:
        void timerCallback() override;
        void selectBand (int band);

        FourColorProcessor& processor;

        ui::Laf laf;
        ui::TopBar topBar { processor };
        ui::Analyzer analyzer { processor };
        ui::BandCards bandCards { processor };
        ui::BandStrip bandStrip { processor.apvts };
        ui::GlobalBar globalBar { processor };
        juce::TooltipWindow tooltips { this, 550 };

        int selectedBand = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEditor)
    };
}
