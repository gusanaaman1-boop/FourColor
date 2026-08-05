#include "TopBar.h"

#include "../Core/PresetLibrary.h"
#include "../PluginProcessor.h"

namespace fourcolor::ui
{
    //  Power symbol toggle (global bypass). Lit ring = plugin ACTIVE.
    class TopBar::PowerButton : public juce::Button
    {
    public:
        PowerButton() : juce::Button ("Bypass") { setClickingTogglesState (true); }

        void paintButton (juce::Graphics& g, bool highlighted, bool) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float d = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 10.0f;
            const auto circle = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());

            //  Toggle ON means BYPASSED, so the symbol dims when toggled.
            const bool bypassed = getToggleState();
            auto c = bypassed ? colour::textDim.withAlpha (0.5f) : colour::text;
            if (highlighted) c = c.brighter (0.2f);

            g.setColour (c);
            juce::Path arc;
            arc.addCentredArc (circle.getCentreX(), circle.getCentreY(), d * 0.5f, d * 0.5f,
                               0.0f, 0.55f, juce::MathConstants<float>::twoPi - 0.55f, true);
            g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.drawLine (circle.getCentreX(), circle.getY() - 2.0f,
                        circle.getCentreX(), circle.getCentreY() - 2.0f, 1.8f);
        }
    };

    //  Undo arrow.
    class TopBar::IconButton : public juce::Button
    {
    public:
        IconButton() : juce::Button ("Undo") {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto bounds = getLocalBounds().toFloat();
            auto c = isEnabled() ? (highlighted || down ? colour::text : colour::textDim)
                                 : colour::textDim.withAlpha (0.35f);
            g.setColour (c);

            const float r = 6.5f;
            const auto centre = bounds.getCentre().translated (1.0f, 1.0f);
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, r, r, 0.0f,
                               juce::MathConstants<float>::pi * 1.75f,
                               juce::MathConstants<float>::pi * 0.55f, true);
            g.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

            //  Arrowhead at the arc's start (upper left).
            const float ax = centre.x + r * std::sin (juce::MathConstants<float>::pi * 1.75f);
            const float ay = centre.y - r * std::cos (juce::MathConstants<float>::pi * 1.75f);
            juce::Path head;
            head.addTriangle (ax - 4.0f, ay - 1.0f, ax + 2.5f, ay - 4.5f, ax + 1.5f, ay + 3.5f);
            g.fillPath (head);
        }
    };

    TopBar::TopBar (FourColorProcessor& processor)
        : proc (processor)
    {
        prevButton = std::make_unique<juce::ArrowButton> ("prev", 0.5f, colour::textDim);
        nextButton = std::make_unique<juce::ArrowButton> ("next", 0.0f, colour::textDim);
        prevButton->setTooltip ("Previous preset");
        nextButton->setTooltip ("Next preset");
        prevButton->onClick = [this] { proc.stepProgram (-1); };
        nextButton->onClick = [this] { proc.stepProgram (+1); };
        addAndMakeVisible (*prevButton);
        addAndMakeVisible (*nextButton);

        presetNameButton.setTooltip ("Choose a preset");
        presetNameButton.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (presetNameButton);

        //  A / B: the active slot is lit; clicking the inactive one switches.
        auto configureAb = [this] (juce::TextButton& b, int index)
        {
            b.setColour (juce::TextButton::buttonOnColourId, colour::panelLine.brighter (0.2f));
            b.onClick = [this, index]
            {
                if (proc.getABIndex() != index)
                    proc.toggleAB();
            };
            addAndMakeVisible (b);
        };
        configureAb (aButton, 0);
        configureAb (bButton, 1);
        aButton.setTooltip ("State slot A");
        bButton.setTooltip ("State slot B");

        undoButton = std::make_unique<IconButton>();
        undoButton->setTooltip ("Undo the last parameter change");
        undoButton->onClick = [this] { proc.undoManager.undo(); };
        addAndMakeVisible (*undoButton);

        qualityBox.addItemList ({ "DRAFT", "NORMAL", "HIGH", "ULTRA" }, 1);
        qualityBox.setTooltip ("Oversampling of the colour engines");
        addAndMakeVisible (qualityBox);
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, param::quality, qualityBox);

        powerButton = std::make_unique<PowerButton>();
        powerButton->setTooltip ("Latency-aligned global bypass");
        addAndMakeVisible (*powerButton);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, param::bypassed, *powerButton);

        startTimerHz (6);
        timerCallback();
    }

    TopBar::~TopBar() = default;

    void TopBar::timerCallback()
    {
        //  Preset caption with the modified asterisk, A/B lights, undo state.
        juce::String name = proc.getProgramName (proc.getCurrentProgram());
        if (proc.isPresetDirty())
            name << "*";
        if (name != presetNameButton.getButtonText())
            presetNameButton.setButtonText (name);

        aButton.setToggleState (proc.getABIndex() == 0, juce::dontSendNotification);
        bButton.setToggleState (proc.getABIndex() == 1, juce::dontSendNotification);
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
            section.addItem (i + 1, PresetLibrary::name (i), true,
                             i == proc.getCurrentProgram());
        }
        if (section.getNumItems() > 0)
            menu.addSubMenu (lastCategory, section);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (presetNameButton),
                            [this] (int result)
                            {
                                if (result > 0)
                                    proc.setCurrentProgram (result - 1);
                            });
    }

    void TopBar::paint (juce::Graphics& g)
    {
        g.setColour (colour::background);
        g.fillAll();

        g.setFont (labelFont (17.0f, false).withExtraKerningFactor (0.32f));
        g.setColour (colour::text);
        g.drawText ("FOUR COLOR", 18, 0, 260, getHeight(), juce::Justification::centredLeft);

        g.setFont (labelFont (11.5f));
        g.setColour (colour::textDim);
        g.drawText ("QUALITY", qualityLabelArea, juce::Justification::centredRight);

        //  "/" between A and B.
        g.setFont (uiFont (13.0f));
        g.drawText ("/", aButton.getRight(), 0, bButton.getX() - aButton.getRight(), getHeight(),
                    juce::Justification::centred);

        g.setColour (colour::panelLine);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void TopBar::resized()
    {
        auto area = getLocalBounds().reduced (10, 8);

        //  Left: the wordmark's reserved space (painted, not a component).
        area.removeFromLeft (200);

        //  Right side: power, quality.
        powerButton->setBounds (area.removeFromRight (36));
        area.removeFromRight (6);
        qualityBox.setBounds (area.removeFromRight (100).reduced (0, 2));
        qualityLabelArea = area.removeFromRight (70);
        area.removeFromRight (10);

        //  Centre: preset navigator + A/B + undo, centred in what remains.
        const int navW = juce::jlimit (180, 420, area.getWidth() - 160);
        auto nav = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), navW + 150),
                                               area.getHeight());

        prevButton->setBounds (nav.removeFromLeft (28).reduced (8, 10));
        presetNameButton.setBounds (nav.removeFromLeft (navW - 56).reduced (2, 1));
        nextButton->setBounds (nav.removeFromLeft (28).reduced (8, 10));
        nav.removeFromLeft (14);
        aButton.setBounds (nav.removeFromLeft (30).reduced (0, 2));
        nav.removeFromLeft (14);   // the painted "/"
        bButton.setBounds (nav.removeFromLeft (30).reduced (0, 2));
        nav.removeFromLeft (8);
        undoButton->setBounds (nav.removeFromLeft (28));
    }
}
