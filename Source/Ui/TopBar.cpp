#include "TopBar.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    TopBar::TopBar (FourColorProcessor& processor)
        : proc (processor)
    {
        refreshPresetList();
        presetBox.setTooltip ("Factory presets");
        presetBox.onChange = [this]
        {
            const int index = presetBox.getSelectedItemIndex();
            if (index >= 0 && index != proc.getCurrentProgram())
                proc.setCurrentProgram (index);
        };
        addAndMakeVisible (presetBox);

        abButton.setTooltip ("Toggle between the A and B states");
        abButton.onClick = [this]
        {
            proc.toggleAB();
            abButton.setButtonText (proc.getABIndex() == 0 ? "A" : "B");
        };
        addAndMakeVisible (abButton);

        copyButton.setTooltip ("Copy the current state to the other slot");
        copyButton.onClick = [this] { proc.copyABToOther(); };
        addAndMakeVisible (copyButton);

        qualityBox.addItemList ({ "Draft 1x", "Normal 2x", "High 4x", "Ultra 8x" }, 1);
        qualityBox.setTooltip ("Oversampling of the colour engines");
        addAndMakeVisible (qualityBox);
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, param::quality, qualityBox);

        bypassButton.setClickingTogglesState (true);
        bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd05545).withAlpha (0.8f));
        bypassButton.setTooltip ("Latency-aligned global bypass");
        addAndMakeVisible (bypassButton);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, param::bypassed, bypassButton);
    }

    void TopBar::refreshPresetList()
    {
        presetBox.clear (juce::dontSendNotification);
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            presetBox.addItem (proc.getProgramName (i), i + 1);
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    }

    void TopBar::paint (juce::Graphics& g)
    {
        g.setColour (colour::background);
        g.fillAll();

        g.setFont (uiFont (17.0f, true));
        g.setColour (colour::text);
        g.drawText ("FOUR COLOR", 14, 0, 200, getHeight(), juce::Justification::centredLeft);

        //  The four colour chips of the wordmark.
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (colour::band[i]);
            g.fillRoundedRectangle (128.0f + i * 12.0f, getHeight() * 0.5f - 3.0f, 8.0f, 6.0f, 2.0f);
        }
    }

    void TopBar::resized()
    {
        auto area = getLocalBounds().reduced (8, 5);
        area.removeFromLeft (190);   // wordmark

        presetBox.setBounds (area.removeFromLeft (juce::jmin (240, area.getWidth() / 3)));
        area.removeFromLeft (8);
        abButton.setBounds (area.removeFromLeft (30));
        area.removeFromLeft (4);
        copyButton.setBounds (area.removeFromLeft (52));

        bypassButton.setBounds (area.removeFromRight (72));
        area.removeFromRight (8);
        qualityBox.setBounds (area.removeFromRight (110));
    }
}
