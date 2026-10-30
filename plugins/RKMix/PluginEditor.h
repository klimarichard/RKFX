#pragma once

#include "plugin/EditorBase.h"
#include "gui/GainReductionMeter.h"
#include "gui/ResponseCurve.h"

#include "MixComponents.h"
#include "MixLayout.h"
#include "TransferCurve.h"

#include <array>

class RKMixProcessor;

/** The console window:

        +------------------------------------------------------+---------+
        | [Channel] [Routing]   top panel: the selected channel |         |
        |                       in full, or the routing page    | master  |
        +------------------------------------------------------+ section |
        | 16 strips of the current layer                       |  main,  |
        |                                                      |  mono   |
        +------------------------------------------------------+---------+
        | [Inputs 1-16] [Inputs 17-32]                    (bottom row)    |

    A linked pair takes one strip. Clicking a strip's name selects it into
    the top panel; double-clicking renames it; right-click picks its colour. */
class RKMixEditor : public rkfx::EditorBase
{
public:
    explicit RKMixEditor (RKMixProcessor&);
    ~RKMixEditor() override;

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 900;

private:
    enum class Page { channel, routing };

    /** A combo box on a choice parameter (item IDs are the values + 1). */
    struct ChoiceBox : public juce::ComboBox
    {
        ChoiceBox (juce::RangedAudioParameter&, const std::function<juce::String (int)>& itemName, int separatorBefore = -1);
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    /** One channel's strip (a linked pair's left channel shows the pair). */
    struct Strip : public juce::Component
    {
        Strip (RKMixEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();
        void setReduction (float gateDb, float compDb);

        RKMixEditor& owner;
        const int channel;
        juce::Label name;
        juce::Slider pan;
        std::unique_ptr<juce::SliderParameterAttachment> panAttachment;
        std::unique_ptr<rkfx::ToggleSwitch> mute;
        MixGui::Fader fader;
        MixGui::Meter meter;
        bool stereo = false;
        std::array<bool, 4> sectionsOn {};     // HPF, gate, EQ, comp
        std::array<bool, 2> reducing {};       // gate, comp
        juce::Rectangle<int> sectionRow;
    };

    /** The main or the mono sum: name, fader, meter, mute. */
    struct MasterStrip : public juce::Component
    {
        MasterStrip (RKMixEditor&, const juce::String& title, const juce::String& faderId, const juce::String& muteId, int bars);
        void resized() override;
        void paint (juce::Graphics&) override;

        juce::String title;
        juce::String targetText;
        std::unique_ptr<rkfx::ToggleSwitch> mute;
        MixGui::Fader fader;
        MixGui::Meter meter;
    };

    /** The selected channel in full (rebuilt when the selection changes). */
    struct ChannelPanel : public juce::Component
    {
        ChannelPanel (RKMixEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();
        void updateMeters (float compDb, float seconds);

        RKMixEditor& owner;
        const int channel;
        bool stereo = false;
        juce::Label name;
        juce::TextButton presets { juce::String::fromUTF8 ("Strip presets \xe2\x96\xbe") };
        std::vector<std::unique_ptr<juce::Component>> controls;
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

        template <typename T, typename... Args> T* make (Args&&... args);
        rkfx::Knob* knob (int section, const juce::String& suffix, const juce::String& label);
        void showPresetMenu();
    };

    /** Where the main and mono go. */
    struct RoutingPanel : public juce::Component
    {
        explicit RoutingPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;

        RKMixEditor& owner;
        std::unique_ptr<ChoiceBox> mainOutput, monoOutput;
        std::unique_ptr<rkfx::Knob> balance;
    };

    RKMixProcessor& mixer;
    rkfx::ParameterWatcher watcher;

    std::vector<std::unique_ptr<Strip>> strips;
    MasterStrip mainStrip, monoStrip;
    std::unique_ptr<ChannelPanel> channelPanel;
    RoutingPanel routingPanel;

    juce::TextButton channelPage { "Channel" }, routingPage { "Routing" };
    juce::TextButton layerButtons[2] { juce::TextButton ("Inputs 1-16"), juce::TextButton ("Inputs 17-32") };
    std::unique_ptr<juce::FileChooser> chooser;

    Page page = Page::channel;
    int layer = 0;
    int selected = 0;
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
    double sampleRateForDisplay() const;

    void select (int channel);
    void setPage (Page);
    void setLayer (int);
    void layoutStrips();
    void refreshAll();
    void onSettingsChange();
    void updateMeters();
    void showColourMenu (int channel, juce::Component& target);

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMixEditor)
};
