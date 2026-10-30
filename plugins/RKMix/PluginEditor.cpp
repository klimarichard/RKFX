#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

//==============================================================================
RKMixEditor::Strip::Strip (RKMixEditor& o, int ch, int b)
    : owner (o), channel (ch), bus (b), meter (o.accent())
{
    auto& s = o.mixer.getSettings();

    styleName (name);
    name.onTextChange = [this]
    {
        if (isBus())
            owner.mixer.setBusName (bus, name.getText());
        else
            owner.mixer.setChannelName (channel, name.getText());
    };
    name.addMouseListener (this, false);
    addAndMakeVisible (name);

    auto& panParam = isBus() ? s.param (busId (bus, "pan")) : s.param (channelId (ch, "pan"));
    pan.setSliderStyle (juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan.setColour (juce::Slider::trackColourId, o.accent().withAlpha (0.5f));
    panAttachment = std::make_unique<juce::SliderParameterAttachment> (panParam, pan);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setPopupDisplayEnabled (true, true, nullptr);
    pan.setTooltip ("Pan (double-click: centre)");
    rkfx::tagParameter (pan, panParam.getParameterID());
    addAndMakeVisible (pan);

    mute = std::make_unique<rkfx::ToggleSwitch> (*o.mixer.findParam (isBus() ? busMuteId (bus) : channelMuteId (ch)),
                                                 rkfx::Palette::warning, "Mute");
    addAndMakeVisible (*mute);

    const auto strip = isBus() ? InsertRack::Strip::bus : InsertRack::Strip::channel;
    for (int slot = 0; slot < InsertRack::numSlots (strip); ++slot)
    {
        insertButtons.push_back (std::make_unique<InsertButton> (o, InsertRack::Slot { strip, isBus() ? bus : channel, slot }));
        addAndMakeVisible (*insertButtons.back());
    }

    if (isBus())
    {
        sendsButton.setTooltip ("Sends on fader: the channel faders set the sends to this bus");
        sendsButton.onClick = [this] { owner.setSendsOnFader (owner.sendsOnFader == bus ? -1 : bus); };
        addChildComponent (sendsButton);
    }

    showSendTo (-1);
    addAndMakeVisible (meter);
    refresh();
}

juce::String RKMixEditor::Strip::label() const
{
    if (! isBus())
        return owner.channelLabel (channel);
    const auto type = owner.mixer.getBusType (bus);
    return busTypeName (type) + (owner.mixer.isBusStereo (bus) ? "" : " mono");
}

void RKMixEditor::Strip::showSendTo (int b)
{
    if (fader != nullptr && b == sendBus)
        return;

    sendBus = b;
    auto& param = sendBus >= 0 ? owner.mixer.getSettings().param (channelSendId (channel, sendBus))
                               : *owner.mixer.findParam (isBus() ? busFaderId (bus) : channelFaderId (channel));
    fader = std::make_unique<MixGui::Fader> (param);
    if (sendBus >= 0)
        fader->setCapColour (MixGui::stripColour (owner.mixer.getBusColour (sendBus)));
    addAndMakeVisible (*fader);
    resized();
}

void RKMixEditor::Strip::refresh()
{
    stereo = isBus() ? owner.mixer.isBusStereo (bus) : owner.isStereo (channel);
    meter.setNumBars (stereo ? 2 : 1);

    const auto text = isBus() ? owner.mixer.getBusName (bus) : owner.mixer.getChannelName (channel);
    if (! name.isBeingEdited())
        name.setText (text.isNotEmpty() ? text : isBus() ? "Bus " + num (bus) : "Ch " + owner.channelLabel (channel),
                      juce::dontSendNotification);

    const auto& s = owner.mixer.getSettings();
    if (isBus())
    {
        const auto type = owner.mixer.getBusType (bus);
        pan.setVisible (type != BusType::aux);
        sendsButton.setVisible (type != BusType::group);
        sendsButton.setToggleState (owner.sendsOnFader == bus, juce::dontSendNotification);
    }
    else
    {
        sectionsOn = { s.flag (channelId (channel, "hpf_on")), s.flag (channelId (channel, "gate_on")),
                       s.flag (channelId (channel, "eq_on")), s.flag (channelId (channel, "comp_on")) };
    }
    repaint();
}

void RKMixEditor::Strip::setReduction (float gateDb, float compDb)
{
    const std::array<bool, 2> now { gateDb < -1.0f, compDb < -1.0f };
    if (now != reducing)
    {
        reducing = now;
        repaint (sectionRow);
    }
}

void RKMixEditor::Strip::resized()
{
    auto r = getLocalBounds().reduced (3, 0);
    r.removeFromTop (kColourBar + 2);
    name.setBounds (r.removeFromTop (kStripNameHeight));
    r.removeFromTop (kStripNumberHeight);
    sectionRow = r.removeFromTop (kSectionRowHeight);
    sendsButton.setBounds (sectionRow.expanded (0, 2));
    r.removeFromTop (4);
    for (auto& b : insertButtons)
    {
        b->setBounds (r.removeFromTop (kInsertRow));
        r.removeFromTop (2);
    }
    r.removeFromTop (2);
    pan.setBounds (r.removeFromTop (kPanHeight).reduced (2, 0));
    r.removeFromTop (4);
    mute->setBounds (r.removeFromTop (kButtonHeight).reduced (2, 0));
    r.removeFromTop (6);
    auto faderArea = r.removeFromLeft (r.getWidth() - kMeterWidth - 2);
    if (fader != nullptr)
    {
        fader->setBounds (faderArea);
        meter.setScaleRange (fader->getTrackRange());
    }
    meter.setBounds (r.withTrimmedLeft (2).withTrimmedBottom (18));
}

void RKMixEditor::Strip::paint (juce::Graphics& g)
{
    const auto isSelected = isBus() ? owner.selectedBus == bus : owner.selected == channel;
    const auto showingSend = sendBus >= 0;
    g.setColour (isSelected ? owner.accent().withAlpha (0.16f)
                            : showingSend ? MixGui::stripColour (owner.mixer.getBusColour (sendBus)).withAlpha (0.12f)
                                          : owner.panelColour());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), rkfx::Style::kPanelRadius);
    if (isSelected)
    {
        g.setColour (owner.accent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), rkfx::Style::kPanelRadius, 1.0f);
    }

    const auto colour = isBus() ? owner.mixer.getBusColour (bus) : owner.mixer.getChannelColour (channel);
    g.setColour (MixGui::stripColour (colour));
    g.fillRoundedRectangle (getLocalBounds().removeFromTop (kColourBar).reduced (4, 0).toFloat().withY (2.0f), 2.0f);

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.0f));
    g.drawText (label(), 0, kColourBar + 2 + kStripNameHeight, getWidth(), kStripNumberHeight, juce::Justification::centred);

    if (isBus())
        return;

    // Sections in use: H(PF) G(ate) E(Q) C(omp); the gate's and the compressor's
    // letters light up while they reduce the gain.
    const char* letters[] = { "H", "G", "E", "C" };
    auto row = sectionRow;
    const auto w = row.getWidth() / 4;
    g.setFont (rkfx::Fonts::bold (10.5f));
    for (int i = 0; i < 4; ++i)
    {
        const auto cell = row.removeFromLeft (w);
        const auto active = (i == 1 && reducing[0]) || (i == 3 && reducing[1]);
        g.setColour (active ? rkfx::Palette::warning : sectionsOn[(size_t) i] ? owner.accent().brighter (0.3f)
                                                                              : rkfx::Palette::knobTrack);
        g.drawText (letters[i], cell, juce::Justification::centred);
    }
}

void RKMixEditor::Strip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        owner.showColourMenu (channel, bus, *this);
        return;
    }
    if (isBus())
        owner.selectBus (bus);
    else
        owner.select (channel);
}

//==============================================================================
RKMixEditor::MasterStrip::MasterStrip (RKMixEditor& o, const juce::String& t, const juce::String& faderId,
                                       const juce::String& muteId, int bars, InsertRack::Strip strip)
    : title (t), fader (*o.mixer.findParam (faderId)), meter (o.accent())
{
    for (int slot = 0; slot < InsertRack::numSlots (strip); ++slot)
    {
        insertButtons.push_back (std::make_unique<InsertButton> (o, InsertRack::Slot { strip, 0, slot }));
        addAndMakeVisible (*insertButtons.back());
    }

    mute = std::make_unique<rkfx::ToggleSwitch> (*o.mixer.findParam (muteId), rkfx::Palette::warning, "Mute");
    addAndMakeVisible (*mute);
    addAndMakeVisible (fader);
    meter.setNumBars (bars);
    addAndMakeVisible (meter);
}

void RKMixEditor::MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    r.removeFromTop (kColourBar + 2 + kStripNameHeight + kStripNumberHeight + 4);
    auto rows = r.removeFromTop (InsertRack::kMainSlots * (kInsertRow + 2));   // the main's and mono's faders line up
    for (auto& b : insertButtons)
    {
        b->setBounds (rows.removeFromTop (kInsertRow));
        rows.removeFromTop (2);
    }
    r.removeFromTop (4);
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
RKMixEditor::RKMixEditor (RKMixProcessor& p)
    : rkfx::EditorBase (p, kWidth, kHeight),
      mixer (p),
      watcher (p.getSettings(), [this] { onSettingsChange(); }),
      mainStrip (*this, "Main", "main_fader", "main_mute", 2, InsertRack::Strip::main),
      monoStrip (*this, "Mono", "mono_fader", "mono_mute", 1, InsertRack::Strip::mono),
      routingPanel (*this)
{
    for (int ch = 0; ch < kChannels; ++ch)
    {
        strips.push_back (std::make_unique<Strip> (*this, ch, -1));
        addToCanvas (*strips.back());
    }
    for (int bus = 0; bus < kBuses; ++bus)
    {
        busStrips.push_back (std::make_unique<Strip> (*this, -1, bus));
        addToCanvas (*busStrips.back());
    }
    addToCanvas (mainStrip);
    addToCanvas (monoStrip);
    addToCanvas (routingPanel);

    for (auto* tab : { &stripPage, &sendsPage, &routingPage })
        addToCanvas (*tab);
    stripPage.onClick = [this] { setPage (Page::strip); };
    sendsPage.onClick = [this] { setPage (Page::sends); };
    routingPage.onClick = [this] { setPage (Page::routing); };

    for (int l = 0; l < 3; ++l)
    {
        layerButtons[l].onClick = [this, l] { setLayer (l); };
        addToCanvas (layerButtons[l]);
    }

    sendsBack.onClick = [this] { setSendsOnFader (-1); };
    addToCanvas (sendsBack);
    sendsBack.setVisible (false);

    // A plugin about to be deleted: its editor goes first.
    mixer.getRack().aboutToRemove = [this] (rkfx::ProcessorBase* plugin)
    {
        if (insertView != nullptr && &insertView->plugin == plugin)
            closeInsert();
    };

    mixer.windowsOpen.fetch_add (1);
    finishSetup();
    select (0);
    setPage (Page::strip);
    refreshAll();
    meterTimer.startTimerHz (15);
}

RKMixEditor::~RKMixEditor()
{
    meterTimer.stopTimer();
    closeInsert();
    mixer.getRack().aboutToRemove = nullptr;
    mixer.windowsOpen.fetch_sub (1);
}

//==============================================================================
bool RKMixEditor::isLinked (int pair) const { return mixer.isLinked (pair); }

double RKMixEditor::sampleRateForDisplay() const
{
    return mixer.getSampleRate() > 0.0 ? mixer.getSampleRate() : 48000.0;
}

juce::String RKMixEditor::channelLabel (int ch) const
{
    return isStereo (ch) ? num (ch) + "/" + num (ch + 1) : num (ch);
}

juce::String RKMixEditor::displayName (int ch) const
{
    const auto name = mixer.getChannelName (ch);
    return name.isNotEmpty() ? name : "Ch " + channelLabel (ch);
}

juce::String RKMixEditor::busDisplayName (int bus) const
{
    const auto name = mixer.getBusName (bus);
    return name.isNotEmpty() ? name : "Bus " + num (bus);
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
    for (auto* tab : { &stripPage, &sendsPage, &routingPage })
    {
        tab->setBounds (tabs.removeFromLeft (100));
        tabs.removeFromLeft (6);
    }

    routingPanel.setBounds (panelArea());
    for (auto* panel : { stripPanel.get(), sendsPanel.get() })
        if (panel != nullptr)
            panel->setBounds (panelArea());

    auto master = masterArea().withTrimmedTop (kTopHeight + kGap);
    mainStrip.setBounds (master.removeFromLeft (master.getWidth() / 2 + 10).withTrimmedRight (4));
    monoStrip.setBounds (master.withTrimmedLeft (4));

    sendsBack.setBounds (masterArea().removeFromTop (kTopHeight).reduced (12).removeFromBottom (30));

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

    for (int bus = 0; bus < kBuses; ++bus)
    {
        auto& strip = *busStrips[(size_t) bus];
        strip.setVisible (layer == kBusLayer);
        strip.setBounds (area.getX() + bus * (width + 4), area.getY(), width, area.getHeight());
    }
}

void RKMixEditor::paintContent (juce::Graphics& g)
{
    // The panel behind the top area's page.
    g.setColour (panelColour().darker (0.15f));
    g.fillRoundedRectangle (panelArea().toFloat().expanded (2.0f), rkfx::Style::kPanelRadius);

    // Master section: sends on fader, when on; later: solo, scenes, talkback.
    auto master = masterArea().removeFromTop (kTopHeight);
    rkfx::drawPanel (g, master, panelColour(), "Master");
    auto text = master.reduced (12, 30);
    if (sendsOnFader >= 0)
    {
        g.setColour (MixGui::stripColour (mixer.getBusColour (sendsOnFader)).brighter (0.2f));
        g.setFont (rkfx::Fonts::bold (14.0f));
        g.drawFittedText ("Faders: sends to\n" + busDisplayName (sendsOnFader), text.removeFromTop (40), juce::Justification::topLeft, 2);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (12.0f));
        g.drawFittedText ("Every channel's fader sets its send to this bus. Its cap takes the bus's colour.",
                          text.removeFromTop (60), juce::Justification::topLeft, 4);
    }
    else
    {
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (12.0f));
        g.drawFittedText ("Solo, scenes and talkback come here in later phases.", text, juce::Justification::topLeft, 3);
    }
}

//==============================================================================
void RKMixEditor::rebuildPanels()
{
    stripPanel.reset();
    sendsPanel.reset();
    if (selectedBus >= 0)
        stripPanel = std::make_unique<BusPanel> (*this, selectedBus);
    else
    {
        stripPanel = std::make_unique<ChannelPanel> (*this, selected);
        sendsPanel = std::make_unique<SendsPanel> (*this, selected);
    }

    for (auto* panel : { stripPanel.get(), sendsPanel.get() })
        if (panel != nullptr)
        {
            addToCanvas (*panel);
            panel->setBounds (panelArea());
        }

    stripPage.setButtonText (selectedBus >= 0 ? "Bus" : "Channel");
    sendsPage.setEnabled (selectedBus < 0);
    for (auto& s : strips)
        s->repaint();
    for (auto& s : busStrips)
        s->repaint();
}

void RKMixEditor::select (int channel)
{
    selected = leaderOf (juce::jlimit (0, kChannels - 1, channel));
    selectedBus = -1;
    rebuildPanels();
    setPage (page == Page::routing ? Page::strip : page);
}

void RKMixEditor::selectBus (int bus)
{
    selectedBus = juce::jlimit (0, kBuses - 1, bus);
    rebuildPanels();
    setPage (Page::strip);
}

void RKMixEditor::setPage (Page p)
{
    if (p == Page::sends && sendsPanel == nullptr)
        p = Page::strip;
    page = p;
    if (stripPanel != nullptr)
        stripPanel->setVisible (page == Page::strip);
    if (sendsPanel != nullptr)
        sendsPanel->setVisible (page == Page::sends);
    routingPanel.setVisible (page == Page::routing);
    if (page == Page::routing)
        routingPanel.refresh();
    stripPage.setToggleState (page == Page::strip, juce::dontSendNotification);
    sendsPage.setToggleState (page == Page::sends, juce::dontSendNotification);
    routingPage.setToggleState (page == Page::routing, juce::dontSendNotification);
}

void RKMixEditor::setLayer (int l)
{
    layer = juce::jlimit (0, kBusLayer, l);
    for (int i = 0; i < 3; ++i)
        layerButtons[i].setToggleState (i == layer, juce::dontSendNotification);
    layoutStrips();
}

void RKMixEditor::setSendsOnFader (int bus)
{
    // Only Aux and FX buses have sends.
    if (bus >= 0 && mixer.getBusType (bus) == BusType::group)
        bus = -1;

    sendsOnFader = bus;
    for (auto& s : strips)
        s->showSendTo (bus);
    sendsBack.setVisible (bus >= 0);
    if (bus >= 0 && layer == kBusLayer)
        setLayer (0);
    refreshAll();
    repaint();
}

void RKMixEditor::refreshAll()
{
    lastNamesVersion = mixer.getNamesVersion();
    for (auto& s : strips)
        s->refresh();
    for (auto& s : busStrips)
        s->refresh();
    for (auto* panel : { stripPanel.get(), sendsPanel.get() })
        if (panel != nullptr)
            panel->refresh();
    if (routingPanel.isVisible())
        routingPanel.refresh();

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
    if (selectedBus < 0)
    {
        auto* panel = dynamic_cast<ChannelPanel*> (stripPanel.get());
        if (panel != nullptr && panel->stereo != isStereo (selected))
            select (selected);
        else if (selected % 2 == 1 && isLinked (selected / 2))
            select (selected - 1);
    }

    // A bus no longer an Aux or FX bus has no sends to show.
    if (sendsOnFader >= 0 && mixer.getBusType (sendsOnFader) == BusType::group)
        setSendsOnFader (-1);

    refreshAll();
}

void RKMixEditor::updateMeters()
{
    if (mixer.getNamesVersion() != lastNamesVersion)
        refreshAll();

    if (const auto v = mixer.getRack().getVersion(); v != lastRackVersion)
    {
        lastRackVersion = v;
        for (auto* list : { &strips, &busStrips })
            for (auto& strip : *list)
                for (auto& b : strip->insertButtons)
                    b->repaint();
        for (auto* master : { &mainStrip, &monoStrip })
            for (auto& b : master->insertButtons)
                b->repaint();
        if (insertView != nullptr)
            insertView->refresh();
    }

    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = (float) juce::jlimit (0.0, 0.5, now - lastMeterTime);
    lastMeterTime = now;

    auto* channelPanel = selectedBus < 0 ? dynamic_cast<ChannelPanel*> (stripPanel.get()) : nullptr;
    for (int ch = 0; ch < kChannels; ++ch)
    {
        if (ch % 2 == 1 && isLinked (ch / 2))
            continue;   // read with its pair's left channel

        auto& strip = *strips[(size_t) ch];
        const auto left = mixer.takeChannelPeak (ch);
        const auto right = strip.stereo ? mixer.takeChannelPeak (ch + 1) : left;
        const auto gateDb = mixer.takeGateReduction (ch);
        const auto compDb = mixer.takeCompReduction (ch);

        if (strip.isVisible())
        {
            strip.meter.setPeaks (left, right, elapsed);
            strip.setReduction (gateDb, compDb);
        }
        if (ch == selected && channelPanel != nullptr)
            channelPanel->updateMeters (compDb, elapsed);
    }

    for (int bus = 0; bus < kBuses; ++bus)
    {
        const auto left = mixer.takeBusPeak (bus, 0);
        const auto right = mixer.takeBusPeak (bus, 1);
        auto& strip = *busStrips[(size_t) bus];
        if (strip.isVisible())
            strip.meter.setPeaks (left, strip.stereo ? right : left, elapsed);
    }

    mainStrip.meter.setPeaks (mixer.takeMainPeak (0), mixer.takeMainPeak (1), elapsed);
    const auto mono = mixer.takeMonoPeak();
    monoStrip.meter.setPeaks (mono, mono, elapsed);
}

void RKMixEditor::showColourMenu (int channel, int bus, juce::Component& target)
{
    const auto current = bus >= 0 ? mixer.getBusColour (bus) : mixer.getChannelColour (channel);
    juce::PopupMenu menu;
    for (int i = 0; i < MixGui::kNumStripColours; ++i)
        menu.addColouredItem (i + 1, MixGui::stripColourName (i), MixGui::stripColour (i), true, current == i);

    juce::Component::SafePointer<RKMixEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [safe, channel, bus] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        if (bus >= 0)
            safe->mixer.setBusColour (bus, result - 1);
        else
            safe->mixer.setChannelColour (channel, result - 1);
    });
}

//==============================================================================
// Inserts

void RKMixEditor::InsertButton::paint (juce::Graphics& g)
{
    auto& rack = owner.mixer.getRack();
    const auto* type = rack.typeOf (slot);
    const auto r = getLocalBounds().toFloat();

    g.setColour (rkfx::Palette::knobBody.withAlpha (type != nullptr ? 1.0f : 0.5f));
    g.fillRoundedRectangle (r, 3.0f);

    if (type == nullptr)
    {
        g.setColour (rkfx::Palette::knobTrack);
        g.setFont (rkfx::Fonts::regular (11.0f));
        g.drawText ("+", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto on = rack.isOn (slot);
    g.setColour (rkfx::Palette::familyColour (type->family).withAlpha (on ? 1.0f : 0.35f));
    g.fillRoundedRectangle (r.withWidth (4.0f), 2.0f);
    g.setColour (on ? rkfx::Palette::text : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (10.5f));
    g.drawFittedText (type->shortName, getLocalBounds().withTrimmedLeft (6).withTrimmedRight (2), juce::Justification::centredLeft, 1);
}

void RKMixEditor::InsertButton::mouseDown (const juce::MouseEvent& e)
{
    auto& rack = owner.mixer.getRack();
    if (e.mods.isPopupMenu() || rack.get (slot) == nullptr)
        owner.showInsertMenu (slot, *this, rack.get (slot) != nullptr);
    else
        owner.openInsert (slot);
}

RKMixEditor::InsertView::InsertView (RKMixEditor& o, InsertRack::Slot s, rkfx::ProcessorBase& p)
    : owner (o), slot (s), plugin (p)
{
    editor.reset (plugin.createEditorAndMakeActive());
    if (editor != nullptr)
    {
        editorBase = editor->getLocalBounds();
        editor->setResizable (false, false);   // the console sizes it
        addAndMakeVisible (*editor);
    }

    onButton.setClickingTogglesState (false);
    onButton.onClick = [this] { owner.mixer.getRack().setOn (slot, ! owner.mixer.getRack().isOn (slot)); };
    removeButton.onClick = [this]
    {
        const auto where = slot;
        owner.mixer.getRack().remove (where);   // closes (deletes) this view first
    };
    closeButton.onClick = [this] { owner.closeInsert(); };
    for (auto* b : { &onButton, &removeButton, &closeButton })
        addAndMakeVisible (*b);
    refresh();
}

RKMixEditor::InsertView::~InsertView()
{
    // As a host does: the plugin forgets its editor before it is deleted.
    if (editor != nullptr)
        plugin.editorBeingDeleted (editor.get());
    editor.reset();
}

void RKMixEditor::InsertView::refresh()
{
    onButton.setToggleState (owner.mixer.getRack().isOn (slot), juce::dontSendNotification);
    repaint();
}

void RKMixEditor::InsertView::resized()
{
    auto header = getLocalBounds().removeFromTop (34).reduced (6, 4);
    closeButton.setBounds (header.removeFromRight (80));
    header.removeFromRight (6);
    removeButton.setBounds (header.removeFromRight (80));
    header.removeFromRight (6);
    onButton.setBounds (header.removeFromRight (60));

    if (editor == nullptr)
        return;

    // The plugin's editor at its own size, scaled down only when it doesn't fit.
    const auto area = getLocalBounds().withTrimmedTop (38).reduced (4);
    const auto scale = juce::jmin (1.0, area.getWidth() / (double) editorBase.getWidth(), area.getHeight() / (double) editorBase.getHeight());
    const auto size = juce::Rectangle<int> (juce::roundToInt (editorBase.getWidth() * scale), juce::roundToInt (editorBase.getHeight() * scale));
    editor->setBounds (size.withCentre (area.getCentre()).withY (area.getY()));
}

void RKMixEditor::InsertView::paint (juce::Graphics& g)
{
    g.fillAll (owner.backgroundColour());
    g.setColour (owner.panelColour());
    g.fillRect (getLocalBounds().removeFromTop (34));
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (owner.slotName (slot) + ": " + plugin.getConfig().name, 12, 0, getWidth() - 280, 34, juce::Justification::centredLeft);
}

juce::String RKMixEditor::slotName (InsertRack::Slot slot) const
{
    juce::String strip;
    switch (slot.strip)
    {
        case InsertRack::Strip::channel: strip = displayName (slot.index); break;
        case InsertRack::Strip::bus:     strip = busDisplayName (slot.index); break;
        case InsertRack::Strip::main:    strip = "Main"; break;
        case InsertRack::Strip::mono:    strip = "Mono"; break;
    }
    return strip + ", insert " + juce::String (slot.slot + 1);
}

juce::Rectangle<int> RKMixEditor::insertArea() const
{
    return contentArea().withTrimmedRight (kMasterWidth + kGap);
}

void RKMixEditor::openInsert (InsertRack::Slot slot)
{
    closeInsert();
    if (auto* plugin = mixer.getRack().get (slot))
    {
        insertView = std::make_unique<InsertView> (*this, slot, *plugin);
        addToCanvas (*insertView);
        insertView->setBounds (insertArea());
    }
}

void RKMixEditor::closeInsert()
{
    insertView.reset();
}

void RKMixEditor::addInsertTypes (juce::PopupMenu& menu, int firstId) const
{
    const auto& types = rkfx::pedals::pedalTypes();
    std::optional<rkfx::Palette::Family> family;
    for (int i = 0; i < (int) types.size(); ++i)
    {
        if (family != types[(size_t) i].family)
        {
            family = types[(size_t) i].family;
            menu.addSectionHeader (rkfx::pedals::familyName (*family));
        }
        menu.addItem (firstId + i, types[(size_t) i].name);
    }
}

void RKMixEditor::showInsertMenu (InsertRack::Slot slot, juce::Component& target, bool full)
{
    enum { openItem = 1, toggleItem, removeItem, firstType = 100 };

    juce::PopupMenu menu;
    if (full)
    {
        menu.addItem (openItem, "Open");
        menu.addItem (toggleItem, "On", true, mixer.getRack().isOn (slot));
        juce::PopupMenu replace;
        addInsertTypes (replace, firstType);
        menu.addSubMenu ("Replace with", replace);
        menu.addSeparator();
        menu.addItem (removeItem, "Remove");
    }
    else
    {
        addInsertTypes (menu, firstType);
    }

    juce::Component::SafePointer<RKMixEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [safe, slot] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& rack = safe->mixer.getRack();
        if (result == openItem)
            safe->openInsert (slot);
        else if (result == toggleItem)
            rack.setOn (slot, ! rack.isOn (slot));
        else if (result == removeItem)
            rack.remove (slot);
        else if (result >= firstType && result - firstType < (int) rkfx::pedals::pedalTypes().size())
        {
            rack.insert (slot, rkfx::pedals::pedalTypes()[(size_t) (result - firstType)].name,
                         safe->mixer.channelsOf (slot.strip, slot.index));
            safe->openInsert (slot);
        }
    });
}
