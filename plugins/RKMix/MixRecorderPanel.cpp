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
    refresh();
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
    lastTime = lastInfo = {};
    tick();
    repaint();
}

void RKMixEditor::RecorderPanel::tick()
{
    auto& rec = owner.mixer.getRecorder();
    const auto time = rec.isRecording() ? duration (rec.getElapsedSeconds()) : juce::String ("0:00:00");
    const auto info = rec.getStatus() + "|" + juce::String (rec.getDropouts()) + "|" + juce::String ((int) (rec.getSecondsLeft() / 60.0));
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
    rkfx::drawPanel (g, tracksArea, owner.panelColour(), "What is recorded (with the mix)");

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
    if (rec.getStatus().isNotEmpty())
    {
        g.setColour (rkfx::Palette::warning);
        g.drawFittedText (rec.getStatus(), info, juce::Justification::topLeft, 1);
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
