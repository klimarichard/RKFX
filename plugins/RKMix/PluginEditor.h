#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "MixComponents.h"
#include "MixLayout.h"

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
    the top panel; double-clicking renames it. */
class RKMixEditor : public rkfx::EditorBase
{
public:
    explicit RKMixEditor (RKMixProcessor&);
    ~RKMixEditor() override;

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 900;

private:
    enum class Page { channel, routing };

    /** One channel's strip (a linked pair's left channel shows the pair). */
    struct Strip : public juce::Component
    {
        Strip (RKMixEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();

        RKMixEditor& owner;
        const int channel;
        juce::Label name;
        std::unique_ptr<rkfx::Knob> pan;
        std::unique_ptr<rkfx::ToggleSwitch> mute;
        MixGui::Fader fader;
        MixGui::Meter meter;
        bool stereo = false;
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
        void refresh();

        RKMixEditor& owner;
        const int channel;
        juce::Label name;
        std::vector<std::unique_ptr<juce::Component>> controls;
        rkfx::Knob *trim = nullptr, *delay = nullptr, *pan = nullptr;
        rkfx::ToggleSwitch *link = nullptr, *polarity = nullptr, *polarityR = nullptr, *delayOn = nullptr, *toMain = nullptr;
        std::array<juce::Rectangle<int>, 8> swatches;
        juce::Rectangle<int> inputArea, delayArea, outputArea;
        bool stereo = false;

        void mouseDown (const juce::MouseEvent&) override;
        template <typename T, typename... Args> T* make (Args&&... args);
    };

    /** Where the main and mono go. */
    struct RoutingPanel : public juce::Component
    {
        explicit RoutingPanel (RKMixEditor&);
        void resized() override;
        void paint (juce::Graphics&) override;

        RKMixEditor& owner;
        juce::ComboBox mainOutput, monoOutput;
        std::unique_ptr<juce::ParameterAttachment> mainAttachment, monoAttachment;
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
