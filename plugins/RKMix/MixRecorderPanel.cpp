#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

// The Recorder page (docs/design/RKMix Recorder.md): Record and Stop, the
// time, the disk and the dropouts, Marker; which inputs, the main and which
// Aux buses are recorded (saved with the mix); the folder, the format and
// pairs as two mono files (this computer's).

namespace
{
    juce::Rectangle<int> inner (juce::Rectangle<int> a) { return a.withTrimmedTop (kSectionTitle).reduced (8, 2); }

    juce::String duration (double seconds)
    {
        const auto s = (int) seconds;
        return juce::String::formatted ("%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    }

    juce::String hoursAndMinutes (double seconds)
    {
        const auto minutes = (int) (seconds / 60.0);
        return minutes >= 60 ? juce::String (minutes / 60) + " h " + juce::String (minutes % 60) + " min" : juce::String (minutes) + " min";
    }

    juce::String gigabytes (double bytes) { return (bytes < 1.0e10 ? juce::String (bytes / 1.0e9, 1) : juce::String (juce::roundToInt (bytes / 1.0e9))) + " GB"; }
}

//==============================================================================
// The recording's position: its markers, the loop, the play head; click to locate.
struct RKMixEditor::RecorderPanel::PositionBar : public juce::Component
{
    explicit PositionBar (RecorderPanel& p) : panel (p) {}

    float xOf (juce::int64 sample) const
    {
        const auto length = juce::jmax<juce::int64> (1, panel.owner.mixer.getPlayback().getRecording().length);
        return 4.0f + (float) ((double) sample / (double) length * (getWidth() - 8));
    }

    void paint (juce::Graphics& g) override
    {
        auto& p = panel.owner.mixer.getPlayback();
        const auto r = getLocalBounds().toFloat();
        g.setColour (rkfx::Palette::knobTrack);
        g.fillRoundedRectangle (r.withTrimmedTop (16), 4.0f);
        if (! p.isLoaded())
            return;
        if (p.isLooping())
        {
            g.setColour (panel.owner.accent().withAlpha (0.3f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xOf (p.getLoopStart()), 16.0f, xOf (p.getLoopEnd()), r.getBottom()));
        }
        g.setFont (rkfx::Fonts::regular (11.0f));
        for (const auto& m : p.getRecording().markers)
        {
            const auto x = xOf (m.at);
            g.setColour (rkfx::Palette::textDim);
            g.drawVerticalLine ((int) x, 12.0f, r.getBottom());
            g.drawText (m.label, juce::Rectangle<float> (x + 2.0f, 0.0f, 90.0f, 14.0f), juce::Justification::centredLeft);
        }
        const auto x = xOf (p.getPosition());
        g.setColour (panel.owner.accent());
        g.fillRect (juce::Rectangle<float> (x - 1.0f, 14.0f, 3.0f, r.getHeight() - 14.0f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& p = panel.owner.mixer.getPlayback();
        if (! p.isLoaded() || panel.owner.isLocked())
            return;
        const auto t = juce::jlimit (0.0, 1.0, (e.position.x - 4.0) / juce::jmax (1.0, getWidth() - 8.0));
        p.locate ((juce::int64) (t * (double) p.getRecording().length));
        repaint();
    }

    RecorderPanel& panel;
};

RKMixEditor::RecorderPanel::RecorderPanel (RKMixEditor& o) : Panel (o)
{
    auto& rec = owner.mixer.getRecorder();
    auto& s = owner.mixer.getSettings();

    recordButton.setClickingTogglesState (false);
    recordButton.onClick = [this]
    {
        auto& r = owner.mixer.getRecorder();
        if (r.isRecording())
            r.stop();
        else
            r.start();
        owner.recordingChanged();
    };
    markerButton.onClick = [this] { owner.mixer.getRecorder().addMarker(); };
    markerButton.setTooltip ("A marker in the recording here (every scene recall writes one too)");

    folderButton.setTooltip ("Where the recordings go (this computer)");
    folderButton.onClick = [this]
    {
        owner.chooser = std::make_unique<juce::FileChooser> ("Recordings folder", owner.mixer.getRecorder().getRootFolder());
        juce::Component::SafePointer<RKMixEditor> safe (&owner);
        owner.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safe] (const juce::FileChooser& fc)
                                    {
                                        if (safe != nullptr && fc.getResult() != juce::File())
                                        {
                                            safe->mixer.getRecorder().setRootFolder (fc.getResult());
                                            safe->recorderPanel->refresh();
                                        }
                                    });
    };
    showButton.onClick = [this]
    {
        auto& r = owner.mixer.getRecorder();
        auto folder = r.isRecording() ? r.getTakeFolder() : r.getRootFolder();
        folder.createDirectory();
        folder.revealToUser();
    };
    int24.onClick = [this] { owner.mixer.getRecorder().setFloat (false); refresh(); };
    float32.onClick = [this] { owner.mixer.getRecorder().setFloat (true); refresh(); };
    pairsMono.onClick = [this] { auto& r = owner.mixer.getRecorder(); r.setPairsAsMono (! r.getPairsAsMono()); refresh(); };
    pairsMono.setTooltip ("A linked pair as two mono files (L and R) instead of one stereo file, for a DAW that prefers mono tracks");
    int24.setTooltip ("This computer's format for new recordings");
    float32.setTooltip ("This computer's format for new recordings: never clips, a third larger");

    const auto setAll = [this] (bool on)
    {
        auto& settings = owner.mixer.getSettings();
        for (int ch = 0; ch < kChannels; ++ch)
            settings.param (recordInputId (ch)).setValueNotifyingHost (on ? 1.0f : 0.0f);
    };
    allInputs.onClick = [setAll] { setAll (true); };
    noInputs.onClick = [setAll] { setAll (false); };

    for (auto* b : { &recordButton, &markerButton, &folderButton, &showButton, &int24, &float32, &pairsMono, &allInputs, &noInputs })
        addAndMakeVisible (*b);

    for (int ch = 0; ch < kChannels; ++ch)
        inputs[(size_t) ch] = make<rkfx::ToggleSwitch> (s.param (recordInputId (ch)), owner.accent(), num (ch));
    mainSwitch = make<rkfx::ToggleSwitch> (s.param ("rec_main"), owner.accent(), "Main");
    for (int bus = 0; bus < kBuses; ++bus)
        buses[(size_t) bus] = make<rkfx::ToggleSwitch> (s.param (recordBusId (bus)), owner.accent(), num (bus));
    juce::ignoreUnused (rec);

    // The two views on the right.
    recordView.onClick = [this] { showView (false); };
    playbackView.onClick = [this] { showView (true); };
    playbackView.setTooltip ("Play a recording into the channels in place of the band, to set the mix without them");
    addAndMakeVisible (recordView);
    addAndMakeVisible (playbackView);

    // Playback: load, the inputs' source, the transport.
    loadButton.setTooltip ("A recording's folder (RKMix's own, or any WAV files named by their first input: \"05 Kick.wav\")");
    loadButton.onClick = [this]
    {
        owner.chooser = std::make_unique<juce::FileChooser> ("A recording to play back", owner.mixer.getRecorder().getRootFolder());
        juce::Component::SafePointer<RKMixEditor> safe (&owner);
        owner.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safe] (const juce::FileChooser& fc)
                                    {
                                        if (safe != nullptr && fc.getResult() != juce::File())
                                            safe->recorderPanel->loadRecording (fc.getResult());
                                    });
    };
    unloadButton.onClick = [this] { owner.mixer.setAllInputsLive(); owner.mixer.getPlayback().unload(); owner.sourcesChanged(); refresh(); };
    useNames.setTooltip ("Names the channels as they were in the recording");
    useNames.onClick = [this]
    {
        const auto& names = owner.mixer.getPlayback().getRecording().names;
        for (int ch = 0; ch < juce::jmin (kChannels, names.size()); ++ch)
            if (names[ch].isNotEmpty())
                owner.mixer.setChannelName (ch, names[ch]);
    };
    allPlayback.setTooltip ("Every input plays the recording (inputs with no file go silent)");
    allPlayback.onClick = [this] { owner.mixer.setAllInputsOnPlayback(); owner.sourcesChanged(); };
    allLive.onClick = [this] { owner.mixer.setAllInputsLive(); owner.sourcesChanged(); };

    auto& pb = owner.mixer.getPlayback();
    playButton.onClick = [this] { auto& p = owner.mixer.getPlayback(); if (p.isPlaying()) p.pause(); else p.play(); tick(); };
    stopButton.onClick = [this] { owner.mixer.getPlayback().stop(); tick(); };
    prevMarker.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80 Marker"));
    nextMarker.setButtonText (juce::String::fromUTF8 ("Marker \xe2\x96\xb6"));
    prevMarker.onClick = [this] { owner.mixer.getPlayback().locate (markerNear (false)); };
    nextMarker.onClick = [this] { owner.mixer.getPlayback().locate (markerNear (true)); };
    loopButton.setTooltip ("Plays the part between the markers around the play position over and over");
    loopButton.onClick = [this]
    {
        auto& p = owner.mixer.getPlayback();
        if (p.isLooping())
            p.setLoop (false, 0, 0);
        else
        {
            juce::int64 start = 0, end = p.getRecording().length;
            for (const auto& m : p.getRecording().markers)
            {
                if (m.at <= p.getPosition())
                    start = juce::jmax (start, m.at);
                else
                    end = juce::jmin (end, m.at);
            }
            p.setLoop (true, start, end);
        }
        tick();
    };
    juce::ignoreUnused (pb);
    for (auto* b : { &loadButton, &unloadButton, &useNames, &allPlayback, &allLive, &playButton, &stopButton, &prevMarker, &nextMarker, &loopButton })
        addChildComponent (*b);
    bar = std::make_unique<PositionBar> (*this);
    addChildComponent (*bar);

    showView (false);
    refresh();
}

void RKMixEditor::RecorderPanel::showView (bool playback)
{
    showingPlayback = playback;
    recordView.setToggleState (! playback, juce::dontSendNotification);
    playbackView.setToggleState (playback, juce::dontSendNotification);
    for (auto* c : inputs)
        c->setVisible (! playback);
    for (auto* c : buses)
        c->setVisible (! playback);
    for (auto* c : std::initializer_list<juce::Component*> { mainSwitch, &allInputs, &noInputs })
        c->setVisible (! playback);
    for (auto* c : std::initializer_list<juce::Component*> { &loadButton, &unloadButton, &useNames, &allPlayback, &allLive, &playButton,
                                                            &stopButton, &prevMarker, &nextMarker, &loopButton, bar.get() })
        c->setVisible (playback);
    for (auto& m : mapping)
        m->setVisible (playback);
    refresh();
}

void RKMixEditor::RecorderPanel::loadRecording (const juce::File& folder)
{
    auto& p = owner.mixer.getPlayback();
    const auto r = p.load (MixPlayback::scan (folder), owner.mixer.getSampleRate() > 0.0 ? owner.mixer.getSampleRate() : 48000.0);
    if (r.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Can't play back this recording", r.getErrorMessage());
    loadedFiles = -1;
    refresh();
}

juce::int64 RKMixEditor::RecorderPanel::markerNear (bool next) const
{
    const auto& p = owner.mixer.getPlayback();
    const auto now = p.getPosition();
    const auto rate = juce::jmax (1.0, p.getRecording().sampleRate);
    juce::int64 best = next ? p.getRecording().length : 0;
    for (const auto& m : p.getRecording().markers)
    {
        if (next && m.at > now && m.at < best)
            best = m.at;
        if (! next && m.at < now - (juce::int64) (0.5 * rate) && m.at > best)   // half a second: a second press goes one further back
            best = m.at;
    }
    return best;
}

void RKMixEditor::RecorderPanel::rebuildMapping()
{
    for (auto& m : mapping)
        removeChildComponent (m.get());
    mapping.clear();
    auto& p = owner.mixer.getPlayback();
    const auto& files = p.getRecording().files;
    for (int i = 0; i < (int) files.size(); ++i)
    {
        const auto& f = files[(size_t) i];
        auto box = std::make_unique<juce::ComboBox>();
        box->addItem ("Off", 1);
        if (f.numChannels == 1)
            for (int in = 0; in < kChannels; ++in)
                box->addItem ("In " + num (in), 2 + in);
        else if (f.numChannels == 2)
            for (int in = 0; in < kChannels; in += 2)
                box->addItem ("In " + num (in) + "/" + num (in + 1), 2 + in);
        box->setSelectedId (f.inputs.empty() || f.inputs[0] < 0 ? 1 : 2 + f.inputs[0], juce::dontSendNotification);
        box->setTooltip (f.file.getFileName() + (f.role.isNotEmpty() ? " (recorded " + f.role + ", not an input)" : juce::String()));
        box->onChange = [this, i, raw = box.get()]
        {
            if (owner.isLocked())
                return;
            owner.mixer.getPlayback().mapFile (i, raw->getSelectedId() - 2);
            loadedFiles = -1;   // others may have let go
            refresh();
        };
        box->setVisible (showingPlayback);
        addAndMakeVisible (*box);
        mapping.push_back (std::move (box));
    }
    loadedFiles = (int) files.size();
    resized();
}

void RKMixEditor::RecorderPanel::refresh()
{
    auto& rec = owner.mixer.getRecorder();
    const auto recording = rec.isRecording();
    recordButton.setButtonText (recording ? "Stop" : "Record");
    recordButton.setColour (juce::TextButton::buttonColourId, recording ? rkfx::Palette::warning : rkfx::Palette::warning.withAlpha (0.35f));
    recordButton.setColour (juce::TextButton::textColourOffId, recording ? juce::Colours::black : rkfx::Palette::text);
    markerButton.setEnabled (recording);
    int24.setToggleState (! rec.isFloat(), juce::dontSendNotification);
    float32.setToggleState (rec.isFloat(), juce::dontSendNotification);
    pairsMono.setToggleState (rec.getPairsAsMono(), juce::dontSendNotification);

    // What a take records is fixed when Record is pressed.
    for (auto* c : std::initializer_list<juce::Component*> { &int24, &float32, &pairsMono, &folderButton, &allInputs, &noInputs, mainSwitch })
        c->setEnabled (! recording);
    for (int ch = 0; ch < kChannels; ++ch)
    {
        inputs[(size_t) ch]->setEnabled (! recording);
    }
    for (int bus = 0; bus < kBuses; ++bus)
    {
        const auto isAux = owner.mixer.getBusType (bus) == BusType::aux;
        buses[(size_t) bus]->setEnabled (isAux && ! recording);
    }
    // Playback
    auto& pb = owner.mixer.getPlayback();
    const auto loaded = pb.isLoaded();
    for (auto* b : { &unloadButton, &useNames, &playButton, &stopButton, &prevMarker, &nextMarker, &loopButton })
        b->setEnabled (loaded);
    useNames.setEnabled (loaded && ! pb.getRecording().names.isEmpty());
    if (! loaded && ! mapping.empty())
    {
        for (auto& m : mapping)
            removeChildComponent (m.get());
        mapping.clear();
        loadedFiles = -1;
    }
    else if (loaded && loadedFiles != (int) pb.getRecording().files.size())
        rebuildMapping();
    else
        for (int i = 0; i < (int) mapping.size(); ++i)
        {
            const auto& f = pb.getRecording().files[(size_t) i];
            mapping[(size_t) i]->setSelectedId (f.inputs.empty() || f.inputs[0] < 0 ? 1 : 2 + f.inputs[0], juce::dontSendNotification);
        }

    lastTime = lastInfo = {};
    tick();
    repaint();
}

void RKMixEditor::RecorderPanel::tick()
{
    auto& rec = owner.mixer.getRecorder();
    const auto time = rec.isRecording() ? duration (rec.getElapsedSeconds()) : juce::String ("0:00:00");
    auto& pb = owner.mixer.getPlayback();
    playButton.setButtonText (pb.isPlaying() ? "Pause" : "Play");
    playButton.setToggleState (pb.isPlaying(), juce::dontSendNotification);
    loopButton.setToggleState (pb.isLooping(), juce::dontSendNotification);
    if (showingPlayback && bar != nullptr)
        bar->repaint();
    const auto info = rec.getStatus() + "|" + juce::String (rec.getDropouts()) + "|" + juce::String ((int) (rec.getSecondsLeft() / 60.0))
                    + "|" + juce::String (pb.getPosition() / 4800) + "|" + juce::String (pb.getUnderruns()) + (pb.isLocating() ? "L" : "");
    if (time != lastTime || info != lastInfo)
    {
        lastTime = time;
        lastInfo = info;
        if (rec.isRecording() != (recordButton.getButtonText() == "Stop"))
            refresh();
        repaint();
    }
}

void RKMixEditor::RecorderPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    controlArea = r.removeFromLeft (330);
    r.removeFromLeft (8);
    tracksArea = r;

    auto c = inner (controlArea);
    auto top = c.removeFromTop (44);
    recordButton.setBounds (top.removeFromLeft (110));
    top.removeFromLeft (8);
    markerButton.setBounds (top.removeFromRight (80).reduced (0, 6));
    timeArea = top;
    c.removeFromTop (8);
    infoArea = c.removeFromTop (88);
    c.removeFromTop (6);
    auto folderRow = c.removeFromTop (26);
    showButton.setBounds (folderRow.removeFromRight (60));
    folderRow.removeFromRight (6);
    folderButton.setBounds (folderRow.removeFromRight (90));
    c.removeFromTop (8);
    auto formatRow = c.removeFromTop (26);
    int24.setBounds (formatRow.removeFromLeft (80));
    formatRow.removeFromLeft (4);
    float32.setBounds (formatRow.removeFromLeft (110));
    c.removeFromTop (8);
    pairsMono.setBounds (c.removeFromTop (26).removeFromLeft (200));

    // The view buttons, top right of the right side.
    {
        auto views = tracksArea.withHeight (kSectionTitle).reduced (6, 2);
        playbackView.setBounds (views.removeFromRight (210));
        views.removeFromRight (4);
        recordView.setBounds (views.removeFromRight (140));
    }

    // Playback
    {
        auto pbArea = inner (tracksArea);
        auto row = pbArea.removeFromTop (26);
        loadButton.setBounds (row.removeFromLeft (80));
        row.removeFromLeft (4);
        unloadButton.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (8);
        allLive.setBounds (row.removeFromRight (80));
        row.removeFromRight (4);
        allPlayback.setBounds (row.removeFromRight (120));
        row.removeFromRight (4);
        useNames.setBounds (row.removeFromRight (190));
        pbArea.removeFromTop (6);
        auto transport = pbArea.removeFromTop (28);
        for (auto* b : { &prevMarker, &playButton, &stopButton, &nextMarker, &loopButton })
        {
            b->setBounds (transport.removeFromLeft (b == &prevMarker || b == &nextMarker ? 90 : 70));
            transport.removeFromLeft (4);
        }
        playbackInfo = transport.withTrimmedLeft (8);
        pbArea.removeFromTop (4);
        bar->setBounds (pbArea.removeFromTop (40));
        pbArea.removeFromTop (8);
        mappingArea = pbArea;
        const auto cols = 4, w = (mappingArea.getWidth() - (cols - 1) * 8) / cols;
        for (int i = 0; i < (int) mapping.size(); ++i)
        {
            const auto x = mappingArea.getX() + (i % cols) * (w + 8), y = mappingArea.getY() + (i / cols) * 24;
            mapping[(size_t) i]->setBounds (x + w - 92, y, 92, 22);
        }
    }

    auto t = inner (tracksArea);
    auto header = t.removeFromTop (24);
    noInputs.setBounds (header.removeFromRight (60).reduced (0, 1));
    header.removeFromRight (6);
    allInputs.setBounds (header.removeFromRight (60).reduced (0, 1));
    t.removeFromTop (4);
    const auto cell = (t.getWidth() - 15 * 4) / 16;
    for (int row = 0; row < 2; ++row)
    {
        auto line = t.removeFromTop (28);
        for (int i = 0; i < 16; ++i)
        {
            inputs[(size_t) (row * 16 + i)]->setBounds (line.removeFromLeft (cell));
            line.removeFromLeft (4);
        }
        t.removeFromTop (4);
    }
    t.removeFromTop (28);   // "Outputs"
    auto line = t.removeFromTop (28);
    mainSwitch->setBounds (line.removeFromLeft (cell * 2 + 4));
    t.removeFromTop (28);   // "Aux buses"
    line = t.removeFromTop (28);
    for (int bus = 0; bus < kBuses; ++bus)
    {
        buses[(size_t) bus]->setBounds (line.removeFromLeft (cell));
        line.removeFromLeft (4);
    }
}

void RKMixEditor::RecorderPanel::paint (juce::Graphics& g)
{
    auto& rec = owner.mixer.getRecorder();
    rkfx::drawPanel (g, controlArea, owner.panelColour(), "Recorder");
    rkfx::drawPanel (g, tracksArea, owner.panelColour(), showingPlayback ? "Playback" : "What is recorded (with the mix)");

    // The time
    g.setColour (rec.isRecording() ? rkfx::Palette::warning : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (26.0f));
    g.drawText (lastTime, timeArea, juce::Justification::centred);

    // The disk, the dropouts, the folder, a problem
    auto info = infoArea;
    g.setFont (rkfx::Fonts::regular (12.5f));
    g.setColour (rkfx::Palette::text);
    const auto channels = rec.countChannels();
    g.drawText (juce::String (rec.makeTake().tracks.size()) + " files, " + juce::String (channels) + " channels  -  "
                    + gigabytes (rec.getFreeBytes()) + " free: " + hoursAndMinutes (rec.getSecondsLeft()),
                info.removeFromTop (18), juce::Justification::centredLeft);
    const auto dropouts = rec.getDropouts();
    g.setColour (dropouts > 0 ? rkfx::Palette::warning : rkfx::Palette::textDim);
    g.drawText (rec.isRecording() ? "Dropouts: " + juce::String (dropouts) + (dropouts > 0 ? " (the disk couldn't keep up)" : "")
                                  : juce::String ("Refuses to start with under 10 minutes of space"),
                info.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    const auto folder = rec.isRecording() ? rec.getTakeFolder() : rec.getRootFolder();
    g.drawFittedText ((rec.isRecording() ? "Into: " : "Folder: ") + folder.getFullPathName(), info.removeFromTop (34),
                      juce::Justification::topLeft, 2);
    // A recorded input playing a recording records the recording (allowed: a virtual soundcheck can be recorded too).
    auto warning = rec.getStatus();
    if (warning.isEmpty() && rec.isRecording())
        for (int ch = 0; ch < kChannels; ++ch)
            if (owner.mixer.getSettings().flag (recordInputId (ch)) && owner.mixer.isInputOnPlayback (ch))
            {
                warning = "Note: some recorded inputs are on Playback (PB)";
                break;
            }
    if (warning.isNotEmpty())
    {
        g.setColour (rkfx::Palette::warning);
        g.drawFittedText (warning, info, juce::Justification::topLeft, 1);
    }

    if (showingPlayback)
    {
        auto& pb = owner.mixer.getPlayback();
        g.setFont (rkfx::Fonts::regular (12.5f));
        if (! pb.isLoaded())
        {
            g.setColour (rkfx::Palette::textDim);
            g.drawFittedText ("Load a recording, then switch inputs to PB (Channel page, or All to playback): the files play into "
                              "those channels in place of the band, before the trim, so every setting can be made without them. "
                              "Every session starts with all inputs Live.", mappingArea, juce::Justification::topLeft, 4);
            return;
        }
        const auto& r = pb.getRecording();
        const auto rate = juce::jmax (1.0, r.sampleRate);
        g.setColour (rkfx::Palette::text);
        g.drawFittedText (r.folder.getFileName() + "   " + duration ((double) pb.getPosition() / rate) + " / " + duration ((double) r.length / rate)
                              + (pb.isLocating() ? "   locating" : juce::String())
                              + (pb.getUnderruns() > 0 ? "   underruns: " + juce::String (pb.getUnderruns()) : juce::String()),
                          playbackInfo, juce::Justification::centredLeft, 1);
        // The files and the inputs they feed.
        const auto cols = 4, w = (mappingArea.getWidth() - (cols - 1) * 8) / cols;
        g.setColour (rkfx::Palette::textDim);
        for (int i = 0; i < (int) mapping.size(); ++i)
        {
            const auto x = mappingArea.getX() + (i % cols) * (w + 8), y = mappingArea.getY() + (i / cols) * 24;
            g.drawFittedText (r.files[(size_t) i].file.getFileNameWithoutExtension(), juce::Rectangle<int> (x, y, w - 96, 22),
                              juce::Justification::centredLeft, 1);
        }
        return;
    }

    // The tracks' labels
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (12.0f));
    auto t = inner (tracksArea);
    g.drawText ("INPUTS (as they arrive, before the trim; a linked pair is one stereo file)", t.removeFromTop (24), juce::Justification::centredLeft);
    g.drawText ("OUTPUTS", juce::Rectangle<int> (t.getX(), mainSwitch->getY() - 26, 300, 24), juce::Justification::centredLeft);
    g.drawText ("AUX BUSES (their outputs)", juce::Rectangle<int> (t.getX(), buses[0]->getY() - 26, 400, 24), juce::Justification::centredLeft);
}

//==============================================================================
void RKMixEditor::recordingChanged()
{
    wasRecording = mixer.getRecorder().isRecording();
    setPresetLoadingEnabled (! wasRecording, "Stop recording to load a mix");
    if (recorderPanel != nullptr)
        recorderPanel->refresh();
    if (stripPanel != nullptr)
        stripPanel->refresh();
    repaint (masterArea().removeFromTop (28));
}

void RKMixEditor::sourcesChanged()
{
    // The PB badges, the Channel page's switch, the page, and the way back to all Live.
    lastSourceVersion = mixer.getSourceVersion();
    const auto any = mixer.anyInputOnPlayback();
    allLiveButton.setVisible (any);
    if (any)
        allLiveButton.toFront (false);
    for (auto& s : strips)
        s->repaint();
    if (stripPanel != nullptr)
        stripPanel->refresh();
    if (recorderPanel != nullptr && recorderPanel->isVisible())
        recorderPanel->refresh();
}
