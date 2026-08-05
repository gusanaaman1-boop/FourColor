// Deterministic screenshot renderer.
//
//   FourColorShot [outputDir]     render the reference set into outputDir
//                                 (default: ./ui-shots)
//
// A fresh processor, a deterministic audio warm-up (so meters show real data),
// fixed sizes and states. The same binary on the same machine produces
// equivalent PNGs, which is what makes UI regressions visible in review.

#include <cstdio>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "../PluginEditor.h"
#include "../PluginProcessor.h"

using namespace juce;
using namespace fourcolor;

namespace
{
    struct Variant
    {
        String fileName;
        int width, height;
        int selectedBand;
        bool driven;          // push audio + hot drives so curves/meters differ
    };

    void renderVariant (const Variant& v, const File& outDir)
    {
        FourColorProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        if (v.driven)
        {
            //  Distinct settings per band so the shot shows a real state.
            auto set = [&proc] (const String& id, float plain)
            {
                auto* p = proc.apvts.getParameter (id);
                p->setValueNotifyingHost (p->convertTo0to1 (plain));
            };
            set (param::band (0, param::color), (float) (int) ColorType::warm);
            set (param::band (1, param::color), (float) (int) ColorType::iron);
            set (param::band (2, param::color), (float) (int) ColorType::bite);
            set (param::band (3, param::color), (float) (int) ColorType::fuzz);
            set (param::band (0, param::drive), 55.0f);
            set (param::band (1, param::drive), 40.0f);
            set (param::band (2, param::drive), 65.0f);
            set (param::band (3, param::drive), 80.0f);
            set (param::band (2, param::behavior), 60.0f);
            set (param::band (0, param::space), 25.0f);
            set (param::band (3, param::level), -4.0f);

            //  Deterministic broadband warm-up (a pink-ish bed of sines) so the
            //  spectrum display and meters show a full, real picture.
            MidiBuffer midi;
            AudioBuffer<float> buf (2, 512);
            int s = 0;

            struct Partial { float freq, amp, phase; };
            std::vector<Partial> partials;
            {
                Random rng (7);
                for (float f = 40.0f; f < 18000.0f; f *= 1.16f)
                    partials.push_back ({ f * (0.97f + 0.06f * rng.nextFloat()),
                                          0.55f / std::sqrt (f / 40.0f),
                                          rng.nextFloat() * MathConstants<float>::twoPi });
            }

            for (int blk = 0; blk < 40; ++blk)
            {
                for (int i = 0; i < 512; ++i, ++s)
                {
                    float x = 0.0f;
                    for (const auto& p : partials)
                        x += p.amp * std::sin (MathConstants<float>::twoPi * p.freq * s / 48000.0f + p.phase);
                    x *= 0.16f;
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x);
                }
                proc.processBlock (buf, midi);
            }
        }

        proc.apvts.state.setProperty ("selectedBand", v.selectedBand, nullptr);
        proc.apvts.state.setProperty ("editorWidth", v.width, nullptr);
        proc.apvts.state.setProperty ("editorHeight", v.height, nullptr);

        std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (v.width, v.height);

        //  Let timers (meter decay, display refresh) fire a few times.
        for (int i = 0; i < 12; ++i)
            MessageManager::getInstance()->runDispatchLoopUntil (20);

        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

        const File outFile = outDir.getChildFile (v.fileName);
        outFile.deleteFile();
        FileOutputStream stream (outFile);
        if (stream.openedOk())
        {
            PNGImageFormat png;
            png.writeImageToStream (image, stream);
            std::printf ("  wrote %s (%dx%d)\n", outFile.getFullPathName().toRawUTF8(),
                         image.getWidth(), image.getHeight());
        }
        else
        {
            std::printf ("  FAILED to write %s\n", outFile.getFullPathName().toRawUTF8());
        }

        editor.reset();
        proc.releaseResources();
    }
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;

    const File outDir = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                 : File::getCurrentWorkingDirectory().getChildFile ("ui-shots");
    outDir.createDirectory();

    const Variant variants[] = {
        { "fourcolor-min-900x560.png",      900,  560, 0, true  },
        { "fourcolor-default-980x620.png",  980,  620, 0, true  },
        { "fourcolor-band-hmid.png",        980,  620, 2, true  },
        { "fourcolor-band-high.png",        980,  620, 3, true  },
        { "fourcolor-large-1400x900.png",  1400,  900, 1, true  },
        { "fourcolor-init.png",             980,  620, 0, false },
    };

    std::printf ("FOUR COLOR screenshot set -> %s\n", outDir.getFullPathName().toRawUTF8());
    for (const auto& v : variants)
        renderVariant (v, outDir);

    return 0;
}
