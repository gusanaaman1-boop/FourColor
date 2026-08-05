#include "BandCards.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    namespace
    {
        constexpr float levelMinDb = -18.0f, levelMaxDb = 12.0f;

        juce::String rangeText (float lo, float hi)
        {
            auto one = [] (float hz)
            {
                return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz < 10000.0f ? 2 : 0) + " kHz"
                                     : juce::String ((int) std::round (hz)) + " Hz";
            };
            return one (lo) + " – " + one (hi);
        }
    }

    BandCards::BandCards (FourColorProcessor& processor)
        : proc (processor)
    {
        auto& state = proc.apvts;

        for (int b = 0; b < numBands; ++b)
        {
            auto& card = cards[b];

            for (auto* button : { &card.solo, &card.mute, &card.bypass })
            {
                button->setClickingTogglesState (true);
                button->setColour (juce::TextButton::buttonOnColourId,
                                   colour::band[b].withAlpha (0.55f));
                addAndMakeVisible (*button);
            }
            card.solo.setTooltip ("Solo band");
            card.mute.setTooltip ("Mute band");
            card.bypass.setTooltip ("Bypass band (passes the clean band)");

            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
            card.aSolo   = std::make_unique<BA> (state, param::band (b, param::solo), card.solo);
            card.aMute   = std::make_unique<BA> (state, param::band (b, param::mute), card.mute);
            card.aBypass = std::make_unique<BA> (state, param::band (b, param::bypass), card.bypass);

            auto* levelParam = state.getParameter (param::band (b, param::level));
            jassert (levelParam != nullptr);
            card.levelAttachment = std::make_unique<juce::ParameterAttachment> (
                *levelParam,
                [this, b] (float v) { cards[b].levelDb = v; repaint(); },
                nullptr);
            card.levelAttachment->sendInitialUpdate();
        }

        startTimerHz (30);
    }

    void BandCards::timerCallback()
    {
        for (int b = 0; b < numBands; ++b)
        {
            const float peak = proc.getEngine().getBand (b).readAndResetOutputPeak();
            auto& shown = cards[b].meterPeak;
            shown = juce::jmax (peak, shown * 0.82f);
        }
        repaint();
    }

    void BandCards::setSelectedBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        repaint();
    }

    void BandCards::setCutFrequencies (float f1, float f2, float f3)
    {
        if (cutHz[0] != f1 || cutHz[1] != f2 || cutHz[2] != f3)
        {
            cutHz[0] = f1; cutHz[1] = f2; cutHz[2] = f3;
            repaint();
        }
    }

    juce::Rectangle<int> BandCards::cardBounds (int b) const
    {
        auto area = getLocalBounds();
        const int gap = 6;
        const int w = (area.getWidth() - gap * (numBands - 1)) / numBands;
        return { area.getX() + b * (w + gap), area.getY(), w, area.getHeight() };
    }

    juce::Rectangle<float> BandCards::meterBounds (int b) const
    {
        auto card = cardBounds (b).toFloat().reduced (10.0f, 8.0f);
        auto bottom = card.removeFromBottom (18.0f);
        bottom.removeFromLeft (3.0f * 24.0f);     // S/M/B row sits to the left
        bottom.removeFromLeft (8.0f);
        return bottom.reduced (0.0f, 4.0f);
    }

    void BandCards::paint (juce::Graphics& g)
    {
        const float edges[5] = { 20.0f, cutHz[0], cutHz[1], cutHz[2], 20000.0f };

        for (int b = 0; b < numBands; ++b)
        {
            const auto card = cardBounds (b).toFloat();
            const bool isSelected = b == selectedBand;
            const auto c = colour::band[b];

            g.setColour (isSelected ? colour::panelHi.interpolatedWith (c, 0.06f) : colour::panel);
            g.fillRoundedRectangle (card, 6.0f);
            g.setColour (isSelected ? c.withAlpha (0.8f) : colour::panelLine);
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, isSelected ? 1.4f : 1.0f);

            //  Header: BAND n + live range.
            auto header = card.reduced (10.0f, 8.0f);
            g.setFont (labelFont (12.0f, true));
            g.setColour (c);
            g.drawText ("BAND " + juce::String (b + 1),
                        header.removeFromTop (16.0f), juce::Justification::topLeft);
            g.setFont (uiFont (10.5f));
            g.setColour (colour::textDim);
            g.drawText (rangeText (edges[b], edges[b + 1]),
                        card.reduced (10.0f, 8.0f).withTrimmedLeft (64.0f).removeFromTop (15.0f),
                        juce::Justification::topLeft);

            //  Meter track + segments + level thumb.
            const auto meter = meterBounds (b);
            const auto track = meter.withTrimmedRight (12.0f);

            g.setColour (juce::Colour (0xff0a0b0d));
            g.fillRoundedRectangle (track, 2.0f);

            const float peakDb = juce::Decibels::gainToDecibels (cards[b].meterPeak, -42.0f);
            const float peakT = juce::jlimit (0.0f, 1.0f, (peakDb + 42.0f) / 48.0f);
            const int numSegments = juce::jmax (8, (int) (track.getWidth() / 5.0f));
            const int litSegments = (int) std::round (peakT * numSegments);

            for (int s = 0; s < numSegments; ++s)
            {
                const float sx = track.getX() + track.getWidth() * (float) s / numSegments;
                g.setColour (s < litSegments ? c.withAlpha (0.9f) : c.withAlpha (0.12f));
                g.fillRect (sx + 0.5f, track.getY() + 1.0f,
                            track.getWidth() / numSegments - 1.5f, track.getHeight() - 2.0f);
            }

            //  Level thumb rides the same track (BAND LEVEL, -18..+12 dB).
            const float lt = juce::jmap (cards[b].levelDb, levelMinDb, levelMaxDb, 0.0f, 1.0f);
            const float tx = track.getX() + lt * track.getWidth();
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (tx - 5.0f, meter.getCentreY() - 4.5f, 11.0f, 11.0f);
            g.setColour (juce::Colour (0xffcfcfd4));
            g.fillEllipse (tx - 5.5f, meter.getCentreY() - 5.5f, 11.0f, 11.0f);
        }
    }

    void BandCards::resized()
    {
        for (int b = 0; b < numBands; ++b)
        {
            auto card = cardBounds (b).reduced (10, 8);
            auto row = card.removeFromBottom (18);
            auto& cd = cards[b];
            cd.solo.setBounds (row.removeFromLeft (20));
            row.removeFromLeft (2);
            cd.mute.setBounds (row.removeFromLeft (20));
            row.removeFromLeft (2);
            cd.bypass.setBounds (row.removeFromLeft (20));
        }
    }

    void BandCards::mouseDown (const juce::MouseEvent& e)
    {
        for (int b = 0; b < numBands; ++b)
        {
            if (meterBounds (b).expanded (4.0f).contains (e.position))
            {
                draggingLevelBand = b;
                cards[b].levelAttachment->beginGesture();
                mouseDrag (e);
                return;
            }

            if (cardBounds (b).contains (e.position.toInt()))
            {
                setSelectedBand (b);
                if (onBandSelected)
                    onBandSelected (b);
                return;
            }
        }
    }

    void BandCards::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingLevelBand < 0)
            return;

        const auto track = meterBounds (draggingLevelBand).withTrimmedRight (12.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (e.position.x - track.getX()) / track.getWidth());
        cards[draggingLevelBand].levelAttachment->setValueAsPartOfGesture (
            juce::jmap (t, 0.0f, 1.0f, levelMinDb, levelMaxDb));
    }

    void BandCards::mouseUp (const juce::MouseEvent&)
    {
        if (draggingLevelBand >= 0)
            cards[draggingLevelBand].levelAttachment->endGesture();
        draggingLevelBand = -1;
    }
}
