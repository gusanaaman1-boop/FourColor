#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include "Core/ParameterIds.h"
#include "Dsp/FourColorEngine.h"

namespace fourcolor
{
    class FourColorProcessor : public juce::AudioProcessor
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
        //  Block-peak meters (atomics written on the audio thread).
        float readAndResetInputPeak() noexcept  { return inputPeak.exchange (0.0f); }
        float readAndResetOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

        //  A/B compare (message thread only): two full-state slots.
        void toggleAB();
        void copyABToOther();
        int getABIndex() const noexcept { return abIndex; }

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

        std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

        juce::MemoryBlock abSlots[2];
        int abIndex = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorProcessor)
    };
}
