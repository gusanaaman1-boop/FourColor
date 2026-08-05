#include "BandCards.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    BandCards::BandCards (FourColorProcessor& processor)
        : proc (processor)
    {
        auto& state = proc.apvts;

        for (int b = 0; b < numBands; ++b)
        {
            auto& card = cards[b];
            const auto accent = tokens::band[b];

            card.drive = std::make_unique<Knob> (state, param::band (b, param::drive), "DRIVE",
                                                 accent, Knob::Size::small,
                                                 "Amount of harmonic saturation");
            card.level = std::make_unique<Knob> (state, param::band (b, param::level), "LEVEL",
                                                 tokens::neutralArcII, Knob::Size::small,
                                                 "Band output level");
            addAndMakeVisible (*card.drive);
            addAndMakeVisible (*card.level);

            for (auto* button : { &card.solo, &card.mute, &card.bypass })
            {
                button->setClickingTogglesState (true);
                button->setColour (juce::TextButton::buttonOnColourId, accent);
                addAndMakeVisible (*button);
            }
            card.solo.setTooltip ("Solo band");
            card.mute.setTooltip ("Mute band");
            card.bypass.setTooltip ("Bypass band (passes the clean band)");

            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
            card.aSolo   = std::make_unique<BA> (state, param::band (b, param::solo), card.solo);
            card.aMute   = std::make_unique<BA> (state, param::band (b, param::mute), card.mute);
            card.aBypass = std::make_unique<BA> (state, param::band (b, param::bypass), card.bypass);

            auto* colorParam = state.getParameter (param::band (b, param::color));
            card.colorAttachment = std::make_unique<juce::ParameterAttachment> (
                *colorParam,
                [this, b] (float v) { cards[b].colorIndex = juce::roundToInt (v); repaint(); },
                nullptr);
            card.colorAttachment->sendInitialUpdate();
        }

        //  Only used to feed the DRIVE knobs' spark energy with real level.
        startTimerHz (20);
    }

    BandCards::~BandCards() = default;

    void BandCards::timerCallback()
    {
        for (int b = 0; b < numBands; ++b)
        {
            const float peak = proc.getEngine().getBand (b).readAndResetOutputPeak();
            cards[b].drive->setEnergy (juce::jlimit (0.0f, 1.0f, peak * 1.6f));
        }
    }

    void BandCards::setSelectedBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        repaint();
    }

    juce::Rectangle<int> BandCards::cardBounds (int b) const
    {
        auto area = getLocalBounds();
        const int gap = (int) metric::cardGap;
        const int w = (area.getWidth() - gap * (numBands - 1)) / numBands;
        return { area.getX() + b * (w + gap), area.getY(), w, area.getHeight() };
    }

    int BandCards::cardAt (juce::Point<int> p) const
    {
        for (int b = 0; b < numBands; ++b)
            if (cardBounds (b).contains (p))
                return b;
        return -1;
    }

    void BandCards::paint (juce::Graphics& g)
    {
        for (int b = 0; b < numBands; ++b)
        {
            auto card = cardBounds (b).toFloat();
            const bool isSelected = b == selectedBand;
            const bool isHover = b == hoverCard && ! isSelected;
            const bool isPressed = b == pressedCard;
            const auto c = tokens::band[b];

            if (isPressed)
                card.translate (0.0f, 1.0f);

            if (isSelected)
                paint::glow (g, card, metric::corner, c, 14.0f, 0.12f);
            else
                paint::dropShadow (g, card, metric::corner, 4.0f, 0.26f, 2.0f);

            if (isSelected)
            {
                //  Vertical wash: 13% of the band colour at the top down to the
                //  panel base at the bottom.
                juce::ColourGradient wash (c.withAlpha (0.13f), card.getCentreX(), card.getY(),
                                           tokens::panelBase, card.getCentreX(), card.getBottom(),
                                           false);
                g.setGradientFill (wash);
                g.fillRoundedRectangle (card, metric::corner);
            }
            else
            {
                auto fill = isHover ? tokens::panelHover : tokens::panelBase;
                if (isPressed)
                    fill = fill.overlaidWith (juce::Colours::black.withAlpha (0.05f));
                g.setColour (fill);
                g.fillRoundedRectangle (card, metric::corner);
            }

            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.drawLine (card.getX() + metric::corner, card.getY() + 1.0f,
                        card.getRight() - metric::corner, card.getY() + 1.0f, 1.0f);

            g.setColour (isSelected ? c : (isHover ? tokens::borderHover : tokens::borderSoft));
            g.drawRoundedRectangle (card.reduced (0.5f), metric::corner, 1.0f);

            //  Band name and engine name.
            auto text = card.reduced (12.0f, 9.0f);
            const float nameAlpha = isSelected ? 1.0f : (isHover ? 0.92f : 0.80f);

            g.setFont (captionFont (12.5f, true));
            g.setColour (c.withAlpha (nameAlpha));
            g.drawText (bandName (b), text.removeFromTop (16.0f), juce::Justification::topLeft);

            text.removeFromTop (4.0f);
            g.setFont (captionFont (11.5f));
            g.setColour (isSelected ? c.withAlpha (0.95f) : tokens::textSecondary);
            g.drawText (colorName ((ColorType) cards[b].colorIndex),
                        text.removeFromTop (14.0f), juce::Justification::topLeft);
        }
    }

    void BandCards::resized()
    {
        for (int b = 0; b < numBands; ++b)
        {
            auto card = cardBounds (b).reduced (10, 7);
            auto& cd = cards[b];

            //  Right half: the two small knobs.
            const int knobW = juce::jmax (46, card.getWidth() / 3);
            cd.level->setBounds (card.removeFromRight (knobW));
            cd.drive->setBounds (card.removeFromRight (knobW));

            //  Bottom-left: S / M / B.
            auto row = card.removeFromBottom (18).removeFromLeft (76);
            const int bw = juce::jmin (22, row.getWidth() / 3 - 2);
            cd.solo.setBounds (row.removeFromLeft (bw));
            row.removeFromLeft (3);
            cd.mute.setBounds (row.removeFromLeft (bw));
            row.removeFromLeft (3);
            cd.bypass.setBounds (row.removeFromLeft (bw));
        }
    }

    void BandCards::mouseMove (const juce::MouseEvent& e)
    {
        const int c = cardAt (e.getPosition());
        if (c != hoverCard)
        {
            hoverCard = c;
            repaint();
        }
    }

    void BandCards::mouseExit (const juce::MouseEvent&)
    {
        if (hoverCard != -1) { hoverCard = -1; repaint(); }
    }

    void BandCards::mouseDown (const juce::MouseEvent& e)
    {
        pressedCard = cardAt (e.getPosition());
        repaint();
    }

    void BandCards::mouseUp (const juce::MouseEvent& e)
    {
        const int c = cardAt (e.getPosition());
        pressedCard = -1;

        if (c >= 0)
        {
            setSelectedBand (c);
            if (onBandSelected)
                onBandSelected (c);
        }
        repaint();
    }
}
