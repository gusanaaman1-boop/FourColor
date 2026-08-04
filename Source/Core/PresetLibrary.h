// Factory presets, defined in code so they cannot drift from the parameter set.
// Phase 1 ships only "Default"; the 24 musical presets arrive in Phase 9.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fourcolor
{
    class PresetLibrary
    {
    public:
        static int numPresets();
        static juce::String name (int index);
        static juce::String category (int index);

        //  Sets every parameter of `apvts` to the preset's values (parameters the
        //  preset does not mention fall back to their defaults).
        static void apply (int index, juce::AudioProcessorValueTreeState& apvts);

    private:
        struct Preset;
        static const std::vector<Preset>& presets();
    };
}
