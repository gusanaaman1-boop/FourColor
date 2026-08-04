#include "PresetLibrary.h"
#include "ParameterIds.h"

namespace fourcolor
{
    struct PresetLibrary::Preset
    {
        const char* presetName;
        const char* presetCategory;
        std::vector<std::pair<juce::String, float>> values;   // parameter ID -> plain value
    };

    const std::vector<PresetLibrary::Preset>& PresetLibrary::presets()
    {
        static const std::vector<Preset> list = []
        {
            std::vector<Preset> p;

            // Phase 1: a single neutral preset proving the mechanism. The 24
            // musical presets are added in Phase 9.
            p.push_back ({ "Default", "Init", {} });

            return p;
        }();

        return list;
    }

    int PresetLibrary::numPresets()                 { return (int) presets().size(); }
    juce::String PresetLibrary::name (int index)     { return presets()[(size_t) index].presetName; }
    juce::String PresetLibrary::category (int index) { return presets()[(size_t) index].presetCategory; }

    void PresetLibrary::apply (int index, juce::AudioProcessorValueTreeState& apvts)
    {
        if (index < 0 || index >= numPresets())
            return;

        const auto& preset = presets()[(size_t) index];

        //  Start from defaults so a preset only needs to state what it changes.
        for (auto* param : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

        for (const auto& [id, plainValue] : preset.values)
        {
            if (auto* param = apvts.getParameter (id))
                param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
            else
                jassertfalse;   // a preset references a parameter that does not exist
        }
    }
}
