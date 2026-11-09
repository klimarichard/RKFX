#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "plugin/ParamBuilder.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kTileHeight = 64;
    constexpr int kStatusHeight = 26;
    constexpr int kSwitchHeight = 48;

    juce::String stateName (LoopEngine::State s)
    {
        switch (s)
        {
            case LoopEngine::State::empty:       return "Empty";
            case LoopEngine::State::recording:   return "Recording";
            case LoopEngine::State::playing:     return "Playing";
            case LoopEngine::State::overdubbing: return "Overdubbing";
            case LoopEngine::State::stopped:     return "Stopped";
        }
        return {};
    }
}

//==============================================================================
RKLoopEditor::Footswitch::Footswitch (juce::RangedAudioParameter& p, const juce::String& text)
    : juce::TextButton (text), param (p)
{
    rkfx::tagParameter (*this, p.getParameterID(), "momentary", text);   // RKBoard: right-click to assign it
    onStateChange = [this]
    {
        const auto pressed = isDown();
        if (pressed == (param.getValue() >= 0.5f))
            return;
        param.beginChangeGesture();
        param.setValueNotifyingHost (pressed ? 1.0f : 0.0f);
        param.endChangeGesture();
    };
}

// A right-click belongs to the host (RKBoard's assign menu), not the switch.
void RKLoopEditor::Footswitch::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseDown (e);
}

void RKLoopEditor::Footswitch::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseUp (e);
}

//==============================================================================
RKLoopEditor::Tile::Tile (RKLoopEditor& o, int i) : owner (o), index (i)
{
    // On RKMix's iPad: a tap picks this loop (not tagged as the Loop parameter: its right-click is Export).
    getProperties().set ("rkfx.pick", "loop");
    getProperties().set ("rkfx.pickValue", index + 1);
}

void RKLoopEditor::Tile::paint (juce::Graphics& g)
{
    const auto& engine = owner.loopProcessor.getEngine();
    const auto state = engine.getState (index);
    const auto selected = engine.getSelected() == index;
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // Body: brighter when it sounds, warning colour while it records.
    const auto sounding = state == LoopEngine::State::playing || state == LoopEngine::State::overdubbing;
    const auto writing = state == LoopEngine::State::recording || state == LoopEngine::State::overdubbing;
    auto body = rkfx::Palette::knobBody;
    if (sounding)
        body = body.interpolatedWith (owner.accent(), 0.35f);
    if (writing)
        body = body.interpolatedWith (rkfx::Palette::warning, 0.45f);
    g.setColour (body);
    g.fillRoundedRectangle (bounds, 4.0f);

    // Queued (waiting for the downbeat or the end of the part): a blinking outline.
    if (selected || (engine.isQueued (index) && owner.blink))
    {
        g.setColour (engine.isQueued (index) ? rkfx::Palette::warning : owner.accent());
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 2.0f);
    }

    auto area = getLocalBounds().reduced (6, 4);
    auto top = area.removeFromTop (20);
    g.setColour (state == LoopEngine::State::empty ? rkfx::Palette::textDim : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (juce::String (index + 1), top, juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.0f));
    const auto isLayer = owner.loopProcessor.findParam (LoopParams::roleId (index))->getValue() >= 0.5f;   // the setting, even before audio runs
    g.drawText (isLayer ? "Layer" : "Part", top, juce::Justification::centredRight);

    // Length, and the position through the loop.
    const auto rate = engine.getMaxLength() / LoopEngine::kMaxSeconds;
    const auto length = engine.getRecordedLength (index);
    if (rate > 0.0 && length > 0)
    {
        g.drawText (rkfx::Units::time ((float) (length / rate * 1000.0)), area.removeFromTop (18), juce::Justification::centredLeft);
        auto bar = area.removeFromBottom (6).toFloat();
        g.setColour (rkfx::Palette::knobTrack);
        g.fillRoundedRectangle (bar, 2.0f);
        if (sounding && engine.getLength (index) > 0)
        {
            g.setColour (owner.accent());
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) (engine.getPosition (index) / engine.getLength (index))), 2.0f);
        }
    }
}

void RKLoopEditor::Tile::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        owner.addExportLoopItem (menu, index, "Export this loop...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
        return;
    }
    auto& loop = *owner.loopProcessor.findParam ("loop");
    loop.beginChangeGesture();
    loop.setValueNotifyingHost (loop.convertTo0to1 ((float) (index + 1)));
    loop.endChangeGesture();
}

//==============================================================================
RKLoopEditor::RKLoopEditor (RKLoopProcessor& p)
    : rkfx::EditorBase (p, 760, 450), loopProcessor (p)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    const char* const labels[] = { "Record", "Stop", "Undo", "Clear", "Next", "Previous" };
    for (int b = 0; b < 6; ++b)
    {
        footswitches[(size_t) b] = std::make_unique<Footswitch> (param (LoopParams::buttonIds[b]), labels[b]);
        addToCanvas (*footswitches[(size_t) b]);
    }
    footswitches[4]->setButtonText (juce::String::fromUTF8 ("Next \xe2\x96\xb6"));
    footswitches[5]->setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80 Prev"));

    for (int i = 0; i < 8; ++i)
    {
        tiles[(size_t) i] = std::make_unique<Tile> (*this, i);
        addToCanvas (*tiles[(size_t) i]);
    }

    saveLoops = std::make_unique<Footswitch> (param ("save_loops"), "Quick save");
    saveLoops->setTooltip ("Saves every loop to the Loops folder, with no dialog (the Save Loops parameter)");
    addToCanvas (*saveLoops);
    exportButton.setTooltip ("Save loops as WAV files");
    exportButton.onClick = [this] { showExportMenu(); };
    addToCanvas (exportButton);

    sync      = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    reverse   = std::make_unique<rkfx::ToggleSwitch> (param ("reverse"), accent(), "Reverse");
    halfSpeed = std::make_unique<rkfx::ToggleSwitch> (param ("half_speed"), accent(), "Half Speed");
    level     = std::make_unique<Knob> (param ("level"), "Loop Level");
    for (juce::Component* c : std::initializer_list<juce::Component*> { sync.get(), reverse.get(), halfSpeed.get(), level.get() })
        addToCanvas (*c);

    showRoleSwitch (p.getEngine().getSelected());
    finishSetup();
    refresher.startTimerHz (30);
}

void RKLoopEditor::showRoleSwitch (int loop)
{
    roleFor = loop;
    if (role != nullptr)
        removeChildComponent (role.get());
    role = std::make_unique<rkfx::SegmentedSwitch> (*loopProcessor.findParam (LoopParams::roleId (loop)), accent(),
                                                    juce::StringArray { "Part", "Layer" });
    // The other loops' roles are reached through this switch too (pick the loop first), also on RKMix's iPad.
    juce::StringArray roles;
    for (int i = 0; i < LoopParams::kLoops; ++i)
        roles.add (LoopParams::roleId (i));
    role->getProperties().set ("rkfx.reaches", roles.joinIntoString (" "));
    addToCanvas (*role);
    resized();
}

juce::Rectangle<int> RKLoopEditor::loopsArea() const   { return contentArea().removeFromTop (kTitleHeight + kTileHeight + 12); }
juce::Rectangle<int> RKLoopEditor::controlArea() const { return contentArea().withTrimmedTop (kTitleHeight + kTileHeight + 12 + 10); }

juce::Rectangle<int> RKLoopEditor::exportStatusArea() const
{
    // The Loops title row, between the title and the buttons.
    return loopsArea().reduced (8, 0).removeFromTop (kTitleHeight).withTrimmedLeft (90).withTrimmedRight (2 * 96 + 12);
}

juce::Rectangle<int> RKLoopEditor::statusArea() const
{
    return controlArea().reduced (8, 0).withTrimmedTop (kTitleHeight).removeFromTop (kStatusHeight);
}

void RKLoopEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = loopsArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        // Export, at the right of the title row.
        {
            auto title = loopsArea().reduced (8, 0).removeFromTop (kTitleHeight).reduced (0, 2);
            exportButton.setBounds (title.removeFromRight (96));
            title.removeFromRight (6);
            saveLoops->setBounds (title.removeFromRight (96));
        }

        auto row = area.removeFromTop (kTileHeight);
        const auto width = (row.getWidth() - 7 * 6) / 8;
        for (auto& t : tiles)
        {
            t->setBounds (row.removeFromLeft (width));
            row.removeFromLeft (6);
        }
    }

    {
        auto area = controlArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + kStatusHeight + 6);

        auto right = area.removeFromRight (120);
        level->setBounds (right.removeFromTop (Knob::kDefaultHeight));

        // Record, Stop, Undo, Clear, then Previous before Next.
        auto switches = area.removeFromTop (kSwitchHeight);
        const auto width = (switches.getWidth() - 5 * 6) / 6;
        for (const auto b : { 0, 1, 2, 3, 5, 4 })
        {
            footswitches[(size_t) b]->setBounds (switches.removeFromLeft (width));
            switches.removeFromLeft (6);
        }

        area.removeFromTop (10);
        auto toggles = area.removeFromTop (28);
        if (role != nullptr)
            role->setBounds (toggles.removeFromLeft (150));
        toggles.removeFromLeft (16);
        sync->setBounds (toggles.removeFromLeft (70));
        toggles.removeFromLeft (8);
        reverse->setBounds (toggles.removeFromLeft (90));
        toggles.removeFromLeft (8);
        halfSpeed->setBounds (toggles.removeFromLeft (110));
    }
}

void RKLoopEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, loopsArea(), "Loops");
    drawSection (g, controlArea(), "Selected loop");

    // Export's status line: "Saved 4 loops", or what went wrong, for a few seconds.
    auto& exporter = loopProcessor.getExporter();
    if (const auto text = exporter.getStatus(); text.isNotEmpty())
    {
        g.setColour (exporter.statusIsError() ? rkfx::Palette::warning : rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (13.0f));
        g.drawFittedText (text, exportStatusArea(), juce::Justification::centredRight, 1);
    }

    // The selected loop's state and time.
    const auto& engine = loopProcessor.getEngine();
    const auto i = engine.getSelected();
    const auto state = engine.getState (i);
    auto area = statusArea();

    juce::String text = "Loop " + juce::String (i + 1) + ": " + stateName (state);
    if (engine.isQueued (i))
        text << (state == LoopEngine::State::empty ? " (records on the downbeat)" : " (starts at the end of the loop)");
    if (engine.canRedo (i))
        text << "   (undone)";

    g.setColour (state == LoopEngine::State::recording || state == LoopEngine::State::overdubbing ? rkfx::Palette::warning
                                                                                                  : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (text, area, juce::Justification::centredLeft);

    const auto rate = engine.getMaxLength() / LoopEngine::kMaxSeconds;
    const auto length = engine.getRecordedLength (i);
    if (rate > 0.0 && state != LoopEngine::State::empty)
    {
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (14.0f));
        g.drawText (state == LoopEngine::State::recording
                        ? rkfx::Units::time ((float) (length / rate * 1000.0))
                        : rkfx::Units::time ((float) (engine.getPosition (i) / rate * 1000.0)) + " / "
                              + rkfx::Units::time ((float) (length / rate * 1000.0)),
                    area, juce::Justification::centredRight);
    }
}

void RKLoopEditor::refresh()
{
    const auto& engine = loopProcessor.getEngine();
    const auto selected = engine.getSelected();
    if (selected != roleFor)
        showRoleSwitch (selected);

    // The Record footswitch says what its next press does.
    juce::String next;
    switch (engine.getState (selected))
    {
        case LoopEngine::State::empty:       next = "Record"; break;
        case LoopEngine::State::recording:   next = "Play"; break;
        case LoopEngine::State::playing:     next = "Overdub"; break;
        case LoopEngine::State::overdubbing: next = "Play"; break;
        case LoopEngine::State::stopped:     next = "Play"; break;
    }
    footswitches[0]->setButtonText (next);
    footswitches[2]->setButtonText (engine.canRedo (selected) ? "Redo" : "Undo");

    if (++blinkCounter >= 8)
    {
        blinkCounter = 0;
        blink = ! blink;
    }

    repaint (statusArea());
    repaint (exportStatusArea());
    for (auto& t : tiles)
        t->repaint();
}

//==============================================================================
void RKLoopEditor::addExportLoopItem (juce::PopupMenu& menu, int loop, const juce::String& text)
{
    // Dimmed, with the reason, when it can't be exported now.
    const auto& engine = loopProcessor.getEngine();
    const auto state = engine.getState (loop);
    auto label = text;
    auto enabled = false;
    if (state == LoopEngine::State::empty)
        label << "   (empty)";
    else if (state == LoopEngine::State::recording)
        label << "   (still recording)";
    else if (engine.isBusy (loop))
        label << "   (finish the overdub first)";
    else
        enabled = ! loopProcessor.getExporter().isRunning();

    juce::Component::SafePointer<RKLoopEditor> safe (this);
    menu.addItem (label, enabled, false, [safe, loop] { if (safe != nullptr) safe->chooseLoopFile (loop); });
}

void RKLoopEditor::showExportMenu()
{
    auto& exporter = loopProcessor.getExporter();
    const auto anything = ! exporter.loopsWithAudio().empty();
    const auto idle = ! exporter.isRunning();
    const auto selected = loopProcessor.getEngine().getSelected();
    juce::Component::SafePointer<RKLoopEditor> safe (this);

    juce::PopupMenu menu;
    addExportLoopItem (menu, selected, "Export Loop " + juce::String (selected + 1) + "...");
    menu.addItem (anything ? juce::String ("Export all loops...") : juce::String ("Export all loops...   (every loop is empty)"),
                  anything && idle, false, [safe] { if (safe != nullptr) safe->chooseExportFolder(); });
    menu.addItem (anything ? juce::String ("Quick save all") : juce::String ("Quick save all   (every loop is empty)"),
                  anything && idle, false, [safe] { if (safe != nullptr) safe->loopProcessor.getExporter().quickSave(); });
    menu.addItem ("Show the Loops folder", [] {
        const auto folder = LoopExport::loopsFolder();
        folder.createDirectory();
        folder.startAsProcess();
    });
    menu.addSeparator();
    const auto isFloat = exporter.getFormat() == LoopExport::Format::float32;
    menu.addItem ("24-bit", true, ! isFloat, [safe] { if (safe != nullptr) safe->loopProcessor.getExporter().setFormat (LoopExport::Format::int24); });
    menu.addItem ("32-bit float", true, isFloat, [safe] { if (safe != nullptr) safe->loopProcessor.getExporter().setFormat (LoopExport::Format::float32); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton));
}

void RKLoopEditor::chooseLoopFile (int loop)
{
    auto& exporter = loopProcessor.getExporter();
    const auto isLayer = loopProcessor.findParam (LoopParams::roleId (loop))->getValue() >= 0.5f;
    const auto bpm = loopProcessor.getHostBpm();
    const auto name = LoopExport::singleFileName (juce::Time::getCurrentTime(), loop, isLayer, bpm);

    chooser = std::make_unique<juce::FileChooser> ("Export Loop " + juce::String (loop + 1),
                                                   exporter.getLastFolder().getChildFile (name), "*.wav");
    juce::Component::SafePointer<RKLoopEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, loop] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safe == nullptr || file == juce::File())
            return;
        auto& x = safe->loopProcessor.getExporter();
        x.setLastFolder (file.getParentDirectory());
        x.exportLoop (loop, file);
    });
}

void RKLoopEditor::chooseExportFolder()
{
    auto& exporter = loopProcessor.getExporter();
    chooser = std::make_unique<juce::FileChooser> ("Export all loops into a new folder in...", exporter.getLastFolder());
    juce::Component::SafePointer<RKLoopEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (safe == nullptr || folder == juce::File())
            return;
        auto& x = safe->loopProcessor.getExporter();
        x.setLastFolder (folder);
        x.exportAll (folder);
    });
}
