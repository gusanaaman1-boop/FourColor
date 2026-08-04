// FOUR COLOR parameter identifiers.
//
// These strings are frozen: presets and host automation reference them, so they
// must never change after Phase 1. New parameters may only be appended.

#pragma once

#include <juce_core/juce_core.h>

namespace fourcolor
{
    inline constexpr int numBands = 4;

    namespace param
    {
        // Global
        inline constexpr const char* input       = "input";
        inline constexpr const char* globalDrive = "globalDrive";
        inline constexpr const char* globalTone  = "globalTone";
        inline constexpr const char* autoLevel   = "autoLevel";
        inline constexpr const char* mix         = "mix";
        inline constexpr const char* output      = "output";
        inline constexpr const char* quality     = "quality";
        inline constexpr const char* bypassed    = "bypassed";
        inline constexpr const char* xover1      = "xover1";
        inline constexpr const char* xover2      = "xover2";
        inline constexpr const char* xover3      = "xover3";

        // Per band: "b0_drive", "b3_color", ...
        inline juce::String band (int bandIndex, const char* suffix)
        {
            jassert (bandIndex >= 0 && bandIndex < numBands);
            return "b" + juce::String (bandIndex) + "_" + suffix;
        }

        inline constexpr const char* color    = "color";
        inline constexpr const char* drive    = "drive";
        inline constexpr const char* behavior = "behavior";
        inline constexpr const char* tone     = "tone";
        inline constexpr const char* space    = "space";
        inline constexpr const char* bandMix  = "bandMix";
        inline constexpr const char* level    = "level";
        inline constexpr const char* solo     = "solo";
        inline constexpr const char* mute     = "mute";
        inline constexpr const char* bypass   = "bypass";
    }

    enum class ColorType { warm = 0, iron, bite, fuzz };
    enum class Quality   { draft = 0, normal, high, ultra };

    inline int oversamplingFactorFor (Quality q) noexcept
    {
        switch (q)
        {
            case Quality::draft:  return 1;
            case Quality::normal: return 2;
            case Quality::high:   return 4;
            case Quality::ultra:  return 8;
        }
        return 4;
    }

    inline const char* colorName (ColorType c) noexcept
    {
        switch (c)
        {
            case ColorType::warm: return "WARM";
            case ColorType::iron: return "IRON";
            case ColorType::bite: return "BITE";
            case ColorType::fuzz: return "FUZZ";
        }
        return "?";
    }

    inline const char* bandName (int i) noexcept
    {
        constexpr const char* names[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        return (i >= 0 && i < 4) ? names[i] : "?";
    }
}
