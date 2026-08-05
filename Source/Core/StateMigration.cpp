#include "StateMigration.h"

#include "ParameterIds.h"

namespace fourcolor::state
{
    namespace
    {
        //  APVTS writes one child per parameter, typed PARAM, carrying "id" and
        //  "value". Anything else in the tree is ours (editor properties) or a
        //  future version's, and is left alone.
        const juce::Identifier paramType { "PARAM" };
        const juce::Identifier idProperty { "id" };
        const juce::Identifier valueProperty { "value" };

        //  Drops PARAM children that cannot be trusted and clamps the rest into
        //  their real range. A dropped child means the parameter keeps its
        //  default, which is always a playable value.
        bool scrubParameters (juce::ValueTree& tree,
                              const juce::AudioProcessorValueTreeState& apvts)
        {
            bool repaired = false;

            for (int i = tree.getNumChildren(); --i >= 0;)
            {
                auto child = tree.getChild (i);
                if (! child.hasType (paramType))
                    continue;

                const auto id = child.getProperty (idProperty).toString();
                auto* parameter = apvts.getParameter (id);

                if (parameter == nullptr)
                {
                    //  A parameter this build does not have. It belongs to a
                    //  newer version; leave it in place so that saving again
                    //  does not destroy it, and let APVTS ignore it.
                    continue;
                }

                if (! child.hasProperty (valueProperty))
                {
                    tree.removeChild (i, nullptr);
                    repaired = true;
                    continue;
                }

                const auto raw = child.getProperty (valueProperty);
                const double value = raw;

                if (! std::isfinite (value))
                {
                    tree.removeChild (i, nullptr);
                    repaired = true;
                    continue;
                }

                const auto& range = parameter->getNormalisableRange();
                const auto clamped = (double) range.snapToLegalValue ((float) value);

                if (std::abs (clamped - value) > 1.0e-9)
                {
                    child.setProperty (valueProperty, clamped, nullptr);
                    repaired = true;
                }
            }

            return repaired;
        }

        //  Editor properties an older session will not carry. Absence is normal,
        //  so they are filled in rather than treated as corruption.
        bool fillEditorDefaults (juce::ValueTree& tree)
        {
            bool changed = false;

            auto ensure = [&] (const char* name, int fallback, int lo, int hi)
            {
                if (! tree.hasProperty (name))
                {
                    tree.setProperty (name, fallback, nullptr);
                    changed = true;
                    return;
                }

                const int v = (int) tree.getProperty (name);
                if (v < lo || v > hi)
                {
                    tree.setProperty (name, juce::jlimit (lo, hi, v), nullptr);
                    changed = true;
                }
            };

            ensure (selectedBandProperty, 0, 0, numBands - 1);
            ensure (editorWidthProperty, 980, 900, 1900);
            ensure (editorHeightProperty, 620, 560, 1200);
            return changed;
        }
    }

    void stampVersion (juce::ValueTree& tree)
    {
        tree.setProperty (versionProperty, currentVersion, nullptr);
    }

    MigrationResult migrate (juce::ValueTree& tree,
                             const juce::AudioProcessorValueTreeState& apvts)
    {
        MigrationResult result;

        if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        {
            result.note = "not a FOUR COLOR state tree";
            return result;
        }

        //  No tag at all means v0: every state this plug-in wrote before the
        //  tag existed. That is not an error and never will be.
        result.fromVersion = tree.hasProperty (versionProperty)
                                 ? (int) tree.getProperty (versionProperty)
                                 : 0;

        if (result.fromVersion < 0)
        {
            result.note = "negative version";
            return result;
        }

        if (result.fromVersion > currentVersion)
        {
            //  Written by a newer build. Load what we recognise; the properties
            //  and PARAM children we do not know are left untouched so the next
            //  save preserves them.
            result.note = "from a newer version (" + juce::String (result.fromVersion)
                        + " > " + juce::String (currentVersion) + "); unknown fields preserved";
        }

        //  v0 -> v1 is a pure stamp: the tree shape did not change, only the
        //  promise that it is versioned from here on. Later migrations belong
        //  in this chain, each one guarded by the version it upgrades from.
        //
        //  if (result.fromVersion < 2) { ...v1 -> v2... }

        const bool scrubbed = scrubParameters (tree, apvts);
        const bool filled = fillEditorDefaults (tree);
        result.wasRepaired = scrubbed || filled;

        if (result.fromVersion <= currentVersion)
            stampVersion (tree);

        result.usable = true;
        return result;
    }
}
