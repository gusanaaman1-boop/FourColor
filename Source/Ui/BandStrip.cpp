#include "BandStrip.h"

namespace fourcolor::ui
{
    namespace
    {
        const char* colorNames[4] = { "WARM", "IRON", "BITE", "FUZZ" };
        constexpr float colorHeaderH = 20.0f;
        constexpr float colorRowGap = 2.0f;
    }

    BandStrip::BandStrip (juce::AudioProcessorValueTreeState& apvts)
        : state (apvts)
    {
        //  The COLOR rows are painted by this component; clicks are handled in
        //  mouseDown, so there are no invisible buttons floating on top.
        auto accent = tokens::band[0];

        drive = std::make_unique<Knob> (state, param::band (0, param::drive), "DRIVE",
                                        accent, Knob::Size::large,
                                        "Amount of harmonic saturation");
        drive->setSparks (true);
        drive->setGlow (0.18f);

        tone = std::make_unique<Knob> (state, param::band (0, param::tone), "TONE",
                                       tokens::bandHiMid.withSaturation (0.45f), Knob::Size::medium,
                                       "Shape harmonic darkness or brightness");
        tone->setSideCaptions ("DARK", "BRIGHT");

        space = std::make_unique<Knob> (state, param::band (0, param::space), "SPACE / SPREAD",
                                        tokens::bandHigh, Knob::Size::medium,
                                        "Widen and diffuse generated harmonics");
        space->setSpreadArcs (true);
        space->setGlow (0.15f);

        bandMix = std::make_unique<Knob> (state, param::band (0, param::bandMix), "MIX",
                                          tokens::neutralArc, Knob::Size::small,
                                          "Blend processed and clean band");
        level = std::make_unique<Knob> (state, param::band (0, param::level), "LEVEL",
                                        tokens::neutralArcII, Knob::Size::small,
                                        "Band output level");

        for (auto* k : { drive.get(), tone.get(), space.get(), bandMix.get(), level.get() })
        {
            addAndMakeVisible (*k);
            k->onDragStateChanged = [this, k] (bool dragging) { setDragging (k, dragging); };
        }

        behaviorSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        behaviorSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        //  The value must never jump to the click position on mouse-down.
        behaviorSlider.setSliderSnapsToMousePosition (false);
        behaviorSlider.setTooltip ("Shift saturation response between body and transients");
        behaviorSlider.setVelocityModeParameters (1.0, 1, 0.08, true,
                                                  juce::ModifierKeys::ctrlModifier);
        behaviorSlider.onDragStart = [this]
        {
            behaviorDragging = true;
            if (onEmphasisChanged)
                onEmphasisChanged (behaviorSlider.getValue() < 0.0 ? 2 : 3, band);
            repaint();
        };
        behaviorSlider.onDragEnd = [this]
        {
            behaviorDragging = false;
            if (onEmphasisChanged) onEmphasisChanged (0, band);
            repaint();
        };
        behaviorSlider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (behaviorSlider);

        setBand (0);
    }

    void BandStrip::setDragging (Knob* which, bool isDragging)
    {
        draggingKnob = isDragging ? which : nullptr;

        if (onEmphasisChanged)
        {
            int kind = 0;
            if (isDragging)
            {
                if (which == drive.get()) kind = 1;
                else if (which == tone.get()) kind = 4;
                else if (which == space.get()) kind = 5;
            }
            onEmphasisChanged (kind, band);
        }

        //  Peers dim to 82% while one control is being handled.
        for (auto* k : { drive.get(), tone.get(), space.get(), bandMix.get(), level.get() })
            k->setAlpha (draggingKnob == nullptr || k == draggingKnob ? 1.0f : 0.82f);
        behaviorSlider.setAlpha (draggingKnob == nullptr ? 1.0f : 0.82f);

        repaint();
    }

    void BandStrip::setInteractionPreview (Control control, bool hover, bool drag)
    {
        auto* knob = control == Control::drive ? drive.get()
                   : control == Control::tone  ? tone.get()
                   : control == Control::space ? space.get()
                                               : nullptr;

        for (auto* k : { drive.get(), tone.get(), space.get(), bandMix.get(), level.get() })
            k->setInteractionPreview (k == knob && hover, k == knob && drag);

        if (control == Control::behavior)
        {
            behaviorSlider.getProperties().set ("forceHover", hover);
            behaviorSlider.getProperties().set ("forceDrag", drag);
            behaviorDragging = drag;
            if (drag && onEmphasisChanged)
                onEmphasisChanged (behaviorSlider.getValue() < 0.0 ? 2 : 3, band);
            behaviorSlider.repaint();
        }
        else if (knob != nullptr && drag)
        {
            setDragging (knob, true);
        }

        repaint();
    }

    void BandStrip::setEnergy (float level01)
    {
        drive->setEnergy (level01);
        space->setEnergy (level01);
    }

    void BandStrip::rebindColorButtons()
    {
        auto* p = state.getParameter (param::band (band, param::color));
        jassert (p != nullptr);

        colorAttachment = std::make_unique<juce::ParameterAttachment> (
            *p,
            [this] (float v) { activeColor = juce::roundToInt (v); repaint(); },
            nullptr);
        colorAttachment->sendInitialUpdate();
    }

    void BandStrip::setBand (int bandIndex)
    {
        band = juce::jlimit (0, numBands - 1, bandIndex);
        const auto accent = tokens::band[band];

        drive  ->rebind (param::band (band, param::drive));
        tone   ->rebind (param::band (band, param::tone));
        space  ->rebind (param::band (band, param::space));
        bandMix->rebind (param::band (band, param::bandMix));
        level  ->rebind (param::band (band, param::level));

        //  DRIVE sweeps from the band's own colour into magenta, as in the
        //  reference; the other controls keep their functional colours.
        drive->setAccent (accent, tokens::bandHiMid);

        behaviorAttachment.reset();
        behaviorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, param::band (band, param::behavior), behaviorSlider);

        rebindColorButtons();
        repaint();
    }

    juce::Rectangle<float> BandStrip::colorRowBounds (int index) const
    {
        //  The four rows always fit the panel: at 900x560 a fixed 31 px row
        //  ran off the bottom edge.
        auto list = colorArea.toFloat().withTrimmedTop (colorHeaderH);
        const float rowH = juce::jlimit (22.0f, 33.0f,
                                         (list.getHeight() - colorRowGap * 3.0f) / 4.0f);
        return { list.getX(), list.getY() + (float) index * (rowH + colorRowGap),
                 list.getWidth(), rowH };
    }

    void BandStrip::paint (juce::Graphics& g)
    {
        const auto accent = tokens::band[band];
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        paint::glow (g, bounds, metric::corner, accent, 14.0f, 0.11f);
        g.setColour (tokens::panelBase);
        g.fillRoundedRectangle (bounds, metric::corner);
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.drawLine (bounds.getX() + metric::corner, bounds.getY() + 1.0f,
                    bounds.getRight() - metric::corner, bounds.getY() + 1.0f, 1.0f);
        g.setColour (accent.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds, metric::corner, 1.0f);

        //  --- COLOR list ---------------------------------------------------------
        g.setFont (captionFont (11.5f));
        g.setColour (tokens::textSecondary);
        g.drawText ("COLOR", colorArea.withHeight (18), juce::Justification::centred);

        for (int i = 0; i < 4; ++i)
        {
            const auto r = colorRowBounds (i);
            const bool on = i == activeColor;
            const bool hover = i == hoverColorRow && ! on;

            if (on)
            {
                juce::ColourGradient grad (accent.withAlpha (0.28f), r.getX(), r.getY(),
                                           accent.withAlpha (0.12f), r.getX(), r.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (r, metric::cornerSmall);
                g.setColour (accent);
                g.drawRoundedRectangle (r.reduced (0.5f), metric::cornerSmall, 1.0f);
            }
            else if (hover)
            {
                g.setColour (juce::Colours::white.withAlpha (0.03f));
                g.fillRoundedRectangle (r, metric::cornerSmall);
            }

            //  Radio dot: filled when active - selection is never colour-only.
            const float cy = r.getCentreY();
            g.setColour (on ? accent : tokens::borderNormal.brighter (0.2f));
            g.drawEllipse (r.getX() + 11.0f, cy - 4.5f, 9.0f, 9.0f, 1.3f);
            if (on)
            {
                g.setColour (accent);
                g.fillEllipse (r.getX() + 13.5f, cy - 2.0f, 4.0f, 4.0f);
            }

            g.setFont (captionFont (12.0f, on));
            g.setColour (on ? tokens::textPrimary : (hover ? tokens::textPrimary : tokens::textSecondary));
            g.drawText (colorNames[i], r.toNearestInt().withTrimmedLeft (30),
                        juce::Justification::centredLeft);
        }

        //  --- BEHAVIOR captions ---------------------------------------------------
        {
            g.setFont (captionFont (11.5f));
            g.setColour (tokens::textSecondary);
            g.drawText ("BEHAVIOR", behaviorArea.withHeight (18), juce::Justification::centred);

            const auto sb = behaviorSlider.getBounds();
            const double value = behaviorSlider.getValue();
            const bool leaningBody = value < -0.5;
            const bool leaningAttack = value > 0.5;

            auto captionColour = [&] (bool lean)
            {
                if (! behaviorDragging) return tokens::textMuted;
                return lean ? tokens::textPrimary : tokens::textMuted;
            };

            g.setFont (captionFont (11.0f, behaviorDragging && leaningBody));
            g.setColour (captionColour (leaningBody));
            g.drawText ("BODY", sb.getX() - 4, sb.getBottom() + 2, 90, 15,
                        juce::Justification::centredLeft);

            g.setFont (captionFont (11.0f, behaviorDragging && leaningAttack));
            g.setColour (captionColour (leaningAttack));
            g.drawText ("ATTACK", sb.getRight() - 86, sb.getBottom() + 2, 90, 15,
                        juce::Justification::centredRight);

            //  Centre value readout.
            g.setFont (uiFont (11.5f));
            g.setColour (tokens::textPrimary);
            const int shown = juce::roundToInt (value);
            g.drawText (juce::String (shown > 0 ? "+" : "") + juce::String (shown),
                        sb.getX(), sb.getBottom() + 2, sb.getWidth(), 15,
                        juce::Justification::centred);
        }

        //  --- divider before MIX / LEVEL -----------------------------------------
        g.setColour (tokens::borderSoft);
        g.fillRect ((float) dividerX, (float) getHeight() * 0.16f, 1.0f, (float) getHeight() * 0.68f);
    }

    void BandStrip::resized()
    {
        auto area = getLocalBounds().reduced (16, 12);
        const int w = area.getWidth();

        colorArea = area.removeFromLeft (juce::roundToInt ((float) w * 0.155f));
        area.removeFromLeft (8);

        drive->setBounds (area.removeFromLeft (juce::roundToInt ((float) w * 0.17f)));
        area.removeFromLeft (6);

        behaviorArea = area.removeFromLeft (juce::roundToInt ((float) w * 0.305f));
        behaviorSlider.setBounds (behaviorArea.withTrimmedTop (24)
                                              .withTrimmedBottom (24)
                                              .reduced (14, 0));
        area.removeFromLeft (6);

        tone->setBounds (area.removeFromLeft (juce::roundToInt ((float) w * 0.135f)));
        space->setBounds (area.removeFromLeft (juce::roundToInt ((float) w * 0.140f)));

        area.removeFromLeft (10);
        dividerX = area.getX();
        area.removeFromLeft (10);

        rightGroup = area;
        const int half = area.getHeight() / 2;
        bandMix->setBounds (area.removeFromTop (half).reduced (2, 0));
        level->setBounds (area.reduced (2, 0));
    }

    void BandStrip::mouseMove (const juce::MouseEvent& e)
    {
        int row = -1;
        for (int i = 0; i < 4; ++i)
            if (colorRowBounds (i).contains (e.position))
                row = i;

        if (row != hoverColorRow)
        {
            hoverColorRow = row;
            repaint();
        }
    }

    void BandStrip::mouseExit (const juce::MouseEvent&)
    {
        if (hoverColorRow != -1) { hoverColorRow = -1; repaint(); }
    }

    void BandStrip::mouseDown (const juce::MouseEvent& e)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (colorRowBounds (i).contains (e.position))
            {
                if (auto* p = state.getParameter (param::band (band, param::color)))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
                return;
            }
        }
    }
}
