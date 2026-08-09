#include "MeterColumn.h"

#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    namespace
    {
        //  Ballistics, all from the brief. Peak rises instantly and falls at
        //  20 dB/s; RMS follows a ~300 ms window; the hold tick sits for 1.5 s.
        constexpr int   refreshHz     = 30;
        constexpr float peakDecayDbPerFrame = 20.0f / (float) refreshHz;
        constexpr float rmsCoeff      = 0.12f;              // ~300 ms at 30 Hz
        constexpr int   holdFrames    = (int) (1.5f * refreshHz);
    }

    MeterColumn::MeterColumn (FourColorProcessor& processor, Side s)
        : proc (processor), side (s)
    {
        const bool isInput = side == Side::input;

        trim = std::make_unique<Knob> (proc.apvts,
                                       isInput ? param::input : param::output,
                                       isInput ? "INPUT" : "OUTPUT",
                                       tokens::neutralArcII, Knob::Size::small,
                                       isInput ? "Level entering the crossover and colour engines."
                                               : "Final level leaving the plugin.");
        addAndMakeVisible (*trim);

        startTimerHz (refreshHz);
    }

    MeterColumn::~MeterColumn() = default;

    void MeterColumn::timerCallback()
    {
        auto& block = side == Side::input ? proc.inputMeter : proc.outputMeter;

        for (int c = 0; c < 2; ++c)
        {
            auto& ch = channels[c];

            const float peak = block.peak[c].exchange (0.0f);
            const float ms   = block.meanSquare[c].load (std::memory_order_relaxed);

            //  Clamped to the bottom of the scale, not to -200 dB. Nothing
            //  below floorDb can be drawn, so letting the ballistics carry on
            //  down to -200 only means the bars keep "moving" invisibly - the
            //  peak decay alone took seven seconds to get there, and the meter
            //  repainted all the way down.
            const float peakDb = juce::jmax (floorDb,
                                             juce::Decibels::gainToDecibels (peak, -200.0f));
            const float rmsDb  = juce::jmax (floorDb,
                                             juce::Decibels::gainToDecibels (std::sqrt (ms), -200.0f));

            //  Peak: instant attack, linear dB decay.
            ch.peakDb = peakDb > ch.peakDb ? peakDb
                                           : juce::jmax (peakDb, ch.peakDb - peakDecayDbPerFrame);

            //  RMS: one-pole in dB, which reads steadier than one in gain.
            ch.rmsDb += rmsCoeff * (rmsDb - ch.rmsDb);

            //  Hold tick.
            if (ch.peakDb >= ch.holdDb) { ch.holdDb = ch.peakDb; ch.holdAge = 0; }
            else if (++ch.holdAge > holdFrames) ch.holdDb = ch.peakDb;
        }

        if (block.clipped.load (std::memory_order_relaxed))
            clipped = true;

        bool changed = clipped != drawnClipped;

        for (int c = 0; c < 2 && ! changed; ++c)
            changed = std::abs (channels[c].peakDb - drawn[c].peakDb) > visibleStepDb
                   || std::abs (channels[c].rmsDb  - drawn[c].rmsDb)  > visibleStepDb
                   || std::abs (channels[c].holdDb - drawn[c].holdDb) > visibleStepDb;

        if (! changed)
            return;

        for (int c = 0; c < 2; ++c)
            drawn[c] = { channels[c].peakDb, channels[c].rmsDb, channels[c].holdDb };
        drawnClipped = clipped;

        repaint();
    }

    void MeterColumn::resized()
    {
        auto area = getLocalBounds().reduced (4, 2);

        //  Trim under the meter: the control and the reading it moves belong
        //  together, and Output attenuation has to be reachable at all times.
        trim->setBounds (area.removeFromBottom (58));
        area.removeFromBottom (2);

        clipArea = area.removeFromTop (12);
        scaleArea = area.removeFromRight (20);
        barsArea = area;
    }

    void MeterColumn::paint (juce::Graphics& g)
    {
        //  Clip indicator: red only when a real clip happened. Click to reset.
        {
            auto r = clipArea.toFloat().reduced (2.0f, 1.0f);
            g.setColour (clipped ? tokens::meterPeak : tokens::panelBase);
            g.fillRoundedRectangle (r, 2.0f);
            g.setFont (captionFont (8.0f, true));
            g.setColour (clipped ? juce::Colour (0xff14181e) : tokens::textDisabled);
            g.drawText ("CLIP", clipArea, juce::Justification::centred);
        }

        //  Scale marks.
        g.setFont (uiFont (8.0f));
        for (int db : { 0, -6, -12, -18, -36 })
        {
            const float y = (float) barsArea.getBottom()
                          - normalised ((float) db) * (float) barsArea.getHeight();
            g.setColour (tokens::gridMinor);
            g.drawHorizontalLine ((int) y, (float) barsArea.getX(), (float) barsArea.getRight());
            g.setColour (tokens::textDisabled);
            g.drawText (juce::String (db), scaleArea.getX(), (int) y - 5,
                        scaleArea.getWidth() - 2, 10, juce::Justification::centredRight);
        }

        //  Two bars, wide enough apart to read L from R.
        const float gap = 3.0f;
        const float barW = ((float) barsArea.getWidth() - gap) * 0.5f;

        for (int c = 0; c < 2; ++c)
        {
            auto bar = juce::Rectangle<float> ((float) barsArea.getX() + (float) c * (barW + gap),
                                               (float) barsArea.getY(), barW,
                                               (float) barsArea.getHeight());

            g.setColour (tokens::analyzerBack);
            g.fillRoundedRectangle (bar, 2.0f);

            auto fillTo = [&] (float db, float alpha)
            {
                const float t = normalised (db);
                if (t <= 0.0f)
                    return;

                const float h = t * bar.getHeight();
                auto filled = bar.withTop (bar.getBottom() - h);

                //  Output runs warmer as it approaches 0 dBFS; both use the
                //  same low-to-high ramp so a glance reads the same way on
                //  either side of the window.
                juce::ColourGradient grad (tokens::meterLow, 0.0f, bar.getBottom(),
                                           tokens::meterPeak, 0.0f, bar.getY(), false);
                grad.addColour (0.55, tokens::meterMid);
                grad.addColour (0.80, tokens::meterHigh);
                g.setGradientFill (grad);
                g.setOpacity (alpha);
                g.fillRoundedRectangle (filled, 2.0f);
                g.setOpacity (1.0f);
            };

            //  RMS as the solid body, peak as a lighter overlay above it.
            fillTo (channels[c].peakDb, 0.45f);
            fillTo (channels[c].rmsDb, 1.0f);

            //  Peak hold tick.
            const float holdT = normalised (channels[c].holdDb);
            if (holdT > 0.0f)
            {
                const float y = bar.getBottom() - holdT * bar.getHeight();
                g.setColour (channels[c].holdDb >= 0.0f ? tokens::meterPeak
                                                        : tokens::textPrimary.withAlpha (0.8f));
                g.fillRect (bar.getX(), y - 1.0f, bar.getWidth(), 1.6f);
            }
        }

        //  L / R.
        g.setFont (uiFont (8.0f));
        g.setColour (tokens::textDisabled);
        g.drawText ("L", barsArea.getX(), barsArea.getBottom() - 10, (int) barW, 9,
                    juce::Justification::centred);
        g.drawText ("R", barsArea.getX() + (int) (barW + gap), barsArea.getBottom() - 10,
                    (int) barW, 9, juce::Justification::centred);
    }

    void MeterColumn::mouseDown (const juce::MouseEvent& e)
    {
        if (clipArea.contains (e.getPosition()))
        {
            clipped = false;
            auto& block = side == Side::input ? proc.inputMeter : proc.outputMeter;
            block.clipped.store (false, std::memory_order_relaxed);
            repaint();
        }
    }
}
