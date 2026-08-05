// Versioning and migration for the plug-in's saved state.
//
// The rules this file exists to enforce:
//
//   * Parameter IDs never change. Versioning is a property ON the tree, not a
//     renaming scheme, so a v0 session and a v1 session address the same
//     parameters by the same names.
//   * A state with no version tag is a v0 state - everything this plug-in wrote
//     before the tag existed - and must keep loading forever.
//   * A state from a FUTURE version loads as far as it can and keeps the
//     properties it does not understand, so saving in an old build and
//     reopening in a new one does not silently discard the newer settings.
//   * A corrupt or hostile state must leave the plug-in playable. No NaN
//     reaches a parameter, and nothing throws.
//
// Nothing about the audio path is stored: no pointers, no buffers, no analyzer
// data. The state is parameters plus a handful of editor properties.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fourcolor::state
{
    //  Bumped only when the SHAPE of the tree changes in a way that needs code
    //  to translate it. Adding a parameter does not need a bump: an absent
    //  PARAM child already falls back to that parameter's default.
    inline constexpr int currentVersion = 1;

    inline constexpr const char* versionProperty = "stateVersion";

    //  Editor-only properties. They are saved, but their absence is never an
    //  error - an older session simply did not have them.
    inline constexpr const char* selectedBandProperty = "selectedBand";
    inline constexpr const char* editorWidthProperty  = "editorWidth";
    inline constexpr const char* editorHeightProperty = "editorHeight";

    //  Stamps the current version onto a tree that is about to be written.
    void stampVersion (juce::ValueTree& tree);

    struct MigrationResult
    {
        bool usable = false;      //  false means: keep the state you already had
        int  fromVersion = 0;
        bool wasRepaired = false; //  a PARAM was dropped or clamped
        juce::String note;
    };

    //  Brings `tree` up to currentVersion in place and scrubs it against the
    //  live parameter set. `tree` is only safe to hand to
    //  AudioProcessorValueTreeState::replaceState when `usable` is true.
    MigrationResult migrate (juce::ValueTree& tree,
                             const juce::AudioProcessorValueTreeState& apvts);
}
