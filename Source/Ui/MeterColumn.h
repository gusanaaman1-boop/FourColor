// A stereo input or output meter column, flanking the analyzer, with its trim
// knob directly under it so the control and the reading it moves are read as
// one thing.
//
// The audio thread only ever writes block peak and block mean-square into a
// lock-free MeterBlock. Everything here - ballistics, decay, peak hold, the
// clip latch - is GUI-side, which is what keeps the audio path free of any
// SmoothedValue, Timer or repaint.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Knob.h"

namespace fourcolor
{
    class FourColorProcessor;
}

namespace fourcolor::ui
{
    class MeterColumn : public juce::Component, private juce::Timer
    {
    public:
        enum class Side { input, output };

        MeterColumn (FourColorProcessor& processor, Side side);
        ~MeterColumn() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;

        //  -60 .. +6 dBFS, the range the brief asks for.
        static constexpr float floorDb = -60.0f, ceilingDb = 6.0f;
        static float normalised (float db) noexcept
        {
            return juce::jlimit (0.0f, 1.0f, (db - floorDb) / (ceilingDb - floorDb));
        }

        FourColorProcessor& proc;
        Side side;

        std::unique_ptr<Knob> trim;

        //  Per channel, all GUI-side.
        struct Channel
        {
            float peakDb = -200.0f;      // decayed peak bar
            float rmsDb  = -200.0f;      // smoothed RMS bar
            float holdDb = -200.0f;      // peak-hold tick
            int   holdAge = 0;
        };
        Channel channels[2];
        bool clipped = false;

        juce::Rectangle<int> barsArea, scaleArea, clipArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterColumn)
    };
}
