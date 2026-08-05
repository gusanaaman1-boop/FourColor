// FOUR COLOR visual language, matched to the product mockup: near-black
// ground, card panels with hairline borders, dotted-tick knobs, and one accent
// colour per band - the product is literally named after them.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace fourcolor::ui
{
    namespace colour
    {
        const juce::Colour background { 0xff0d0e11 };
        const juce::Colour panel      { 0xff16171c };
        const juce::Colour panelHi    { 0xff1c1d23 };
        const juce::Colour panelLine  { 0xff2a2b33 };
        const juce::Colour text       { 0xffe8e8ec };
        const juce::Colour textDim    { 0xff9094a0 };
        const juce::Colour accent     { 0xffd8dade };
        const juce::Colour knobBody   { 0xff232228 };

        //  The four band colours: LOW, LOW MID, HIGH MID, HIGH.
        const juce::Colour band[4] = {
            juce::Colour (0xffc9a83c),   // BAND 1  mustard
            juce::Colour (0xffe07830),   // BAND 2  orange
            juce::Colour (0xffe53f63),   // BAND 3  pink
            juce::Colour (0xff3fb5b0),   // BAND 4  teal
        };
    }

    inline juce::Font uiFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    //  Spaced uppercase, the mockup's label voice.
    inline juce::Font labelFont (float height, bool bold = false)
    {
        return uiFont (height, bold).withExtraKerningFactor (0.12f);
    }

    //  Shared LookAndFeel: dark knob bodies ringed by tick dots that light up
    //  to the current value, and the BODY<->ATTACK gradient slider.
    class Laf : public juce::LookAndFeel_V4
    {
    public:
        Laf();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool highlighted, bool down) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return labelFont (12.5f); }
        juce::Font getLabelFont (juce::Label&) override       { return uiFont (12.0f); }
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };
}
