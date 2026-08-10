#include <functional>

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

    namespace
    {
        //  Terse builders so each preset below reads as a musical intent.
        using V = std::vector<std::pair<juce::String, float>>;

        void band (V& v, int b, ColorType color, float drive, float behavior = 0.0f,
                   float tone = 0.0f, float space = 0.0f, float mix = 100.0f, float level = 0.0f)
        {
            namespace p = param;
            v.emplace_back (p::band (b, p::color), (float) (int) color);
            v.emplace_back (p::band (b, p::drive), drive);
            v.emplace_back (p::band (b, p::behavior), behavior);
            v.emplace_back (p::band (b, p::tone), tone);
            v.emplace_back (p::band (b, p::space), space);
            v.emplace_back (p::band (b, p::bandMix), mix);
            v.emplace_back (p::band (b, p::level), level);
        }

        void bandBypass (V& v, int b) { v.emplace_back (param::band (b, param::bypass), 1.0f); }
        void bandMute (V& v, int b)   { v.emplace_back (param::band (b, param::mute), 1.0f); }
    }

    const std::vector<PresetLibrary::Preset>& PresetLibrary::presets()
    {
        static const std::vector<Preset> list = []
        {
            namespace p = param;
            std::vector<Preset> out;

            auto add = [&out] (const char* name, const char* category,
                               const std::function<void (V&)>& build)
            {
                V v;
                build (v);
                out.push_back ({ name, category, std::move (v) });
            };

            add ("Default", "Init", [] (V&) {});

            // --- BASS ---------------------------------------------------------
            add ("Sub Weight", "Bass", [] (V& v)
            {
                //  Fat, round low end; transient of the sub stays clean.
                v.emplace_back (p::xover1, 95.0f);
                band (v, 0, ColorType::warm, 48.0f, -35.0f, -15.0f);
                band (v, 1, ColorType::warm, 22.0f, -10.0f);
                band (v, 2, ColorType::warm, 12.0f);
                band (v, 3, ColorType::warm, 8.0f);
            });

            add ("Rolling Bass Body", "Bass", [] (V& v)
            {
                //  Saturation rides the sustain: rolling basslines thicken.
                band (v, 0, ColorType::warm, 55.0f, -55.0f, -10.0f);
                band (v, 1, ColorType::iron, 42.0f, -30.0f, 0.0f, 12.0f);
                band (v, 2, ColorType::iron, 20.0f);
                band (v, 3, ColorType::warm, 10.0f);
            });

            add ("Acid Bite", "Bass", [] (V& v)
            {
                //  303-style forward mids with fast grit.
                v.emplace_back (p::xover2, 520.0f);
                band (v, 0, ColorType::warm, 25.0f, -20.0f);
                band (v, 1, ColorType::bite, 68.0f, 35.0f, 15.0f);
                band (v, 2, ColorType::bite, 62.0f, 45.0f, 28.0f, 15.0f);
                band (v, 3, ColorType::bite, 35.0f, 20.0f, 20.0f);
            });

            add ("Mid Bass Iron", "Bass", [] (V& v)
            {
                //  Dense, heavy mid-bass; sub politely warm.
                band (v, 0, ColorType::warm, 28.0f, -25.0f);
                band (v, 1, ColorType::iron, 62.0f, 10.0f, -5.0f);
                band (v, 2, ColorType::iron, 45.0f, 10.0f);
                band (v, 3, ColorType::warm, 12.0f);
            });

            add ("Bass Harmonic Lift", "Bass", [] (V& v)
            {
                //  The sub passes untouched; audible harmonics are built above
                //  it so the bass reads on small speakers.
                bandBypass (v, 0);
                band (v, 1, ColorType::warm, 38.0f, 0.0f, 10.0f, 28.0f);
                band (v, 2, ColorType::bite, 32.0f, 10.0f, 18.0f, 22.0f);
                band (v, 3, ColorType::warm, 15.0f, 0.0f, 10.0f);
            });

            add ("Controlled Bass Fuzz", "Bass", [] (V& v)
            {
                //  Parallel fuzz on the mids only; the low stays solid.
                band (v, 0, ColorType::warm, 32.0f, -30.0f);
                band (v, 1, ColorType::fuzz, 48.0f, 0.0f, -5.0f, 0.0f, 55.0f);
                band (v, 2, ColorType::fuzz, 38.0f, 15.0f, 0.0f, 12.0f, 50.0f);
                band (v, 3, ColorType::warm, 10.0f);
            });

            // --- DRUMS --------------------------------------------------------
            add ("Kick Weight", "Drums", [] (V& v)
            {
                //  The click stays clean; the body swells.
                v.emplace_back (p::xover1, 105.0f);
                band (v, 0, ColorType::warm, 58.0f, -70.0f, -12.0f);
                band (v, 1, ColorType::iron, 30.0f, -25.0f);
                band (v, 2, ColorType::warm, 12.0f);
                band (v, 3, ColorType::warm, 8.0f);
            });

            add ("Kick Attack", "Drums", [] (V& v)
            {
                //  The beater hits harder without extra level.
                band (v, 0, ColorType::warm, 30.0f, -20.0f);
                band (v, 1, ColorType::bite, 42.0f, 60.0f);
                band (v, 2, ColorType::bite, 55.0f, 68.0f, 18.0f);
                band (v, 3, ColorType::bite, 25.0f, 40.0f, 15.0f);
            });

            add ("Drum Bus Warmth", "Drums", [] (V& v)
            {
                //  Gentle four-band glue voiced for a kit.
                v.emplace_back (p::mix, 88.0f);
                band (v, 0, ColorType::warm, 32.0f, -25.0f);
                band (v, 1, ColorType::warm, 36.0f, -15.0f);
                band (v, 2, ColorType::warm, 30.0f, 0.0f, 5.0f);
                band (v, 3, ColorType::warm, 24.0f, 0.0f, 8.0f);
            });

            add ("Crunchy Loop", "Drums", [] (V& v)
            {
                //  Break/loop treatment: mid grit, controlled top.
                band (v, 0, ColorType::iron, 28.0f, -20.0f);
                band (v, 1, ColorType::iron, 38.0f);
                band (v, 2, ColorType::bite, 60.0f, 45.0f, 12.0f);
                band (v, 3, ColorType::bite, 44.0f, 30.0f, -8.0f);
            });

            add ("Snare Bite", "Drums", [] (V& v)
            {
                //  Crack forward, a breath of space on the shell.
                band (v, 0, ColorType::warm, 15.0f);
                band (v, 1, ColorType::warm, 25.0f, -15.0f);
                band (v, 2, ColorType::bite, 68.0f, 62.0f, 22.0f, 15.0f);
                band (v, 3, ColorType::bite, 40.0f, 35.0f, 10.0f, 20.0f);
            });

            add ("Parallel Fuzz Drums", "Drums", [] (V& v)
            {
                //  Full-range fuzz folded under the dry kit.
                v.emplace_back (p::mix, 35.0f);
                band (v, 0, ColorType::fuzz, 55.0f, -20.0f);
                band (v, 1, ColorType::fuzz, 68.0f);
                band (v, 2, ColorType::fuzz, 72.0f, 25.0f);
                band (v, 3, ColorType::fuzz, 60.0f, 20.0f, -6.0f);
            });

            // --- SYNTHS -------------------------------------------------------
            add ("Warm Pad", "Synths", [] (V& v)
            {
                //  Round, slightly dark, saturation on the sustain.
                band (v, 0, ColorType::warm, 25.0f, -30.0f);
                band (v, 1, ColorType::warm, 35.0f, -35.0f, -8.0f);
                band (v, 2, ColorType::warm, 32.0f, -30.0f, -10.0f, 25.0f);
                band (v, 3, ColorType::warm, 22.0f, -20.0f, -12.0f, 30.0f);
            });

            add ("Dirty Pad Halo", "Synths", [] (V& v)
            {
                //  The halo IS the preset: heavy Space on driven uppers.
                band (v, 0, ColorType::warm, 20.0f, -20.0f);
                band (v, 1, ColorType::iron, 40.0f, -15.0f);
                band (v, 2, ColorType::iron, 52.0f, 0.0f, 8.0f, 60.0f);
                band (v, 3, ColorType::bite, 42.0f, 0.0f, 15.0f, 70.0f);
            });

            add ("Melodic Lead Bite", "Synths", [] (V& v)
            {
                //  Present lead that cuts without simple EQ brightness.
                band (v, 0, ColorType::warm, 12.0f);
                band (v, 1, ColorType::warm, 28.0f, -10.0f);
                band (v, 2, ColorType::bite, 62.0f, 32.0f, 15.0f, 18.0f);
                band (v, 3, ColorType::bite, 38.0f, 20.0f, 28.0f, 12.0f);
            });

            add ("Dark Stab Iron", "Synths", [] (V& v)
            {
                //  Dense, dark, punchy chord stabs.
                v.emplace_back (p::globalTone, -20.0f);
                band (v, 0, ColorType::iron, 30.0f, -15.0f);
                band (v, 1, ColorType::iron, 64.0f, 22.0f, -30.0f);
                band (v, 2, ColorType::iron, 55.0f, 18.0f, -35.0f, 10.0f);
                band (v, 3, ColorType::warm, 18.0f, 0.0f, -40.0f);
            });

            add ("Acid Destruction", "Synths", [] (V& v)
            {
                //  The creative extreme: gated fuzz mids, screaming top.
                v.emplace_back (p::globalDrive, 62.0f);
                band (v, 0, ColorType::iron, 35.0f, -20.0f);
                band (v, 1, ColorType::fuzz, 78.0f, 20.0f, 5.0f);
                band (v, 2, ColorType::fuzz, 85.0f, 45.0f, 12.0f, 30.0f);
                band (v, 3, ColorType::bite, 58.0f, 30.0f, 20.0f, 18.0f);
            });

            add ("Upper Harmonic Lift", "Synths", [] (V& v)
            {
                //  Air made of harmonics, not shelving.
                bandBypass (v, 0);
                bandBypass (v, 1);
                band (v, 2, ColorType::warm, 26.0f, 0.0f, 15.0f, 15.0f);
                band (v, 3, ColorType::warm, 42.0f, 0.0f, 38.0f, 45.0f);
            });

            // --- VOCALS -------------------------------------------------------
            add ("Vocal Edge", "Vocals", [] (V& v)
            {
                //  Presence and consonant grip for a buried vocal.
                band (v, 0, ColorType::warm, 10.0f);
                band (v, 1, ColorType::warm, 22.0f, -10.0f);
                band (v, 2, ColorType::bite, 46.0f, 38.0f, 12.0f);
                band (v, 3, ColorType::warm, 26.0f, 15.0f, 18.0f);
            });

            add ("Dark Vocal Grit", "Vocals", [] (V& v)
            {
                //  Low-mid density, softened top: close and rough.
                band (v, 0, ColorType::warm, 15.0f);
                band (v, 1, ColorType::iron, 56.0f, -22.0f, -12.0f);
                band (v, 2, ColorType::iron, 44.0f, -15.0f, -28.0f);
                band (v, 3, ColorType::warm, 15.0f, 0.0f, -20.0f);
            });

            add ("Harmonic Air", "Vocals", [] (V& v)
            {
                //  A halo of created air above the voice.
                bandBypass (v, 0);
                bandBypass (v, 1);
                band (v, 2, ColorType::warm, 20.0f, 0.0f, 10.0f, 20.0f);
                band (v, 3, ColorType::bite, 34.0f, 0.0f, 48.0f, 55.0f);
            });

            add ("Telephone Fuzz", "Vocals", [] (V& v)
            {
                //  Band-limited fuzz FX voice.
                v.emplace_back (p::xover1, 340.0f);
                v.emplace_back (p::xover3, 3400.0f);
                bandMute (v, 0);
                bandMute (v, 3);
                band (v, 1, ColorType::fuzz, 58.0f, 10.0f, 10.0f);
                band (v, 2, ColorType::fuzz, 70.0f, 20.0f, 15.0f);
            });

            // --- UTILITY / MIX ------------------------------------------------
            add ("Gentle Four-Band Glue", "Mix", [] (V& v)
            {
                //  Barely-there colour for a mix bus.
                v.emplace_back (p::mix, 72.0f);
                band (v, 0, ColorType::warm, 18.0f, -18.0f);
                band (v, 1, ColorType::warm, 22.0f, -12.0f);
                band (v, 2, ColorType::warm, 20.0f, -10.0f, 4.0f);
                band (v, 3, ColorType::warm, 16.0f, 0.0f, 6.0f);
            });

            add ("Low-End Safe Saturation", "Mix", [] (V& v)
            {
                //  Colour everywhere except the untouchable low end.
                bandBypass (v, 0);
                band (v, 1, ColorType::warm, 36.0f, -10.0f);
                band (v, 2, ColorType::iron, 34.0f, 0.0f, 5.0f);
                band (v, 3, ColorType::bite, 24.0f, 10.0f, 12.0f);
            });

            add ("Brightness Without EQ", "Mix", [] (V& v)
            {
                //  Perceived brightness from created harmonics.
                band (v, 0, ColorType::warm, 8.0f);
                band (v, 1, ColorType::warm, 14.0f);
                band (v, 2, ColorType::bite, 26.0f, 12.0f, 22.0f, 10.0f);
                band (v, 3, ColorType::bite, 38.0f, 15.0f, 42.0f, 15.0f);
            });

            add ("Parallel Color", "Mix", [] (V& v)
            {
                //  A driven copy folded quietly under the dry signal.
                v.emplace_back (p::mix, 40.0f);
                band (v, 0, ColorType::iron, 45.0f, -20.0f);
                band (v, 1, ColorType::iron, 55.0f);
                band (v, 2, ColorType::iron, 52.0f, 15.0f, 8.0f);
                band (v, 3, ColorType::warm, 35.0f, 0.0f, 12.0f);
            });

            //  --- SHAPE demonstrations ------------------------------------
            //  Four presets whose only job is to make the two sides of SHAPE
            //  obvious on the material each side is for. Deliberately plain
            //  otherwise: no Space, no crossover moves, so the only thing to
            //  listen to is the axis.
            add ("BASS - Body Lift", "Shape", [] (V& v)
            {
                //  Full BODY on the two bands a bass line lives in: the note's
                //  decay gets colour it was not getting, the pluck stays put.
                band (v, 0, ColorType::warm, 55.0f, -100.0f);
                band (v, 1, ColorType::warm, 48.0f, -85.0f);
                band (v, 2, ColorType::iron, 25.0f, -40.0f);
                band (v, 3, ColorType::warm, 12.0f);
            });

            add ("808 - Body Sustain", "Shape", [] (V& v)
            {
                //  An 808 is mostly tail. BODY on the low band with the tone
                //  left alone, so the long part thickens without the attack
                //  changing character.
                v.emplace_back (p::xover1, 95.0f);
                band (v, 0, ColorType::warm, 62.0f, -100.0f);
                band (v, 1, ColorType::iron, 40.0f, -70.0f);
                band (v, 2, ColorType::warm, 18.0f);
                band (v, 3, ColorType::warm, 10.0f);
            });

            add ("KICK - Attack Color", "Shape", [] (V& v)
            {
                //  The mirror: ATTACK on the bands that carry a kick's click,
                //  so the hit gets crunch and the body stays clean.
                band (v, 0, ColorType::iron, 45.0f, 55.0f);
                band (v, 1, ColorType::bite, 52.0f, 85.0f);
                band (v, 2, ColorType::bite, 58.0f, 100.0f);
                band (v, 3, ColorType::bite, 40.0f, 80.0f);
            });

            add ("MELODY - Attack Bite", "Shape", [] (V& v)
            {
                //  Plucked material: full ATTACK where the pick lives.
                band (v, 0, ColorType::warm, 20.0f);
                band (v, 1, ColorType::bite, 45.0f, 70.0f);
                band (v, 2, ColorType::bite, 62.0f, 100.0f, 15.0f);
                band (v, 3, ColorType::bite, 50.0f, 90.0f, 10.0f);
            });


            // ==================================================================
            //  SIGNATURE SET
            //
            //  These differ from the presets above in one structural way: they
            //  MOVE THE CROSSOVERS. The four bands are the instrument, and
            //  leaving them at 120 / 700 / 4500 for every job wastes them -
            //  a preset that wants the drum "thwack" region and a preset that
            //  wants a lead's presence are asking for different bands, not
            //  different drives.
            //
            //  The frequency targets are the ones mixing practice actually
            //  uses: 200-400 Hz for punch, 400-600 Hz for the boxiness to keep
            //  out of it, 2-5 kHz for presence, 5 kHz for a snare's crack, and
            //  for sub work a split low enough that the fundamental and its
            //  second harmonic land in different bands.
            //
            //  Voiced BOLD on purpose. Every one of these does something you
            //  can hear the moment it loads; Band Mix and Master Mix are the
            //  way back down. A preset nobody can hear teaches nothing about
            //  the plug-in.
            // ==================================================================

            // --- SUB ------------------------------------------------------------
            add ("SUB - Translate Small Speakers", "Sub", [] (V& v)
            {
                //  A phone has no 40 Hz driver. What makes a sub audible there
                //  is its HARMONICS, so the split goes low enough that the
                //  fundamental sits alone in LOW while the second and third
                //  harmonics land in LOW MID, where they are driven hard and
                //  the ear reconstructs the missing fundamental from them.
                //  LOW itself is barely touched - the weight on a real system
                //  has to survive intact.
                v.emplace_back (p::xover1, 75.0f);
                v.emplace_back (p::xover2, 320.0f);
                band (v, 0, ColorType::warm, 18.0f, -20.0f);
                band (v, 1, ColorType::bite, 72.0f, -15.0f, 10.0f);
                band (v, 2, ColorType::warm, 25.0f);
                bandBypass (v, 3);
            });

            add ("SUB - Saturated Weight", "Sub", [] (V& v)
            {
                //  The opposite intent: colour the fundamental itself. IRON's
                //  feedback loop returns only the bottom of its own band, which
                //  is what reads as weight rather than as mud.
                v.emplace_back (p::xover1, 90.0f);
                band (v, 0, ColorType::iron, 62.0f, -45.0f, -12.0f);
                band (v, 1, ColorType::warm, 30.0f, -20.0f);
                band (v, 2, ColorType::warm, 20.0f);
                bandBypass (v, 3);
            });

            add ("SUB - Guard", "Sub", [] (V& v)
            {
                //  Everything above the sub gets colour; the sub is powered off
                //  and passes clean. For a mastering chain, or any time the low
                //  end is already exactly right and must not be touched.
                v.emplace_back (p::xover1, 85.0f);
                bandBypass (v, 0);
                band (v, 1, ColorType::warm, 45.0f, -15.0f);
                band (v, 2, ColorType::bite, 40.0f);
                band (v, 3, ColorType::warm, 32.0f, 0.0f, -8.0f);
            });

            // --- BASS -----------------------------------------------------------
            add ("BASS - Harmonic Stack", "Bass", [] (V& v)
            {
                //  Three different engines across three bands, so the harmonic
                //  series is built from three different curves instead of one
                //  repeated. The split at 250 puts the fundamental region and
                //  the growl region under separate control.
                v.emplace_back (p::xover1, 110.0f);
                v.emplace_back (p::xover2, 900.0f);
                band (v, 0, ColorType::warm, 52.0f, -30.0f);
                band (v, 1, ColorType::iron, 68.0f, -15.0f, 8.0f);
                band (v, 2, ColorType::bite, 58.0f, 10.0f);
                band (v, 3, ColorType::warm, 25.0f, 0.0f, -10.0f);
            });

            add ("BASS - Parallel Grit", "Bass", [] (V& v)
            {
                //  Hard drive folded under the dry bass at 45%. The dry leg is
                //  phase-aligned, so this thickens without the comb filtering a
                //  hand-built parallel bus gives you.
                v.emplace_back (p::mix, 45.0f);
                v.emplace_back (p::xover1, 120.0f);
                v.emplace_back (p::xover2, 800.0f);
                band (v, 0, ColorType::warm, 40.0f, -25.0f);
                band (v, 1, ColorType::bite, 82.0f, 0.0f, 12.0f);
                band (v, 2, ColorType::fuzz, 65.0f, 15.0f);
                band (v, 3, ColorType::bite, 45.0f, 10.0f, -6.0f);
            });

            add ("BASS - Sustain Body", "Bass", [] (V& v)
            {
                //  Full BODY on the two lowest bands: the note's tail gets
                //  density while the pluck at the front is left alone. This is
                //  the preset that shows what the new BODY actually does.
                v.emplace_back (p::xover1, 130.0f);
                band (v, 0, ColorType::warm, 58.0f, -100.0f);
                band (v, 1, ColorType::iron, 55.0f, -100.0f, 6.0f);
                band (v, 2, ColorType::warm, 30.0f, -40.0f);
                band (v, 3, ColorType::warm, 20.0f);
            });

            // --- DRUMS ----------------------------------------------------------
            add ("DRUMS - Punch 200-450", "Drums", [] (V& v)
            {
                //  The crossovers ARE this preset. LOW MID is set to 130-450,
                //  which is the thwack region, and it is driven hard with
                //  ATTACK. HIGH MID starts at 450 and is left gentle, because
                //  400-600 Hz is where boxiness lives and driving it undoes the
                //  punch you just bought.
                v.emplace_back (p::xover1, 130.0f);
                v.emplace_back (p::xover2, 450.0f);
                v.emplace_back (p::xover3, 4000.0f);
                band (v, 0, ColorType::warm, 42.0f, -20.0f);
                band (v, 1, ColorType::iron, 78.0f, 85.0f, 6.0f);
                band (v, 2, ColorType::warm, 22.0f, 20.0f, -10.0f);
                band (v, 3, ColorType::bite, 48.0f, 60.0f);
            });

            add ("DRUMS - Snare Crack", "Drums", [] (V& v)
            {
                //  HIGH starts at 3.8 kHz so the top band is exactly the crack
                //  region, and BITE with full ATTACK sharpens it. The body of
                //  the kit stays where it was.
                v.emplace_back (p::xover2, 600.0f);
                v.emplace_back (p::xover3, 3800.0f);
                band (v, 0, ColorType::warm, 28.0f, -15.0f);
                band (v, 1, ColorType::warm, 35.0f, 30.0f);
                band (v, 2, ColorType::iron, 45.0f, 55.0f);
                band (v, 3, ColorType::bite, 76.0f, 100.0f, 14.0f);
            });

            add ("DRUMS - Harmonic Kit", "Drums", [] (V& v)
            {
                //  Four engines, one per band, all moderate. The point is the
                //  DIFFERENCE between them: weight from WARM, density from
                //  IRON, presence from BITE, air texture from FUZZ.
                v.emplace_back (p::xover1, 110.0f);
                v.emplace_back (p::xover2, 500.0f);
                v.emplace_back (p::xover3, 4200.0f);
                band (v, 0, ColorType::warm, 50.0f, -30.0f);
                band (v, 1, ColorType::iron, 55.0f, 25.0f);
                band (v, 2, ColorType::bite, 52.0f, 40.0f);
                band (v, 3, ColorType::fuzz, 38.0f, 50.0f, -8.0f, 20.0f);
            });

            add ("DRUMS - Parallel Slam", "Drums", [] (V& v)
            {
                //  Everything driven far past sensible, then folded in at 30%.
                //  The dry transient survives; what you add underneath is pure
                //  harmonic weight.
                v.emplace_back (p::mix, 30.0f);
                v.emplace_back (p::xover1, 120.0f);
                v.emplace_back (p::xover2, 480.0f);
                band (v, 0, ColorType::iron, 88.0f, -30.0f);
                band (v, 1, ColorType::fuzz, 85.0f, 20.0f);
                band (v, 2, ColorType::bite, 90.0f, 40.0f);
                band (v, 3, ColorType::fuzz, 70.0f, 55.0f, -10.0f);
            });

            // --- LEAD -----------------------------------------------------------
            add ("LEAD - Forward 2k-5k", "Lead", [] (V& v)
            {
                //  HIGH MID is set to 1.2k-5k, which is the presence region a
                //  lead needs to cut. It gets BITE, whose pre-emphasis pushes
                //  the top of its own band into the clipper - so this is
                //  "distorted forward", not simply brighter.
                v.emplace_back (p::xover2, 1200.0f);
                v.emplace_back (p::xover3, 5000.0f);
                band (v, 0, ColorType::warm, 25.0f, -20.0f);
                band (v, 1, ColorType::warm, 38.0f);
                band (v, 2, ColorType::bite, 74.0f, 25.0f, 12.0f);
                band (v, 3, ColorType::warm, 35.0f, 0.0f, -12.0f);
            });

            add ("LEAD - Wide Harmonics", "Lead", [] (V& v)
            {
                //  Space diffuses ONLY the non-linear product, so the lead's
                //  own note stays centred while the harmonics it generates
                //  spread. The low band is left mono and clean underneath.
                v.emplace_back (p::xover2, 1000.0f);
                v.emplace_back (p::xover3, 5200.0f);
                band (v, 0, ColorType::warm, 20.0f);
                band (v, 1, ColorType::warm, 45.0f, -20.0f, 0.0f, 25.0f);
                band (v, 2, ColorType::iron, 62.0f, 15.0f, 6.0f, 55.0f);
                band (v, 3, ColorType::bite, 48.0f, 20.0f, 0.0f, 60.0f);
            });

            add ("LEAD - Sustain Density", "Lead", [] (V& v)
            {
                //  BODY on the mids: a held note thickens through its tail
                //  instead of only at the attack. Pairs with a long release.
                v.emplace_back (p::xover2, 1100.0f);
                v.emplace_back (p::xover3, 5000.0f);
                band (v, 0, ColorType::warm, 22.0f);
                band (v, 1, ColorType::iron, 58.0f, -100.0f);
                band (v, 2, ColorType::warm, 62.0f, -100.0f, 8.0f);
                band (v, 3, ColorType::warm, 30.0f, -40.0f);
            });

            // --- PARALLEL -------------------------------------------------------
            add ("PARALLEL - Warm Under", "Parallel", [] (V& v)
            {
                //  The safe one. Heavy WARM everywhere, blended at 35%: adds
                //  weight and glue to anything without changing its character.
                v.emplace_back (p::mix, 35.0f);
                band (v, 0, ColorType::warm, 70.0f, -40.0f);
                band (v, 1, ColorType::warm, 72.0f, -25.0f);
                band (v, 2, ColorType::warm, 68.0f);
                band (v, 3, ColorType::warm, 55.0f, 0.0f, -8.0f);
            });

            add ("PARALLEL - Iron Density", "Parallel", [] (V& v)
            {
                //  IRON in parallel is the one that makes a thin source feel
                //  expensive: its core-loss loop returns the bottom of each
                //  band, so the blend adds weight rather than fizz.
                v.emplace_back (p::mix, 40.0f);
                v.emplace_back (p::xover2, 600.0f);
                band (v, 0, ColorType::iron, 75.0f, -35.0f);
                band (v, 1, ColorType::iron, 80.0f, -20.0f);
                band (v, 2, ColorType::iron, 72.0f, 15.0f);
                band (v, 3, ColorType::warm, 45.0f, 0.0f, -10.0f);
            });

            add ("PARALLEL - Edge Blend", "Parallel", [] (V& v)
            {
                //  BITE and FUZZ hard, folded in at 25%. For sources that are
                //  already dense and need aggression rather than weight.
                v.emplace_back (p::mix, 25.0f);
                v.emplace_back (p::xover2, 700.0f);
                v.emplace_back (p::xover3, 4500.0f);
                band (v, 0, ColorType::warm, 45.0f, -30.0f);
                band (v, 1, ColorType::bite, 85.0f, 20.0f);
                band (v, 2, ColorType::fuzz, 78.0f, 35.0f);
                band (v, 3, ColorType::bite, 70.0f, 45.0f, 8.0f);
            });

            return out;
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
