#include "MixMonitorEqView.h"

#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"

using namespace monitoreq;
using Step = MixMonitorEqSession::Step;

namespace
{
    constexpr int kRowHeight = 26, kLabelWidth = 78, kControlsWidth = 330;
    constexpr float kRangeDb = 15.0f;
    const juce::Colour kEqColour (0xffa77ce8);    // RKGraphEQ's lavender

    juce::String gainText (float db) { return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB"; }

    void styleSlider (juce::Slider& s, double low, double high, double step, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        s.setRange (low, high, step);
        s.setTextValueSuffix (suffix);
    }

    /** Sets a combo box's items only when they differ (an open menu stays open while measuring). */
    void setItems (juce::ComboBox& box, const std::vector<std::pair<int, juce::String>>& items, bool separatorBeforeLast = false)
    {
        bool same = box.getNumItems() == (int) items.size();
        for (int i = 0; same && i < (int) items.size(); ++i)
            same = box.getItemId (i) == items[(size_t) i].first && box.getItemText (i) == items[(size_t) i].second;
        if (same)
            return;
        box.clear (juce::dontSendNotification);
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (separatorBeforeLast && i + 1 == items.size())
                box.addSeparator();
            box.addItem (items[i].second, items[i].first);
        }
    }

    juce::String bandLabel (double hz)
    {
        return hz >= 1000.0 ? juce::String (hz / 1000.0, hz < 10000.0 && std::fmod (hz, 1000.0) > 0.0 ? 1 : 0) + "k" : juce::String ((int) hz);
    }
}

MixMonitorEqView::MixMonitorEqView (RKMixProcessor& p, int b, juce::Colour a, std::function<juce::String (int)> names)
    : session (p.getMonitorEq()), bus (b), accent (a), inputName (std::move (names))
{
    for (auto* t : { &speaker, &inEar })
    {
        t->setClickingTogglesState (false);
        t->setColour (juce::TextButton::buttonOnColourId, accent);
        addAndMakeVisible (*t);
    }
    speaker.onClick = [this] { edit ([] (auto& o) { o.type = Type::speaker; }); };
    inEar.onClick = [this] { edit ([] (auto& o) { o.type = Type::inEar; }); };

    input.onChange = [this] { edit ([this] (auto& o) { o.input = input.getSelectedId() - 1; }); };

    mic.onChange = [this]
    {
        const auto id = mic.getSelectedId();
        if (id == loadCalibrationItem)
            return chooseFile (true);
        edit ([id] (auto& o)
        {
            o.useCalibration = id == calibrationItem;
            if (id >= micFirst && id < calibrationItem)
                o.mic = (Mic) (id - micFirst);
        });
    };
    target.onChange = [this]
    {
        const auto t = (Target) (target.getSelectedId() - 1);
        edit ([t] (auto& o) { (o.type == Type::speaker ? o.speakerTarget : o.inEarTarget) = t; });
    };
    sides.addItemList ({ "Both", "Left", "Right", "Each ear" }, 1);
    sides.onChange = [this] { edit ([this] (auto& o) { o.sides = (MixMonitorEqSession::Sides) (sides.getSelectedId() - 1); }); };
    source.onChange = [this]
    {
        const auto id = source.getSelectedId();
        if (id == loadFileItem)
            return chooseFile (false);
        edit ([id] (auto& o) { o.useFile = id == fileItem; });
    };

    styleSlider (level, MixMeasure::kMinNoiseDb, MixMeasure::kMaxNoiseDb, 1.0, " dBFS");
    level.setTooltip ("The noise's RMS level. Start low and raise it until the room's noise is well below it.");
    level.onValueChange = [this]
    {
        const auto v = (float) level.getValue();
        edit ([v] (auto& o) { (o.type == Type::speaker ? o.speakerLevelDb : o.inEarLevelDb) = v; });
    };
    styleSlider (positions, 1.0, MixMonitorEqSession::kMaxPositions, 1.0, {});
    positions.onValueChange = [this]
    {
        const auto v = (int) positions.getValue();
        edit ([v] (auto& o) { (o.type == Type::speaker ? o.speakerPositions : o.inEarPositions) = v; });
    };
    styleSlider (bass, 0.0, 8.0, 0.5, " dB");
    bass.onValueChange = [this] { edit ([this] (auto& o) { o.bassDb = (float) bass.getValue(); }); };
    styleSlider (strength, 0.0, 100.0, 5.0, " %");
    strength.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
    strength.setTooltip ("How much of the correction to use.");
    strength.onValueChange = [this] { edit ([this] (auto& o) { o.strength = (float) strength.getValue() / 100.0f; }); };

    for (auto* c : std::initializer_list<juce::Component*> { &input, &mic, &target, &sides, &source, &level, &positions, &bass, &strength,
                                                             &startButton, &nextButton, &finishButton, &stopButton, &applyButton, &undoButton,
                                                             &status })
        addAndMakeVisible (*c);

    startButton.onClick = [this] { startedHere = session.start (bus); refresh(); };
    nextButton.onClick = [this] { session.next(); refresh(); };
    finishButton.onClick = [this] { session.finish(); refresh(); };
    stopButton.onClick = [this] { session.stop(); refresh(); };
    applyButton.onClick = [this] { session.apply(); refresh(); };
    undoButton.onClick = [this] { session.undo(); refresh(); };
    applyButton.setColour (juce::TextButton::buttonColourId, accent.withMultipliedAlpha (0.6f));

    status.setFont (rkfx::Fonts::regular (13.0f));
    status.setJustificationType (juce::Justification::centredLeft);
    status.setMinimumHorizontalScale (0.8f);

    session.addChangeListener (this);
    refresh();
}

MixMonitorEqView::~MixMonitorEqView()
{
    session.removeChangeListener (this);
    if (startedHere && session.isRunning() && session.getBus() == bus)
        session.stop();
}

void MixMonitorEqView::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void MixMonitorEqView::edit (const std::function<void (MixMonitorEqSession::Options&)>& change)
{
    auto o = session.getOptions();
    change (o);
    session.setOptions (o);
    refresh();
}

void MixMonitorEqView::chooseFile (bool calibration)
{
    chooser = std::make_unique<juce::FileChooser> (calibration ? "Load a mic calibration file" : "Load an in-ear measurement",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.txt;*.csv;*.frd;*.cal");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, calibration] (const juce::FileChooser& fc)
                          {
                              if (const auto file = fc.getResult(); file.existsAsFile())
                              {
                                  if (calibration)
                                      session.loadCalibration (file);
                                  else
                                      session.loadMeasurementFile (file);
                              }
                              refresh();   // back to what is chosen, whether loaded or cancelled
                          });
}

//==============================================================================
void MixMonitorEqView::refresh()
{
    const auto& o = session.getOptions();
    const auto running = session.isRunning();
    const auto inEarType = o.type == Type::inEar;
    const auto mine = session.getBus() == bus || session.getStep() == Step::idle;

    speaker.setToggleState (! inEarType, juce::dontSendNotification);
    inEar.setToggleState (inEarType, juce::dontSendNotification);

    std::vector<std::pair<int, juce::String>> items;
    for (int ch = 0; ch < mix::kChannels; ++ch)
        items.push_back ({ ch + 1, inputName (ch) });
    setItems (input, items);
    input.setSelectedId (o.input + 1, juce::dontSendNotification);

    items.clear();
    for (auto m : { Mic::flat, Mic::sm58, Mic::sm57, Mic::v7 })
        items.push_back ({ micFirst + (int) m, micName (m) });
    if (o.hasCalibration)
        items.push_back ({ calibrationItem, "Calibration: " + o.calibrationName });
    items.push_back ({ loadCalibrationItem, "Load calibration file..." });
    setItems (mic, items, true);
    mic.setSelectedId (o.useCalibration ? (int) calibrationItem : micFirst + (int) o.mic, juce::dontSendNotification);

    items.clear();
    for (auto t : targetsFor (o.type))
        items.push_back ({ (int) t + 1, targetName (t) });
    setItems (target, items);
    target.setSelectedId ((int) session.currentTarget() + 1, juce::dontSendNotification);

    sides.setSelectedId ((int) o.sides + 1, juce::dontSendNotification);

    items.clear();
    items.push_back ({ measureItem, "Measure (coupler)" });
    if (o.hasFile)
        items.push_back ({ fileItem, "File: " + o.fileName });
    items.push_back ({ loadFileItem, "Load measurement file..." });
    setItems (source, items, true);
    source.setSelectedId (o.useFile ? (int) fileItem : (int) measureItem, juce::dontSendNotification);

    level.setRange (MixMeasure::kMinNoiseDb, inEarType ? MixMonitorEqSession::kMaxInEarLevelDb : MixMeasure::kMaxNoiseDb, 1.0);
    level.setValue (session.currentLevelDb(), juce::dontSendNotification);
    positions.setValue (session.currentPositions(), juce::dontSendNotification);
    bass.setValue (o.bassDb, juce::dontSendNotification);
    strength.setValue (o.strength * 100.0f, juce::dontSendNotification);

    // Rows: In-ear adds sides, source and (Speakers in a room) bass.
    const auto usingFile = inEarType && o.useFile;
    rows.clear();
    rows.push_back ({ inEarType ? "Source" : "", inEarType ? (juce::Component*) &source : nullptr });
    if (! usingFile)
    {
        rows.push_back ({ inEarType ? "Coupler on" : "Mic on", &input });
        rows.push_back ({ inEarType ? "Coupler" : "Mic", &mic });
    }
    rows.push_back ({ "Target", &target });
    if (inEarType && session.currentTarget() == Target::speakersInRoom)
        rows.push_back ({ "Bass", &bass });
    if (! usingFile)
    {
        rows.push_back ({ "Level", &level });
        if (inEarType)
            rows.push_back ({ "Sides", &sides });
        rows.push_back ({ "Positions", &positions });
    }
    rows.erase (std::remove_if (rows.begin(), rows.end(), [] (const Row& r) { return r.control == nullptr; }), rows.end());
    for (auto* c : std::initializer_list<juce::Component*> { &input, &mic, &target, &sides, &source, &level, &positions, &bass })
        c->setVisible (std::any_of (rows.begin(), rows.end(), [c] (const Row& r) { return r.control == c; }));

    // What can change while measuring: target, mic, level, strength, bass; not what the run is measuring.
    for (auto* c : std::initializer_list<juce::Component*> { &speaker, &inEar, &input, &sides, &source, &positions })
        c->setEnabled (! running);

    const auto step = session.getStep();
    startButton.setButtonText (usingFile ? "Use file" : session.hasResult() && mine ? "Measure again" : "Start");
    startButton.setEnabled (! running);
    nextButton.setEnabled (running && mine && step == Step::waiting);
    finishButton.setEnabled (running && mine && step == Step::waiting && session.positionsMeasured() > 0);
    stopButton.setEnabled (running && mine);
    applyButton.setEnabled (mine && session.canApply());
    applyButton.setTooltip (session.canApply() ? "Write the suggestion into the bus's RKGraphEQ" : session.applyProblem());
    undoButton.setEnabled (session.canUndo());

    const auto msg = ! mine ? "Monitor EQ is measuring bus " + juce::String (session.getBus() + 1) + "."
                            : session.getMessage().isNotEmpty() ? session.getMessage() : session.describeStep();
    status.setText (msg, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, mine && session.getMessage().isNotEmpty() ? rkfx::Palette::warning : rkfx::Palette::text);

    resized();
    repaint();
}

void MixMonitorEqView::resized()
{
    auto r = getLocalBounds().reduced (4);
    controlsArea = r.removeFromLeft (kControlsWidth);
    r.removeFromLeft (8);

    auto c = controlsArea.reduced (8, 6);
    auto typeRow = c.removeFromTop (kRowHeight);
    typeRow.removeFromLeft (kLabelWidth);
    speaker.setBounds (typeRow.removeFromLeft (typeRow.getWidth() / 2).reduced (1, 1));
    inEar.setBounds (typeRow.reduced (1, 1));
    c.removeFromTop (4);
    for (auto& row : rows)
    {
        auto line = c.removeFromTop (kRowHeight);
        line.removeFromLeft (kLabelWidth);
        row.control->setBounds (line.reduced (0, 2));
    }

    auto buttons = r.removeFromBottom (kRowHeight + 4).reduced (0, 2);
    for (auto* b : { &startButton, &nextButton, &finishButton, &stopButton })
    {
        b->setBounds (buttons.removeFromLeft (b == &startButton ? 104 : 64));
        buttons.removeFromLeft (4);
    }
    undoButton.setBounds (buttons.removeFromRight (56));
    buttons.removeFromRight (4);
    applyButton.setBounds (buttons.removeFromRight (76));
    buttons.removeFromRight (8);
    strength.setBounds (buttons.removeFromRight (juce::jmin (180, juce::jmax (0, buttons.getWidth() - 70))));
    strengthLabel = buttons.removeFromRight (60);

    status.setBounds (r.removeFromBottom (22));
    progressArea = r.removeFromBottom (4);
    r.removeFromBottom (2);
    graphArea = r;
}

void MixMonitorEqView::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, controlsArea, rkfx::Palette::panel);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    auto c = controlsArea.reduced (8, 6);
    g.drawText ("Type", c.removeFromTop (kRowHeight), juce::Justification::centredLeft);
    c.removeFromTop (4);
    for (auto& row : rows)
        g.drawText (row.label, c.removeFromTop (kRowHeight).withWidth (kLabelWidth), juce::Justification::centredLeft);

    // The strength's label sits left of it.
    g.drawText ("Strength", strengthLabel, juce::Justification::centredRight);

    paintGraph (g, graphArea);

    const auto step = session.getStep();
    if (session.getBus() == bus && (step == Step::floor || step == Step::settling || step == Step::measuring))
    {
        g.setColour (rkfx::Palette::knobTrack);
        g.fillRect (progressArea);
        g.setColour (accent);
        g.fillRect (progressArea.withWidth (juce::roundToInt ((float) progressArea.getWidth() * session.getProgress())));
    }
}

void MixMonitorEqView::paintGraph (juce::Graphics& g, juce::Rectangle<int> area)
{
    rkfx::drawPanel (g, area, rkfx::Palette::panel);
    auto plot = area.reduced (34, 18).withTrimmedBottom (4).toFloat();
    const auto columnWidth = plot.getWidth() / (float) kBands;
    const auto xOf = [&] (int b) { return plot.getX() + ((float) b + 0.5f) * columnWidth; };
    const auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (-kRangeDb, kRangeDb, db), kRangeDb, -kRangeDb, plot.getY(), plot.getBottom()); };

    // Grid
    g.setFont (rkfx::Fonts::regular (11.0f));
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        g.setColour (rkfx::Palette::outline.withAlpha (db == 0.0f ? 1.0f : 0.5f));
        g.drawHorizontalLine (juce::roundToInt (yOf (db)), plot.getX(), plot.getRight());
        g.setColour (rkfx::Palette::textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (area.toFloat().getX(), yOf (db) - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredRight);
    }
    for (int b = 2; b < kBands; b += 3)
        g.drawText (bandLabel (MixMeasure::centres()[(size_t) b]),
                    juce::Rectangle<float> (xOf (b) - 20.0f, plot.getBottom() + 2.0f, 40.0f, 14.0f), juce::Justification::centred);

    const auto& s = session.getSuggestion();
    const auto mine = session.getBus() == bus;
    const auto hasData = mine && std::any_of (s.noData.begin(), s.noData.end(), [] (bool v) { return ! v; });

    // The target (as the suggestion normalises it; idle: as it is, 0 dB at 1 kHz).
    auto targetDb = s.targetDb;
    if (! hasData)
    {
        targetDb = monitoreq::target (session.currentTarget(), session.getOptions().bassDb);
        const auto at1k = targetDb[17];   // 1 kHz
        for (auto& v : targetDb)
            v -= at1k;
    }
    juce::Path targetPath;
    for (int b = 0; b < kBands; ++b)
        b == 0 ? targetPath.startNewSubPath (xOf (b), yOf (targetDb[(size_t) b])) : targetPath.lineTo (xOf (b), yOf (targetDb[(size_t) b]));
    g.setColour (rkfx::Palette::textDim);
    const float dashes[] = { 4.0f, 4.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, targetPath, dashes, 2);
    g.fillPath (dashed);

    if (hasData)
    {
        // The correction: bars from 0 dB (outlined where a null or out of range kept it at 0).
        for (int b = 0; b < kBands; ++b)
        {
            const auto c = s.correction[(size_t) b];
            const auto x = xOf (b) - columnWidth * 0.32f;
            const auto top = juce::jmin (yOf (0.0f), yOf (c)), bottom = juce::jmax (yOf (0.0f), yOf (c));
            g.setColour (kEqColour.withAlpha (s.lowConfidence[(size_t) b] ? 0.35f : 0.7f));
            g.fillRect (juce::Rectangle<float> (x, top, columnWidth * 0.64f, juce::jmax (1.0f, bottom - top)));
            if (s.null[(size_t) b] || s.outOfRange[(size_t) b])
            {
                g.setColour (rkfx::Palette::textDim);
                g.drawRect (juce::Rectangle<float> (x, yOf (0.0f) - 3.0f, columnWidth * 0.64f, 6.0f), 1.0f);
            }
        }

        // The response, and what it becomes with the correction.
        juce::Path response, after;
        bool open = false;
        for (int b = 0; b < kBands; ++b)
        {
            if (s.noData[(size_t) b])
            {
                g.setColour (rkfx::Palette::textDim);
                g.drawText ("-", juce::Rectangle<float> (xOf (b) - 6.0f, yOf (0.0f) - 16.0f, 12.0f, 12.0f), juce::Justification::centred);
                open = false;
                continue;
            }
            const auto y = yOf (s.normalised[(size_t) b]), ya = yOf (s.normalised[(size_t) b] + s.correction[(size_t) b]);
            if (open)
            {
                response.lineTo (xOf (b), y);
                after.lineTo (xOf (b), ya);
            }
            else
            {
                response.startNewSubPath (xOf (b), y);
                after.startNewSubPath (xOf (b), ya);
            }
            open = true;
        }
        if (session.hasResult())
        {
            g.setColour (rkfx::Palette::text.withAlpha (0.55f));
            g.strokePath (after, juce::PathStrokeType (1.2f));
        }
        g.setColour (accent);
        g.strokePath (response, juce::PathStrokeType (2.0f));
        for (int b = 0; b < kBands; ++b)
            if (! s.noData[(size_t) b])
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ xOf (b), yOf (s.normalised[(size_t) b]) })
                                   .expanded (s.lowConfidence[(size_t) b] ? -1.0f : 0.0f));
    }

    // Legend
    auto legend = area.reduced (40, 2).removeFromTop (14).toFloat();
    const auto key = [&] (juce::Colour colour, const juce::String& text)
    {
        g.setColour (colour);
        g.fillRect (legend.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 3.0f));
        legend.removeFromLeft (4.0f);
        g.setColour (rkfx::Palette::textDim);
        const auto w = juce::GlyphArrangement::getStringWidth (rkfx::Fonts::regular (11.0f), text) + 14.0f;
        g.drawText (text, legend.removeFromLeft (w), juce::Justification::centredLeft);
    };
    key (accent, "Measured");
    key (rkfx::Palette::textDim, "Target");
    key (kEqColour, "Correction");
    if (session.hasResult() && mine)
        key (rkfx::Palette::text.withAlpha (0.55f), "With it");
    if (mine && session.getEarDifferenceDb() > 0.0f)
        key (rkfx::Palette::warning, "L/R up to " + gainText (session.getEarDifferenceDb()));
}
