#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"
#include "gui/ResponseCurve.h"

#include "MixComponents.h"
#include "MixConsole.h"
#include "MixInserts.h"
#include "MixLayout.h"
#include "MixScenes.h"
#include "TransferCurve.h"

#include <array>

class RKMixProcessor;

/** The console window:

        +------------------------------------------------------+---------+
        | [Channel] [Sends] [Routing]  top panel: the selected |         |
        | channel or bus in full, its sends, or the routing    | master  |
        +------------------------------------------------------+ section |
        | 16 strips of the current layer                       |  main,  |
        |                                                      |  mono   |
        +------------------------------------------------------+---------+
        | [Inputs 1-16] [Inputs 17-32] [Buses]            (bottom row)    |

    A linked pair takes one strip. Clicking a strip's name selects it into
    the top panel; double-clicking renames it; right-click picks its colour.
    Sends on fader: an Aux or FX bus's Sends button turns every channel's
    fader into its send to that bus. Panels live in MixPanels.cpp. */
class RKMixEditor : public rkfx::EditorBase
{
public:
    explicit RKMixEditor (RKMixProcessor&);
    ~RKMixEditor() override;

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 900;

private:
    enum class Page { strip, sends, routing, monitor, scenes, assign, remote };
    static constexpr int kBusLayer = 2;
    static constexpr int kDcaLayer = 3;       // the DCAs and mute groups (or a DCA's members, spilled)
    static constexpr int kUserLayer = 4;      // and 5: the user pages
    static constexpr int kNumLayers = 6;

    /** A channel's or bus's Solo button (solo isn't a parameter: never saved). */
    struct SoloButton : public juce::Component,
                        public juce::SettableTooltipClient
    {
        SoloButton (RKMixEditor& o, int ch, int b) : owner (o), channel (ch), bus (b) { setTooltip ("Solo"); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        bool isOn() const;

        RKMixEditor& owner;
        const int channel, bus;
    };

    /** Talk: held (momentary) or clicked on and off (latch), as set. */
    struct TalkButton : public juce::Component
    {
        TalkButton (RKMixEditor&, juce::RangedAudioParameter& talk);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        RKMixEditor& owner;
        juce::ParameterAttachment attachment;
        bool on = false;
    };

    /** Lies over what the show lock protects: takes the clicks, does nothing. */
    struct LockShield : public juce::Component,
                        public juce::SettableTooltipClient
    {
        LockShield() { setTooltip ("Locked: hold Lock to unlock"); }
        void paint (juce::Graphics&) override;
    };

    /** Lock: click for the lock's menu; while locked, hold it a second to unlock. */
    struct LockButton : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
    {
        explicit LockButton (RKMixEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void timerCallback() override;

        RKMixEditor& owner;
        double holdStart = 0.0;
        bool holding = false;
    };

    /** One insert slot on a strip: the plugin's short name (dimmed while
        off), or "+" when empty. Click: open it (or pick a plugin); right-click:
        on/off, replace, remove. */
    struct InsertButton : public juce::Component
    {
        InsertButton (RKMixEditor& o, InsertRack::Slot s) : owner (o), slot (s) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        RKMixEditor& owner;
        const InsertRack::Slot slot;
    };

    /** An insert's own editor over the console, with its header. */
    struct InsertView : public juce::Component
    {
        InsertView (RKMixEditor&, InsertRack::Slot, rkfx::ProcessorBase&);
        ~InsertView() override;
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh();

        RKMixEditor& owner;
        const InsertRack::Slot slot;
        rkfx::ProcessorBase& plugin;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        juce::Rectangle<int> editorBase;
        juce::TextButton onButton { "On" }, removeButton { "Remove" }, closeButton { "Close" };
    };

    /** A combo box on a choice parameter (item IDs are the values + 1). */
    struct ChoiceBox : public juce::ComboBox
    {
        ChoiceBox (juce::RangedAudioParameter&, const std::function<juce::String (int)>& itemName, int separatorBefore = -1);

        /** Offers only the values @p offered accepts (and Off, value 0, always). */
        void filter (const std::function<bool (int)>& offered);

        std::unique_ptr<juce::ParameterAttachment> attachment;
        std::function<juce::String (int)> nameOf;
        int numValues = 0;
    };

    /** A switch that tells the window after a click (ganged mutes follow it). */
    struct GangSwitch : public rkfx::ToggleSwitch
    {
        using rkfx::ToggleSwitch::ToggleSwitch;
        void mouseDown (const juce::MouseEvent& e) override
        {
            rkfx::ToggleSwitch::mouseDown (e);
            if (afterClick != nullptr)
                afterClick();
        }
        std::function<void()> afterClick;
    };

    /** A strip: a channel (a linked pair's left channel shows the pair) or a bus. */
    struct Strip : public juce::Component
    {
        Strip (RKMixEditor&, int channel, int bus);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();
        void setReduction (float gateDb, float compDb);
        /** Sends on fader: show the send to @p bus instead of the fader (-1: the fader). */
        void showSendTo (int bus);
        /** The pan shows the send's own pan while the faders show sends to an Own-mode bus. */
        void bindPan();

        RKMixEditor& owner;
        const int channel, bus;              // one of them is -1
        juce::Label name;
        juce::Slider pan;
        std::unique_ptr<juce::SliderParameterAttachment> panAttachment;
        juce::String panId;                  // the parameter the pan shows
        std::unique_ptr<GangSwitch> mute;
        std::unique_ptr<SoloButton> solo;
        std::unique_ptr<MixGui::Fader> fader;
        MixGui::Meter meter;
        juce::TextButton sendsButton { "Sends" };   // Aux and FX buses
        std::vector<std::unique_ptr<InsertButton>> insertButtons;
        bool stereo = false;
        bool groupMuted = false;             // a DCA or a mute group silences it
        int sendBus = -1;
        std::array<bool, 4> sectionsOn {};     // HPF, gate, EQ, comp
        std::array<bool, 2> reducing {};       // gate, comp
        juce::Rectangle<int> sectionRow;

        bool isBus() const noexcept { return bus >= 0; }
        juce::String label() const;
        /** As the user pages list strips: the channel, or kUserPageBus + the bus. */
        int id() const noexcept;
    };

    /** A DCA (name, Spill, mute, fader) or a mute group (name, its big Mute,
        its members). Click the name: its members page. */
    struct GroupStrip : public juce::Component
    {
        GroupStrip (RKMixEditor&, int index, bool muteGroup);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();

        RKMixEditor& owner;
        const int index;
        const bool muteGroup;
        juce::Label name;
        std::unique_ptr<rkfx::ToggleSwitch> mute;
        std::unique_ptr<MixGui::Fader> fader;       // a DCA
        juce::TextButton spill { "Spill" };          // a DCA
        juce::StringArray members;                   // their names
        juce::Rectangle<int> membersArea;
    };

    /** The main or the mono sum: name, fader, meter, mute. Click: its page. */
    struct MasterStrip : public juce::Component
    {
        MasterStrip (RKMixEditor&, const juce::String& title, const juce::String& faderId, const juce::String& muteId, int bars,
                     InsertRack::Strip, int chain);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        RKMixEditor& owner;
        const int chain;

        juce::String title;
        juce::String targetText;
        std::unique_ptr<rkfx::ToggleSwitch> mute;
        MixGui::Fader fader;
        MixGui::Meter meter;
        std::vector<std::unique_ptr<InsertButton>> insertButtons;
    };

    /** A page of the top panel, holding its controls. */
    struct Panel : public juce::Component
    {
        explicit Panel (RKMixEditor& o) : owner (o) {}
        virtual void refresh() {}

        template <typename T, typename... Args> T* make (Args&&... args)
        {
            auto owned = std::make_unique<T> (std::forward<Args> (args)...);
            auto* raw = owned.get();
            addAndMakeVisible (*raw);
            controls.push_back (std::move (owned));
            return raw;
        }

        RKMixEditor& owner;
        std::vector<std::unique_ptr<juce::Component>> controls;
    };

    /** The selected channel in full. */
    struct ChannelPanel : public Panel
    {
        ChannelPanel (RKMixEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh() override;
        void updateMeters (float compDb, float seconds);

        const int channel;
        bool stereo = false;
        juce::Label name;
        juce::TextButton presets { juce::String::fromUTF8 ("Strip presets \xe2\x96\xbe") };
        std::array<juce::Rectangle<int>, 8> swatches;

        struct Section
        {
            juce::String title;
            juce::Rectangle<int> area;
            std::vector<rkfx::Knob*> knobs;
        };
        enum SectionIndex { input, delay, hpf, gate, output, eq, comp, numSections };
        std::array<Section, numSections> sections;

        rkfx::ToggleSwitch *link = nullptr, *polarity = nullptr, *polarityR = nullptr, *toMain = nullptr, *soloSafe = nullptr;
        std::array<rkfx::ToggleSwitch*, numSections> onSwitches {};
        rkfx::SegmentedSwitch *hpfSlope = nullptr, *lowType = nullptr, *highType = nullptr;
        ChoiceBox *gateKey = nullptr, *compKey = nullptr;
        rkfx::Knob *trim = nullptr, *pan = nullptr;
        std::array<rkfx::Knob*, 12> eqKnobs {};
        std::unique_ptr<rkfx::ResponseCurve> eqCurve;
        std::unique_ptr<TransferCurve> compCurve;
        std::unique_ptr<rkfx::GainReductionMeter> compMeter;

        rkfx::Knob* knob (int section, const juce::String& suffix, const juce::String& label);
        void showPresetMenu();
    };

    /** The selected channel's sends: a column per bus (group assign, or send level and tap). */
    struct SendsPanel : public Panel
    {
        SendsPanel (RKMixEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        const int channel;
        struct Column
        {
            rkfx::ToggleSwitch* assign = nullptr;
            rkfx::Knob* send = nullptr;
            juce::Slider* pan = nullptr;     // the send's own pan (an Own-mode Aux)
            std::unique_ptr<juce::SliderParameterAttachment> panAttachment;
            MixGui::OptionList* tap = nullptr;
            juce::Rectangle<int> area;
        };
        std::array<Column, mix::kBuses> columns;
        ChoiceBox* directOut = nullptr;      // the channel straight to an output (header)
        ChoiceBox* directTap = nullptr;
    };

    /** The EQ, compressor and limiter of a bus, the main or the mono sum. */
    struct ProcessingPanel : public Panel
    {
        ProcessingPanel (RKMixEditor&, int chain);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;
        void updateMeters (float compDb, float limitDb, float seconds);
        void mouseDown (const juce::MouseEvent&) override;

        const int chain;
        juce::Rectangle<int> eqArea, compArea, limitArea;
        rkfx::ToggleSwitch *eqOn = nullptr, *hpfOn = nullptr, *lpfOn = nullptr, *compOn = nullptr, *compAuto = nullptr, *limitOn = nullptr;
        rkfx::SegmentedSwitch *hpfSlope = nullptr, *lpfSlope = nullptr, *lowType = nullptr, *highType = nullptr;
        rkfx::Knob *hpf = nullptr, *lpf = nullptr;
        std::array<rkfx::Knob*, 18> bands {};
        std::array<rkfx::Knob*, 6> comp {};
        std::array<rkfx::Knob*, 2> limit {};
        std::unique_ptr<rkfx::ResponseCurve> curve;
        std::unique_ptr<rkfx::GainReductionMeter> compMeter, limitMeter;

        rkfx::Knob* knob (const char* suffix, const juce::String& label);
    };

    /** The selected bus: type, format, output, main, pan, tap, and its processing. */
    struct BusPanel : public Panel
    {
        BusPanel (RKMixEditor&, int bus);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh() override;

        const int bus;
        juce::Label name;
        std::array<juce::Rectangle<int>, 8> swatches;
        rkfx::SegmentedSwitch *type = nullptr, *format = nullptr, *panMode = nullptr, *soloSafe = nullptr;
        ChoiceBox *output = nullptr, *defaultTap = nullptr;
        rkfx::ToggleSwitch* toMain = nullptr;
        juce::Slider pan;
        std::unique_ptr<juce::SliderParameterAttachment> panAttachment;
        juce::TextButton sendsOnFader { "Sends on fader" };
        std::unique_ptr<ProcessingPanel> processing;
        juce::Rectangle<int> setupArea;
    };

    /** What the selected bus feeds: later groups (assign), Aux and FX buses (send). */
    struct BusSendsPanel : public Panel
    {
        BusSendsPanel (RKMixEditor&, int bus);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        const int bus;
        std::array<rkfx::ToggleSwitch*, mix::kBuses> assigns {};
        std::array<rkfx::Knob*, mix::kBuses> sends {};
        std::array<juce::Rectangle<int>, mix::kBuses> columns;
    };

    /** The main or the mono sum: its output (and the main's balance) and its processing. */
    struct MainPanel : public Panel
    {
        MainPanel (RKMixEditor&, int chain);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        const int chain;
        juce::TextButton mainButton { "Main" }, monoButton { "Mono" };
        std::unique_ptr<ChoiceBox> output;
        std::unique_ptr<rkfx::Knob> balance;
        std::unique_ptr<ProcessingPanel> processing;
        juce::Rectangle<int> setupArea;
    };

    /** Talkback and the oscillator: sources, levels and where they go. */
    struct MonitorPanel : public Panel
    {
        explicit MonitorPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        std::unique_ptr<ChoiceBox> talkSource, oscOutput;
        rkfx::Knob *talkLevel = nullptr, *oscFreq = nullptr, *oscLevel = nullptr;
        rkfx::ToggleSwitch *talkLatch = nullptr, *talkMain = nullptr, *oscOn = nullptr, *oscMain = nullptr;
        rkfx::SegmentedSwitch* oscType = nullptr;
        std::array<rkfx::ToggleSwitch*, mix::kBuses> talkTo {}, oscTo {};
        juce::Rectangle<int> talkArea, oscArea;
    };

    /** Scenes: the glide, the recall-safe channels and buses, each scene's recall scope. */
    struct ScenesPanel : public Panel
    {
        explicit ScenesPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;
        /** Shows (and edits) the recall scope of @p scene. */
        void editScope (int scene);

        rkfx::Knob* glide = nullptr;
        std::array<rkfx::ToggleSwitch*, mix::kChannels> channelSafe {};
        std::array<rkfx::ToggleSwitch*, mix::kBuses> busSafe {};
        juce::Rectangle<int> glideArea, safeArea, scopeArea;

        juce::ComboBox scopeScene;
        int scopeFor = 0, lastCurrent = -1;
        std::array<juce::TextButton, MixScenes::Scope::numCategories> categoryButtons;
        std::array<juce::TextButton, mix::kBuses> sendButtons;
        juce::TextButton everything { "Everything" }, copyToAll { "Copy to all scenes" };

        void applyScope();
    };

    /** The 32 assignables: what each drives, its range, invert. */
    struct AssignPanel : public Panel
    {
        explicit AssignPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        struct Row
        {
            juce::Slider min, max;
            juce::ToggleButton invert { "Inv" };
            juce::TextButton clear { juce::String::fromUTF8 ("\xc3\x97") };
            juce::TextButton learn { "MIDI" };          // standalone: the controller learned to it
            juce::Rectangle<int> area;
        };
        std::array<std::unique_ptr<Row>, 32> rows;
        void applyRange (int assign);
    };

    /** A DCA's or a mute group's members: every channel and bus, on or off. */
    struct GroupPanel : public Panel
    {
        GroupPanel (RKMixEditor&, int index, bool muteGroup);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh() override;

        const int index;
        const bool muteGroup;
        juce::Label name;
        std::array<juce::Rectangle<int>, 8> swatches;
        std::array<rkfx::ToggleSwitch*, mix::kChannels> channels {};
        std::array<rkfx::ToggleSwitch*, mix::kBuses> buses {};
        juce::TextButton spill { "Spill" }, clear { "Clear" };
        juce::Rectangle<int> infoArea, membersArea;
    };

    /** The monitor remote: on or off, its port, each Aux bus's link as a QR code. */
    struct RemotePanel : public Panel
    {
        explicit RemotePanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh() override;
        /** The phones connected (polled while shown). */
        void updateConnections();

        juce::TextButton onButton { "Remote on" }, ipButton { "Links by IP address" }, newLinks { "New links" };
        juce::Label port;
        juce::Rectangle<int> setupArea, linksArea;

        // The console remote (the iPad): on/off, the PIN, its QR code, the iPads connected.
        juce::TextButton consoleOn { "Console on" }, newPin { "New PIN" };
        juce::Label pin;
        juce::Rectangle<int> consoleArea, consoleQr;
        std::vector<MixConsole::Device> devices;
        std::vector<std::unique_ptr<juce::TextButton>> disconnects;
        int lastConsoleVersion = -1;
        void updateDevices();
        struct Link { int bus; juce::String url; juce::Rectangle<int> area; };
        std::vector<Link> links;
        std::array<int, mix::kBuses> connected {};
        int lastVersion = -1;
        juce::String copied;               // the link just copied (a note)
    };

    /** Where the main and mono go, and what each output carries. */
    struct RoutingPanel : public Panel
    {
        explicit RoutingPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        std::unique_ptr<ChoiceBox> mainOutput, monoOutput;
        std::unique_ptr<rkfx::Knob> balance;
        std::unique_ptr<rkfx::ToggleSwitch> midiThru;
        juce::Rectangle<int> midiArea;
        std::array<juce::Label, mix::kChannels> outputNames;
        std::array<juce::String, mix::kChannels> sources;
        std::array<int, mix::kChannels> sourceCount {};
        juce::Rectangle<int> mainArea, outputsArea;
    };

    RKMixProcessor& mixer;
    rkfx::ParameterWatcher watcher;

    std::vector<std::unique_ptr<Strip>> strips, busStrips;
    std::vector<std::unique_ptr<GroupStrip>> dcaStrips, muteGroupStrips;
    MasterStrip mainStrip, monoStrip;
    std::unique_ptr<Panel> stripPanel, sendsPanel;
    RoutingPanel routingPanel;

    juce::TextButton stripPage { "Channel" }, sendsPage { "Sends" }, routingPage { "Routing" }, monitorPage { "Talk / Osc" },
                     scenesPage { "Scenes" }, assignPage { "Assign" }, remotePage { "Remote" };
    std::unique_ptr<RemotePanel> remotePanel;
    MonitorPanel monitorPanel;
    ScenesPanel scenesPanel;
    AssignPanel assignPanel;
    std::array<juce::TextButton, 16> sceneButtons;
    int lastSceneVersion = -1, lastAssignVersion = -1;

    // Standalone: tempo, tap, MIDI clock; MIDI learn
    juce::Label tempoField;
    juce::TextButton tapButton { "Tap" }, clockButton { "Clock" };
    int lastLearnVersion = -1, lastLearning = -1;

    // The Master section's solo, cue, talk and oscillator controls
    std::unique_ptr<rkfx::SegmentedSwitch> soloMode, soloTap;
    std::unique_ptr<ChoiceBox> cueOutput, cueSource;
    juce::TextButton clearSolo { "Clear solo" };
    std::unique_ptr<TalkButton> talkButton;
    std::unique_ptr<rkfx::ToggleSwitch> oscButton;
    int lastSoloVersion = -1;
    juce::TextButton layerButtons[kNumLayers] { juce::TextButton ("Inputs 1-16"), juce::TextButton ("Inputs 17-32"), juce::TextButton ("Buses"),
                                                juce::TextButton ("DCA"), juce::TextButton ("User 1"), juce::TextButton ("User 2") };
    juce::TextButton sendsBack { "Back to faders" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<InsertView> insertView;
    int lastRackVersion = -1;

    Page page = Page::strip;
    int layer = 0;
    int selected = 0;          // a channel, or -1 while a bus is selected
    int selectedBus = -1;
    int selectedChain = -1;    // the main or the mono sum's page (mix::kMainChain, kMonoChain), or -1
    int selectedDca = -1, selectedMuteGroup = -1;   // a DCA's or a mute group's members page, or -1
    int spillDca = -1;         // the DCA whose members the DCA page shows, or -1
    int sendsOnFader = -1;     // the bus whose sends the channel faders show, or -1
    int lastNamesVersion = -1;
    double lastMeterTime = 0.0;

    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    // Undo, redo and the show lock (in the tab row)
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    LockButton lockButton { *this };
    std::array<LockShield, 7> shields;   // the top panel, the master section (two parts), the faders, the scene bar,
                                         // the preset bar, Bypass
    int lastUndoVersion = -1;
    bool isLocked() const noexcept;
    void applyLock();
    void refreshUndo();
    bool keyPressed (const juce::KeyPress&) override;

    // Ganging: strips chosen with Cmd- or Shift-click on their names move together.
    juce::Array<int> gang;                       // strip ids (channels, kUserPageBus + buses)
    std::vector<std::pair<juce::RangedAudioParameter*, float>> gangStarts;   // at a gesture's start
    Strip* gangLeader = nullptr;
    bool isGanged (int stripId) const { return gang.size() > 1 && gang.contains (stripId); }
    Strip* stripFor (int stripId) const;
    void toggleGang (int stripId);
    void clearGang();
    void gangGestureStart (Strip&);
    void gangMove (Strip&, float travel);
    void gangMute (Strip&);

    // Copy and paste of a strip's sections
    juce::var clipboard;
    /** The channels a paste on channel @p ch goes to: the ganged channels when it's one of them. */
    juce::Array<int> pasteTargets (int ch) const;
    /** The slots a copied insert pastes into: @p slot, and the same slot of the other ganged strips that have it. */
    std::vector<InsertRack::Slot> insertPasteTargets (InsertRack::Slot) const;
    void addCopyPasteItems (juce::PopupMenu&, int channel, int chain, int section, int firstId) const;
    bool handleCopyPaste (int result, int channel, int chain, int section, int firstId);
    void showSectionMenu (int channel, int chain, int section, juce::Component& target, juce::Point<int> where);

    juce::Rectangle<int> topArea() const;
    juce::Rectangle<int> panelArea() const;
    juce::Rectangle<int> stripsArea() const;
    juce::Rectangle<int> masterArea() const;

    bool isLinked (int pair) const;
    bool isStereo (int channel) const { return channel % 2 == 0 && isLinked (channel / 2); }
    /** A stereo Aux bus whose channels sit by their own send pans. */
    bool hasOwnPans (int bus) const;
    int leaderOf (int channel) const { return channel % 2 == 1 && isLinked (channel / 2) ? channel - 1 : channel; }
    juce::String channelLabel (int channel) const;      // "1", or "1/2" for a linked pair
    juce::String displayName (int channel) const;       // the name, or "Ch 1"
    juce::String busDisplayName (int bus) const;        // the name, or "Bus 1"
    double sampleRateForDisplay() const;

    void select (int channel);
    void selectBus (int bus);
    void selectMain (int chain);
    void selectGroup (int index, bool muteGroup);
    bool isChannelSelected() const noexcept { return selectedBus < 0 && selectedChain < 0 && selectedDca < 0 && selectedMuteGroup < 0; }
    void setSpill (int dca);
    juce::String dcaDisplayName (int dca) const;        // the name, or "DCA 1"
    juce::String muteGroupDisplayName (int group) const; // the name, or "Mute 1"
    juce::String stripDisplayName (int strip) const;    // a channel, or kUserPageBus + a bus
    ProcessingPanel* shownProcessing() const;
    void rebuildPanels();
    void setPage (Page);
    void setLayer (int);
    void setSendsOnFader (int bus);
    void layoutStrips();
    void refreshAll();
    void onSettingsChange();
    void updateMeters();
    void showColourMenu (int channel, int bus, juce::Component& target);

    // User pages
    juce::Array<int> userPageStrips (int userPage) const;   // as shown: linked pairs once, by their left channel
    void toggleOnUserPage (int userPage, int strip);
    void showUserPageMenu (int userPage);

    // Inserts
    juce::String slotName (InsertRack::Slot) const;          // "Vocal, insert 1"
    void openInsert (InsertRack::Slot);
    void closeInsert();
    void showInsertMenu (InsertRack::Slot, juce::Component& target, bool full);
    void addInsertTypes (juce::PopupMenu&, int firstId) const;
    juce::Rectangle<int> insertArea() const;

    // Right-click on any control: assign it, make an insert knob scene controlled.
    void mouseDown (const juce::MouseEvent&) override;
    void showControlMenu (const juce::String& paramId, int insertSlot, juce::Component& target);
    void showSceneMenu (int scene);
    /** Standalone: learn or forget a controller for @p target (a MixLearn target). */
    void addLearnItems (juce::PopupMenu&, int target, int firstId);
    bool handleLearnResult (int result, int target, int firstId);
    void showLearnMenu (int target, juce::Component& component);
    void refreshStandalone();
    void refreshScenes();
    void editSceneScope (int scene);

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMixEditor)
};
