#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kMasterWidth = 200;
    constexpr int kTopHeight = 300;
    constexpr int kTabHeight = 28;
    constexpr int kStripsPerLayer = 16;
    constexpr int kColourBar = 5;
    constexpr int kStripNameHeight = 22;
    constexpr int kStripNumberHeight = 16;
    constexpr int kStripKnobHeight = 66;
    constexpr int kButtonHeight = 22;
    constexpr int kMeterWidth = 9;

    using namespace mix;

    void styleName (juce::Label& label)
    {
        label.setEditable (false, true, false);   // double-click to rename
        label.setFont (rkfx::Fonts::bold (12.0f));
        label.setJustificationType (juce::Justification::centred);
        label.setMinimumHorizontalScale (0.6f);
        label.setColour (juce::Label::textColourId, rkfx::Palette::text);
        label.setColour (juce::Label::textWhenEditingColourId, rkfx::Palette::text);
        label.setTooltip ("Click to select, double-click to rename");
    }
}

//==============================================================================
RKMixEditor::Strip::Strip (RKMixEditor& o, int ch)
    : owner (o), channel (ch),
      fader (*o.mixer.findParam (channelFaderId (ch))),
      meter (o.accent())
{
    styleName (name);
    name.onTextChange = [this] { owner.mixer.setChannelName (channel, name.getText()); };
    name.addMouseListener (this, false);
    addAndMakeVisible (name);

    pan = std::make_unique<rkfx::Knob> (o.mixer.getSettings().param (channelId (ch, "pan")), "Pan");
    addAndMakeVisible (*pan);

    mute = std::make_unique<rkfx::ToggleSwitch> (*o.mixer.findParam (channelMuteId (ch)), rkfx::Palette::warning, "Mute");
    addAndMakeVisible (*mute);

    addAndMakeVisible (fader);
    addAndMakeVisible (meter);
    refresh();
}

void RKMixEditor::Strip::refresh()
{
    stereo = owner.isStereo (channel);
    meter.setNumBars (stereo ? 2 : 1);
    const auto text = owner.mixer.getChannelName (channel);
    if (! name.isBeingEdited())
        name.setText (text.isNotEmpty() ? text : "Ch " + owner.channelLabel (channel), juce::dontSendNotification);
    repaint();
}

void RKMixEditor::Strip::resized()
{
    auto r = getLocalBounds().reduced (3, 0);
    r.removeFromTop (kColourBar + 2);
    name.setBounds (r.removeFromTop (kStripNameHeight));
    r.removeFromTop (kStripNumberHeight);
    pan->setBounds (r.removeFromTop (kStripKnobHeight));
    r.removeFromTop (4);
    mute->setBounds (r.removeFromTop (kButtonHeight).reduced (2, 0));
    r.removeFromTop (6);
    auto faderArea = r.removeFromLeft (r.getWidth() - kMeterWidth - 2);
    fader.setBounds (faderArea);
    meter.setBounds (r.withTrimmedLeft (2).withTrimmedBottom (18));
    meter.setScaleRange (fader.getTrackRange());
}

void RKMixEditor::Strip::paint (juce::Graphics& g)
{
    const auto isSelected = owner.selected == channel;
    g.setColour (isSelected ? owner.accent().withAlpha (0.16f) : owner.panelColour());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), rkfx::Style::kPanelRadius);
    if (isSelected)
    {
        g.setColour (owner.accent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), rkfx::Style::kPanelRadius, 1.0f);
    }

    g.setColour (MixGui::stripColour (owner.mixer.getChannelColour (channel)));
    g.fillRoundedRectangle (getLocalBounds().removeFromTop (kColourBar).reduced (4, 0).toFloat().withY (2.0f), 2.0f);

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.0f));
    g.drawText (owner.channelLabel (channel), 0, kColourBar + 2 + kStripNameHeight, getWidth(), kStripNumberHeight,
                juce::Justification::centred);
}

void RKMixEditor::Strip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        owner.showColourMenu (channel, *this);
        return;
    }
    owner.select (channel);
}

//==============================================================================
RKMixEditor::MasterStrip::MasterStrip (RKMixEditor& o, const juce::String& t, const juce::String& faderId,
                                       const juce::String& muteId, int bars)
    : title (t), fader (*o.mixer.findParam (faderId)), meter (o.accent())
{
    mute = std::make_unique<rkfx::ToggleSwitch> (*o.mixer.findParam (muteId), rkfx::Palette::warning, "Mute");
    addAndMakeVisible (*mute);
    addAndMakeVisible (fader);
    meter.setNumBars (bars);
    addAndMakeVisible (meter);
}

void RKMixEditor::MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    r.removeFromTop (kColourBar + 2 + kStripNameHeight + kStripNumberHeight + kStripKnobHeight + 4);
    mute->setBounds (r.removeFromTop (kButtonHeight).reduced (4, 0));
    r.removeFromTop (6);
    const auto meterWidth = 2 * kMeterWidth + 1;
    fader.setBounds (r.removeFromLeft (r.getWidth() - meterWidth - 4));
    meter.setBounds (r.withTrimmedLeft (4).withTrimmedBottom (18));
    meter.setScaleRange (fader.getTrackRange());
}

void RKMixEditor::MasterStrip::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::panel.brighter (0.06f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), rkfx::Style::kPanelRadius);
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (13.0f));
    g.drawText (title, 0, kColourBar + 2, getWidth(), kStripNameHeight, juce::Justification::centred);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92 ") + targetText, 0, kColourBar + 2 + kStripNameHeight, getWidth(),
                kStripNumberHeight, juce::Justification::centred);
}

//==============================================================================
template <typename T, typename... Args>
T* RKMixEditor::ChannelPanel::make (Args&&... args)
{
    auto owned = std::make_unique<T> (std::forward<Args> (args)...);
    auto* raw = owned.get();
    addAndMakeVisible (*raw);
    controls.push_back (std::move (owned));
    return raw;
}

RKMixEditor::ChannelPanel::ChannelPanel (RKMixEditor& o, int ch) : owner (o), channel (ch)
{
    auto& s = o.mixer.getSettings();
    const auto accent = o.accent();
    stereo = o.isStereo (ch);

    styleName (name);
    name.setFont (rkfx::Fonts::bold (16.0f));
    name.setJustificationType (juce::Justification::centredLeft);
    name.onTextChange = [this] { owner.mixer.setChannelName (channel, name.getText()); };
    addAndMakeVisible (name);

    link = make<rkfx::ToggleSwitch> (s.param (linkId (ch / 2)), accent, "Link " + num (ch / 2 * 2) + "/" + num (ch / 2 * 2 + 1));

    trim = make<rkfx::Knob> (s.param (channelId (ch, "trim")), "Trim");
    polarity = make<rkfx::ToggleSwitch> (s.param (channelId (ch, "pol")), accent,
                                         juce::String::fromUTF8 (stereo ? "\xc3\x98 L" : "\xc3\x98"));
    polarityR = make<rkfx::ToggleSwitch> (s.param (channelId (ch, "polr")), accent, juce::String::fromUTF8 ("\xc3\x98 R"));
    polarityR->setVisible (stereo);

    delayOn = make<rkfx::ToggleSwitch> (s.param (channelId (ch, "delay_on")), accent, "On");
    delay = make<rkfx::Knob> (s.param (channelId (ch, "delay")), "Time");

    pan = make<rkfx::Knob> (s.param (channelId (ch, "pan")), stereo ? "Balance" : "Pan");
    toMain = make<rkfx::ToggleSwitch> (s.param (channelId (ch, "main")), accent, "Main");

    refresh();
}

void RKMixEditor::ChannelPanel::refresh()
{
    if (! name.isBeingEdited())
        name.setText (owner.mixer.getChannelName (channel), juce::dontSendNotification);
    delay->setDimmed (! owner.mixer.getSettings().flag (channelId (channel, "delay_on")));
    repaint();
}

void RKMixEditor::ChannelPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (34).reduced (4, 4);
    header.removeFromLeft (64);   // "Ch 1/2"
    link->setBounds (header.removeFromRight (90));
    header.removeFromRight (16);
    for (int i = MixGui::kNumStripColours - 1; i >= 0; --i)
    {
        swatches[(size_t) i] = header.removeFromRight (20).reduced (2, 3);
        header.removeFromRight (2);
    }
    header.removeFromRight (12);
    name.setBounds (header.withWidth (juce::jmin (header.getWidth(), 300)));

    r.removeFromTop (6);
    const auto knobW = rkfx::Knob::kDefaultWidth, knobH = rkfx::Knob::kDefaultHeight;
    const auto sectionTop = 22;

    inputArea = r.removeFromLeft (stereo ? 210 : 170);
    {
        auto a = inputArea.withTrimmedTop (sectionTop).reduced (8, 4);
        trim->setBounds (a.removeFromLeft (knobW).withHeight (knobH));
        a.removeFromLeft (8);
        auto switches = a.withHeight (knobH).withSizeKeepingCentre (a.getWidth(), 26);
        if (stereo)
        {
            polarity->setBounds (switches.removeFromLeft (52));
            switches.removeFromLeft (6);
            polarityR->setBounds (switches.removeFromLeft (52));
        }
        else
        {
            polarity->setBounds (switches.removeFromLeft (52));
        }
    }
    r.removeFromLeft (kGap);

    delayArea = r.removeFromLeft (170);
    {
        auto a = delayArea.withTrimmedTop (sectionTop).reduced (8, 4);
        delayOn->setBounds (a.removeFromLeft (52).withHeight (knobH).withSizeKeepingCentre (52, 26));
        a.removeFromLeft (8);
        delay->setBounds (a.removeFromLeft (knobW).withHeight (knobH));
    }
    r.removeFromLeft (kGap);

    outputArea = r.removeFromLeft (170);
    {
        auto a = outputArea.withTrimmedTop (sectionTop).reduced (8, 4);
        pan->setBounds (a.removeFromLeft (knobW).withHeight (knobH));
        a.removeFromLeft (8);
        toMain->setBounds (a.removeFromLeft (60).withHeight (knobH).withSizeKeepingCentre (60, 26));
    }
}

void RKMixEditor::ChannelPanel::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (14.0f));
    g.drawText ("Ch " + owner.channelLabel (channel), 4, 4, 64, 26, juce::Justification::centredLeft);
    if (name.getText().isEmpty() && ! name.isBeingEdited())
    {
        g.setFont (rkfx::Fonts::regular (14.0f));
        g.drawText ("Double-click to name it", name.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft);
    }

    const auto current = owner.mixer.getChannelColour (channel);
    for (int i = 0; i < MixGui::kNumStripColours; ++i)
    {
        const auto r = swatches[(size_t) i].toFloat();
        g.setColour (MixGui::stripColour (i));
        g.fillRoundedRectangle (r, 3.0f);
        if (i == current)
        {
            g.setColour (rkfx::Palette::text);
            g.drawRoundedRectangle (r.expanded (1.5f), 3.0f, 1.5f);
        }
    }

    const auto panel = owner.panelColour();
    rkfx::drawPanel (g, inputArea, panel, "Input");
    rkfx::drawPanel (g, delayArea, panel, "Delay");
    rkfx::drawPanel (g, outputArea, panel, "Output");
}

void RKMixEditor::ChannelPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < MixGui::kNumStripColours; ++i)
        if (swatches[(size_t) i].expanded (2).contains (e.getPosition()))
        {
            owner.mixer.setChannelColour (channel, i);
            return;
        }
}

//==============================================================================
RKMixEditor::RoutingPanel::RoutingPanel (RKMixEditor& o) : owner (o)
{
    auto& s = o.mixer.getSettings();

    const auto setUp = [this] (juce::ComboBox& box, juce::RangedAudioParameter& param,
                               std::unique_ptr<juce::ParameterAttachment>& attachment)
    {
        for (int t = 0; t < kNumTargets; ++t)
        {
            if (t == kFirstPairTarget)
                box.addSeparator();
            box.addItem (targetName (t), t + 1);
        }
        attachment = std::make_unique<juce::ParameterAttachment> (param, [&box] (float value)
        {
            box.setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification);
        });
        auto* a = attachment.get();
        box.onChange = [&box, a]
        {
            if (box.getSelectedId() > 0)
                a->setValueAsCompleteGesture ((float) (box.getSelectedId() - 1));
        };
        attachment->sendInitialUpdate();
        addAndMakeVisible (box);
    };

    setUp (mainOutput, s.param ("main_out"), mainAttachment);
    setUp (monoOutput, s.param ("mono_out"), monoAttachment);

    balance = std::make_unique<rkfx::Knob> (s.param ("main_bal"), "Balance");
    addAndMakeVisible (*balance);
}

void RKMixEditor::RoutingPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 30);
    auto row = r.removeFromTop (28);
    row.removeFromLeft (110);
    mainOutput.setBounds (row.removeFromLeft (140));
    r.removeFromTop (10);
    row = r.removeFromTop (28);
    row.removeFromLeft (110);
    monoOutput.setBounds (row.removeFromLeft (140));
    balance->setBounds (getLocalBounds().reduced (8, 30).withTrimmedLeft (290).removeFromLeft (rkfx::Knob::kDefaultWidth)
                            .withHeight (rkfx::Knob::kDefaultHeight));
}

void RKMixEditor::RoutingPanel::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, getLocalBounds().withWidth (400), owner.panelColour(), "Main and mono");
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::regular (13.0f));
    g.drawText ("Main output", 16, 30, 100, 28, juce::Justification::centredLeft);
    g.drawText ("Mono output", 16, 68, 100, 28, juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawFittedText ("The mono sum is (L + R) / 2 of the main after its fader. A stereo source on a single output is summed the same way; "
                      "several sources on one output add up.",
                      16, 110, 260, 60, juce::Justification::topLeft, 4);
}

//==============================================================================
RKMixEditor::RKMixEditor (RKMixProcessor& p)
    : rkfx::EditorBase (p, kWidth, kHeight),
      mixer (p),
      watcher (p.getSettings(), [this] { onSettingsChange(); }),
      mainStrip (*this, "Main", "main_fader", "main_mute", 2),
      monoStrip (*this, "Mono", "mono_fader", "mono_mute", 1),
      routingPanel (*this)
{
    for (int ch = 0; ch < kChannels; ++ch)
    {
        strips.push_back (std::make_unique<Strip> (*this, ch));
        addToCanvas (*strips.back());
    }
    addToCanvas (mainStrip);
    addToCanvas (monoStrip);
    addToCanvas (routingPanel);

    for (auto* tab : { &channelPage, &routingPage })
    {
        tab->setClickingTogglesState (false);
        addToCanvas (*tab);
    }
    channelPage.onClick = [this] { setPage (Page::channel); };
    routingPage.onClick = [this] { setPage (Page::routing); };

    for (int l = 0; l < 2; ++l)
    {
        layerButtons[l].onClick = [this, l] { setLayer (l); };
        addToCanvas (layerButtons[l]);
    }

    mixer.windowsOpen.fetch_add (1);
    finishSetup();
    select (0);
    setPage (Page::channel);
    refreshAll();
    meterTimer.startTimerHz (15);
}

RKMixEditor::~RKMixEditor()
{
    meterTimer.stopTimer();
    mixer.windowsOpen.fetch_sub (1);
}

//==============================================================================
bool RKMixEditor::isLinked (int pair) const { return mixer.isLinked (pair); }

juce::String RKMixEditor::channelLabel (int ch) const
{
    return isStereo (ch) ? num (ch) + "/" + num (ch + 1) : num (ch);
}

juce::String RKMixEditor::displayName (int ch) const
{
    const auto name = mixer.getChannelName (ch);
    return name.isNotEmpty() ? name : "Ch " + channelLabel (ch);
}

juce::Rectangle<int> RKMixEditor::masterArea() const { return contentArea().removeFromRight (kMasterWidth); }

juce::Rectangle<int> RKMixEditor::topArea() const
{
    return contentArea().withTrimmedRight (kMasterWidth + kGap).removeFromTop (kTopHeight);
}

juce::Rectangle<int> RKMixEditor::panelArea() const { return topArea().withTrimmedTop (kTabHeight + 4); }

juce::Rectangle<int> RKMixEditor::stripsArea() const
{
    return contentArea().withTrimmedRight (kMasterWidth + kGap).withTrimmedTop (kTopHeight + kGap);
}

void RKMixEditor::layoutContent()
{
    auto tabs = topArea().removeFromTop (kTabHeight);
    channelPage.setBounds (tabs.removeFromLeft (100));
    tabs.removeFromLeft (6);
    routingPage.setBounds (tabs.removeFromLeft (100));

    routingPanel.setBounds (panelArea());
    if (channelPanel != nullptr)
        channelPanel->setBounds (panelArea());

    auto master = masterArea().withTrimmedTop (kTopHeight + kGap);
    mainStrip.setBounds (master.removeFromLeft (master.getWidth() / 2 + 10).withTrimmedRight (4));
    monoStrip.setBounds (master.withTrimmedLeft (4));

    auto layers = globalArea().withSizeKeepingCentre (globalArea().getWidth(), 30);
    for (auto& b : layerButtons)
    {
        b.setBounds (layers.removeFromLeft (120));
        layers.removeFromLeft (6);
    }

    layoutStrips();
}

void RKMixEditor::layoutStrips()
{
    const auto area = stripsArea();
    const auto width = (area.getWidth() - (kStripsPerLayer - 1) * 4) / kStripsPerLayer;
    int x = area.getX();
    for (int ch = 0; ch < kChannels; ++ch)
    {
        auto& strip = *strips[(size_t) ch];
        const auto inLayer = ch / kStripsPerLayer == layer;
        const auto follower = ch % 2 == 1 && isLinked (ch / 2);
        strip.setVisible (inLayer && ! follower);
        if (! strip.isVisible())
            continue;
        strip.setBounds (x, area.getY(), width, area.getHeight());
        x += width + 4;
    }
}

void RKMixEditor::paintContent (juce::Graphics& g)
{
    // The panel behind the top area's page.
    g.setColour (panelColour().darker (0.15f));
    g.fillRoundedRectangle (panelArea().toFloat().expanded (2.0f), rkfx::Style::kPanelRadius);

    // Master section title
    auto master = masterArea().removeFromTop (kTopHeight);
    rkfx::drawPanel (g, master, panelColour(), "Master");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawFittedText ("Solo, scenes and talkback come here in later phases.", master.reduced (12, 30), juce::Justification::topLeft, 3);
}

//==============================================================================
void RKMixEditor::select (int channel)
{
    selected = leaderOf (juce::jlimit (0, kChannels - 1, channel));
    channelPanel.reset();
    channelPanel = std::make_unique<ChannelPanel> (*this, selected);
    addToCanvas (*channelPanel);
    channelPanel->setBounds (panelArea());
    channelPanel->setVisible (page == Page::channel);
    for (auto& s : strips)
        s->repaint();
    if (page != Page::channel)
        setPage (Page::channel);
}

void RKMixEditor::setPage (Page p)
{
    page = p;
    if (channelPanel != nullptr)
        channelPanel->setVisible (page == Page::channel);
    routingPanel.setVisible (page == Page::routing);
    channelPage.setToggleState (page == Page::channel, juce::dontSendNotification);
    routingPage.setToggleState (page == Page::routing, juce::dontSendNotification);
}

void RKMixEditor::setLayer (int l)
{
    layer = juce::jlimit (0, 1, l);
    for (int i = 0; i < 2; ++i)
        layerButtons[i].setToggleState (i == layer, juce::dontSendNotification);
    layoutStrips();
}

void RKMixEditor::refreshAll()
{
    lastNamesVersion = mixer.getNamesVersion();
    for (auto& s : strips)
        s->refresh();
    if (channelPanel != nullptr)
        channelPanel->refresh();

    const auto target = [this] (const char* id) { return targetName (mixer.getSettings().choice (id)); };
    mainStrip.targetText = target ("main_out");
    monoStrip.targetText = target ("mono_out");
    mainStrip.repaint();
    monoStrip.repaint();
    setLayer (layer);
}

void RKMixEditor::onSettingsChange()
{
    // A pair linked or unlinked changes which strips there are.
    if (channelPanel != nullptr && channelPanel->stereo != isStereo (selected))
        select (selected);
    else if (selected % 2 == 1 && isLinked (selected / 2))
        select (selected - 1);
    refreshAll();
}

void RKMixEditor::updateMeters()
{
    if (mixer.getNamesVersion() != lastNamesVersion)
        refreshAll();

    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = (float) juce::jlimit (0.0, 0.5, now - lastMeterTime);
    lastMeterTime = now;

    for (int ch = 0; ch < kChannels; ++ch)
    {
        auto& strip = *strips[(size_t) ch];
        if (! strip.isVisible())
        {
            if (ch % 2 == 0 || ! isLinked (ch / 2))
                mixer.takeChannelPeak (ch);
            continue;
        }
        const auto left = mixer.takeChannelPeak (ch);
        const auto right = strip.stereo ? mixer.takeChannelPeak (ch + 1) : left;
        strip.meter.setPeaks (left, right, elapsed);
    }

    mainStrip.meter.setPeaks (mixer.takeMainPeak (0), mixer.takeMainPeak (1), elapsed);
    const auto mono = mixer.takeMonoPeak();
    monoStrip.meter.setPeaks (mono, mono, elapsed);
}

void RKMixEditor::showColourMenu (int channel, juce::Component& target)
{
    juce::PopupMenu menu;
    for (int i = 0; i < MixGui::kNumStripColours; ++i)
        menu.addColouredItem (i + 1, MixGui::stripColourName (i), MixGui::stripColour (i), true,
                              mixer.getChannelColour (channel) == i);

    juce::Component::SafePointer<RKMixEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [safe, channel] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->mixer.setChannelColour (channel, result - 1);
    });
}
