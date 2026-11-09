#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"
#include "MixOsc.h"

#include <optional>

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
        const auto& markers = p.getRecording().markers;
        for (size_t i = 0; i < markers.size(); ++i)
        {
            const auto x = xOf (markers[i].at);
            // A label stops short of the next marker (the list shows the whole name).
            const auto room = juce::jmin (90.0f, (i + 1 < markers.size() ? xOf (markers[i + 1].at) : r.getRight()) - x - 4.0f);
            // A scene marker (it recalls its scene, and export follows it): in the accent colour,
            // its name in bold, a flag on its line.
            const auto isScene = MixScenes::sceneOfMarker (markers[i].label) >= 0;
            g.setColour (isScene ? panel.owner.accent().brighter (0.4f) : rkfx::Palette::textDim);
            g.drawVerticalLine ((int) x, 12.0f, r.getBottom());
            if (isScene)
            {
                juce::Path flag;
                flag.addTriangle ((float) (int) x - 4.5f, 12.0f, (float) (int) x + 5.5f, 12.0f, (float) (int) x + 0.5f, 18.0f);
                g.fillPath (flag);
            }
            g.setFont (isScene ? rkfx::Fonts::bold (11.0f) : rkfx::Fonts::regular (11.0f));
            if (room > 8.0f)
                g.drawText (markers[i].label, juce::Rectangle<float> (x + 2.0f, 0.0f, room, 14.0f), juce::Justification::centredLeft, true);
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

//==============================================================================
// The loaded recording's markers, in time order: click to go there, double-click
// the name to rename it, x or Delete removes it (Undo delete brings it back).
struct RKMixEditor::RecorderPanel::MarkerList : public juce::Component, private juce::ListBoxModel
{
    struct Row : public juce::Component
    {
        explicit Row (MarkerList& l) : list (l)
        {
            name.setFont (rkfx::Fonts::regular (12.5f));
            name.setColour (juce::Label::textColourId, rkfx::Palette::text);
            name.setColour (juce::Label::textWhenEditingColourId, rkfx::Palette::text);
            name.setColour (juce::Label::backgroundWhenEditingColourId, rkfx::Palette::knobTrack);
            name.setColour (juce::Label::outlineWhenEditingColourId, list.panel.owner.accent());
            name.setTooltip ("Double-click to rename");
            name.addMouseListener (this, false);
            name.onTextChange = [this]
            {
                auto& p = list.panel.owner.mixer.getPlayback();
                if (list.panel.canEditMarkers())
                    p.renameMarker (row, name.getText());   // an empty name keeps the old one
                set (row);
                list.panel.bar->repaint();
            };
            // A scene marker shows its number on a chip; renaming edits the whole label ("Scene 3 Hallelujah").
            name.onEditorShow = [this]
            {
                name.setBounds (nameArea (false));
                if (auto* editor = name.getCurrentTextEditor())
                {
                    editor->setBounds (name.getLocalBounds());
                    editor->setText (label(), false);
                    editor->selectAll();
                }
            };
            name.onEditorHide = [this]
            {
                name.setBounds (nameArea (true));   // still editing at this point: resized() would skip it
                repaint();
            };
            remove.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
            remove.setTooltip ("Delete this marker");
            remove.onClick = [this] { list.removeLater (row); };
            addAndMakeVisible (name);
            addAndMakeVisible (remove);
        }

        juce::String label() const
        {
            const auto& markers = list.panel.owner.mixer.getPlayback().getRecording().markers;
            return juce::isPositiveAndBelow (row, (int) markers.size()) ? markers[(size_t) row].label : juce::String();
        }

        void set (int newRow)
        {
            row = newRow;
            const auto text = label();
            scene = MixScenes::sceneOfMarker (text);
            if (! name.isBeingEdited())
            {
                name.setFont (scene >= 0 ? rkfx::Fonts::bold (12.5f) : rkfx::Fonts::regular (12.5f));
                name.setText (scene >= 0 ? MixScenes::nameOfSceneMarker (text) : text, juce::dontSendNotification);
                resized();
            }
            const auto editable = list.panel.canEditMarkers();
            name.setEditable (false, editable, false);
            remove.setVisible (editable);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& r = list.panel.owner.mixer.getPlayback().getRecording();
            if (! juce::isPositiveAndBelow (row, (int) r.markers.size()))
                return;
            const auto s = (int) ((double) r.markers[(size_t) row].at / juce::jmax (1.0, r.sampleRate));
            g.setColour (rkfx::Palette::textDim);
            g.setFont (rkfx::Fonts::regular (12.0f));
            g.drawText (s >= 3600 ? juce::String::formatted ("%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60)
                                  : juce::String::formatted ("%d:%02d", s / 60, s % 60),
                        getLocalBounds().removeFromLeft (kTimeWidth).withTrimmedLeft (4), juce::Justification::centredLeft);

            // A scene marker: "Scene 3" on an outlined chip in the accent colour, before its name.
            if (scene >= 0 && ! name.isBeingEdited())
            {
                const auto chip = chipArea().toFloat().reduced (0.5f, 3.5f);
                g.setColour (list.panel.owner.accent().brighter (0.4f));
                g.drawRoundedRectangle (chip, 3.0f, 1.0f);
                g.setFont (rkfx::Fonts::bold (10.5f));
                g.drawText (chipText(), chip, juce::Justification::centred, false);
            }
        }

        juce::String chipText() const { return "Scene " + juce::String (scene + 1); }

        juce::Rectangle<int> chipArea() const
        {
            const auto width = (int) std::ceil (juce::GlyphArrangement::getStringWidth (rkfx::Fonts::bold (10.5f), chipText())) + 10;
            return getLocalBounds().withTrimmedLeft (kTimeWidth + 2).withWidth (width);
        }

        // The name's place: after the chip for a scene marker (unless the whole label is being edited).
        juce::Rectangle<int> nameArea (bool afterChip) const
        {
            auto b = getLocalBounds();
            b.removeFromLeft (kTimeWidth);
            b.removeFromRight (22);
            if (afterChip && scene >= 0)
                b.setLeft (chipArea().getRight() + 1);
            return b;
        }

        void resized() override
        {
            remove.setBounds (getLocalBounds().removeFromRight (22).reduced (1, 2));
            if (! name.isBeingEdited())
                name.setBounds (nameArea (true));
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            // Shift: a range, Cmd: one more (for Export and Export each); a plain click also goes there.
            list.list.selectRowsBasedOnModifierKeys (row, e.mods, false);
            if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
                list.locate (row);
        }

        static constexpr int kTimeWidth = 58;
        MarkerList& list;
        int row = -1;
        int scene = -1;   // a scene marker's scene, or -1
        juce::Label name;
        juce::TextButton remove;
    };

    explicit MarkerList (RecorderPanel& p) : panel (p)
    {
        list.setModel (this);
        list.setRowHeight (22);
        list.setColour (juce::ListBox::backgroundColourId, rkfx::Palette::knobTrack.withAlpha (0.5f));
        list.setOutlineThickness (0);
        addAndMakeVisible (list);
        undoDelete.setTooltip ("Brings back the marker deleted last");
        undoDelete.onClick = [this]
        {
            if (! deleted.has_value() || ! panel.canEditMarkers())
                return;
            const auto index = panel.owner.mixer.getPlayback().addMarker (deleted->at, deleted->label);
            deleted.reset();
            update (true);
            list.selectRow (index);
        };
        addChildComponent (undoDelete);
        list.setMultipleSelectionEnabled (true);

        exportOne.setTooltip ("The main mix, as it is set now, from the selected marker to the next one (Shift-click: several "
                              "markers in a row, one file), into the recording's Exports folder; faster than real time");
        exportEach.setTooltip ("One file per selected marker, each to the next marker");
        showExports.setTooltip ("Opens the recording's Exports folder");
        cancelExport.setTooltip ("Stops the export (a file not finished is not kept)");
        exportOne.onClick = [this] { startExport (false); };
        exportEach.onClick = [this] { startExport (true); };
        showExports.onClick = [this]
        {
            const auto exports = exportFolder();
            if (exports == juce::File())
                return;
            exports.createDirectory();
            exports.revealToUser();
        };
        cancelExport.onClick = [this] { panel.owner.mixer.getExport().cancel(); };
        for (auto* b : { &exportOne, &exportEach, &showExports })
            addAndMakeVisible (*b);
        addChildComponent (progressBar);
        addChildComponent (cancelExport);
        progressBar.setColour (juce::ProgressBar::foregroundColourId, panel.owner.accent());
        progressBar.setColour (juce::ProgressBar::backgroundColourId, rkfx::Palette::knobTrack);
    }

    juce::File exportFolder() const
    {
        auto& p = panel.owner.mixer.getPlayback();
        return p.isLoaded() ? p.getRecording().folder.getChildFile ("Exports") : juce::File();
    }

    /** What the selection exports: from a marker to the next one (or the end). Nothing
        selected: the part around the play position, as Loop. No markers: all of it. */
    std::vector<MixExport::Region> regionsToExport (bool each) const
    {
        auto& p = panel.owner.mixer.getPlayback();
        const auto& r = p.getRecording();
        const auto& markers = r.markers;
        const auto count = (int) markers.size();
        const auto startOf = [&] (int i) { return i < 0 ? (juce::int64) 0 : markers[(size_t) i].at; };
        const auto endOf = [&] (int i) { return i + 1 < count ? markers[(size_t) i + 1].at : r.length; };
        const auto nameOf = [&] (int i) { return i < 0 ? r.folder.getFileName() : markers[(size_t) i].label; };

        std::vector<int> rows;
        const auto chosen = list.getSelectedRows();
        for (int i = 0; i < chosen.size(); ++i)
            if (juce::isPositiveAndBelow (chosen[i], count))
                rows.push_back (chosen[i]);
        std::sort (rows.begin(), rows.end());
        if (rows.empty())
            rows.push_back (p.markerIndexAt (p.getPosition()));   // -1: before the first marker

        std::vector<MixExport::Region> regions;
        if (each)
        {
            for (const auto i : rows)
                regions.push_back ({ startOf (i), endOf (i), nameOf (i) });
        }
        else
        {
            const auto first = rows.front(), last = rows.back();
            regions.push_back ({ startOf (first), endOf (last), first == last ? nameOf (first) : nameOf (first) + " - " + nameOf (last) });
        }
        regions.erase (std::remove_if (regions.begin(), regions.end(), [] (const MixExport::Region& g) { return g.end <= g.start; }),
                       regions.end());
        return regions;
    }

    void startExport (bool each)
    {
        auto& console = panel.owner.mixer;
        auto& p = console.getPlayback();
        if (! p.isLoaded() || console.getExport().isRunning())
            return;
        auto regions = regionsToExport (each);
        if (regions.empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Nothing to export",
                                                    "The selected markers sit on the same spot: there is nothing between them.");
            return;
        }
        auto mixToExport = console.exportMix (regions);   // with the scene markers on the way
        console.getExport().start (std::move (mixToExport), p.getRecording(), std::move (regions), exportFolder(), console.getRecorder().isFloat());
        shownExportVersion = -1;
        update (true);
    }

    /** The export's progress and its result (from update). */
    void updateExport()
    {
        auto& ex = panel.owner.mixer.getExport();
        const auto running = ex.isRunning();
        exportProgress = ex.getProgress();
        if (running)
            progressBar.setTextToDisplay (ex.getStatus());
        if (running != progressBar.isVisible())
        {
            progressBar.setVisible (running);
            cancelExport.setVisible (running);
            for (auto* b : { &exportOne, &exportEach, &showExports })
                b->setVisible (! running);
        }
        const auto loaded = panel.owner.mixer.getPlayback().isLoaded();
        for (auto* b : { &exportOne, &exportEach, &showExports })
            b->setEnabled (loaded);

        if (! running && ex.getVersion() != shownExportVersion)
        {
            const auto first = shownExportVersion == -2;
            shownExportVersion = ex.getVersion();
            if (! first)
            {
                result = ex.getStatus();
                resultUntil = juce::Time::getMillisecondCounter() + 10000;
                if (ex.lastFailed())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export", result);
                repaint();
            }
        }
        if (result.isNotEmpty() && juce::Time::getMillisecondCounter() > resultUntil)
        {
            result = {};
            repaint();
        }
    }

    /** From the panel's tick: new markers, the one under the play head, the lock. */
    void update (bool force = false)
    {
        auto& p = panel.owner.mixer.getPlayback();
        const auto& r = p.getRecording();
        if (r.folder != folder)
        {
            folder = r.folder;   // another recording: what was deleted belonged to the last one
            deleted.reset();
            force = true;
        }
        const auto editable = panel.canEditMarkers();
        const auto count = p.isLoaded() ? (int) r.markers.size() : 0;
        if (force || count != shownCount || editable != shownEditable)
        {
            shownCount = count;
            shownEditable = editable;
            list.updateContent();   // every row shown again: its name, renamable or not
            undoDelete.setVisible (deleted.has_value() && editable);
            panel.bar->repaint();
            repaint();
        }
        updateExport();
        const auto now = p.isLoaded() ? p.markerIndexAt (p.getPosition()) : -1;
        if (now != current)
        {
            current = now;
            list.repaint();
            if (now >= 0 && ! list.isMouseOverOrDragging (true))
                list.scrollToEnsureRowIsOnscreen (now);
        }
    }

    void locate (int row)
    {
        auto& p = panel.owner.mixer.getPlayback();
        if (! p.isLoaded() || panel.owner.isLocked() || ! juce::isPositiveAndBelow (row, (int) p.getRecording().markers.size()))
            return;
        p.locate (p.getRecording().markers[(size_t) row].at);
        panel.tick();
    }

    /** Not from inside the row's own button: the list rebuilds its rows. */
    void removeLater (int row)
    {
        juce::Component::SafePointer<MarkerList> safe (this);
        juce::MessageManager::callAsync ([safe, row]
        {
            if (safe == nullptr || ! safe->panel.canEditMarkers())
                return;
            auto& p = safe->panel.owner.mixer.getPlayback();
            if (! juce::isPositiveAndBelow (row, (int) p.getRecording().markers.size()))
                return;
            const auto marker = p.getRecording().markers[(size_t) row];
            p.removeMarker (row);
            safe->deleted = marker;
            safe->update (true);
            safe->list.selectRow (juce::jmin (row, safe->getNumRows() - 1));
        });
    }

    int getNumRows() override { return shownCount; }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected) override
    {
        if (row == current)
        {
            g.setColour (panel.owner.accent().withAlpha (0.25f));
            g.fillRect (0, 0, width, height);
            g.setColour (panel.owner.accent());
            g.fillRect (0, 0, 3, height);
        }
        else if (isSelected)
        {
            g.setColour (rkfx::Palette::text.withAlpha (0.08f));
            g.fillRect (0, 0, width, height);
        }
    }

    juce::Component* refreshComponentForRow (int row, bool, juce::Component* existing) override
    {
        auto* r = dynamic_cast<Row*> (existing);
        if (row >= shownCount)
        {
            delete existing;
            return nullptr;
        }
        if (r == nullptr)
        {
            delete existing;
            r = new Row (*this);
        }
        r->set (row);
        return r;
    }

    void deleteKeyPressed (int row) override { removeLater (row); }
    void backgroundClicked (const juce::MouseEvent&) override { list.deselectAllRows(); }

    void returnKeyPressed (int row) override
    {
        if (auto* r = dynamic_cast<Row*> (list.getComponentForRowNumber (row)); r != nullptr && panel.canEditMarkers())
            r->name.showEditor();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (12.0f));
        auto header = getLocalBounds().removeFromTop (kHeader);
        if (undoDelete.isVisible())
            header.removeFromRight (100);
        if (result.isNotEmpty())
        {
            g.setColour (panel.owner.mixer.getExport().lastFailed() ? rkfx::Palette::warning : panel.owner.accent());
            g.setFont (rkfx::Fonts::regular (12.0f));
            g.drawFittedText (result, header, juce::Justification::centredLeft, 1);
        }
        else
            g.drawText ("MARKERS (" + juce::String (shownCount) + ")", header, juce::Justification::centredLeft);
        if (shownCount == 0 && panel.owner.mixer.getPlayback().isLoaded())
        {
            g.setFont (rkfx::Fonts::regular (12.0f));
            g.drawFittedText ("None yet: Add marker here puts one at the play position. Export takes all of it.",
                              getLocalBounds().withTrimmedTop (kHeader).withTrimmedBottom (kFooter).reduced (6), juce::Justification::topLeft, 4);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto header = b.removeFromTop (kHeader);
        undoDelete.setBounds (header.removeFromRight (96).reduced (0, 1));
        b.removeFromTop (2);
        auto footer = b.removeFromBottom (kFooter - 4);
        b.removeFromBottom (4);
        list.setBounds (b);
        exportOne.setBounds (footer.removeFromLeft (64));
        footer.removeFromLeft (4);
        exportEach.setBounds (footer.removeFromLeft (92));
        showExports.setBounds (footer.removeFromRight (64));
        auto running = list.getBounds().withY (exportOne.getY()).withHeight (exportOne.getHeight());
        cancelExport.setBounds (running.removeFromRight (64));
        running.removeFromRight (4);
        progressBar.setBounds (running);
        current = -2;   // scrolled to again at this size
        update();
    }

    static constexpr int kHeader = 22, kFooter = 30;
    RecorderPanel& panel;
    juce::ListBox list;
    juce::TextButton undoDelete { "Undo delete" };
    juce::TextButton exportOne { "Export" }, exportEach { "Export each" }, showExports { "Show" }, cancelExport { "Cancel" };
    double exportProgress = 0.0;
    juce::ProgressBar progressBar { exportProgress };
    int shownExportVersion = -2;   // -2: what an earlier export said isn't shown again
    juce::String result;
    juce::uint32 resultUntil = 0;
    std::optional<MixPlayback::Marker> deleted;
    juce::File folder;
    int shownCount = -1, current = -2;
    bool shownEditable = false;
};

//==============================================================================
// Each file of the loaded recording and the input it feeds (scrolls when there are many).
struct RKMixEditor::RecorderPanel::MappingContent : public juce::Component
{
    explicit MappingContent (RecorderPanel& p) : panel (p) {}

    static constexpr int kCols = 2, kRow = 24, kGap = 8, kBox = 92;

    void paint (juce::Graphics& g) override
    {
        auto& pb = panel.owner.mixer.getPlayback();
        if (! pb.isLoaded())
            return;
        const auto& files = pb.getRecording().files;
        const auto w = (getWidth() - (kCols - 1) * kGap) / kCols;
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (12.5f));
        for (int i = 0; i < juce::jmin ((int) files.size(), (int) panel.mapping.size()); ++i)
        {
            const auto x = (i % kCols) * (w + kGap), y = (i / kCols) * kRow;
            g.drawFittedText (files[(size_t) i].file.getFileNameWithoutExtension(), juce::Rectangle<int> (x, y, w - kBox - 4, 22),
                              juce::Justification::centredLeft, 1);
        }
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
    gpMarkers.onClick = [this] { auto& r = owner.mixer.getRecorder(); r.setNameMarkers (! r.getNameMarkers()); refresh(); };
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

    for (auto* b : { &recordButton, &markerButton, &folderButton, &showButton, &int24, &float32, &pairsMono, &gpMarkers, &allInputs, &noInputs })
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
    prevMarker.onClick = [this] { auto& p = owner.mixer.getPlayback(); p.locate (p.markerNear (false)); };
    nextMarker.onClick = [this] { auto& p = owner.mixer.getPlayback(); p.locate (p.markerNear (true)); };
    loopButton.setTooltip ("Plays the part between the markers around the play position over and over");
    loopButton.onClick = [this] { auto& p = owner.mixer.getPlayback(); p.loopAroundPosition (! p.isLooping()); tick(); };
    juce::ignoreUnused (pb);
    back60.onClick = [this] { nudge (-60.0); };
    back10.onClick = [this] { nudge (-10.0); };
    ahead10.onClick = [this] { nudge (10.0); };
    ahead60.onClick = [this] { nudge (60.0); };
    addMarkerHere.setTooltip ("A marker at the play position, saved with the recording");
    addMarkerHere.onClick = [this]
    {
        if (! canEditMarkers())
            return;
        auto& p = owner.mixer.getPlayback();
        const auto index = p.addMarker (p.getPosition());
        auto* list = static_cast<MarkerList*> (markerList.get());
        list->update (true);
        list->list.selectRow (index);
    };
    followScenesButton.setClickingTogglesState (false);
    followScenesButton.setColour (juce::TextButton::buttonOnColourId, owner.accent());
    followScenesButton.setTooltip ("A marker named \"Scene N\" (\"Scene 3\", \"Scene 3 Chorus\") recalls scene N when playback passes it, "
                                   "and a jump recalls the scene in effect where it lands. Export follows them too. This computer's setting");
    followScenesButton.onClick = [this]
    {
        auto& r = owner.mixer.getRecorder();
        r.setFollowScenes (! r.getFollowScenes());
        refresh();
    };
    for (auto* b : { &loadButton, &unloadButton, &useNames, &allPlayback, &allLive, &playButton, &stopButton, &prevMarker, &nextMarker, &loopButton,
                     &back60, &back10, &ahead10, &ahead60, &addMarkerHere, &followScenesButton })
        addChildComponent (*b);
    bar = std::make_unique<PositionBar> (*this);
    addChildComponent (*bar);
    markerList = std::make_unique<MarkerList> (*this);
    addChildComponent (*markerList);
    mappingContent = std::make_unique<MappingContent> (*this);
    mappingView.setViewedComponent (mappingContent.get(), false);
    mappingView.setScrollBarsShown (true, false);
    mappingView.setScrollBarThickness (8);
    addChildComponent (mappingView);

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
                                                            &stopButton, &prevMarker, &nextMarker, &loopButton, bar.get(),
                                                            &back60, &back10, &ahead10, &ahead60, &addMarkerHere, &followScenesButton,
                                                            markerList.get(), &mappingView })
        c->setVisible (playback);
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

void RKMixEditor::RecorderPanel::nudge (double seconds)
{
    auto& p = owner.mixer.getPlayback();
    if (! p.isLoaded() || owner.isLocked())
        return;
    const auto rate = juce::jmax (1.0, p.getRecording().sampleRate);
    p.locate (juce::jlimit<juce::int64> (0, p.getRecording().length, p.getPosition() + (juce::int64) (seconds * rate)));
    tick();
}

bool RKMixEditor::RecorderPanel::canEditMarkers() const
{
    auto& p = owner.mixer.getPlayback();
    auto& rec = owner.mixer.getRecorder();
    return p.isLoaded() && ! owner.isLocked() && ! (rec.isRecording() && rec.getTakeFolder() == p.getRecording().folder);
}

juce::String RKMixEditor::RecorderPanel::inputChannels (int input) const
{
    // The input patch: the channels an input feeds ("  > 3 Vocal, 20 Vocal IEM").
    juce::StringArray names;
    for (const auto ch : owner.mixer.channelsOfInput (input))
    {
        if (ch % 2 == 1 && owner.mixer.isLinked (ch / 2) && owner.mixer.getChannelInput (ch - 1) == input - 1)
            continue;   // the right side of a pair already listed
        const auto leader = ch % 2 == 1 && owner.mixer.isLinked (ch / 2) ? ch - 1 : ch;
        const auto n = owner.mixer.getChannelName (leader);
        names.add (num (leader) + (n.isNotEmpty() ? " " + n : juce::String()));
    }
    return names.isEmpty() ? juce::String ("  (no channel)") : "  > Ch " + names.joinIntoString (", ");
}

void RKMixEditor::RecorderPanel::rebuildMapping()
{
    for (auto& m : mapping)
        mappingContent->removeChildComponent (m.get());
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
                box->addItem ("In " + num (in) + inputChannels (in), 2 + in);
        else if (f.numChannels == 2)
            for (int in = 0; in < kChannels; in += 2)
                box->addItem ("In " + num (in) + "/" + num (in + 1) + inputChannels (in), 2 + in);
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
        mappingContent->addAndMakeVisible (*box);
        mapping.push_back (std::move (box));
    }
    loadedFiles = (int) files.size();
    layoutMapping();
}

void RKMixEditor::RecorderPanel::layoutMapping()
{
    using M = MappingContent;
    const auto width = mappingView.getMaximumVisibleWidth();
    const auto rows = ((int) mapping.size() + M::kCols - 1) / M::kCols;
    mappingContent->setSize (width, juce::jmax (mappingView.getHeight(), rows * M::kRow));
    const auto w = (width - (M::kCols - 1) * M::kGap) / M::kCols;
    for (int i = 0; i < (int) mapping.size(); ++i)
        mapping[(size_t) i]->setBounds ((i % M::kCols) * (w + M::kGap) + w - M::kBox, (i / M::kCols) * M::kRow, M::kBox, 22);
    mappingContent->repaint();
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
    gpMarkers.setToggleState (rec.getNameMarkers(), juce::dontSendNotification);
    gpMarkers.setTooltip (juce::String ("While recording, a marker named after each new rackspace (or song in the setlist) that Gig Performer sends "
                                        "by OSC; with a scene recalled at the same time, one marker: \"Scene 3 <name>\". This computer's setting.")
                          + (owner.mixer.getRemote().getOsc().isEnabled() ? juce::String()
                                                                           : "\nNames by OSC are off now: switch them on on the Remote page."));

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
    for (auto* b : { &unloadButton, &useNames, &playButton, &stopButton, &prevMarker, &nextMarker, &loopButton,
                     &back60, &back10, &ahead10, &ahead60 })
        b->setEnabled (loaded);
    addMarkerHere.setEnabled (canEditMarkers());
    followScenesButton.setToggleState (rec.getFollowScenes(), juce::dontSendNotification);
    useNames.setEnabled (loaded && ! pb.getRecording().names.isEmpty());
    if (! loaded && ! mapping.empty())
    {
        for (auto& m : mapping)
            mappingContent->removeChildComponent (m.get());
        mapping.clear();
        loadedFiles = -1;
        layoutMapping();
    }
    else if (loaded && loadedFiles != (int) pb.getRecording().files.size())
        rebuildMapping();
    else
        for (int i = 0; i < (int) mapping.size(); ++i)
        {
            const auto& f = pb.getRecording().files[(size_t) i];
            mapping[(size_t) i]->setSelectedId (f.inputs.empty() || f.inputs[0] < 0 ? 1 : 2 + f.inputs[0], juce::dontSendNotification);
        }

    static_cast<MarkerList*> (markerList.get())->update (true);
    mappingContent->repaint();
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
    {
        bar->repaint();
        static_cast<MarkerList*> (markerList.get())->update();
        addMarkerHere.setEnabled (canEditMarkers());
    }
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
    c.removeFromTop (6);
    gpMarkers.setBounds (c.removeFromTop (26).removeFromLeft (230));

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
        auto nav = pbArea.removeFromTop (26);
        for (auto* b : { &back60, &back10, &ahead10, &ahead60 })
        {
            b->setBounds (nav.removeFromLeft (64));
            nav.removeFromLeft (4);
        }
        addMarkerHere.setBounds (nav.removeFromRight (130));
        nav.removeFromRight (8);
        followScenesButton.setBounds (nav.removeFromRight (170));
        pbArea.removeFromTop (4);
        bar->setBounds (pbArea.removeFromTop (40));
        pbArea.removeFromTop (8);
        markerList->setBounds (pbArea.removeFromRight (240));
        pbArea.removeFromRight (12);
        mappingArea = pbArea;
        mappingView.setBounds (mappingArea);
        layoutMapping();
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
        return;   // the files and the inputs they feed: MappingContent
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
