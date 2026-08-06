#include "Analyzer.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    namespace
    {
        constexpr float minHz = 20.0f, maxHz = 20000.0f;
        constexpr float topDb = 12.0f, bottomDb = -12.0f;

        constexpr float tagRow    = 24.0f;   // frequency values above the plot
        constexpr float rightAxis = 22.0f;   // L / C / R marks
        constexpr float bottomAxis = 20.0f;  // frequency labels

        //  Display mapping: -78 dBFS .. -6 dBFS spans the half height.
        constexpr float floorDb = -78.0f, ceilDb = -6.0f;

        juce::String hzText (float hz)
        {
            return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                                 : juce::String ((int) std::round (hz)) + " Hz";
        }

        //  |LR4| magnitudes: an LR4 low-pass is a squared 2nd-order
        //  Butterworth, so |H| = 1 / (1 + (f/fc)^4) exactly.
        double lr4Low  (double f, double fc) { const double r = std::pow (f / fc, 4.0); return 1.0 / (1.0 + r); }
        double lr4High (double f, double fc) { const double r = std::pow (f / fc, 4.0); return r / (1.0 + r); }
    }

    Analyzer::Analyzer (FourColorProcessor& processor)
        : proc (processor), state (processor.apvts)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                         * (float) i / (float) (fftSize - 1));

        const char* cutIds[3] = { param::xover1, param::xover2, param::xover3 };
        for (int i = 0; i < 3; ++i)
        {
            auto* p = state.getParameter (cutIds[i]);
            jassert (p != nullptr);
            cutDefaults[i] = p->convertFrom0to1 (p->getDefaultValue());

            cutAttachments[i] = std::make_unique<juce::ParameterAttachment> (
                *p,
                [this, i] (float newValue) { cutValues[i] = newValue; repaint(); },
                nullptr);
            cutAttachments[i]->sendInitialUpdate();
        }

        //  36 FPS: within the 30-45 band the specification asks for.
        //  30 FPS, the bottom of the brief's 30-45 range. The analyzer grew
        //  from 29% of the window to 39.5% when the band cards were removed,
        //  which is 36% more area to fill every frame; trading six frames a
        //  second for that is a better answer than making the picture coarser.
        startTimerHz (analyzerFps);
    }

    Analyzer::~Analyzer() = default;

    void Analyzer::setBandPower (int band, float fade)
    {
        const int b = juce::jlimit (0, numBands - 1, band);
        if (std::abs (bandPower[b] - fade) > 1.0e-3f)
        {
            bandPower[b] = fade;
            repaint();
        }
    }

    void Analyzer::setSelectedBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        repaint();
    }

    void Analyzer::setEmphasis (Emphasis e, int band)
    {
        if (emphasis != e || emphasisBand != band)
        {
            emphasis = e;
            emphasisBand = juce::jlimit (0, numBands - 1, band);
            repaint();
        }
    }

    void Analyzer::updateSpectrum()
    {
        //  Drain the processor's mid/side tap into the rings.
        for (;;)
        {
            const int frames = proc.readSpectrumFrames (drain.data(), 2048);
            if (frames == 0)
                break;

            for (int i = 0; i < frames; ++i)
            {
                midRing[(size_t) ringPos]  = drain[(size_t) (i * 2)];
                sideRing[(size_t) ringPos] = drain[(size_t) (i * 2 + 1)];
                ringPos = (ringPos + 1) % fftSize;
            }
        }

        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        const double binHz = sr / fftSize;

        auto analyse = [&] (const std::vector<float>& ring, std::vector<float>& outDb,
                            float releaseScale)
        {
            for (int i = 0; i < fftSize; ++i)
                scratch[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)] * window[(size_t) i];
            std::fill (scratch.begin() + fftSize, scratch.end(), 0.0f);
            fft.performRealOnlyForwardTransform (scratch.data());

            auto binMag = [this] (int b)
            {
                const float re = scratch[(size_t) (2 * b)];
                const float im = scratch[(size_t) (2 * b + 1)];
                return std::sqrt (re * re + im * im);
            };

            float raw[numColumns];
            for (int col = 0; col < numColumns; ++col)
            {
                const float t0 = (float) col / numColumns;
                const float t1 = (float) (col + 1) / numColumns;
                const double f0 = minHz * std::pow (maxHz / minHz, t0);
                const double f1 = minHz * std::pow (maxHz / minHz, t1);

                float mag;
                if (f1 - f0 < binHz)
                {
                    //  Narrow column: interpolate between bins so the low end
                    //  is a curve rather than a staircase.
                    const float bin = (float) (0.5 * (f0 + f1) / binHz);
                    const int b0 = juce::jlimit (1, fftSize / 2 - 2, (int) bin);
                    const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) b0);
                    mag = binMag (b0) + frac * (binMag (b0 + 1) - binMag (b0));
                }
                else
                {
                    const int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / binHz));
                    const int b1 = juce::jlimit (1, fftSize / 2 - 1, (int) std::ceil (f1 / binHz));
                    float peak = 0.0f;
                    for (int b = b0; b <= b1; ++b)
                        peak = juce::jmax (peak, binMag (b));
                    mag = peak;
                }

                raw[col] = juce::Decibels::gainToDecibels (mag * (2.0f / fftSize), -96.0f);
            }

            //  Neighbour blur, then attack/release ballistics (~45 ms attack,
            //  ~240 ms release at 36 FPS).
            for (int col = 0; col < numColumns; ++col)
            {
                const float l = raw[juce::jmax (0, col - 1)];
                const float r = raw[juce::jmin (numColumns - 1, col + 1)];
                const float db = 0.25f * l + 0.5f * raw[col] + 0.25f * r;

                auto& s = outDb[(size_t) col];
                if (db > s) s += attackCoeff * (db - s);
                else        s = juce::jmax (db, s - releaseDbPerFrame * releaseScale);
            }
        };

        analyse (midRing, midDb, 1.0f);
        analyse (sideRing, sideDb, 1.25f);   // sides settle a touch faster

        //  Silence detection: with no audio the display comes to rest instead
        //  of animating.
        float loudest = -200.0f;
        for (float v : midDb)
            loudest = juce::jmax (loudest, v);
        silent = loudest < floorDb + 1.0f;
    }

    void Analyzer::timerCallback()
    {
        const bool wasSilent = silent;
        updateSpectrum();

        if (! silent || ! wasSilent)
            repaint();
    }

    juce::Rectangle<float> Analyzer::plotArea() const
    {
        return getLocalBounds().toFloat()
            .withTrimmedTop (tagRow)
            .withTrimmedLeft (metric::leftAxis)
            .withTrimmedRight (rightAxis)
            .withTrimmedBottom (bottomAxis);
    }

    float Analyzer::xForFrequency (float hz) const
    {
        const auto area = plotArea();
        return area.getX() + std::log (hz / minHz) / std::log (maxHz / minHz) * area.getWidth();
    }

    float Analyzer::frequencyForX (float x) const
    {
        const auto area = plotArea();
        const float t = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth());
        return minHz * std::pow (maxHz / minHz, t);
    }

    float Analyzer::yForDb (float db) const
    {
        const auto area = plotArea();
        return juce::jmap (db, topDb, bottomDb, area.getY(), area.getBottom());
    }

    int Analyzer::bandForFrequency (float hz) const
    {
        if (hz >= cutValues[2]) return 3;
        if (hz >= cutValues[1]) return 2;
        if (hz >= cutValues[0]) return 1;
        return 0;
    }

    void Analyzer::resized()
    {
        rebuildBackdrop();
    }

    void Analyzer::rebuildBackdrop()
    {
        if (getWidth() <= 0 || getHeight() <= 0)
            return;

        const auto scale = (float) juce::Desktop::getInstance().getGlobalScaleFactor();
        backdrop = juce::Image (juce::Image::ARGB,
                                juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                                juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)),
                                true);

        juce::Graphics g (backdrop);
        g.addTransform (juce::AffineTransform::scale (scale));

        const auto area = plotArea();
        const auto full = getLocalBounds().toFloat();

        //  Container.
        g.setColour (tokens::analyzerBack);
        g.fillRoundedRectangle (full, metric::corner);

        //  Inner shadow along the top edge.
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.20f * (1.0f - (float) i / 6.0f)));
            g.drawLine (full.getX() + metric::corner, full.getY() + 1.0f + (float) i,
                        full.getRight() - metric::corner, full.getY() + 1.0f + (float) i, 1.0f);
        }

        //  --- grid ---------------------------------------------------------------
        g.setFont (uiFont (10.0f));
        for (int db : { 12, 6, 0, -6, -12 })
        {
            const float ty = yForDb ((float) db);
            g.setColour (db == 0 ? tokens::gridMajor.withMultipliedAlpha (1.6f) : tokens::gridMinor);
            g.drawHorizontalLine ((int) ty, area.getX(), area.getRight());
            g.setColour (tokens::textMuted);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db),
                        2, (int) ty - 7, (int) metric::leftAxis - 6, 14,
                        juce::Justification::centredRight);
        }

        for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                         2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            const float x = xForFrequency (f);
            g.setColour (tokens::gridMinor);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (tokens::textMuted);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        (int) x - 18, (int) area.getBottom() + 4, 36, 12,
                        juce::Justification::centred);
        }
    }

    void Analyzer::paint (juce::Graphics& g)
    {
        const auto area = plotArea();
        const auto full = getLocalBounds().toFloat();

        if (backdrop.isNull())
            rebuildBackdrop();

        g.drawImage (backdrop, full, juce::RectanglePlacement::stretchToFit);



        //  --- LR4 responses, thin, behind everything -----------------------------
        {
            const double f1 = cutValues[0], f2 = cutValues[1], f3 = cutValues[2];
            for (int b = 0; b < numBands; ++b)
            {
                juce::Path curve;
                bool started = false;
                for (float px = area.getX(); px <= area.getRight(); px += 3.0f)
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
                    const float ty = yForDb ((float) juce::jlimit (-14.0, 0.0,
                                              juce::Decibels::gainToDecibels (mag, -60.0)));
                    if (! started) { curve.startNewSubPath (px, ty); started = true; }
                    else             curve.lineTo (px, ty);
                }
                g.setColour (tokens::band[b].withAlpha ((b == selectedBand ? 0.28f : 0.18f)
                                                            * bandPower[b]));
                g.strokePath (curve, juce::PathStrokeType (1.0f));
            }
        }

        //  --- the measured spectrum ----------------------------------------------
        const float mid = area.getCentreY();
        const float halfH = area.getHeight() * 0.5f - 3.0f;

        auto heightFor = [halfH] (float db)
        {
            return juce::jlimit (0.0f, halfH, juce::jmap (db, floorDb, ceilDb, 0.0f, halfH));
        };

        auto columnX = [&area] (int col)
        {
            return area.getX() + area.getWidth() * ((float) col + 0.5f) / numColumns;
        };

        //  Each shape is built ONCE as a single path containing both the upper
        //  and the mirrored lower half - one fill and one stroke per band
        //  instead of two, which is what brings the repaint cost down.
        //  Built PER BAND, over that band's columns only. Building one path
        //  across the whole width and then drawing it four times under four
        //  clip regions rasterises every column four times; restricting the
        //  geometry does the same picture for a quarter of the work.
        const float edgeHz[5] = { minHz, cutValues[0], cutValues[1], cutValues[2], maxHz };

        auto columnForX = [&area] (float px)
        {
            const float t = juce::jlimit (0.0f, 1.0f,
                                          (px - area.getX()) / juce::jmax (1.0f, area.getWidth()));
            return juce::jlimit (0, numColumns - 1, (int) std::round (t * (numColumns - 1)));
        };

        struct BandPaths { juce::Path shape, outline, side[2]; };

        auto buildBand = [&] (int first, int last)
        {
            BandPaths paths;

            //  One column of overlap on each side, so neighbouring bands meet
            //  without a seam.
            first = juce::jmax (0, first - 1);
            last  = juce::jmin (numColumns - 1, last + 1);

            paths.shape.preallocateSpace ((last - first + 2) * 6);
            paths.outline.preallocateSpace ((last - first + 2) * 6);

            paths.shape.startNewSubPath (columnX (first), mid - heightFor (midDb[(size_t) first]));
            paths.outline.startNewSubPath (columnX (first), mid - heightFor (midDb[(size_t) first]));
            for (int col = first + 1; col <= last; ++col)
            {
                const float py = mid - heightFor (midDb[(size_t) col]);
                paths.shape.lineTo (columnX (col), py);
                paths.outline.lineTo (columnX (col), py);
            }

            paths.shape.lineTo (columnX (last), mid + heightFor (midDb[(size_t) last]));
            paths.outline.startNewSubPath (columnX (last), mid + heightFor (midDb[(size_t) last]));
            for (int col = last - 1; col >= first; --col)
            {
                const float py = mid + heightFor (midDb[(size_t) col]);
                paths.shape.lineTo (columnX (col), py);
                paths.outline.lineTo (columnX (col), py);
            }
            paths.shape.closeSubPath();

            //  Side contours: distance from the centre is the MEASURED side
            //  energy. Below the low crossover the DSP keeps the signal mono,
            //  so these close onto the centre on their own.
            for (int k = 0; k < 2; ++k)
            {
                const float weight = 0.50f + 0.42f * (float) k;
                auto heightAt = [&] (int col)
                {
                    return heightFor (midDb[(size_t) col])
                         + weight * heightFor (sideDb[(size_t) col]) * 0.55f;
                };

                paths.side[k].startNewSubPath (columnX (first), mid - heightAt (first));
                for (int col = first + 1; col <= last; ++col)
                    paths.side[k].lineTo (columnX (col), mid - heightAt (col));

                paths.side[k].startNewSubPath (columnX (first), mid + heightAt (first));
                for (int col = first + 1; col <= last; ++col)
                    paths.side[k].lineTo (columnX (col), mid + heightAt (col));
            }

            return paths;
        };

        for (int b = 0; b < numBands; ++b)
        {
            const float x0 = juce::jmax (area.getX(), xForFrequency (edgeHz[b]));
            const float x1 = juce::jmin (area.getRight(), xForFrequency (edgeHz[b + 1]));
            if (x1 <= x0)
                continue;

            const auto paths = buildBand (columnForX (x0), columnForX (x1));

            //  Drained towards the neutral text colour rather than removed:
            //  the band is still passing audio, so its contour must stay.
            const auto c = tokens::textMuted.interpolatedWith (tokens::band[b], bandPower[b]);
            const bool isSel = b == selectedBand;
            const bool isEmph = emphasis != Emphasis::none && b == emphasisBand;

            //  Per-band fill opacities from the specification.
            const float topAlpha = (b == 0 ? 0.30f : b == 1 ? 0.27f : b == 2 ? 0.32f : 0.27f)
                                 * (isSel ? 1.0f : 0.72f) * (isEmph ? 1.25f : 1.0f);
            const float botAlpha = (b == 0 ? 0.08f : b == 1 ? 0.07f : b == 2 ? 0.09f : 0.07f)
                                 * (isSel ? 1.0f : 0.72f);

            //  Symmetric vertical gradient: dense at the contour, faint at the
            //  centre line, mirrored below.
            juce::ColourGradient grad (c.withAlpha (topAlpha), 0.0f, mid - halfH,
                                       c.withAlpha (topAlpha), 0.0f, mid + halfH, false);
            grad.addColour (0.5, c.withAlpha (botAlpha));
            g.setGradientFill (grad);
            g.fillPath (paths.shape);

            //  Side contours.
            for (int k = 0; k < 2; ++k)
            {
                g.setColour (c.withAlpha ((isSel ? 0.34f : 0.16f) * (1.0f - 0.20f * (float) k)));
                g.strokePath (paths.side[k], juce::PathStrokeType (1.0f));
            }

            //  Main contour.
            g.setColour (c.withAlpha (isSel ? 0.98f : 0.70f));
            g.strokePath (paths.outline, juce::PathStrokeType (isSel ? 1.4f : 1.1f));

            //  Optional residual layer - only if a real provider is installed.
            if (residualProvider != nullptr)
            {
                std::vector<float> residual;
                if (residualProvider (b, residual) && (int) residual.size() == numColumns)
                {
                    juce::Path rp;
                    rp.startNewSubPath (area.getX(), mid - heightFor (residual[0]));
                    for (int col = 1; col < numColumns; ++col)
                        rp.lineTo (columnX (col), mid - heightFor (residual[(size_t) col]));
                    rp.startNewSubPath (area.getX(), mid + heightFor (residual[0]));
                    for (int col = 1; col < numColumns; ++col)
                        rp.lineTo (columnX (col), mid + heightFor (residual[(size_t) col]));

                    g.setColour (c.withAlpha (isSel ? 0.45f : 0.20f));
                    g.strokePath (rp, juce::PathStrokeType (0.8f));
                }
            }
        }

        //  Centre line.
        g.setColour (tokens::gridMajor);
        g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());

        //  L / C / R marks on the right edge.
        g.setFont (uiFont (9.5f));
        g.setColour (tokens::textMuted);
        struct Mark { const char* label; float y; };
        const Mark marks[] = {
            { "L", mid - halfH * 0.80f }, { "L", mid - halfH * 0.42f },
            { "C", mid },
            { "R", mid + halfH * 0.42f }, { "R", mid + halfH * 0.80f },
        };
        for (const auto& m : marks)
            g.drawText (m.label, (int) area.getRight() + 4, (int) m.y - 7,
                        (int) rightAxis - 6, 14, juce::Justification::centred);

        //  --- crossover lines, capsule handles and value tags ---------------------
        for (int i = 0; i < 3; ++i)
        {
            const float x = xForFrequency (cutValues[i]);
            const bool active = i == draggingHandle || i == hoverHandle;
            //  The line takes the colour of the band to its right, unless a
            //  neighbouring band is selected.
            const auto c = (selectedBand == i || selectedBand == i + 1)
                               ? tokens::band[selectedBand] : tokens::band[i + 1];

            if (i == draggingHandle)
                for (float r = 7.0f; r >= 1.0f; r -= 1.5f)
                {
                    g.setColour (c.withAlpha (0.10f * (1.0f - r / 8.0f)));
                    g.drawLine (x, area.getY(), x, area.getBottom(), r * 2.0f);
                }

            g.setColour (c.withAlpha (active ? 0.90f : 0.55f));
            g.drawLine (x, area.getY(), x, area.getBottom(), active ? 2.0f : 1.0f);

            //  Capsule handle, 16x30.
            const auto pill = juce::Rectangle<float> (16.0f, 30.0f)
                                  .withCentre ({ x, area.getY() + 17.0f });
            g.setColour (juce::Colour (0xff14181e));
            g.fillRoundedRectangle (pill, 8.0f);
            g.setColour (c.withAlpha (active ? 1.0f : 0.75f));
            g.drawRoundedRectangle (pill.reduced (0.5f), 8.0f, 1.2f);

            g.setColour (tokens::textPrimary.withAlpha (0.9f));
            for (int d = 0; d < 3; ++d)
                g.fillEllipse (x - 1.6f, pill.getY() + 7.5f + (float) d * 6.0f, 3.2f, 3.2f);

            //  Value above the plot.
            g.setFont (uiFont (11.0f));
            g.setColour (active ? tokens::textPrimary : tokens::textSecondary);
            g.drawText (hzText (cutValues[i]), (int) x - 44, 3, 88, (int) tagRow - 6,
                        juce::Justification::centred);
        }
    }

    int Analyzer::handleAt (juce::Point<float> pos) const
    {
        for (int i = 0; i < 3; ++i)
            if (std::abs (pos.x - xForFrequency (cutValues[i])) < 9.0f)
                return i;
        return -1;
    }

    void Analyzer::mouseMove (const juce::MouseEvent& e)
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

    void Analyzer::mouseExit (const juce::MouseEvent&)
    {
        if (hoverHandle != -1)
        {
            hoverHandle = -1;
            repaint();
        }
    }

    void Analyzer::mouseDown (const juce::MouseEvent& e)
    {
        draggingHandle = handleAt (e.position);

        if (draggingHandle >= 0)
        {
            cutAttachments[draggingHandle]->beginGesture();
            repaint();
            return;
        }

        const int band = bandForFrequency (frequencyForX (e.position.x));
        setSelectedBand (band);
        if (onBandSelected)
            onBandSelected (band);
    }

    void Analyzer::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingHandle >= 0)
            cutAttachments[draggingHandle]->setValueAsPartOfGesture (frequencyForX (e.position.x));
    }

    void Analyzer::mouseUp (const juce::MouseEvent&)
    {
        if (draggingHandle >= 0)
        {
            cutAttachments[draggingHandle]->endGesture();
            draggingHandle = -1;
            repaint();
        }
    }

    void Analyzer::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int h = handleAt (e.position);
        if (h >= 0)
            cutAttachments[h]->setValueAsCompleteGesture (cutDefaults[h]);
    }
}
