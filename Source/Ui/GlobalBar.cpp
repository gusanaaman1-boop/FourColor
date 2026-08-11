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

        mix = std::make_unique<Knob> (apvts, param::mix, "MASTER MIX",
                                      tokens::neutralArc, Knob::Size::medium,
                                      "Latency-aligned dry/wet across the whole plug-in.");
        output = std::make_unique<Knob> (apvts, param::output, "OUTPUT",
                                         tokens::neutralArcII, Knob::Size::medium,
                                         "Final level leaving the plugin.");

        for (auto* k : { mix.get(), output.get() })
            addAndMakeVisible (*k);

        autoLevelButton = std::make_unique<RoundToggle>();
        autoLevelButton->setTooltip ("Slow internal loudness match (max +/-12 dB)");
        addAndMakeVisible (*autoLevelButton);
        autoLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, param::autoLevel, *autoLevelButton);

    }

    GlobalBar::~GlobalBar() = default;

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

        g.setFont (captionFont (11.5f));
        g.setColour (tokens::textSecondary);
        g.drawText ("AUTO LEVEL",
                    autoLevelButton->getBounds().withY (autoLevelButton->getY() - 19)
                                                .withHeight (15).expanded (34, 0),
                    juce::Justification::centred);
    }

    void GlobalBar::setMeterWidth (int w)
    {
        if (meterWidth != w)
        {
            meterWidth = w;
            resized();
            repaint();
        }
    }

    void GlobalBar::resized()
    {
        auto area = getLocalBounds().reduced (14, 9);
        separatorX.clear();

        auto place = [] (Knob& k, juce::Rectangle<int> cell)
        {
            const int w = juce::jmin (cell.getWidth(), 112);
            k.setBounds (cell.withSizeKeepingCentre (w, cell.getHeight()));
        };

        //  OUTPUT is the same parameter as the trim under the output meter, and
        //  it used to sit at five sixths of the bar while the meter column sat
        //  hard against the right edge - two controls for one thing, on two
        //  different lines. It now centres on the meter column's own centre, so
        //  the output meter, its trim and this knob form a single column.
        if (meterWidth > 0)
        {
            const int outCentre = getWidth() - meterWidth / 2;

            //  The cell has to FIT, not merely be centred: the meter column's
            //  axis sits only half a meter width from the right edge, so a
            //  112 px cell centred there hangs 26 px outside the bar and the
            //  knob is clipped. Measured off the rendered pixels, the dial came
            //  out 20 px left of the meter it was supposed to line up with -
            //  the bounds agreed while the picture did not.
            //
            //  So the width is whatever fits symmetrically about that axis.
            //  It lands close to the meter trim's own diameter, which is right:
            //  meter, trim and OUTPUT then read as one narrow column.
            const int room = juce::jmin (outCentre, getWidth() - outCentre);
            const int outW = juce::jmin (112, room * 2);

            auto outputCell = juce::Rectangle<int> (outCentre - outW / 2, area.getY(),
                                                    outW, area.getHeight());
            place (*output, outputCell);

            area = area.withRight (outputCell.getX() - 14);
        }
        else
        {
            place (*output, area.removeFromRight (area.getWidth() / 3));
        }

        const int gw = area.getWidth() / 2;

        auto first = area.removeFromLeft (gw);
        autoLevelButton->setBounds (first.withSizeKeepingCentre (48, 48)
                                         .withY (first.getY() + 26));

        separatorX.push_back (area.getX());
        place (*mix, area);

        separatorX.push_back (area.getRight() + 14);
    }
}
