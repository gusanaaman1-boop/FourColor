#include "CrossoverDisplay.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    namespace
    {
        constexpr float minHz = 20.0f, maxHz = 20000.0f;
        constexpr float tagHeight = 26.0f;      // frequency boxes above the plot
        constexpr float axisWidth = 34.0f;      // dB scale on the left
        constexpr float bottomAxis = 20.0f;     // frequency labels

        juce::String hzText (float hz)
        {
            return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                                 : juce::String ((int) std::round (hz)) + " Hz";
        }
    }

    CrossoverDisplay::CrossoverDisplay (FourColorProcessor& processor)
        : proc (processor), state (processor.apvts)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1));

        const char* cutIds[3] = { param::xover1, param::xover2, param::xover3 };
        for (int i = 0; i < 3; ++i)
        {
            auto* p = state.getParameter (cutIds[i]);
            jassert (p != nullptr);

            cutAttachments[i] = std::make_unique<juce::ParameterAttachment> (
                *p,
                [this, i] (float newValue) { cutValues[i] = newValue; repaint(); },
                nullptr);
            cutAttachments[i]->sendInitialUpdate();
        }

        startTimerHz (30);
    }

    CrossoverDisplay::~CrossoverDisplay() = default;

    void CrossoverDisplay::updateSpectrum()
    {
        //  Drain the processor's tap into the ring.
        float chunk[1024];
        for (;;)
        {
            const int got = proc.readSpectrumSamples (chunk, 1024);
            if (got == 0)
                break;
            for (int i = 0; i < got; ++i)
            {
                sampleRing[(size_t) ringPos] = chunk[i];
                ringPos = (ringPos + 1) % fftSize;
            }
        }

        //  Windowed FFT of the most recent fftSize samples.
        for (int i = 0; i < fftSize; ++i)
            fftScratch[(size_t) i] = sampleRing[(size_t) ((ringPos + i) % fftSize)] * window[(size_t) i];
        std::fill (fftScratch.begin() + fftSize, fftScratch.end(), 0.0f);
        fft.performRealOnlyForwardTransform (fftScratch.data());

        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        const double binHz = sr / fftSize;

        //  Column magnitudes on the log axis. Narrow columns (low frequencies,
        //  where one FFT bin spans many pixels) interpolate between bins so
        //  the curve doesn't staircase; wide columns take the bin peak.
        float raw[numColumns];

        auto magAt = [this] (float bin)
        {
            const int b0 = juce::jlimit (1, fftSize / 2 - 2, (int) bin);
            const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) b0);

            auto binMag = [this] (int b)
            {
                const float re = fftScratch[(size_t) (2 * b)];
                const float im = fftScratch[(size_t) (2 * b + 1)];
                return std::sqrt (re * re + im * im);
            };
            return binMag (b0) + frac * (binMag (b0 + 1) - binMag (b0));
        };

        for (int col = 0; col < numColumns; ++col)
        {
            const float t0 = (float) col / numColumns;
            const float t1 = (float) (col + 1) / numColumns;
            const double f0 = minHz * std::pow (maxHz / minHz, t0);
            const double f1 = minHz * std::pow (maxHz / minHz, t1);

            float mag;
            if (f1 - f0 < binHz)
            {
                mag = magAt ((float) (0.5 * (f0 + f1) / binHz));
            }
            else
            {
                const int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / binHz));
                const int b1 = juce::jlimit (1, fftSize / 2 - 1, (int) std::ceil (f1 / binHz));
                float peak = 0.0f;
                for (int b = b0; b <= b1; ++b)
                {
                    const float re = fftScratch[(size_t) (2 * b)];
                    const float im = fftScratch[(size_t) (2 * b + 1)];
                    peak = juce::jmax (peak, re * re + im * im);
                }
                mag = std::sqrt (peak);
            }

            raw[col] = juce::Decibels::gainToDecibels (mag * (2.0f / fftSize), -90.0f);
        }

        //  Light neighbour blur + fast-rise / slow-fall ballistics.
        for (int col = 0; col < numColumns; ++col)
        {
            const float left  = raw[juce::jmax (0, col - 1)];
            const float right = raw[juce::jmin (numColumns - 1, col + 1)];
            const float db = 0.25f * left + 0.5f * raw[col] + 0.25f * right;

            auto& smoothed = columnDb[(size_t) col];
            if (db > smoothed)
                smoothed += 0.6f * (db - smoothed);            // fast rise
            else
                smoothed = juce::jmax (db, smoothed - 2.2f);   // slow fall (dB/frame)
        }
    }

    void CrossoverDisplay::timerCallback()
    {
        updateSpectrum();
        repaint();
    }

    juce::Rectangle<float> CrossoverDisplay::plotArea() const
    {
        return getLocalBounds().toFloat()
            .withTrimmedTop (tagHeight)
            .withTrimmedLeft (axisWidth)
            .withTrimmedRight (10.0f)
            .withTrimmedBottom (bottomAxis);
    }

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

    void CrossoverDisplay::setSelectedBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        repaint();
    }

    void CrossoverDisplay::paint (juce::Graphics& g)
    {
        const auto area = plotArea();

        g.setColour (juce::Colour (0xff0a0b0d));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

        //  dB scale + horizontal grid.
        g.setFont (uiFont (10.0f));
        for (int db : { 12, 6, 0, -6, -12 })
        {
            const float ty = juce::jmap ((float) db, 14.0f, -14.0f, area.getY(), area.getBottom());
            g.setColour (colour::panelLine.withAlpha (db == 0 ? 0.8f : 0.35f));
            g.drawHorizontalLine ((int) ty, area.getX(), area.getRight());
            g.setColour (colour::textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), 2, (int) ty - 6,
                        (int) axisWidth - 6, 12, juce::Justification::centredRight);
        }

        //  Frequency grid + labels.
        for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                         2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            const float x = xForFrequency (f);
            g.setColour (colour::panelLine.withAlpha (0.30f));
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (colour::textDim);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        (int) x - 16, (int) area.getBottom() + 4, 32, 12,
                        juce::Justification::centred);
        }

        //  Band region tints (selected band glows a little).
        const float edges[5] = { minHz, cutValues[0], cutValues[1], cutValues[2], maxHz };
        for (int b = 0; b < numBands; ++b)
        {
            const float x0 = xForFrequency (edges[b]);
            const float x1 = xForFrequency (edges[b + 1]);
            g.setColour (colour::band[b].withAlpha (b == selectedBand ? 0.10f : 0.03f));
            g.fillRect (juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight()));
        }

        //  The real output spectrum, mirrored around the centre line and
        //  tinted per band region.
        const float mid = area.getCentreY();
        const float halfH = area.getHeight() * 0.5f - 2.0f;

        juce::Path top;
        top.preallocateSpace (numColumns * 3 + 8);
        bool started = false;
        for (int col = 0; col < numColumns; ++col)
        {
            const float px = area.getX() + area.getWidth() * ((float) col + 0.5f) / numColumns;
            //  -66 dB..+12 dB onto 0..halfH.
            const float h = juce::jlimit (0.0f, halfH,
                juce::jmap (columnDb[(size_t) col], -66.0f, 12.0f, 0.0f, halfH));

            if (! started) { top.startNewSubPath (px, mid - h); started = true; }
            else             top.lineTo (px, mid - h);
        }

        for (int b = 0; b < numBands; ++b)
        {
            const float x0 = juce::jmax (area.getX(), xForFrequency (edges[b]));
            const float x1 = juce::jmin (area.getRight(), xForFrequency (edges[b + 1]));

            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (juce::Rectangle<int> ((int) x0, (int) area.getY(),
                                                      (int) (x1 - x0) + 1, (int) area.getHeight()));

            //  Mirrored fill.
            juce::Path fill (top);
            fill.lineTo (area.getRight(), mid);
            fill.lineTo (area.getX(), mid);
            fill.closeSubPath();

            const auto c = colour::band[b];
            g.setColour (c.withAlpha (b == selectedBand ? 0.30f : 0.18f));
            g.fillPath (fill);
            g.fillPath (fill, juce::AffineTransform::verticalFlip (mid * 2.0f));

            g.setColour (c.withAlpha (b == selectedBand ? 0.95f : 0.65f));
            g.strokePath (top, juce::PathStrokeType (1.2f));
            g.strokePath (top, juce::PathStrokeType (1.2f),
                          juce::AffineTransform::verticalFlip (mid * 2.0f));
        }

        //  Centre line.
        g.setColour (colour::textDim.withAlpha (0.5f));
        g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());

        //  Crossover lines, pill handles, and the tag boxes above.
        for (int i = 0; i < 3; ++i)
        {
            const float x = xForFrequency (cutValues[i]);
            const bool active = i == draggingHandle || i == hoverHandle;
            const auto lineColour = colour::band[i + 1];   // right-side band's colour

            g.setColour (lineColour.withAlpha (active ? 0.95f : 0.6f));
            g.drawLine (x, area.getY(), x, area.getBottom(), active ? 1.8f : 1.2f);

            //  Pill handle with two dots.
            const auto pill = juce::Rectangle<float> (x - 6.5f, area.getY() + 6.0f, 13.0f, 22.0f);
            g.setColour (colour::panelHi);
            g.fillRoundedRectangle (pill, 6.5f);
            g.setColour (lineColour.withAlpha (active ? 1.0f : 0.7f));
            g.drawRoundedRectangle (pill, 6.5f, 1.2f);
            g.setColour (colour::text.withAlpha (0.85f));
            g.fillEllipse (x - 1.5f, pill.getY() + 6.0f, 3.0f, 3.0f);
            g.fillEllipse (x - 1.5f, pill.getY() + 13.0f, 3.0f, 3.0f);

            //  Frequency tag above the plot.
            const auto tagText = hzText (cutValues[i]);
            g.setFont (uiFont (11.5f));
            const float tw = juce::jmax (58.0f, (float) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tagText) + 16.0f);
            auto tag = juce::Rectangle<float> (x - tw * 0.5f, 2.0f, tw, tagHeight - 6.0f);
            tag.setX (juce::jlimit (area.getX(), area.getRight() - tw, tag.getX()));

            g.setColour (active ? colour::panelHi.brighter (0.1f) : colour::panelHi);
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (active ? lineColour : colour::panelLine);
            g.drawRoundedRectangle (tag, 4.0f, 1.0f);
            g.setColour (colour::text);
            g.drawText (tagText, tag, juce::Justification::centred);
        }
    }

    int CrossoverDisplay::handleAt (juce::Point<float> pos) const
    {
        for (int i = 0; i < 3; ++i)
        {
            const float x = xForFrequency (cutValues[i]);
            //  The line, the pill, or the tag box above all grab the handle.
            if (std::abs (pos.x - x) < 8.0f || (pos.y < tagHeight && std::abs (pos.x - x) < 34.0f))
                return i;
        }
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

        const float f = frequencyForX (e.position.x);
        int band = 0;
        if (f >= cutValues[2]) band = 3;
        else if (f >= cutValues[1]) band = 2;
        else if (f >= cutValues[0]) band = 1;

        setSelectedBand (band);
        if (onBandSelected)
            onBandSelected (band);
    }

    void CrossoverDisplay::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingHandle >= 0)
            cutAttachments[draggingHandle]->setValueAsPartOfGesture (frequencyForX (e.position.x));
    }

    void CrossoverDisplay::mouseUp (const juce::MouseEvent&)
    {
        if (draggingHandle >= 0)
            cutAttachments[draggingHandle]->endGesture();
        draggingHandle = -1;
    }
}
