#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include "Core/ParameterIds.h"
#include "Dsp/FourColorEngine.h"

namespace fourcolor
{
    class FourColorProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        FourColorProcessor();
        ~FourColorProcessor() override = default;

        // --- AudioProcessor ------------------------------------------------------
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override                        { return true; }

        const juce::String getName() const override            { return "FOUR COLOR"; }
        bool acceptsMidi() const override                      { return false; }
        bool producesMidi() const override                     { return false; }
        bool isMidiEffect() const override                     { return false; }
        double getTailLengthSeconds() const override           { return 0.1; }

        int getNumPrograms() override;
        int getCurrentProgram() override                       { return currentProgram; }
        void setCurrentProgram (int index) override;
        const juce::String getProgramName (int index) override;
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        juce::AudioProcessorParameter* getBypassParameter() const override;

        // --- shared with the editor ---------------------------------------------
        juce::AudioProcessorValueTreeState apvts;

        FourColorEngine& getEngine() noexcept { return engine; }

        //  --- UI services ---------------------------------------------------------
        //  Everything the meters need, published lock-free from the audio
        //  thread: block peak and block mean-square per channel, plus a clip
        //  latch. Ballistics, hold and decay all live in the GUI - the audio
        //  thread only ever writes raw numbers. Channel 0 = L, 1 = R.
        //
        //  There is exactly ONE of these per measurement point. A second set of
        //  plain peak atomics used to live alongside them: the output pair was
        //  written by a duplicate loop nobody read, and the input pair was
        //  never written at all, so every legacy caller asking for the input
        //  peak got a permanent zero.
        struct MeterBlock
        {
            std::atomic<float> peak[2] { { 0.0f }, { 0.0f } };
            std::atomic<float> meanSquare[2] { { 0.0f }, { 0.0f } };
            std::atomic<bool>  clipped { false };

            void submit (const juce::AudioBuffer<float>& buffer, int n, int chans) noexcept
            {
                for (int m = 0; m < 2; ++m)
                {
                    const int src = juce::jmin (m, chans - 1);
                    const auto* d = buffer.getReadPointer (src);

                    float p = 0.0f;
                    double sumSq = 0.0;
                    for (int i = 0; i < n; ++i)
                    {
                        const float v = d[i];
                        p = juce::jmax (p, std::abs (v));
                        sumSq += (double) v * v;
                    }

                    if (p > peak[m].load (std::memory_order_relaxed))
                        peak[m].store (p, std::memory_order_relaxed);

                    meanSquare[m].store ((float) (sumSq / juce::jmax (1, n)),
                                         std::memory_order_relaxed);

                    if (p >= 1.0f)
                        clipped.store (true, std::memory_order_relaxed);
                }
            }
        };

        MeterBlock inputMeter, outputMeter;

        //  Thin readers over the blocks above, so there is no second copy of
        //  the truth to fall out of date. Each is a destructive read: whoever
        //  calls it takes the peak and leaves the accumulator at zero, which
        //  is why there must be a single consumer per meter.
        float readAndResetInputPeak (int channel) noexcept
        {
            return inputMeter.peak[channel & 1].exchange (0.0f, std::memory_order_relaxed);
        }

        float readAndResetOutputPeak (int channel) noexcept
        {
            return outputMeter.peak[channel & 1].exchange (0.0f, std::memory_order_relaxed);
        }

        float readAndResetInputPeak() noexcept
        {
            return juce::jmax (readAndResetInputPeak (0), readAndResetInputPeak (1));
        }

        float readAndResetOutputPeak() noexcept
        {
            return juce::jmax (readAndResetOutputPeak (0), readAndResetOutputPeak (1));
        }

        //  A/B compare (message thread only): two full-state slots.
        void toggleAB();
        void copyABToOther();
        int getABIndex() const noexcept { return abIndex; }

        //  Preset navigation + modified marker for the UI.
        void stepProgram (int delta);
        bool isPresetDirty() const noexcept { return presetDirty.load(); }

        //  Undo of parameter edits (the editor groups transactions on idle).
        juce::UndoManager undoManager;

        //  --- output spectrum tap (lock-free, interleaved MID/SIDE) ---------------
        //  The editor drains this to run its FFTs; nothing in the display is
        //  faked. `dest` receives `maxFrames * 2` floats as [mid, side] pairs.
        //  Returns the number of FRAMES written.
        int readSpectrumFrames (float* dest, int maxFrames) noexcept;

        static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    private:
        void cacheParameterPointers();
        void pushParametersToEngine() noexcept;

        //  Raw-value pointers cached at construction so the audio thread never
        //  builds parameter-ID strings. (The allocation tripwire in the test
        //  suite caught exactly that.)
        struct BandPointers
        {
            std::atomic<float>* color;
            std::atomic<float>* drive;
            std::atomic<float>* behavior;
            std::atomic<float>* tone;
            std::atomic<float>* space;
            std::atomic<float>* bandMix;
            std::atomic<float>* level;
            std::atomic<float>* solo;
            std::atomic<float>* mute;
            std::atomic<float>* bypass;
        };
        struct GlobalPointers
        {
            std::atomic<float>* input;
            std::atomic<float>* globalDrive;
            std::atomic<float>* globalTone;
            std::atomic<float>* autoLevel;
            std::atomic<float>* mix;
            std::atomic<float>* output;
            std::atomic<float>* quality;
            std::atomic<float>* bypassed;
            std::atomic<float>* xover[3];
        };

        GlobalPointers globalPtrs {};
        BandPointers   bandPtrs[numBands] {};

        FourColorEngine engine;
        int currentProgram = 0;

        void parameterChanged (const juce::String&, float) override { presetDirty.store (true); }
        void pushSpectrumSamples (const juce::AudioBuffer<float>& buffer) noexcept;

        std::atomic<bool> presetDirty { false };

        //  Frames of [mid, side]; sized once, never reallocated.
        static constexpr int spectrumFifoFrames = 8192;
        juce::AbstractFifo spectrumFifo { spectrumFifoFrames };
        std::vector<float> spectrumData = std::vector<float> (spectrumFifoFrames * 2, 0.0f);

        juce::MemoryBlock abSlots[2];
        int abIndex = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorProcessor)
    };
}
