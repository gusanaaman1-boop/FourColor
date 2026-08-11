#include "PluginEditor.h"

namespace fourcolor
{
    FourColorEditor::FourColorEditor (FourColorProcessor& p)
        : AudioProcessorEditor (p), proc (p)
    {
        setLookAndFeel (&laf);

        addAndMakeVisible (topBar);
        addAndMakeVisible (inputMeter);
        addAndMakeVisible (analyzer);
        addAndMakeVisible (outputMeter);
        addAndMakeVisible (bandHeaders);
        addAndMakeVisible (bandStrip);
        addAndMakeVisible (globalBar);

        selectedBand = juce::jlimit (0, numBands - 1,
                                     (int) proc.apvts.state.getProperty ("selectedBand", 0));
        analyzer.setSelectedBand (selectedBand);
        bandHeaders.setSelectedBand (selectedBand);
        bandStrip.setBand (selectedBand);

        analyzer.onBandSelected  = [this] (int b) { selectBand (b); };
        bandHeaders.onBandSelected = [this] (int b) { selectBand (b); };

        //  While a control is being dragged, the analyzer emphasises the region
        //  it affects. The kind codes match Analyzer::Emphasis.
        bandStrip.onEmphasisChanged = [this] (int kind, int band)
        {
            analyzer.setEmphasis ((ui::Analyzer::Emphasis) kind, band);
        };

        setResizable (true, true);
        setResizeLimits (900, 560, 1900, 1200);

        const int w = (int) proc.apvts.state.getProperty ("editorWidth", 980);
        const int h = (int) proc.apvts.state.getProperty ("editorHeight", 620);
        setSize (juce::jlimit (900, 1900, w), juce::jlimit (560, 1200, h));

        setWantsKeyboardFocus (true);
        startTimerHz (12);
    }

    FourColorEditor::~FourColorEditor()
    {
        setLookAndFeel (nullptr);
    }

    void FourColorEditor::selectBand (int band)
    {
        selectedBand = juce::jlimit (0, numBands - 1, band);
        analyzer.setSelectedBand (selectedBand);
        bandHeaders.setSelectedBand (selectedBand);
        bandStrip.setBand (selectedBand);
        proc.apvts.state.setProperty ("selectedBand", selectedBand, nullptr);
    }

    void FourColorEditor::timerCallback()
    {
        //  Undo granularity: close the running transaction whenever the mouse
        //  is up, so one drag or click undoes as a single step.
        if (! juce::Desktop::getInstance().getMainMouseSource().isDragging()
            && proc.undoManager.getNumActionsInCurrentTransaction() > 0)
            proc.undoManager.beginNewTransaction();

        //  Feed the selected band's real output level to the DRIVE sparks and
        //  the SPACE arcs, so they rest when there is no audio.
        const float peak = proc.getEngine().getBand (selectedBand).readAndResetOutputPeak();
        bandStrip.setEnergy (juce::jlimit (0.0f, 1.0f, peak * 1.6f));
    }

    bool FourColorEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            topBar.closeMasterDrawer();
            return true;
        }
        return false;
    }

    void FourColorEditor::mouseDown (const juce::MouseEvent&)
    {
        //  A click that reaches the editor itself landed outside every child,
        //  which includes outside the drawer.
        topBar.closeMasterDrawer();
    }

    void FourColorEditor::paint (juce::Graphics& g)
    {
        backgroundPaints.fetch_add (1, std::memory_order_relaxed);
        backgroundPaintArea.fetch_add ((long long) g.getClipBounds().getWidth()
                                           * g.getClipBounds().getHeight(),
                                       std::memory_order_relaxed);

        //  Ground: a very slight vertical lift towards the top bar.
        juce::ColourGradient bg (ui::tokens::backgroundTop, 0.0f, 0.0f,
                                 ui::tokens::backgroundDeep, 0.0f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    void FourColorEditor::resized()
    {
        proc.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
        proc.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);

        const float h = (float) getHeight();
        const float w = (float) getWidth();
        const int margin = juce::roundToInt (ui::metric::sideMargin * (w / 980.0f));

        using namespace ui::metric;
        auto rowBetween = [this, h, margin] (float top, float bottom)
        {
            return juce::Rectangle<int> (margin, juce::roundToInt (h * top),
                                         getWidth() - margin * 2,
                                         juce::roundToInt (h * (bottom - top)));
        };

        topBar.setBounds (0, 0, getWidth(), juce::roundToInt (h * topBarBottom));

        //  Meters flank the analyzer, each with its own trim beneath it, so the
        //  reading and the control that moves it are one object. The headers
        //  align to the analyzer's plot area, so both are inset by the same
        //  meter width.
        const int meterW = juce::jlimit (54, 76, juce::roundToInt (w * 0.062f));
        auto headerRow = rowBetween (headersTop, headersBottom);
        auto plotRow = rowBetween (analyzerTop, analyzerBottom);

        auto meterSpan = plotRow.getUnion (headerRow);
        inputMeter.setBounds (meterSpan.removeFromLeft (meterW));
        outputMeter.setBounds (meterSpan.removeFromRight (meterW));

        bandHeaders.setBounds (headerRow.withTrimmedLeft (meterW + 6)
                                        .withTrimmedRight (meterW + 6));
        analyzer.setBounds (plotRow.withTrimmedLeft (meterW + 6)
                                   .withTrimmedRight (meterW + 6));
        bandStrip.setBounds (rowBetween (panelTop, panelBottom));
        globalBar.setBounds (rowBetween (globalTop, 1.0f).withTrimmedBottom (margin / 2));
        //  ...and tell it where the output meter column sits, so the OUTPUT
        //  knob lines up under the meter it belongs to.
        globalBar.setMeterWidth (meterW);

    }
}
