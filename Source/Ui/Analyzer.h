// The analyzer: the most important surface in the product, and the one that
// must never lie.
//
// Everything drawn here derives from measured audio:
//   * MID spectrum  -> the filled, mirrored centre shape
//   * SIDE spectrum -> the thin outline contours around it (real M/S, so the
//                      low band reads closed to the centre because the DSP
//                      actually keeps it mono, not because we drew it that way)
// Samples arrive through the processor's lock-free FIFO; the audio thread only
// writes to it.
//
// The LR4 crossover responses are drawn as thin analytic curves BEHIND the
// spectrum - they are context, not the subject.
//
// Harmonic-residual visualisation is deliberately absent: the engine does not
// expose a residual measurement to the GUI, and inventing one is not allowed.
// `setResidualProvider()` is the seam for it.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../Core/ParameterIds.h"
#include "Design.h"

namespace fourcolor
{
    class FourColorProcessor;
}

namespace fourcolor::ui
{
    class Analyzer : public juce::Component, private juce::Timer
    {
    public:
        Analyzer (FourColorProcessor& processor);
        ~Analyzer() override;

        std::function<void (int band)> onBandSelected;

        void setSelectedBand (int band);
        float getCutHz (int i) const noexcept { return cutValues[i]; }

        //  Temporary emphasis while the user drags a control elsewhere.
        enum class Emphasis { none, drive, behaviorBody, behaviorAttack, tone, space };
        void setEmphasis (Emphasis e, int band);

        //  Future seam: supply per-band nonlinear-residual magnitudes and the
        //  analyzer will draw them as the thin secondary layer. Nothing is
        //  drawn while this is unset.
        using ResidualProvider = std::function<bool (int band, std::vector<float>& magnitudes)>;
        void setResidualProvider (ResidualProvider provider) { residualProvider = std::move (provider); }

        void paint (juce::Graphics&) override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        void updateSpectrum();

        float xForFrequency (float hz) const;
        float frequencyForX (float x) const;
        float yForDb (float db) const;
        int handleAt (juce::Point<float> pos) const;
        juce::Rectangle<float> plotArea() const;
        int bandForFrequency (float hz) const;

        FourColorProcessor& proc;
        juce::AudioProcessorValueTreeState& state;

        std::unique_ptr<juce::ParameterAttachment> cutAttachments[3];
        float cutValues[3] = { 120.0f, 700.0f, 4500.0f };
        float cutDefaults[3] = { 120.0f, 700.0f, 4500.0f };

        //  --- analysis -----------------------------------------------------------
        static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;   // 2048
        static constexpr int numColumns = 240;

        juce::dsp::FFT fft { fftOrder };
        std::vector<float> window   = std::vector<float> ((size_t) fftSize, 0.0f);
        std::vector<float> midRing  = std::vector<float> ((size_t) fftSize, 0.0f);
        std::vector<float> sideRing = std::vector<float> ((size_t) fftSize, 0.0f);
        int ringPos = 0;
        std::vector<float> scratch  = std::vector<float> ((size_t) fftSize * 2, 0.0f);
        std::vector<float> drain    = std::vector<float> (2048 * 2, 0.0f);

        std::vector<float> midDb  = std::vector<float> ((size_t) numColumns, -96.0f);
        std::vector<float> sideDb = std::vector<float> ((size_t) numColumns, -96.0f);
        float attackCoeff = 0.35f, releaseDbPerFrame = 1.4f;
        bool silent = true;

        ResidualProvider residualProvider;

        int selectedBand = 0;
        int draggingHandle = -1;
        int hoverHandle = -1;
        Emphasis emphasis = Emphasis::none;
        int emphasisBand = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Analyzer)
    };
}
