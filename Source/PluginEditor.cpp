#include "PluginEditor.h"

namespace fourcolor
{
    FourColorEditor::FourColorEditor (FourColorProcessor& proc)
        : AudioProcessorEditor (proc), processor (proc)
    {
        setLookAndFeel (&laf);

        addAndMakeVisible (topBar);
        addAndMakeVisible (analyzer);
        addAndMakeVisible (bandCards);
        addAndMakeVisible (bandStrip);
        addAndMakeVisible (globalBar);

        selectedBand = juce::jlimit (0, numBands - 1,
                                     (int) processor.apvts.state.getProperty ("selectedBand", 0));
        analyzer.setSelectedBand (selectedBand);
        bandCards.setSelectedBand (selectedBand);
        bandStrip.setBand (selectedBand);

        analyzer.onBandSelected  = [this] (int b) { selectBand (b); };
        bandCards.onBandSelected = [this] (int b) { selectBand (b); };

        //  While a control is being dragged, the analyzer emphasises the region
        //  it affects. The kind codes match Analyzer::Emphasis.
        bandStrip.onEmphasisChanged = [this] (int kind, int band)
        {
            analyzer.setEmphasis ((ui::Analyzer::Emphasis) kind, band);
        };

        setResizable (true, true);
        setResizeLimits (900, 560, 1900, 1200);

        const int w = (int) processor.apvts.state.getProperty ("editorWidth", 980);
        const int h = (int) processor.apvts.state.getProperty ("editorHeight", 620);
        setSize (juce::jlimit (900, 1900, w), juce::jlimit (560, 1200, h));

        startTimerHz (12);
    }

    FourColorEditor::~FourColorEditor()
    {
        setLookAndFeel (nullptr);
    }

    void FourColorEditor::selectBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        analyzer.setSelectedBand (selectedBand);
        bandCards.setSelectedBand (selectedBand);
        bandStrip.setBand (selectedBand);
        processor.apvts.state.setProperty ("selectedBand", selectedBand, nullptr);
    }

    void FourColorEditor::timerCallback()
    {
        //  Undo granularity: close the running transaction whenever the mouse
        //  is up, so one drag or click undoes as a single step.
        if (! juce::Desktop::getInstance().getMainMouseSource().isDragging()
            && processor.undoManager.getNumActionsInCurrentTransaction() > 0)
            processor.undoManager.beginNewTransaction();

        //  Feed the selected band's real output level to the DRIVE sparks and
        //  the SPACE arcs, so they rest when there is no audio.
        const float peak = processor.getEngine().getBand (selectedBand).readAndResetOutputPeak();
        bandStrip.setEnergy (juce::jlimit (0.0f, 1.0f, peak * 1.6f));
    }

    void FourColorEditor::paint (juce::Graphics& g)
    {
        //  Ground: a very slight vertical lift towards the top bar.
        juce::ColourGradient bg (ui::tokens::backgroundTop, 0.0f, 0.0f,
                                 ui::tokens::backgroundDeep, 0.0f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    void FourColorEditor::resized()
    {
        processor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
        processor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);

        const float h = (float) getHeight();
        const float w = (float) getWidth();
        const int margin = juce::roundToInt (ui::metric::sideMargin * (w / 980.0f));

        using namespace ui::metric;
        auto rowBetween = [this, h, margin] (float top, float bottom)
        {
            return juce::Rectangle<int> (margin, juce::roundToInt (h * top),
                                         getWidth() - margin * 2,
                                         juce::roundToInt (h * (bottom - top)));
        };

        topBar.setBounds (0, 0, getWidth(), juce::roundToInt (h * topBarBottom));
        analyzer.setBounds (rowBetween (analyzerTop, analyzerBottom));
        bandCards.setBounds (rowBetween (cardsTop, cardsBottom));
        bandStrip.setBounds (rowBetween (panelTop, panelBottom));
        globalBar.setBounds (rowBetween (globalTop, 1.0f).withTrimmedBottom (margin / 2));
    }
}
