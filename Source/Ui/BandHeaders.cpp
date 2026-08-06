#include "BandHeaders.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    namespace
    {
        const char* const bandNames[numBands]  = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const char* const colorNames[4]        = { "WARM", "IRON", "BITE", "FUZZ" };
        constexpr float minHz = 20.0f, maxHz = 20000.0f;
    }

    //  --- one icon button -------------------------------------------------------
    class BandHeaders::IconToggle : public juce::Button
    {
    public:
        enum class Kind { power, solo, mute };

        IconToggle (Kind k, juce::Colour accentColour)
            : juce::Button ("band"), kind (k), accent (accentColour)
        {
            setClickingTogglesState (true);
        }

        //  Power reads inverted: the parameter is bypass, the light is power.
        void setInverted (bool shouldInvert) noexcept { inverted = shouldInvert; }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const bool lit = inverted ? ! getToggleState() : getToggleState();
            auto bounds = getLocalBounds().toFloat().reduced (2.0f);
            if (down) bounds.translate (0.0f, 0.5f);

            //  Background only when the state is the notable one, so a row of
            //  four default bands reads as calm.
            const bool notable = kind == Kind::power ? ! lit : lit;
            if (notable || highlighted)
            {
                auto fill = kind == Kind::power ? tokens::meterHigh
                          : kind == Kind::solo  ? accent
                                                : tokens::textMuted;
                g.setColour (fill.withAlpha (notable ? 0.85f : 0.16f));
                g.fillEllipse (bounds);
            }

            g.setColour (notable ? juce::Colour (0xff14181e)
                       : lit     ? tokens::textSecondary
                                 : tokens::textDisabled);

            switch (kind)
            {
                case Kind::power:
                {
                    //  IEC power mark: broken ring plus a stem.
                    const auto c = bounds.getCentre();
                    const float r = bounds.getWidth() * 0.28f;
                    juce::Path arc;
                    arc.addCentredArc (c.x, c.y + 0.5f, r, r, 0.0f, 0.7f,
                                       juce::MathConstants<float>::twoPi - 0.7f, true);
                    g.strokePath (arc, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
                    g.drawLine (c.x, c.y - r - 1.0f, c.x, c.y + 0.5f, 1.5f);
                    break;
                }

                case Kind::solo:
                case Kind::mute:
                    g.setFont (captionFont (10.0f, true));
                    g.drawText (kind == Kind::solo ? "S" : "M", getLocalBounds(),
                                juce::Justification::centred);
                    break;
            }
        }

    private:
        Kind kind;
        juce::Colour accent;
        bool inverted = false;
    };

    // --- headers ------------------------------------------------------------------
    BandHeaders::BandHeaders (FourColorProcessor& processor)
        : proc (processor)
    {
        auto& state = proc.apvts;

        for (int b = 0; b < numBands; ++b)
        {
            auto& band = bands[b];
            const auto accent = tokens::band[b];

            band.power = std::make_unique<IconToggle> (IconToggle::Kind::power, accent);
            band.solo  = std::make_unique<IconToggle> (IconToggle::Kind::solo, accent);
            band.mute  = std::make_unique<IconToggle> (IconToggle::Kind::mute, accent);

            band.power->setInverted (true);
            band.power->setTooltip ("Bypasses colour processing for this band while keeping "
                                    "the frequency range clean.");
            band.solo->setTooltip ("Auditions this band by itself.");
            band.mute->setTooltip ("Removes this frequency band from the output.");

            for (auto* button : { band.power.get(), band.solo.get(), band.mute.get() })
            {
                //  Clicking a button must not also change the selected band.
                button->setInterceptsMouseClicks (true, false);
                addAndMakeVisible (*button);
            }

            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
            band.aPower = std::make_unique<BA> (state, param::band (b, param::bypass), *band.power);
            band.aSolo  = std::make_unique<BA> (state, param::band (b, param::solo), *band.solo);
            band.aMute  = std::make_unique<BA> (state, param::band (b, param::mute), *band.mute);

            //  Colour name and drive percentage in the caption, straight off the
            //  parameters so host automation moves them.
            if (auto* p = state.getParameter (param::band (b, param::color)))
            {
                band.colorAttachment = std::make_unique<juce::ParameterAttachment> (
                    *p, [this, b] (float v) { bands[b].colorIndex = juce::roundToInt (v); repaint(); },
                    nullptr);
                band.colorAttachment->sendInitialUpdate();
            }

            if (auto* p = state.getParameter (param::band (b, param::drive)))
            {
                band.driveAttachment = std::make_unique<juce::ParameterAttachment> (
                    *p, [this, b] (float v) { bands[b].drivePercent = v; repaint(); }, nullptr);
                band.driveAttachment->sendInitialUpdate();
            }
        }

        startTimerHz (30);
    }

    BandHeaders::~BandHeaders() = default;

    void BandHeaders::timerCallback()
    {
        //  Power state drives a 150 ms fade that the analyzer reads too, so the
        //  header and the band's colour go quiet together.
        constexpr float step = 1.0f / (0.150f * 30.0f);
        bool needsRepaint = false;

        for (int b = 0; b < numBands; ++b)
        {
            auto& band = bands[b];
            const bool powered = ! band.power->getToggleState();

            if (powered != band.powered)
            {
                band.powered = powered;
                needsRepaint = true;
            }

            const float target = powered ? 1.0f : 0.0f;
            if (std::abs (band.powerFade - target) > 1.0e-3f)
            {
                band.powerFade += juce::jlimit (-step, step, target - band.powerFade);
                needsRepaint = true;
            }
            else if (band.powerFade != target)
            {
                band.powerFade = target;
                needsRepaint = true;
            }
        }

        if (needsRepaint)
            repaint();
    }

    void BandHeaders::setSelectedBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        repaint();
    }

    void BandHeaders::setCutFrequencies (float f1, float f2, float f3)
    {
        if (cutHz[0] != f1 || cutHz[1] != f2 || cutHz[2] != f3)
        {
            cutHz[0] = f1; cutHz[1] = f2; cutHz[2] = f3;
            resized();
            repaint();
        }
    }

    //  Each header sits over its own band's frequency range, using the same log
    //  mapping the analyzer plot uses, so a header always points at the region
    //  it controls even while a crossover is being dragged.
    juce::Rectangle<int> BandHeaders::headerBounds (int band) const
    {
        const auto area = getLocalBounds().toFloat()
                              .withTrimmedLeft (metric::leftAxis)
                              .withTrimmedRight (10.0f);

        const float edges[5] = { minHz, cutHz[0], cutHz[1], cutHz[2], maxHz };
        auto xFor = [&area] (float hz)
        {
            const float t = std::log (hz / minHz) / std::log (maxHz / minHz);
            return area.getX() + t * area.getWidth();
        };

        const float x0 = xFor (edges[band]);
        const float x1 = xFor (edges[band + 1]);
        return juce::Rectangle<float> (x0, area.getY(), juce::jmax (2.0f, x1 - x0),
                                       area.getHeight()).toNearestInt();
    }

    void BandHeaders::resized()
    {
        for (int b = 0; b < numBands; ++b)
        {
            auto area = headerBounds (b).reduced (4, 2);

            //  Buttons on the right, 24 px hit targets as the brief requires,
            //  in the same order in every band.
            auto buttons = area.removeFromRight (juce::jmin (area.getWidth(), 78));
            const int size = juce::jmin (24, buttons.getHeight());

            auto place = [&buttons, size] (juce::Component& c)
            {
                c.setBounds (buttons.removeFromLeft (size + 2)
                                 .withSizeKeepingCentre (size, size));
            };

            place (*bands[b].power);
            place (*bands[b].solo);
            place (*bands[b].mute);
        }
    }

    void BandHeaders::paint (juce::Graphics& g)
    {
        for (int b = 0; b < numBands; ++b)
        {
            const auto area = headerBounds (b).toFloat().reduced (2.0f, 1.0f);
            if (area.getWidth() < 8.0f)
                continue;

            auto& band = bands[b];
            const bool isSelected = b == selectedBand;
            const bool soloed = band.solo->getToggleState();
            const bool muted  = band.mute->getToggleState();
            const float lit = band.powerFade;

            //  A powered band wears its colour; an unpowered one goes neutral.
            //  Mute uses a DIFFERENT language from power off - it darkens the
            //  whole plate rather than draining the colour - so the two states
            //  never read as the same thing.
            const auto accent = tokens::band[b];
            const auto plate = tokens::panelBase.interpolatedWith (accent, 0.10f * lit);

            g.setColour (muted ? tokens::panelPressed : plate);
            g.fillRoundedRectangle (area, metric::cornerSmall);

            g.setColour (isSelected ? accent.withAlpha (0.30f + 0.55f * lit)
                                    : tokens::borderSoft);
            g.drawRoundedRectangle (area.reduced (0.5f), metric::cornerSmall,
                                    isSelected ? 1.3f : 1.0f);

            //  Caption: BAND . COLOUR . DRIVE, dropped progressively as the
            //  header narrows rather than squashed.
            auto text = area.reduced (7.0f, 0.0f);
            text = text.withTrimmedRight (82.0f);

            const auto nameColour = tokens::textPrimary
                                        .interpolatedWith (accent, 0.55f * lit)
                                        .withMultipliedAlpha (muted ? 0.45f : 1.0f);

            juce::String caption (bandNames[b]);
            const juce::String colourText = lit > 0.5f ? colorNames[band.colorIndex] : "CLEAN";

            g.setFont (captionFont (10.5f, true));
            const float nameWidth =
                juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), caption);

            g.setColour (nameColour);
            g.drawText (caption, text, juce::Justification::centredLeft, false);

            auto rest = text.withTrimmedLeft (nameWidth + 8.0f);
            if (rest.getWidth() > 40.0f)
            {
                juce::String detail = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7 ")) + colourText;
                if (rest.getWidth() > 96.0f)
                    detail << juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))
                           << juce::String (juce::roundToInt (band.drivePercent)) << "%";

                g.setFont (uiFont (10.0f));
                g.setColour ((lit > 0.5f ? tokens::textSecondary : tokens::textMuted)
                                 .withMultipliedAlpha (muted ? 0.5f : 1.0f));
                g.drawText (detail, rest, juce::Justification::centredLeft, false);
            }

            //  Solo dims everything that is not soloed - shown here as a thin
            //  accent rule under the soloed header.
            if (soloed)
            {
                g.setColour (accent);
                g.fillRect (area.getX() + 3.0f, area.getBottom() - 2.0f,
                            area.getWidth() - 6.0f, 1.5f);
            }
        }
    }

    void BandHeaders::mouseDown (const juce::MouseEvent& e)
    {
        //  Clicks on the plate select the band. Clicks that land on a button
        //  never reach here, so selection cannot change by accident.
        for (int b = 0; b < numBands; ++b)
        {
            if (headerBounds (b).contains (e.getPosition()))
            {
                setSelectedBand (b);
                if (onBandSelected)
                    onBandSelected (b);
                return;
            }
        }
    }
}
