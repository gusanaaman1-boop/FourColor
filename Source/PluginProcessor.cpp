#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/PresetLibrary.h"
#include "Core/StateMigration.h"

namespace fourcolor
{
    namespace
    {
        juce::NormalisableRange<float> logHzRange (float lo, float hi)
        {
            juce::NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (std::sqrt (lo * hi));
            return r;
        }

        auto percent()  { return juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f); }
        auto bipolar()  { return juce::NormalisableRange<float> (-100.0f, 100.0f, 0.0f); }

        juce::String dbText (float v, int)      { return juce::String (v, 1) + " dB"; }
        juce::String pctText (float v, int)     { return juce::String (juce::roundToInt (v)) + " %"; }
        juce::String hzText (float v, int)
        {
            return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (v)) + " Hz";
        }
        juce::String bipolarText (const char* neg, const char* pos, float v)
        {
            const int i = juce::roundToInt (v);
            if (i == 0) return "0";
            return juce::String (std::abs (i)) + " " + (i < 0 ? neg : pos);
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout FourColorProcessor::createParameterLayout()
    {
        using P     = juce::AudioParameterFloat;
        using Pc    = juce::AudioParameterChoice;
        using Pb    = juce::AudioParameterBool;
        namespace p = param;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto attr = [] (auto textFn) {
            return juce::AudioParameterFloatAttributes().withStringFromValueFunction (textFn);
        };

        // --- global ------------------------------------------------------------
        layout.add (std::make_unique<P> (juce::ParameterID { p::input, 1 }, "Input",
                        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f, attr (dbText)));
        layout.add (std::make_unique<P> (juce::ParameterID { p::globalDrive, 1 }, "Global Drive",
                        percent(), 50.0f, attr (pctText)));
        layout.add (std::make_unique<P> (juce::ParameterID { p::globalTone, 1 }, "Global Tone",
                        bipolar(), 0.0f,
                        attr ([] (float v, int) { return bipolarText ("DARK", "BRIGHT", v); })));
        layout.add (std::make_unique<Pb> (juce::ParameterID { p::autoLevel, 1 }, "Auto Level", true));
        layout.add (std::make_unique<P> (juce::ParameterID { p::mix, 1 }, "Mix",
                        percent(), 100.0f, attr (pctText)));
        layout.add (std::make_unique<P> (juce::ParameterID { p::output, 1 }, "Output",
                        juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f, attr (dbText)));
        layout.add (std::make_unique<Pc> (juce::ParameterID { p::quality, 1 }, "Quality",
                        juce::StringArray { "Draft", "Normal", "High", "Ultra" }, (int) Quality::high));
        layout.add (std::make_unique<Pb> (juce::ParameterID { p::bypassed, 1 }, "Bypass", false));

        layout.add (std::make_unique<P> (juce::ParameterID { p::xover1, 1 }, "Crossover 1",
                        logHzRange (40.0f, 400.0f), 120.0f, attr (hzText)));
        layout.add (std::make_unique<P> (juce::ParameterID { p::xover2, 1 }, "Crossover 2",
                        logHzRange (250.0f, 2500.0f), 700.0f, attr (hzText)));
        layout.add (std::make_unique<P> (juce::ParameterID { p::xover3, 1 }, "Crossover 3",
                        logHzRange (1500.0f, 12000.0f), 4500.0f, attr (hzText)));

        // --- per band ----------------------------------------------------------
        for (int b = 0; b < numBands; ++b)
        {
            const juce::String prefix = juce::String (bandName (b)) + " ";
            auto id = [b] (const char* suffix) {
                return juce::ParameterID { param::band (b, suffix), 1 };
            };

            layout.add (std::make_unique<Pc> (id (p::color), prefix + "Color",
                            juce::StringArray { "Warm", "Iron", "Bite", "Fuzz" }, 0));
            layout.add (std::make_unique<P> (id (p::drive), prefix + "Drive",
                            percent(), 25.0f, attr (pctText)));
            layout.add (std::make_unique<P> (id (p::behavior), prefix + "Behavior",
                            bipolar(), 0.0f,
                            attr ([] (float v, int) { return bipolarText ("BODY", "ATTACK", v); })));
            layout.add (std::make_unique<P> (id (p::tone), prefix + "Tone",
                            bipolar(), 0.0f,
                            attr ([] (float v, int) { return bipolarText ("DARK", "BRIGHT", v); })));
            layout.add (std::make_unique<P> (id (p::space), prefix + "Space",
                            percent(), 0.0f, attr (pctText)));
            layout.add (std::make_unique<P> (id (p::bandMix), prefix + "Mix",
                            percent(), 100.0f, attr (pctText)));
            layout.add (std::make_unique<P> (id (p::level), prefix + "Level",
                            juce::NormalisableRange<float> (-18.0f, 12.0f), 0.0f, attr (dbText)));
            layout.add (std::make_unique<Pb> (id (p::solo), prefix + "Solo", false));
            layout.add (std::make_unique<Pb> (id (p::mute), prefix + "Mute", false));
            layout.add (std::make_unique<Pb> (id (p::bypass), prefix + "Bypass", false));
        }

        return layout;
    }

    FourColorProcessor::FourColorProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, &undoManager, "FOURCOLOR", createParameterLayout())
    {
        apvts.state.setProperty ("selectedBand", 0, nullptr);
        cacheParameterPointers();

        //  Any parameter edit marks the current preset as modified in the UI.
        for (auto* p : getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                apvts.addParameterListener (ranged->getParameterID(), this);
    }

    void FourColorProcessor::cacheParameterPointers()
    {
        namespace p = param;

        auto raw = [this] (const juce::String& id) {
            auto* v = apvts.getRawParameterValue (id);
            jassert (v != nullptr);
            return v;
        };

        globalPtrs.input       = raw (p::input);
        globalPtrs.globalDrive = raw (p::globalDrive);
        globalPtrs.globalTone  = raw (p::globalTone);
        globalPtrs.autoLevel   = raw (p::autoLevel);
        globalPtrs.mix         = raw (p::mix);
        globalPtrs.output      = raw (p::output);
        globalPtrs.quality     = raw (p::quality);
        globalPtrs.bypassed    = raw (p::bypassed);
        globalPtrs.xover[0]    = raw (p::xover1);
        globalPtrs.xover[1]    = raw (p::xover2);
        globalPtrs.xover[2]    = raw (p::xover3);

        for (int b = 0; b < numBands; ++b)
        {
            auto& bp   = bandPtrs[b];
            bp.color    = raw (p::band (b, p::color));
            bp.drive    = raw (p::band (b, p::drive));
            bp.behavior = raw (p::band (b, p::behavior));
            bp.tone     = raw (p::band (b, p::tone));
            bp.space    = raw (p::band (b, p::space));
            bp.bandMix  = raw (p::band (b, p::bandMix));
            bp.level    = raw (p::band (b, p::level));
            bp.solo     = raw (p::band (b, p::solo));
            bp.mute     = raw (p::band (b, p::mute));
            bp.bypass   = raw (p::band (b, p::bypass));
        }
    }

    bool FourColorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& in  = layouts.getMainInputChannelSet();
        const auto& out = layouts.getMainOutputChannelSet();

        if (in != out)
            return false;

        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    }

    void FourColorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
        pushParametersToEngine();
        engine.reset();
        setLatencySamples (engine.getLatencySamples());
    }

    void FourColorProcessor::releaseResources() {}

    void FourColorProcessor::pushParametersToEngine() noexcept
    {
        EngineParameters ep;

        ep.inputDb     = globalPtrs.input->load();
        ep.globalDrive = globalPtrs.globalDrive->load();
        ep.globalTone  = globalPtrs.globalTone->load();
        ep.autoLevel   = globalPtrs.autoLevel->load() > 0.5f;
        ep.mixPercent  = globalPtrs.mix->load();
        ep.outputDb    = globalPtrs.output->load();
        ep.quality     = (Quality) (int) globalPtrs.quality->load();
        ep.bypassed    = globalPtrs.bypassed->load() > 0.5f;
        for (int i = 0; i < 3; ++i)
            ep.xoverHz[i] = globalPtrs.xover[i]->load();

        for (int b = 0; b < numBands; ++b)
        {
            auto& band    = ep.bands[b];
            const auto& q = bandPtrs[b];
            band.color    = (ColorType) (int) q.color->load();
            band.drive    = q.drive->load();
            band.behavior = q.behavior->load();
            band.tone     = q.tone->load();
            band.space    = q.space->load();
            band.bandMix  = q.bandMix->load();
            band.levelDb  = q.level->load();
            band.solo     = q.solo->load() > 0.5f;
            band.mute     = q.mute->load() > 0.5f;
            band.bypass   = q.bypass->load() > 0.5f;
        }

        engine.setParameters (ep);
    }

    void FourColorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());

        //  Stereo input meters (block peaks, lock-free). Mono runs feed both.
        const int meterChannels = juce::jmin (2, buffer.getNumChannels());
        for (int m = 0; m < 2; ++m)
        {
            const int src = juce::jmin (m, meterChannels - 1);
            const float peak = buffer.getMagnitude (src, 0, buffer.getNumSamples());
            if (peak > inputPeak[m].load (std::memory_order_relaxed))
                inputPeak[m].store (peak, std::memory_order_relaxed);
        }

        pushParametersToEngine();
        engine.process (buffer);

        pushSpectrumSamples (buffer);

        for (int m = 0; m < 2; ++m)
        {
            const int src = juce::jmin (m, meterChannels - 1);
            const float peak = buffer.getMagnitude (src, 0, buffer.getNumSamples());
            if (peak > outputPeak[m].load (std::memory_order_relaxed))
                outputPeak[m].store (peak, std::memory_order_relaxed);
        }

        //  Latency is a constant of the design and was reported in
        //  prepareToPlay. Telling a host about a new latency while the
        //  transport is running is exactly what Phase 12 set out to stop.
    }

    // --- output spectrum tap (mid/side, lock-free, allocation-free) ------------------
    void FourColorProcessor::pushSpectrumSamples (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples();
        const int chans = juce::jmin (2, buffer.getNumChannels());

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        spectrumFifo.prepareToWrite (n, start1, size1, start2, size2);

        auto writeRange = [&] (int start, int size, int offset)
        {
            for (int i = 0; i < size; ++i)
            {
                const float l = buffer.getSample (0, offset + i);
                const float r = chans > 1 ? buffer.getSample (1, offset + i) : l;
                spectrumData[(size_t) ((start + i) * 2)]     = 0.5f * (l + r);   // mid
                spectrumData[(size_t) ((start + i) * 2 + 1)] = 0.5f * (l - r);   // side
            }
        };

        writeRange (start1, size1, 0);
        writeRange (start2, size2, size1);
        spectrumFifo.finishedWrite (size1 + size2);
    }

    int FourColorProcessor::readSpectrumFrames (float* dest, int maxFrames) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        spectrumFifo.prepareToRead (maxFrames, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
        {
            dest[i * 2]     = spectrumData[(size_t) ((start1 + i) * 2)];
            dest[i * 2 + 1] = spectrumData[(size_t) ((start1 + i) * 2 + 1)];
        }
        for (int i = 0; i < size2; ++i)
        {
            dest[(size1 + i) * 2]     = spectrumData[(size_t) ((start2 + i) * 2)];
            dest[(size1 + i) * 2 + 1] = spectrumData[(size_t) ((start2 + i) * 2 + 1)];
        }

        spectrumFifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

    // --- preset navigation ----------------------------------------------------------
    void FourColorProcessor::stepProgram (int delta)
    {
        const int count = getNumPrograms();
        setCurrentProgram (((getCurrentProgram() + delta) % count + count) % count);
    }

    // --- A/B compare (message thread) ---------------------------------------------
    void FourColorProcessor::toggleAB()
    {
        //  Store the live state into the current slot, then load the other
        //  slot if it holds anything.
        abSlots[abIndex].reset();
        getStateInformation (abSlots[abIndex]);

        abIndex = 1 - abIndex;

        if (abSlots[abIndex].getSize() > 0)
            setStateInformation (abSlots[abIndex].getData(), (int) abSlots[abIndex].getSize());
    }

    void FourColorProcessor::copyABToOther()
    {
        abSlots[1 - abIndex].reset();
        getStateInformation (abSlots[1 - abIndex]);
    }

    juce::AudioProcessorParameter* FourColorProcessor::getBypassParameter() const
    {
        return apvts.getParameter (param::bypassed);
    }

    // --- programs (factory presets) ---------------------------------------------
    int FourColorProcessor::getNumPrograms()
    {
        return juce::jmax (1, PresetLibrary::numPresets());
    }

    void FourColorProcessor::setCurrentProgram (int index)
    {
        if (index < 0 || index >= PresetLibrary::numPresets())
            return;

        currentProgram = index;
        PresetLibrary::apply (index, apvts);
        presetDirty.store (false);
    }

    const juce::String FourColorProcessor::getProgramName (int index)
    {
        if (index < 0 || index >= PresetLibrary::numPresets())
            return "Default";

        return PresetLibrary::name (index);
    }

    // --- state -------------------------------------------------------------------
    void FourColorProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        //  Parameters plus a few editor properties. Nothing from the audio path
        //  is written: no pointers, no buffers, no analyzer data.
        auto state = apvts.copyState();
        state::stampVersion (state);

        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }

    void FourColorProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        if (data == nullptr || sizeInBytes <= 0)
            return;

        auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

        //  A state that cannot be migrated leaves the plug-in exactly as it
        //  was. Refusing to load is always better than loading half of it.
        const auto result = state::migrate (tree, apvts);
        if (! result.usable)
            return;

        apvts.replaceState (tree);
        presetDirty.store (false);
    }

    juce::AudioProcessorEditor* FourColorProcessor::createEditor()
    {
        return new FourColorEditor (*this);
    }
}

// Global JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fourcolor::FourColorProcessor();
}
