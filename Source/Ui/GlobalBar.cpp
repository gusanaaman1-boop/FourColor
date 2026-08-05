#include "GlobalBar.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    //  Round amber-ring AUTO LEVEL toggle.
    class GlobalBar::RoundToggle : public juce::Button
    {
    public:
        RoundToggle() : juce::Button ("AUTO LEVEL") { setClickingTogglesState (true); }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float d = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 8.0f;
            auto circle = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());
            if (down) circle.translate (0.0f, 1.0f);

            const bool on = getToggleState();

            if (on)
                for (float grow = 5.0f; grow >= 1.0f; grow -= 1.5f)
                {
                    g.setColour (tokens::amberRing.withAlpha (0.09f * (1.0f - grow / 6.0f)));
                    g.drawEllipse (circle.expanded (grow), 2.0f);
                }

            juce::ColourGradient body (juce::Colour (0xff1d2229), circle.getCentreX(), circle.getY(),
                                       juce::Colour (0xff12161b), circle.getCentreX(), circle.getBottom(),
                                       false);
            g.setGradientFill (body);
            g.fillEllipse (circle);

            auto ring = on ? tokens::amberRing
                           : (highlighted ? tokens::borderHover : tokens::borderNormal);
            g.setColour (ring);
            g.drawEllipse (circle.reduced (1.0f), on ? 2.2f : 1.4f);
        }
    };

    GlobalBar::GlobalBar (FourColorProcessor& processor)
        : proc (processor)
    {
        auto& apvts = proc.apvts;

        input = std::make_unique<Knob> (apvts, param::input, "INPUT",
                                        tokens::neutralArcII, Knob::Size::medium, "Input trim");
        globalDrive = std::make_unique<Knob> (apvts, param::globalDrive, "GLOBAL DRIVE",
                                              tokens::bandLow.withMultipliedSaturation (0.75f),
                                              Knob::Size::medium,
                                              "Scales all four band drives around their settings");
        globalTone = std::make_unique<Knob> (apvts, param::globalTone, "GLOBAL TONE",
                                             tokens::neutralArcII, Knob::Size::medium,
                                             "Overall tilt around 800 Hz");
        mix = std::make_unique<Knob> (apvts, param::mix, "MIX",
                                      tokens::neutralArc, Knob::Size::medium,
                                      "Latency-aligned dry/wet");
        output = std::make_unique<Knob> (apvts, param::output, "OUTPUT",
                                         tokens::neutralArcII, Knob::Size::medium, "Output trim");

        for (auto* k : { input.get(), globalDrive.get(), globalTone.get(), mix.get(), output.get() })
            addAndMakeVisible (*k);

        autoLevelButton = std::make_unique<RoundToggle>();
        autoLevelButton->setTooltip ("Slow internal loudness match (max +/-12 dB)");
        addAndMakeVisible (*autoLevelButton);
        autoLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, param::autoLevel, *autoLevelButton);

        startTimerHz (30);
    }

    GlobalBar::~GlobalBar() = default;

    void GlobalBar::timerCallback()
    {
        auto update = [] (float& level, float& hold, int& age, float peak)
        {
            level = juce::jmax (peak, level * 0.82f);       // smooth visual release
            if (peak >= hold) { hold = peak; age = 0; }
            else if (++age > 24) hold = juce::jmax (level, hold * 0.90f);
        };

        for (int c = 0; c < 2; ++c)
        {
            update (inLevel[c], inPeakHold[c], inPeakAge[c], proc.readAndResetInputPeak (c));
            update (outLevel[c], outPeakHold[c], outPeakAge[c], proc.readAndResetOutputPeak (c));
        }

        repaint (inMeter.getSmallestIntegerContainer().expanded (10, 2));
        repaint (outMeter.getSmallestIntegerContainer().expanded (10, 2));
    }

    void GlobalBar::drawStereoMeter (juce::Graphics& g, juce::Rectangle<float> area,
                                     const float* levels, const float* peaks) const
    {
        auto labels = area.removeFromBottom (11.0f);
        const float barW = 6.0f;
        const float gap = 4.0f;
        const float totalW = barW * 2.0f + gap;
        auto bars = area.withWidth (totalW).withX (area.getCentreX() - totalW * 0.5f);

        constexpr int segments = 22;

        for (int c = 0; c < 2; ++c)
        {
            auto bar = bars.removeFromLeft (barW);
            if (c == 0) bars.removeFromLeft (gap);

            g.setColour (tokens::analyzerBack);
            g.fillRoundedRectangle (bar, 2.0f);

            const float db = juce::Decibels::gainToDecibels (levels[c], -60.0f);
            const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * segments);

            for (int s = 0; s < segments; ++s)
            {
                const float t = (float) s / (segments - 1);
                const auto c0 = t > 0.93f ? tokens::meterPeak
                              : t > 0.80f ? tokens::meterHigh
                              : t > 0.58f ? tokens::meterMid
                                          : tokens::meterLow;
                const float sy = bar.getBottom() - bar.getHeight() * (float) (s + 1) / segments;
                g.setColour (s < lit ? c0 : c0.withAlpha (0.13f));
                g.fillRect (bar.getX(), sy + 0.8f, bar.getWidth(),
                            bar.getHeight() / segments - 1.6f);
            }

            //  Short peak hold.
            const float pdb = juce::Decibels::gainToDecibels (peaks[c], -60.0f);
            if (pdb > -59.0f)
            {
                const float pt = juce::jlimit (0.0f, 1.0f, (pdb + 60.0f) / 60.0f);
                const float py = bar.getBottom() - bar.getHeight() * pt;
                g.setColour (pt > 0.93f ? tokens::meterPeak : tokens::textPrimary.withAlpha (0.75f));
                g.fillRect (bar.getX(), py - 1.0f, bar.getWidth(), 1.6f);
            }
        }

        g.setFont (uiFont (9.0f));
        g.setColour (tokens::textMuted);
        g.drawText ("L", labels.removeFromLeft (labels.getWidth() * 0.5f), juce::Justification::centred);
        g.drawText ("R", labels, juce::Justification::centred);
    }

    void GlobalBar::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (tokens::globalBack);
        g.fillRoundedRectangle (bounds, metric::corner);
        g.setColour (tokens::borderSoft);
        g.drawLine (bounds.getX() + metric::corner, bounds.getY() + 0.5f,
                    bounds.getRight() - metric::corner, bounds.getY() + 0.5f, 1.0f);

        for (int x : separatorX)
        {
            g.setColour (tokens::borderSoft);
            g.fillRect ((float) x, bounds.getY() + 14.0f, 1.0f, bounds.getHeight() - 28.0f);
        }

        drawStereoMeter (g, inMeter, inLevel, inPeakHold);
        drawStereoMeter (g, outMeter, outLevel, outPeakHold);

        g.setFont (captionFont (11.5f));
        g.setColour (tokens::textSecondary);
        g.drawText ("AUTO LEVEL",
                    autoLevelButton->getBounds().withY (autoLevelButton->getY() - 19)
                                                .withHeight (15).expanded (34, 0),
                    juce::Justification::centred);
    }

    void GlobalBar::resized()
    {
        auto area = getLocalBounds().reduced (12, 9);
        separatorX.clear();

        inMeter = area.removeFromLeft (26).toFloat().reduced (0.0f, 6.0f);
        area.removeFromLeft (6);
        outMeter = area.removeFromRight (26).toFloat().reduced (0.0f, 6.0f);
        area.removeFromRight (6);

        //  Six equal groups: INPUT | GLOBAL DRIVE | AUTO LEVEL | GLOBAL TONE |
        //  MIX | OUTPUT.
        const int groups = 6;
        const int gw = area.getWidth() / groups;

        auto place = [] (Knob& k, juce::Rectangle<int> cell)
        {
            const int w = juce::jmin (cell.getWidth(), 104);
            k.setBounds (cell.withSizeKeepingCentre (w, cell.getHeight()));
        };

        place (*input, area.removeFromLeft (gw));
        separatorX.push_back (area.getX());
        place (*globalDrive, area.removeFromLeft (gw));

        separatorX.push_back (area.getX());
        auto middle = area.removeFromLeft (gw);
        autoLevelButton->setBounds (middle.withSizeKeepingCentre (46, 46)
                                          .withY (middle.getY() + 26));

        separatorX.push_back (area.getX());
        place (*globalTone, area.removeFromLeft (gw));

        separatorX.push_back (area.getX());
        place (*mix, area.removeFromLeft (gw));

        separatorX.push_back (area.getX());
        place (*output, area);
    }
}
