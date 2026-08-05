// The shared LookAndFeel. All colours and metrics come from Design.h; this
// file only draws.
//
// One knob renderer serves every rotary in the product. Per-knob variation is
// carried in the Slider's property set, so the renderer stays single-source:
//
//   "arcThickness"  float   stroke width of the value arc
//   "arcColour"     int     ARGB of the active arc (start colour)
//   "arcColourEnd"  int     ARGB the arc fades to along its sweep (optional)
//   "glowAlpha"     float   0..1 outer glow strength (0 = none)
//   "sparks"        bool    DRIVE-style harmonic sparks around the arc
//   "spreadArcs"    bool    SPACE-style expanding outer arcs
//   "energy"        float   0..1 live audio level, drives sparks/arcs motion

#pragma once

#include "Design.h"

namespace fourcolor::ui
{
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

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

        juce::Label* createSliderTextBox (juce::Slider&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return captionFont (12.0f); }
        juce::Font getLabelFont (juce::Label&) override       { return uiFont (11.5f); }
        juce::Font getPopupMenuFont() override                { return uiFont (13.0f); }
    };
}
