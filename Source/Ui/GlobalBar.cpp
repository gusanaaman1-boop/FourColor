#include "GlobalBar.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    //  The round, ring-glowing AUTO LEVEL toggle of the mockup.
    class GlobalBar::RoundToggle : public juce::Button
    {
    public:
        RoundToggle() : juce::Button ("AUTO LEVEL") { setClickingTogglesState (true); }

        void paintButton (juce::Graphics& g, bool highlighted, bool) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float d = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 8.0f;
            auto circle = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());

            const auto ringColour = juce::Colour (0xffe0a33c);
            const bool on = getToggleState();

            if (on)
            {
                //  Soft glow.
                for (float grow = 6.0f; grow > 0.0f; grow -= 2.0f)
                {
                    g.setColour (ringColour.withAlpha (0.05f * (7.0f - grow)));
                    g.drawEllipse (circle.expanded (grow), 2.0f);
                }
            }

            juce::ColourGradient body (colour::knobBody.brighter (highlighted ? 0.25f : 0.15f),
                                       circle.getX(), circle.getY(),
                                       colour::knobBody.darker (0.3f),
                                       circle.getX(), circle.getBottom(), false);
            g.setGradientFill (body);
            g.fillEllipse (circle);

            g.setColour (on ? ringColour : colour::panelLine.brighter (0.1f));
            g.drawEllipse (circle.reduced (1.0f), on ? 2.2f : 1.4f);
        }
    };

    GlobalBar::GlobalBar (FourColorProcessor& processor)
        : proc (processor)
    {
        auto& apvts = proc.apvts;
        const auto neutral = colour::accent.withAlpha (0.85f);

        input       = std::make_unique<Knob> (apvts, param::input, "INPUT", neutral, "Input trim");
        globalDrive = std::make_unique<Knob> (apvts, param::globalDrive, "GLOBAL DRIVE", neutral,
                                              "Scales all four band drives around their settings");
        globalTone  = std::make_unique<Knob> (apvts, param::globalTone, "GLOBAL TONE", neutral,
                                              "Tilt around 800 Hz");
        mix         = std::make_unique<Knob> (apvts, param::mix, "MIX", neutral,
                                              "Latency-aligned dry/wet");
        output      = std::make_unique<Knob> (apvts, param::output, "OUTPUT", neutral, "Output trim");

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
        const float in  = proc.readAndResetInputPeak();
        const float out = proc.readAndResetOutputPeak();

        displayedIn  = juce::jmax (in, displayedIn * 0.85f);
        displayedOut = juce::jmax (out, displayedOut * 0.85f);
        repaint();
    }

    void GlobalBar::paint (juce::Graphics& g)
    {
        g.setColour (colour::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (colour::panelLine);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

        //  Section separators.
        g.setColour (colour::panelLine.withAlpha (0.7f));
        for (int x : separatorX)
            g.fillRect (x, 12, 1, getHeight() - 24);

        //  Segmented vertical meters flanking INPUT and OUTPUT.
        auto drawMeter = [&g] (juce::Rectangle<float> r, float peak)
        {
            const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
            const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            constexpr int segments = 18;
            const int lit = (int) std::round (t * segments);

            for (int s = 0; s < segments; ++s)
            {
                const float sy = r.getBottom() - r.getHeight() * (float) (s + 1) / segments;
                const bool isLit = s < lit;
                const auto c = s >= segments - 3 ? juce::Colour (0xffd05545)
                             : s >= segments - 7 ? juce::Colour (0xffe0a33c)
                                                 : colour::textDim;
                g.setColour (isLit ? c : c.withAlpha (0.15f));
                g.fillRect (r.getX(), sy + 1.0f, r.getWidth(), r.getHeight() / segments - 2.0f);
            }
        };

        drawMeter (inMeter, displayedIn);
        drawMeter (outMeter, displayedOut);

        //  AUTO LEVEL caption above the round toggle.
        g.setFont (labelFont (11.5f));
        g.setColour (colour::textDim);
        g.drawText ("AUTO LEVEL", autoLevelButton->getBounds().withY (autoLevelButton->getY() - 16)
                                                              .withHeight (14).expanded (30, 0),
                    juce::Justification::centred);
    }

    void GlobalBar::resized()
    {
        auto area = getLocalBounds().reduced (14, 8);
        separatorX.clear();

        const int sectionW = area.getWidth() / 5;

        auto inputSection = area.removeFromLeft (sectionW);
        inMeter = inputSection.removeFromLeft (8).toFloat().reduced (0.0f, 10.0f);
        inputSection.removeFromLeft (6);
        input->setBounds (inputSection.reduced (juce::jmax (0, (inputSection.getWidth() - 96) / 2), 0));

        separatorX.push_back (area.getX());
        globalDrive->setBounds (area.removeFromLeft (sectionW)
                                    .reduced (juce::jmax (0, (sectionW - 96) / 2), 0));

        separatorX.push_back (area.getX());
        auto centreSection = area.removeFromLeft (sectionW);
        globalTone->setBounds (centreSection.removeFromLeft (centreSection.getWidth() / 2)
                                            .withTrimmedLeft (4));
        autoLevelButton->setBounds (centreSection.withTrimmedTop (18)
                                                 .withSizeKeepingCentre (44, 44));

        separatorX.push_back (area.getX());
        mix->setBounds (area.removeFromLeft (sectionW)
                            .reduced (juce::jmax (0, (sectionW - 96) / 2), 0));

        separatorX.push_back (area.getX());
        auto outputSection = area;
        outMeter = outputSection.removeFromRight (8).toFloat().reduced (0.0f, 10.0f);
        outputSection.removeFromRight (6);
        output->setBounds (outputSection.reduced (juce::jmax (0, (outputSection.getWidth() - 96) / 2), 0));
    }
}
