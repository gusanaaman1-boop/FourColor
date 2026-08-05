#include "TopBar.h"

#include "../Core/PresetLibrary.h"
#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    //  Power symbol. Lit = plugin active; dimmed with a faint red ring when
    //  bypassed.
    class TopBar::PowerButton : public juce::Button
    {
    public:
        PowerButton() : juce::Button ("Bypass") { setClickingTogglesState (true); }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float d = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 12.0f;
            auto circle = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());
            if (down) circle.translate (0.0f, 1.0f);

            const bool bypassed = getToggleState();
            auto c = bypassed ? tokens::textMuted : tokens::textPrimary;
            if (highlighted) c = c.brighter (0.25f);

            if (bypassed)
            {
                g.setColour (tokens::meterHigh.withAlpha (0.35f));
                g.drawEllipse (circle.expanded (4.0f), 1.0f);
            }

            g.setColour (c);
            juce::Path arc;
            const float r = d * 0.5f;
            arc.addCentredArc (circle.getCentreX(), circle.getCentreY(), r, r, 0.0f,
                               0.62f, juce::MathConstants<float>::twoPi - 0.62f, true);
            g.strokePath (arc, juce::PathStrokeType (1.9f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.drawLine (circle.getCentreX(), circle.getY() - 2.5f,
                        circle.getCentreX(), circle.getCentreY() - 1.5f, 1.9f);
        }
    };

    class TopBar::UndoButton : public juce::Button
    {
    public:
        UndoButton() : juce::Button ("Undo") {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto bounds = getLocalBounds().toFloat();
            auto c = isEnabled() ? (highlighted || down ? tokens::textPrimary : tokens::textSecondary)
                                 : tokens::textDisabled;
            g.setColour (c);

            const float r = 6.5f;
            const auto centre = bounds.getCentre().translated (1.0f, 1.0f);
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, r, r, 0.0f,
                               juce::MathConstants<float>::pi * 1.72f,
                               juce::MathConstants<float>::pi * 0.60f, true);
            g.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

            const float a = juce::MathConstants<float>::pi * 1.72f;
            const float ax = centre.x + r * std::sin (a);
            const float ay = centre.y - r * std::cos (a);
            juce::Path head;
            head.addTriangle (ax - 4.2f, ay - 1.2f, ax + 2.4f, ay - 4.6f, ax + 1.6f, ay + 3.4f);
            g.fillPath (head);
        }
    };

    //  The preset name field: a framed, centred caption that opens the menu.
    class TopBar::PresetField : public juce::Button
    {
    public:
        PresetField() : juce::Button ("Preset") {}

        void setDisplayText (const juce::String& t) { if (t != text) { text = t; repaint(); } }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (0.5f);

            g.setColour (down ? tokens::panelPressed : tokens::globalBack);
            g.fillRoundedRectangle (bounds, metric::cornerSmall);
            g.setColour (highlighted ? tokens::borderFocus : tokens::borderNormal);
            g.drawRoundedRectangle (bounds, metric::cornerSmall, 1.0f);

            g.setFont (uiFont (13.0f));
            g.setColour (tokens::textPrimary);
            g.drawText (text, getLocalBounds(), juce::Justification::centred);
        }

    private:
        juce::String text { "Default" };
    };

    TopBar::TopBar (FourColorProcessor& processor)
        : proc (processor)
    {
        prevButton = std::make_unique<juce::ArrowButton> ("prev", 0.5f, tokens::textSecondary);
        nextButton = std::make_unique<juce::ArrowButton> ("next", 0.0f, tokens::textSecondary);
        prevButton->setTooltip ("Previous preset");
        nextButton->setTooltip ("Next preset");
        prevButton->onClick = [this] { proc.stepProgram (-1); };
        nextButton->onClick = [this] { proc.stepProgram (+1); };
        addAndMakeVisible (*prevButton);
        addAndMakeVisible (*nextButton);

        presetField = std::make_unique<PresetField>();
        presetField->setTooltip ("Choose a preset");
        presetField->onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (*presetField);

        auto configureAb = [this] (juce::TextButton& b, int index)
        {
            b.setColour (juce::TextButton::buttonOnColourId, tokens::bandHigh);
            b.onClick = [this, index] { if (proc.getABIndex() != index) proc.toggleAB(); };
            addAndMakeVisible (b);
        };
        configureAb (aButton, 0);
        configureAb (bButton, 1);
        aButton.setTooltip ("State slot A");
        bButton.setTooltip ("State slot B");

        undoButton = std::make_unique<UndoButton>();
        undoButton->setTooltip ("Undo the last parameter change");
        undoButton->onClick = [this] { proc.undoManager.undo(); };
        addAndMakeVisible (*undoButton);

        qualityBox.addItemList ({ "DRAFT 1x", "NORMAL 2x", "HIGH 4x", "ULTRA 8x" }, 1);
        qualityBox.setTooltip ("Oversampling of the colour engines");
        addAndMakeVisible (qualityBox);
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, param::quality, qualityBox);

        powerButton = std::make_unique<PowerButton>();
        powerButton->setTooltip ("Latency-aligned global bypass");
        addAndMakeVisible (*powerButton);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, param::bypassed, *powerButton);

        startTimerHz (8);
        timerCallback();
    }

    TopBar::~TopBar() = default;

    void TopBar::timerCallback()
    {
        juce::String name = proc.getProgramName (proc.getCurrentProgram());
        if (proc.isPresetDirty())
            name << "*";
        presetField->setDisplayText (name);

        const bool aOn = proc.getABIndex() == 0;
        if (aButton.getToggleState() != aOn)
        {
            aButton.setToggleState (aOn, juce::dontSendNotification);
            bButton.setToggleState (! aOn, juce::dontSendNotification);
        }

        undoButton->setEnabled (proc.undoManager.canUndo());
    }

    void TopBar::showPresetMenu()
    {
        juce::PopupMenu menu;
        juce::String lastCategory;
        juce::PopupMenu section;

        for (int i = 0; i < PresetLibrary::numPresets(); ++i)
        {
            const auto category = PresetLibrary::category (i);
            if (category != lastCategory)
            {
                if (section.getNumItems() > 0)
                    menu.addSubMenu (lastCategory, section);
                section = {};
                lastCategory = category;
            }
            section.addItem (i + 1, PresetLibrary::name (i), true, i == proc.getCurrentProgram());
        }
        if (section.getNumItems() > 0)
            menu.addSubMenu (lastCategory, section);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetField.get()),
                            [this] (int result) { if (result > 0) proc.setCurrentProgram (result - 1); });
    }

    void TopBar::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (tokens::backgroundTop);
        g.fillRect (bounds);

        //  Bottom hairline plus its soft shadow.
        g.setColour (tokens::borderSoft);
        g.fillRect (0.0f, bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.38f * (1.0f - i / 5.0f) * 0.5f));
            g.fillRect (0.0f, bounds.getBottom() + (float) i, bounds.getWidth(), 1.0f);
        }

        //  Wordmark: light, widely spaced, never bold.
        g.setFont (uiFont (17.0f).withExtraKerningFactor (0.30f));
        g.setColour (tokens::textPrimary);
        const auto textArea = logoArea.toFloat();
        g.drawText ("FOUR COLOR", textArea, juce::Justification::centredLeft);

        //  Four band dots after the wordmark.
        const float dotSize = 7.0f;
        const float dotsX = textArea.getX()
                          + juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "FOUR COLOR")
                          + 16.0f;
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (tokens::band[i]);
            g.fillEllipse (dotsX + (float) i * (dotSize + 7.0f),
                           textArea.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
        }

        //  The "/" between A and B.
        g.setFont (uiFont (13.0f));
        g.setColour (tokens::textMuted);
        g.drawText ("/", aButton.getRight(), 0, bButton.getX() - aButton.getRight(), getHeight(),
                    juce::Justification::centred);
    }

    void TopBar::resized()
    {
        auto area = getLocalBounds().reduced (16, 9);

        logoArea = area.removeFromLeft (215);

        powerButton->setBounds (area.removeFromRight (34));
        area.removeFromRight (10);
        qualityBox.setBounds (area.removeFromRight (112));
        area.removeFromRight (12);

        //  Preset browser is ~26% of the window and stays centred; A/B and undo
        //  sit immediately to its right, as in the reference.
        const int presetW = juce::jlimit (170, 340, juce::roundToInt (getWidth() * 0.26f));
        const int groupW = presetW + 56 + 14 + 76 + 10 + 26;
        auto group = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), groupW),
                                                 area.getHeight());

        prevButton->setBounds (group.removeFromLeft (26).reduced (7, 10));
        presetField->setBounds (group.removeFromLeft (presetW).reduced (2, 0));
        nextButton->setBounds (group.removeFromLeft (26).reduced (7, 10));

        group.removeFromLeft (16);
        aButton.setBounds (group.removeFromLeft (32).reduced (0, 2));
        group.removeFromLeft (12);   // the painted "/"
        bButton.setBounds (group.removeFromLeft (32).reduced (0, 2));
        group.removeFromLeft (10);
        undoButton->setBounds (group.removeFromLeft (26));
    }
}
