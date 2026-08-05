// The central view, matched to the mockup: a REAL output spectrum (FFT of the
// processed signal, drained from the processor's lock-free tap) drawn mirrored
// around the centre line, tinted per band region; frequency tag boxes above
// three draggable crossover handles; a dB scale on the left. Nothing here is
// animated from fake data - a silent plugin shows a flat line.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../Core/ParameterIds.h"
#include "Theme.h"

namespace fourcolor
{
    class FourColorProcessor;
}

namespace fourcolor::ui
{
    class CrossoverDisplay : public juce::Component, private juce::Timer
    {
    public:
        CrossoverDisplay (FourColorProcessor& processor);
        ~CrossoverDisplay() override;

        std::function<void (int band)> onBandSelected;

        void setSelectedBand (int band);
        void paint (juce::Graphics&) override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        float getCutHz (int i) const noexcept { return cutValues[i]; }

    private:
        void timerCallback() override;
        void updateSpectrum();

        float xForFrequency (float hz) const;
        float frequencyForX (float x) const;
        int handleAt (juce::Point<float> pos) const;
        juce::Rectangle<float> plotArea() const;

        FourColorProcessor& proc;
        juce::AudioProcessorValueTreeState& state;

        std::unique_ptr<juce::ParameterAttachment> cutAttachments[3];
        float cutValues[3] = { 120.0f, 700.0f, 4500.0f };

        //  --- analyzer -----------------------------------------------------------
        static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;   // 2048
        juce::dsp::FFT fft { fftOrder };
        std::vector<float> sampleRing = std::vector<float> ((size_t) fftSize, 0.0f);
        int ringPos = 0;
        std::vector<float> fftScratch = std::vector<float> ((size_t) fftSize * 2, 0.0f);
        std::vector<float> window = std::vector<float> ((size_t) fftSize, 0.0f);

        static constexpr int numColumns = 256;
        std::vector<float> columnDb = std::vector<float> ((size_t) numColumns, -90.0f);

        int selectedBand = 0;
        int draggingHandle = -1;
        int hoverHandle = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrossoverDisplay)
    };
}
