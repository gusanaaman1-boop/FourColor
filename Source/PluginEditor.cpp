#include "PluginEditor.h"

namespace fourcolor
{
    FourColorEditor::FourColorEditor (FourColorProcessor& proc)
        : AudioProcessorEditor (proc), processor (proc)
    {
        setLookAndFeel (&laf);

        addAndMakeVisible (topBar);
        addAndMakeVisible (display);
        addAndMakeVisible (bandStrip);
        addAndMakeVisible (globalBar);

        //  Selection: display drives the strip; both restore from state.
        const int savedBand = (int) processor.apvts.state.getProperty ("selectedBand", 0);
        display.setSelectedBand (savedBand);
        bandStrip.setBand (savedBand);

        display.onBandSelected = [this] (int band)
        {
            bandStrip.setBand (band);
            processor.apvts.state.setProperty ("selectedBand", band, nullptr);
        };

        setResizable (true, true);
        setResizeLimits (900, 560, 1800, 1150);

        const int w = (int) processor.apvts.state.getProperty ("editorWidth", 980);
        const int h = (int) processor.apvts.state.getProperty ("editorHeight", 620);
        setSize (juce::jlimit (900, 1800, w), juce::jlimit (560, 1150, h));
    }

    FourColorEditor::~FourColorEditor()
    {
        setLookAndFeel (nullptr);
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
        topBar.setBounds (area.removeFromTop (36));
        globalBar.setBounds (area.removeFromBottom (104));
        bandStrip.setBounds (area.removeFromBottom (150));
        display.setBounds (area.reduced (4, 2));
    }
}
