#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "Ui/BandStrip.h"
#include "Ui/CrossoverDisplay.h"
#include "Ui/GlobalBar.h"
#include "Ui/Theme.h"
#include "Ui/TopBar.h"

namespace fourcolor
{
    //  Layout: TopBar / CrossoverDisplay (flexible) / BandStrip / GlobalBar.
    //  Only the selected band's controls are ever shown - the display is the
    //  navigation.
    class FourColorEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit FourColorEditor (FourColorProcessor&);
        ~FourColorEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        FourColorProcessor& processor;

        ui::Laf laf;
        ui::TopBar topBar { processor };
        ui::CrossoverDisplay display { processor.apvts };
        ui::BandStrip bandStrip { processor.apvts };
        ui::GlobalBar globalBar { processor };
        juce::TooltipWindow tooltips { this, 600 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEditor)
    };
}
