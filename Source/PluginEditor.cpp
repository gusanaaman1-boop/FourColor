#include "PluginEditor.h"

namespace fourcolor
{
    FourColorEditor::FourColorEditor (FourColorProcessor& proc)
        : AudioProcessorEditor (proc), processor (proc)
    {
        setLookAndFeel (&laf);

        addAndMakeVisible (topBar);
        addAndMakeVisible (display);
        addAndMakeVisible (bandCards);
        addAndMakeVisible (bandStrip);
        addAndMakeVisible (globalBar);

        //  Selection: display and cards both drive the strip; all restore
        //  from state.
        const int savedBand = (int) processor.apvts.state.getProperty ("selectedBand", 0);
        display.setSelectedBand (savedBand);
        bandCards.setSelectedBand (savedBand);
        bandStrip.setBand (savedBand);

        auto selectBand = [this] (int band)
        {
            display.setSelectedBand (band);
            bandCards.setSelectedBand (band);
            bandStrip.setBand (band);
            processor.apvts.state.setProperty ("selectedBand", band, nullptr);
        };
        display.onBandSelected = selectBand;
        bandCards.onBandSelected = selectBand;

        setResizable (true, true);
        setResizeLimits (980, 620, 1900, 1200);

        const int w = (int) processor.apvts.state.getProperty ("editorWidth", 1180);
        const int h = (int) processor.apvts.state.getProperty ("editorHeight", 740);
        setSize (juce::jlimit (980, 1900, w), juce::jlimit (620, 1200, h));

        startTimerHz (10);
    }

    FourColorEditor::~FourColorEditor()
    {
        setLookAndFeel (nullptr);
    }

    void FourColorEditor::timerCallback()
    {
        //  Undo granularity: while the mouse is up, close the running
        //  transaction so each drag/click undoes as one step.
        if (! juce::Desktop::getInstance().getMainMouseSource().isDragging()
            && processor.undoManager.getNumActionsInCurrentTransaction() > 0)
            processor.undoManager.beginNewTransaction();

        //  Keep the band cards' frequency captions live.
        bandCards.setCutFrequencies (display.getCutHz (0), display.getCutHz (1),
                                     display.getCutHz (2));
    }

    void FourColorEditor::paint (juce::Graphics& g)
    {
        g.fillAll (ui::colour::background);
    }

    void FourColorEditor::resized()
    {
        processor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
        processor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);

        auto area = getLocalBounds();
        topBar.setBounds (area.removeFromTop (46));

        auto content = area.reduced (10, 8);
        globalBar.setBounds (content.removeFromBottom (118));
        content.removeFromBottom (8);
        bandStrip.setBounds (content.removeFromBottom (168));
        content.removeFromBottom (8);
        bandCards.setBounds (content.removeFromBottom (62));
        content.removeFromBottom (8);
        display.setBounds (content);
    }
}
