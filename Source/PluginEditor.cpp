#include "PluginEditor.h"

namespace fourcolor
{
    FourColorEditor::FourColorEditor (FourColorProcessor& proc)
        : AudioProcessorEditor (proc), processor (proc)
    {
        addAndMakeVisible (generic);
        setResizable (true, true);
        setResizeLimits (600, 400, 1600, 1100);
        setSize (900, 560);
    }

    void FourColorEditor::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff14161a));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
        g.drawText ("FOUR COLOR  (development panel)", 0, 0, getWidth(), 28,
                    juce::Justification::centred);
    }

    void FourColorEditor::resized()
    {
        generic.setBounds (getLocalBounds().withTrimmedTop (28));
    }
}
