// FOUR COLOR visual language: dark, flat, quiet. Each band owns one accent
// colour - the product is literally named after them.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace fourcolor::ui
{
    namespace colour
    {
        const juce::Colour background { 0xff121419 };
        const juce::Colour panel      { 0xff1a1d24 };
        const juce::Colour panelLine  { 0xff262a33 };
        const juce::Colour text       { 0xffe6e7ea };
        const juce::Colour textDim    { 0xff8a8f9a };
        const juce::Colour accent     { 0xffd8dade };

        //  The four band colours: LOW, LOW MID, HIGH MID, HIGH.
        const juce::Colour band[4] = {
            juce::Colour (0xffc96a4a),   // LOW      terracotta
            juce::Colour (0xffcfa03f),   // LOW MID  amber
            juce::Colour (0xff56a89a),   // HIGH MID teal
            juce::Colour (0xff7a8fd0),   // HIGH     periwinkle
        };
    }

    inline juce::Font uiFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    //  Shared LookAndFeel: flat rotaries with a coloured value arc.
    class Laf : public juce::LookAndFeel_V4
    {
    public:
        Laf();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool highlighted, bool down) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (13.0f); }
        juce::Font getLabelFont (juce::Label&) override       { return uiFont (12.0f); }
    };
}
