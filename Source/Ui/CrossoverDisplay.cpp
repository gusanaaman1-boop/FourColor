#include "CrossoverDisplay.h"

namespace fourcolor::ui
{
    namespace
    {
        constexpr float minHz = 20.0f, maxHz = 20000.0f;

        //  |LR4| magnitudes: an LR4 low-pass is a squared 2nd-order Butterworth,
        //  so |H| = 1 / (1 + (f/fc)^4) exactly.
        inline double lr4Low (double f, double fc)
        {
            const double r = std::pow (f / fc, 4.0);
            return 1.0 / (1.0 + r);
        }
        inline double lr4High (double f, double fc)
        {
            const double r = std::pow (f / fc, 4.0);
            return r / (1.0 + r);
        }
    }

    CrossoverDisplay::CrossoverDisplay (juce::AudioProcessorValueTreeState& apvts)
        : state (apvts)
    {
        const char* cutIds[3] = { param::xover1, param::xover2, param::xover3 };
        for (int i = 0; i < 3; ++i)
        {
            auto* p = state.getParameter (cutIds[i]);
            jassert (p != nullptr);
            cutValues[i] = p->convertFrom0to1 (p->getValue());

            cutAttachments[i] = std::make_unique<juce::ParameterAttachment> (
                *p,
                [this, i] (float newValue)
                {
                    cutValues[i] = newValue;
                    repaint();
                },
                nullptr);
            cutAttachments[i]->sendInitialUpdate();
        }

        for (int b = 0; b < numBands; ++b)
        {
            auto& bb = bandButtons[b];
            for (auto* button : { &bb.solo, &bb.mute, &bb.bypass })
            {
                button->setClickingTogglesState (true);
                button->setColour (juce::TextButton::buttonOnColourId,
                                   colour::band[b].withAlpha (0.85f));
                addAndMakeVisible (*button);
            }
            bb.solo.setTooltip ("Solo band");
            bb.mute.setTooltip ("Mute band");
            bb.bypass.setTooltip ("Bypass band (passes the clean band)");

            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
            bb.aSolo   = std::make_unique<BA> (state, param::band (b, param::solo), bb.solo);
            bb.aMute   = std::make_unique<BA> (state, param::band (b, param::mute), bb.mute);
            bb.aBypass = std::make_unique<BA> (state, param::band (b, param::bypass), bb.bypass);
        }

        //  Drive/level readouts change without notifying this component, so a
        //  modest refresh keeps them honest.
        startTimerHz (15);
    }

    CrossoverDisplay::~CrossoverDisplay() = default;

    void CrossoverDisplay::timerCallback() { repaint(); }

    void CrossoverDisplay::setSelectedBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        repaint();
    }

    juce::Rectangle<float> CrossoverDisplay::plotArea() const
    {
        return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedBottom (16.0f);
    }

    float CrossoverDisplay::currentCut (int i) const { return cutValues[i]; }

    float CrossoverDisplay::xForFrequency (float hz) const
    {
        const auto area = plotArea();
        const float t = std::log (hz / minHz) / std::log (maxHz / minHz);
        return area.getX() + t * area.getWidth();
    }

    float CrossoverDisplay::frequencyForX (float x) const
    {
        const auto area = plotArea();
        const float t = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth());
        return minHz * std::pow (maxHz / minHz, t);
    }

    void CrossoverDisplay::paint (juce::Graphics& g)
    {
        const auto area = plotArea();

        g.setColour (colour::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

        //  Frequency grid.
        g.setFont (uiFont (10.0f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xForFrequency (f);
            g.setColour (colour::panelLine.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (colour::textDim);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        (int) x - 16, (int) area.getBottom() + 2, 32, 12,
                        juce::Justification::centred);
        }

        const double f1 = currentCut (0), f2 = currentCut (1), f3 = currentCut (2);

        //  Band regions + REAL band magnitude curves (level-scaled).
        const float edges[5] = { minHz, (float) f1, (float) f2, (float) f3, maxHz };

        for (int b = 0; b < numBands; ++b)
        {
            const bool isSelected = b == selectedBand;
            const float x0 = xForFrequency (edges[b]);
            const float x1 = xForFrequency (edges[b + 1]);

            //  Region tint.
            g.setColour (colour::band[b].withAlpha (isSelected ? 0.16f : 0.06f));
            g.fillRect (juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight()));

            //  Analytic band curve.
            const bool muted = state.getRawParameterValue (param::band (b, param::mute))->load() > 0.5f;
            const float levelDb = state.getRawParameterValue (param::band (b, param::level))->load();
            const double levelGain = juce::Decibels::decibelsToGain (levelDb);

            juce::Path curve;
            bool started = false;
            for (float px = area.getX(); px <= area.getRight(); px += 2.0f)
            {
                const double f = frequencyForX (px);
                double mag = 1.0;
                switch (b)
                {
                    case 0: mag = lr4Low (f, f2) * lr4Low (f, f1); break;
                    case 1: mag = lr4Low (f, f2) * lr4High (f, f1); break;
                    case 2: mag = lr4High (f, f2) * lr4Low (f, f3); break;
                    case 3: mag = lr4High (f, f2) * lr4High (f, f3); break;
                }
                mag *= levelGain;

                //  Map -36..+12 dB onto the plot height.
                const double db = juce::Decibels::gainToDecibels (mag, -60.0);
                const float ty = (float) juce::jmap (juce::jlimit (-36.0, 12.0, db),
                                                     -36.0, 12.0,
                                                     (double) area.getBottom(), (double) area.getY());
                if (! started) { curve.startNewSubPath (px, ty); started = true; }
                else             curve.lineTo (px, ty);
            }

            g.setColour (colour::band[b].withAlpha (muted ? 0.25f : (isSelected ? 1.0f : 0.55f)));
            g.strokePath (curve, juce::PathStrokeType (isSelected ? 2.0f : 1.3f));

            //  Compact info: name, colour, drive, level.
            const auto colorIdx = (int) state.getRawParameterValue (param::band (b, param::color))->load();
            const auto driveVal = state.getRawParameterValue (param::band (b, param::drive))->load();

            const juce::String info = juce::String (bandName (b)) + "  ·  "
                                    + colorName ((ColorType) colorIdx) + "  "
                                    + juce::String ((int) driveVal) + "%  "
                                    + juce::String (levelDb, 1) + "dB";

            g.setFont (uiFont (10.5f, isSelected));
            g.setColour (isSelected ? colour::text : colour::textDim);
            g.drawText (info, (int) x0 + 4, (int) area.getY() + 3,
                        (int) (x1 - x0) - 8, 14, juce::Justification::centredLeft);
        }

        //  Crossover handles.
        for (int i = 0; i < 3; ++i)
        {
            const float x = xForFrequency (currentCut (i));
            const bool active = i == draggingHandle || i == hoverHandle;

            g.setColour (colour::text.withAlpha (active ? 0.9f : 0.4f));
            g.drawLine (x, area.getY(), x, area.getBottom(), active ? 2.0f : 1.2f);
            g.fillEllipse (x - 4.0f, area.getCentreY() - 4.0f, 8.0f, 8.0f);

            if (active)
            {
                const float hz = currentCut (i);
                g.setFont (uiFont (10.5f, true));
                g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                                          : juce::String ((int) hz) + " Hz",
                            (int) x - 34, (int) area.getCentreY() + 8, 68, 14,
                            juce::Justification::centred);
            }
        }
    }

    void CrossoverDisplay::resized()
    {
        //  S/M/B rows anchored to the bottom of each band region; positions
        //  depend on the cut frequencies, so re-place on every paint tick too.
        const auto area = plotArea();
        const float edges[5] = { minHz, currentCut (0), currentCut (1), currentCut (2), maxHz };

        for (int b = 0; b < numBands; ++b)
        {
            const float x0 = xForFrequency (edges[b]);
            auto& bb = bandButtons[b];
            const int y = (int) area.getBottom() - 20;
            const int bx = (int) x0 + 4;
            bb.solo.setBounds (bx, y, 18, 16);
            bb.mute.setBounds (bx + 20, y, 18, 16);
            bb.bypass.setBounds (bx + 40, y, 18, 16);
        }
    }

    int CrossoverDisplay::handleAt (juce::Point<float> pos) const
    {
        for (int i = 0; i < 3; ++i)
            if (std::abs (pos.x - xForFrequency (currentCut (i))) < 7.0f)
                return i;
        return -1;
    }

    void CrossoverDisplay::mouseMove (const juce::MouseEvent& e)
    {
        const int h = handleAt (e.position);
        if (h != hoverHandle)
        {
            hoverHandle = h;
            setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                                   : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void CrossoverDisplay::mouseDown (const juce::MouseEvent& e)
    {
        draggingHandle = handleAt (e.position);

        if (draggingHandle >= 0)
        {
            cutAttachments[draggingHandle]->beginGesture();
            return;
        }

        //  Click on a region selects the band.
        const float f = frequencyForX (e.position.x);
        int band = 0;
        if (f >= currentCut (2)) band = 3;
        else if (f >= currentCut (1)) band = 2;
        else if (f >= currentCut (0)) band = 1;

        setSelectedBand (band);
        if (onBandSelected)
            onBandSelected (band);
    }

    void CrossoverDisplay::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingHandle < 0)
            return;

        cutAttachments[draggingHandle]->setValueAsPartOfGesture (frequencyForX (e.position.x));
        resized();   // S/M/B rows track the moving edges
    }

    void CrossoverDisplay::mouseUp (const juce::MouseEvent&)
    {
        if (draggingHandle >= 0)
            cutAttachments[draggingHandle]->endGesture();
        draggingHandle = -1;
    }
}
