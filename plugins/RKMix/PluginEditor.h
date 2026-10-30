#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"
#include "gui/ResponseCurve.h"

#include "MixComponents.h"
#include "MixInserts.h"
#include "MixLayout.h"
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
    enum class Page { strip, sends, routing };
    static constexpr int kBusLayer = 2;

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

        RKMixEditor& owner;
        const int channel, bus;              // one of them is -1
        juce::Label name;
        juce::Slider pan;
        std::unique_ptr<juce::SliderParameterAttachment> panAttachment;
        std::unique_ptr<rkfx::ToggleSwitch> mute;
        std::unique_ptr<MixGui::Fader> fader;
        MixGui::Meter meter;
        juce::TextButton sendsButton { "Sends" };   // Aux and FX buses
        std::vector<std::unique_ptr<InsertButton>> insertButtons;
        bool stereo = false;
        int sendBus = -1;
        std::array<bool, 4> sectionsOn {};     // HPF, gate, EQ, comp
        std::array<bool, 2> reducing {};       // gate, comp
        juce::Rectangle<int> sectionRow;

        bool isBus() const noexcept { return bus >= 0; }
        juce::String label() const;
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

        rkfx::ToggleSwitch *link = nullptr, *polarity = nullptr, *polarityR = nullptr, *toMain = nullptr;
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
            MixGui::OptionList* tap = nullptr;
            juce::Rectangle<int> area;
        };
        std::array<Column, mix::kBuses> columns;
    };

    /** The EQ, compressor and limiter of a bus, the main or the mono sum. */
    struct ProcessingPanel : public Panel
    {
        ProcessingPanel (RKMixEditor&, int chain);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;
        void updateMeters (float compDb, float limitDb, float seconds);

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
        rkfx::SegmentedSwitch *type = nullptr, *format = nullptr, *panMode = nullptr;
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

    /** Where the main and mono go, and what each output carries. */
    struct RoutingPanel : public Panel
    {
        explicit RoutingPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh() override;

        std::unique_ptr<ChoiceBox> mainOutput, monoOutput;
        std::unique_ptr<rkfx::Knob> balance;
        std::array<juce::Label, mix::kChannels> outputNames;
        std::array<juce::String, mix::kChannels> sources;
        std::array<int, mix::kChannels> sourceCount {};
        juce::Rectangle<int> mainArea, outputsArea;
    };

    RKMixProcessor& mixer;
    rkfx::ParameterWatcher watcher;

    std::vector<std::unique_ptr<Strip>> strips, busStrips;
    MasterStrip mainStrip, monoStrip;
    std::unique_ptr<Panel> stripPanel, sendsPanel;
    RoutingPanel routingPanel;

    juce::TextButton stripPage { "Channel" }, sendsPage { "Sends" }, routingPage { "Routing" };
    juce::TextButton layerButtons[3] { juce::TextButton ("Inputs 1-16"), juce::TextButton ("Inputs 17-32"), juce::TextButton ("Buses") };
    juce::TextButton sendsBack { "Back to faders" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<InsertView> insertView;
    int lastRackVersion = -1;

    Page page = Page::strip;
    int layer = 0;
    int selected = 0;          // a channel, or -1 while a bus is selected
    int selectedBus = -1;
    int selectedChain = -1;    // the main or the mono sum's page (mix::kMainChain, kMonoChain), or -1
    int sendsOnFader = -1;     // the bus whose sends the channel faders show, or -1
    int lastNamesVersion = -1;
    double lastMeterTime = 0.0;

    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    juce::Rectangle<int> topArea() const;
    juce::Rectangle<int> panelArea() const;
    juce::Rectangle<int> stripsArea() const;
    juce::Rectangle<int> masterArea() const;

    bool isLinked (int pair) const;
    bool isStereo (int channel) const { return channel % 2 == 0 && isLinked (channel / 2); }
    int leaderOf (int channel) const { return channel % 2 == 1 && isLinked (channel / 2) ? channel - 1 : channel; }
    juce::String channelLabel (int channel) const;      // "1", or "1/2" for a linked pair
    juce::String displayName (int channel) const;       // the name, or "Ch 1"
    juce::String busDisplayName (int bus) const;        // the name, or "Bus 1"
    double sampleRateForDisplay() const;

    void select (int channel);
    void selectBus (int bus);
    void selectMain (int chain);
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

    // Inserts
    juce::String slotName (InsertRack::Slot) const;          // "Vocal, insert 1"
    void openInsert (InsertRack::Slot);
    void closeInsert();
    void showInsertMenu (InsertRack::Slot, juce::Component& target, bool full);
    void addInsertTypes (juce::PopupMenu&, int firstId) const;
    juce::Rectangle<int> insertArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMixEditor)
};
