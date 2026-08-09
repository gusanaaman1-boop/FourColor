#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "Ui/Analyzer.h"
#include "Ui/BandHeaders.h"
#include "Ui/BandStrip.h"
#include "Ui/GlobalBar.h"
#include "Ui/MeterColumn.h"
#include "Ui/Theme.h"
#include "Ui/TopBar.h"

namespace fourcolor
{
    //  Layout (Design.h, metric::*): top bar 0-8% | band headers 8-12.8% |
    //  analyzer 13-52.5% | selected band 53.5-80.5% | global strip 81.5-100%.
    //
    //  The band cards are gone. Their S/M/B live in the headers now, directly
    //  over the frequency range each one controls, and the analyzer took the
    //  space back.
    class FourColorEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit FourColorEditor (FourColorProcessor&);
        ~FourColorEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDown (const juce::MouseEvent&) override;

        //  QA affordance for the screenshot tool: render the real hover/drag
        //  styling of one selected-band control without synthesising events.
        ui::BandStrip& getBandStrip() noexcept { return bandStrip; }
        ui::Analyzer& getAnalyzer() noexcept   { return analyzer; }

        //  Diagnostics only: how many times the editor's own background has
        //  repainted. Compared against the analyzer's count, this is what says
        //  whether a steady playback frame redraws the analyzer alone or drags
        //  the whole window with it.
        std::atomic<int> backgroundPaints { 0 };

        //  ...and how much of the window each of those paints actually covered.
        //  A child that is not opaque makes its parent paint the background
        //  behind it, so a per-frame CALL to the editor's paint() is expected
        //  and says nothing. The clipped AREA is what distinguishes "repaint
        //  the strip behind the analyzer" from "repaint the whole window".
        std::atomic<long long> backgroundPaintArea { 0 };

    private:
        void timerCallback() override;
        void selectBand (int band);

        FourColorProcessor& proc;

        ui::Laf laf;
        ui::TopBar topBar { proc };
        ui::Analyzer analyzer { proc };
        ui::BandHeaders bandHeaders { proc };
        ui::BandStrip bandStrip { proc.apvts };
        ui::MeterColumn inputMeter { proc, ui::MeterColumn::Side::input };
        ui::MeterColumn outputMeter { proc, ui::MeterColumn::Side::output };
        ui::GlobalBar globalBar { proc };
        juce::TooltipWindow tooltips { this, 550 };

        int selectedBand = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEditor)
    };
}
