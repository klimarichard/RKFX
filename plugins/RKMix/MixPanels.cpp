#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

namespace
{
    juce::String keyName (int value)
    {
        return value == 0 ? juce::String ("Self") : "Key: In " + juce::String (value);
    }

    juce::Rectangle<int> inner (juce::Rectangle<int> a) { return a.withTrimmedTop (kSectionTitle).reduced (8, 2); }

    juce::Rectangle<int> place (juce::Rectangle<int>& row, int width)
    {
        auto a = row.removeFromLeft (width);
        row.removeFromLeft (kGap);
        return a;
    }

    juce::Rectangle<int> switchIn (juce::Rectangle<int>& a, int width = kSwitchW)
    {
        return a.removeFromLeft (width).withHeight (kKnobH).withSizeKeepingCentre (width, kSwitchH);
    }
}

//==============================================================================
RKMixEditor::ChoiceBox::ChoiceBox (juce::RangedAudioParameter& param, const std::function<juce::String (int)>& itemName,
                                   int separatorBefore)
{
    const auto numItems = (int) param.getNormalisableRange().end + 1;
    for (int v = 0; v < numItems; ++v)
    {
        if (v == separatorBefore)
            addSeparator();
        addItem (itemName (v), v + 1);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float value)
    {
        setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification);
    });
    onChange = [this]
    {
        if (getSelectedId() > 0)
            attachment->setValueAsCompleteGesture ((float) (getSelectedId() - 1));
    };
    attachment->sendInitialUpdate();
    rkfx::tagParameter (*this, param.getParameterID());
}

//==============================================================================
// Channel page

rkfx::Knob* RKMixEditor::ChannelPanel::knob (int section, const juce::String& suffix, const juce::String& label)
{
    auto* k = make<rkfx::Knob> (owner.mixer.getSettings().param (channelId (channel, suffix.toRawUTF8())), label);
    sections[(size_t) section].knobs.push_back (k);
    return k;
}

RKMixEditor::ChannelPanel::ChannelPanel (RKMixEditor& o, int ch) : Panel (o), channel (ch)
{
    auto& s = o.mixer.getSettings();
    const auto accent = o.accent();
    const auto param = [&] (const char* suffix) -> juce::RangedAudioParameter& { return s.param (channelId (ch, suffix)); };
    stereo = o.isStereo (ch);

    sections[input].title = "Input";
    sections[delay].title = "Delay";
    sections[hpf].title = "High-pass";
    sections[gate].title = "Gate";
    sections[output].title = "Output";
    sections[eq].title = "EQ";
    sections[comp].title = "Compressor";

    styleName (name, 16.0f);
    name.setJustificationType (juce::Justification::centredLeft);
    name.onTextChange = [this] { owner.mixer.setChannelName (channel, name.getText()); };
    addAndMakeVisible (name);

    presets.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presets);
    link = make<rkfx::ToggleSwitch> (s.param (linkId (ch / 2)), accent, "Link " + num (ch / 2 * 2) + "/" + num (ch / 2 * 2 + 1));

    // Input
    trim = knob (input, "trim", "Trim");
    polarity = make<rkfx::ToggleSwitch> (param ("pol"), accent, juce::String::fromUTF8 (stereo ? "\xc3\x98 L" : "\xc3\x98"));
    polarityR = make<rkfx::ToggleSwitch> (param ("polr"), accent, juce::String::fromUTF8 ("\xc3\x98 R"));
    polarityR->setVisible (stereo);

    // Delay
    onSwitches[delay] = make<rkfx::ToggleSwitch> (param ("delay_on"), accent, "On");
    knob (delay, "delay", "Time");

    // HPF
    onSwitches[hpf] = make<rkfx::ToggleSwitch> (param ("hpf_on"), accent, "On");
    knob (hpf, "hpf", "Freq");
    hpfSlope = make<rkfx::SegmentedSwitch> (param ("hpf_slope"), accent, juce::StringArray { "12", "24" }, "dB/oct");

    // Gate
    onSwitches[gate] = make<rkfx::ToggleSwitch> (param ("gate_on"), accent, "On");
    knob (gate, "gate_thr", "Threshold");
    knob (gate, "gate_range", "Range");
    knob (gate, "gate_att", "Attack");
    knob (gate, "gate_hold", "Hold");
    knob (gate, "gate_rel", "Release");
    gateKey = make<ChoiceBox> (param ("gate_key"), keyName, 1);
    gateKey->setTooltip ("Key: the gate listens to another input (after its trim) instead of this channel");

    // Output
    pan = knob (output, "pan", stereo ? "Balance" : "Pan");
    toMain = make<rkfx::ToggleSwitch> (param ("main"), accent, "Main");

    // EQ
    onSwitches[eq] = make<rkfx::ToggleSwitch> (param ("eq_on"), accent, "On");
    lowType = make<rkfx::SegmentedSwitch> (param ("eq_low_type"), accent, juce::StringArray { "Shelf", "Bell" }, "Low");
    highType = make<rkfx::SegmentedSwitch> (param ("eq_high_type"), accent, juce::StringArray { "Shelf", "Bell" }, "High");
    const char* bandIds[] = { "low", "lmid", "hmid", "high" };
    const char* bandNames[] = { "Low", "Lo Mid", "Hi Mid", "High" };
    for (int b = 0; b < 4; ++b)
    {
        const auto prefix = juce::String ("eq_") + bandIds[b];
        eqKnobs[(size_t) (3 * b)] = knob (eq, prefix + "_freq", bandNames[b]);
        eqKnobs[(size_t) (3 * b + 1)] = knob (eq, prefix + "_gain", "Gain");
        eqKnobs[(size_t) (3 * b + 2)] = knob (eq, prefix + "_q", "Q");
    }
    const auto rate = o.sampleRateForDisplay();
    eqCurve = std::make_unique<rkfx::ResponseCurve> ([this, rate] (double hz)
    {
        return StripDsp::responseDb (owner.mixer.channelSettings (channel).strip, hz, rate);
    }, kEqRangeDb, accent);
    addAndMakeVisible (*eqCurve);

    // Compressor
    onSwitches[comp] = make<rkfx::ToggleSwitch> (param ("comp_on"), accent, "On");
    knob (comp, "comp_thr", "Threshold");
    knob (comp, "comp_ratio", "Ratio");
    knob (comp, "comp_knee", "Knee");
    knob (comp, "comp_att", "Attack");
    knob (comp, "comp_rel", "Release");
    knob (comp, "comp_makeup", "Makeup");
    compKey = make<ChoiceBox> (param ("comp_key"), keyName, 1);
    compKey->setTooltip ("Key: the compressor listens to another input (after its trim), e.g. to duck under a voice");
    compCurve = std::make_unique<TransferCurve> ([this] (float inputDb)
    {
        return StripDsp::compTransferDb (owner.mixer.channelSettings (channel).strip.comp, inputDb);
    }, accent);
    addAndMakeVisible (*compCurve);

    compMeter = std::make_unique<rkfx::GainReductionMeter> (accent, 24.0f);
    addAndMakeVisible (*compMeter);

    refresh();
}

void RKMixEditor::ChannelPanel::refresh()
{
    if (! name.isBeingEdited())
        name.setText (owner.mixer.getChannelName (channel), juce::dontSendNotification);

    // Knobs of a section that is off are dimmed (they stay usable).
    const auto& s = owner.mixer.getSettings();
    for (int i = 0; i < numSections; ++i)
        if (auto* on = onSwitches[(size_t) i])
        {
            const auto isOn = s.flag (rkfx::findTaggedParameter (on));
            for (auto* k : sections[(size_t) i].knobs)
                k->setDimmed (! isOn);
        }

    // A shelf has no Q.
    if (s.flag (channelId (channel, "eq_on")))
    {
        eqKnobs[2]->setDimmed (s.choice (channelId (channel, "eq_low_type")) == 0);
        eqKnobs[11]->setDimmed (s.choice (channelId (channel, "eq_high_type")) == 0);
    }

    eqCurve->refresh();
    compCurve->refresh();
    repaint();
}

void RKMixEditor::ChannelPanel::updateMeters (float compDb, float seconds)
{
    compMeter->setReduction (compDb, seconds);
}

void RKMixEditor::ChannelPanel::resized()
{
    auto r = getLocalBounds();

    // Header: "Ch 1", name, colours, strip presets, link
    auto header = r.removeFromTop (kPanelHeader).reduced (4, 2);
    header.removeFromLeft (64);
    link->setBounds (header.removeFromRight (90));
    header.removeFromRight (8);
    presets.setBounds (header.removeFromRight (120));
    header.removeFromRight (16);
    placeSwatches (header, swatches);
    header.removeFromRight (12);
    name.setBounds (header.withWidth (juce::jmin (header.getWidth(), 300)));
    r.removeFromTop (4);

    // Row A: input, delay, HPF, gate, output
    auto rowA = r.removeFromTop (kRowA);
    r.removeFromTop (kGap);

    sections[input].area = place (rowA, 140);
    {
        auto a = inner (sections[input].area);
        trim->setBounds (a.removeFromLeft (kKnobW).withHeight (kKnobH));
        a.removeFromLeft (6);
        auto column = a.removeFromLeft (kSwitchW + 6).withHeight (kKnobH);
        if (stereo)
        {
            auto pair = column.withSizeKeepingCentre (column.getWidth(), 2 * kSwitchH + 6);
            polarity->setBounds (pair.removeFromTop (kSwitchH));
            polarityR->setBounds (pair.removeFromBottom (kSwitchH));
        }
        else
        {
            polarity->setBounds (column.withSizeKeepingCentre (column.getWidth(), kSwitchH));
        }
    }

    sections[delay].area = place (rowA, 128);
    {
        auto a = inner (sections[delay].area);
        onSwitches[delay]->setBounds (switchIn (a));
        a.removeFromLeft (6);
        sections[delay].knobs[0]->setBounds (a.removeFromLeft (kKnobW).withHeight (kKnobH));
    }

    sections[hpf].area = place (rowA, 196);
    {
        auto a = inner (sections[hpf].area);
        onSwitches[hpf]->setBounds (switchIn (a));
        a.removeFromLeft (6);
        sections[hpf].knobs[0]->setBounds (a.removeFromLeft (kKnobW).withHeight (kKnobH));
        a.removeFromLeft (6);
        hpfSlope->setBounds (a.removeFromLeft (64).withHeight (kKnobH).withSizeKeepingCentre (64, 44));
    }

    sections[gate].area = place (rowA, 404);
    {
        auto a = inner (sections[gate].area);
        auto column = a.removeFromLeft (84).withHeight (kKnobH).withSizeKeepingCentre (84, 2 * kSwitchH + 8);
        onSwitches[gate]->setBounds (column.removeFromTop (kSwitchH).withWidth (kSwitchW));
        gateKey->setBounds (column.removeFromBottom (kSwitchH));
        a.removeFromLeft (4);
        for (auto* k : sections[gate].knobs)
            k->setBounds (a.removeFromLeft (kKnobW).withHeight (kKnobH));
    }

    sections[output].area = rowA;
    {
        auto a = inner (sections[output].area);
        pan->setBounds (a.removeFromLeft (kKnobW).withHeight (kKnobH));
        a.removeFromLeft (6);
        toMain->setBounds (switchIn (a, 56));
    }

    // Row B: EQ, compressor
    auto rowB = r;

    sections[eq].area = place (rowB, 624);
    {
        auto a = inner (sections[eq].area);
        auto column = a.removeFromLeft (76);
        onSwitches[eq]->setBounds (column.removeFromTop (kSwitchH).withWidth (kSwitchW));
        column.removeFromTop (8);
        lowType->setBounds (column.removeFromTop (44));
        column.removeFromTop (8);
        highType->setBounds (column.removeFromTop (44));
        a.removeFromLeft (6);
        eqCurve->setBounds (a.removeFromLeft (170).reduced (0, 6));
        a.removeFromLeft (8);
        for (int row = 0; row < 2; ++row)
        {
            auto line = a.removeFromTop (kKnobH);
            for (int k = 0; k < 6; ++k)
                eqKnobs[(size_t) (6 * row + k)]->setBounds (line.removeFromLeft (kKnobW).withHeight (kKnobH));
        }
    }

    sections[comp].area = rowB;
    {
        auto a = inner (sections[comp].area);
        auto column = a.removeFromLeft (84);
        onSwitches[comp]->setBounds (column.removeFromTop (kSwitchH).withWidth (kSwitchW));
        column.removeFromTop (8);
        compKey->setBounds (column.removeFromTop (kSwitchH));
        a.removeFromLeft (4);
        auto knobs = a.removeFromLeft (3 * kKnobW);
        for (int row = 0; row < 2; ++row)
        {
            auto line = knobs.removeFromTop (kKnobH);
            for (int k = 0; k < 3; ++k)
                sections[comp].knobs[(size_t) (3 * row + k)]->setBounds (line.removeFromLeft (kKnobW).withHeight (kKnobH));
        }
        a.removeFromLeft (4);
        compMeter->setBounds (a.removeFromLeft (26).withTrimmedBottom (14));
        a.removeFromLeft (6);
        const auto side = juce::jmin (a.getWidth(), a.getHeight());
        compCurve->setBounds (a.withSizeKeepingCentre (side, side));
    }
}

void RKMixEditor::ChannelPanel::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (14.0f));
    g.drawText ("Ch " + owner.channelLabel (channel), 4, 2, 64, 26, juce::Justification::centredLeft);
    if (name.getText().isEmpty() && ! name.isBeingEdited())
    {
        g.setFont (rkfx::Fonts::regular (14.0f));
        g.drawText ("Double-click to name it", name.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft);
    }

    paintSwatches (g, swatches, owner.mixer.getChannelColour (channel), MixGui::stripColour);

    for (const auto& section : sections)
        rkfx::drawPanel (g, section.area, owner.panelColour(), section.title);
}

void RKMixEditor::ChannelPanel::mouseDown (const juce::MouseEvent& e)
{
    if (const auto i = swatchAt (swatches, e.getPosition()); i >= 0)
        owner.mixer.setChannelColour (channel, i);
}

void RKMixEditor::ChannelPanel::showPresetMenu()
{
    enum { save = 1, reset = 2, reveal = 3, firstFile = 100 };

    const auto folder = owner.mixer.getChannelPresetFolder();
    auto files = std::make_shared<std::vector<juce::File>>();
    juce::PopupMenu found;
    rkfx::addPresetMenuItems (found, folder, *files, {});

    juce::PopupMenu menu;
    menu.addItem (save, "Save this strip as...");
    if (files->empty())
    {
        menu.addItem (-1, "No strip presets yet", false);
    }
    else
    {
        juce::PopupMenu load;
        for (int i = 0; i < (int) files->size(); ++i)
            load.addItem (firstFile + i, (*files)[(size_t) i].getFileNameWithoutExtension());
        menu.addSubMenu ("Load", load);
    }
    menu.addSeparator();
    menu.addItem (reset, "Reset this strip");
    menu.addItem (reveal, "Show strip presets folder");

    juce::Component::SafePointer<RKMixEditor> safe (&owner);
    const auto ch = channel;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presets), [safe, files, folder, ch] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& editor = *safe;
        if (result == reset)
        {
            editor.mixer.resetChannelStrip (ch);
        }
        else if (result == reveal)
        {
            folder.createDirectory();
            folder.revealToUser();
        }
        else if (result == save)
        {
            folder.createDirectory();
            const auto channelName = editor.mixer.getChannelName (ch);
            editor.chooser = std::make_unique<juce::FileChooser> ("Save strip preset as",
                                                                  folder.getChildFile (juce::File::createLegalFileName (channelName.isNotEmpty() ? channelName : "Strip") + ".json"),
                                                                  "*.json");
            editor.chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                             | juce::FileBrowserComponent::warnAboutOverwriting,
                                         [safe, ch] (const juce::FileChooser& fc)
                                         {
                                             auto file = fc.getResult();
                                             if (safe == nullptr || file == juce::File())
                                                 return;
                                             if (! file.hasFileExtension ("json"))
                                                 file = file.withFileExtension ("json");
                                             safe->mixer.saveChannelPreset (ch, file);
                                         });
        }
        else if (result >= firstFile && result - firstFile < (int) files->size())
        {
            editor.mixer.loadChannelPreset (ch, (*files)[(size_t) (result - firstFile)]);
        }
    });
}

//==============================================================================
// Sends page: one column per bus

RKMixEditor::SendsPanel::SendsPanel (RKMixEditor& o, int ch) : Panel (o), channel (ch)
{
    auto& s = o.mixer.getSettings();
    for (int bus = 0; bus < kBuses; ++bus)
    {
        auto& c = columns[(size_t) bus];
        c.assign = make<rkfx::ToggleSwitch> (s.param (channelAssignId (ch, bus)), o.accent(), "Assign");
        c.send = make<rkfx::Knob> (s.param (channelSendId (ch, bus)), "Send");
        c.tap = make<MixGui::OptionList> (s.param (channelTapId (ch, bus)),
                                          juce::StringArray { "Default", "Input", "Pre-EQ", "Pre-Comp", "Pre-Ins", "Pre-Fader", "Post-Fader" },
                                          o.accent());
        c.tap->setTooltip ("Where this send takes the channel's signal: the bus's default (dot) unless set here");
    }
    refresh();
}

void RKMixEditor::SendsPanel::refresh()
{
    for (int bus = 0; bus < kBuses; ++bus)
    {
        const auto type = owner.mixer.getBusType (bus);
        auto& c = columns[(size_t) bus];
        c.assign->setVisible (type == BusType::group);
        c.send->setVisible (type != BusType::group);
        c.tap->setVisible (type == BusType::aux);
        c.tap->setMarked (1 + owner.mixer.getSettings().choice (busId (bus, "tap")));
    }
    repaint();
}

void RKMixEditor::SendsPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kPanelHeader + 4);
    const auto width = (r.getWidth() - (kBuses - 1) * 4) / kBuses;
    for (int bus = 0; bus < kBuses; ++bus)
    {
        auto& c = columns[(size_t) bus];
        c.area = r.removeFromLeft (width);
        r.removeFromLeft (4);
        auto a = c.area.withTrimmedTop (48).reduced (2, 0);
        c.send->setBounds (a.removeFromTop (kKnobH));
        c.assign->setBounds (c.send->getBounds().withSizeKeepingCentre (a.getWidth(), kSwitchH));
        a.removeFromTop (8);
        c.tap->setBounds (a.removeFromTop (7 * 22));
    }
}

void RKMixEditor::SendsPanel::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (owner.displayName (channel) + " - sends", 6, 2, 600, kPanelHeader, juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawText ("Groups take the channel after its fader; Aux sends take it at their tap; FX sends after the fader.",
                getLocalBounds().removeFromTop (kPanelHeader).withTrimmedLeft (280).withTrimmedRight (6), juce::Justification::centredRight);

    for (int bus = 0; bus < kBuses; ++bus)
    {
        const auto& c = columns[(size_t) bus];
        rkfx::drawPanel (g, c.area, owner.panelColour());
        g.setColour (MixGui::stripColour (owner.mixer.getBusColour (bus)));
        g.fillRoundedRectangle (c.area.withHeight (4).reduced (4, 0).toFloat().withY ((float) c.area.getY() + 2.0f), 2.0f);
        g.setColour (rkfx::Palette::text);
        g.setFont (rkfx::Fonts::bold (11.5f));
        g.drawFittedText (owner.busDisplayName (bus), c.area.withHeight (22).withY (c.area.getY() + 8).reduced (2, 0),
                          juce::Justification::centred, 1);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (10.5f));
        const auto type = owner.mixer.getBusType (bus);
        g.drawText (busTypeName (type) + (owner.mixer.isBusStereo (bus) ? "" : " mono"),
                    c.area.withHeight (14).withY (c.area.getY() + 28), juce::Justification::centred);
        if (type == BusType::fx)
            g.drawText ("Post-Fader", c.area.withHeight (kSwitchH).withY (c.area.getY() + 48 + kKnobH + 6),
                        juce::Justification::centred);
    }
}

//==============================================================================
// Bus page

RKMixEditor::BusPanel::BusPanel (RKMixEditor& o, int b) : Panel (o), bus (b)
{
    auto& s = o.mixer.getSettings();
    const auto accent = o.accent();
    const auto param = [&] (const char* suffix) -> juce::RangedAudioParameter& { return s.param (busId (b, suffix)); };

    styleName (name, 16.0f);
    name.setJustificationType (juce::Justification::centredLeft);
    name.onTextChange = [this] { owner.mixer.setBusName (bus, name.getText()); };
    addAndMakeVisible (name);

    sendsOnFader.setClickingTogglesState (false);
    sendsOnFader.onClick = [this] { owner.setSendsOnFader (owner.sendsOnFader == bus ? -1 : bus); };
    addAndMakeVisible (sendsOnFader);

    type = make<rkfx::SegmentedSwitch> (param ("type"), accent, juce::StringArray { "Group", "Aux", "FX" }, "Type");
    format = make<rkfx::SegmentedSwitch> (param ("format"), accent, juce::StringArray { "Mono", "Stereo" }, "Format");
    output = make<ChoiceBox> (param ("out"), targetName, kFirstPairTarget);
    toMain = make<rkfx::ToggleSwitch> (param ("main"), accent, "Main");
    pan = make<rkfx::Knob> (param ("pan"), "Pan");
    defaultTap = make<ChoiceBox> (param ("tap"), [] (int v) { return tapName ((Tap) v); });
    defaultTap->setTooltip ("Where this bus's sends take the channels, unless a send says otherwise");
    panFollows = make<rkfx::ToggleSwitch> (param ("panfollow"), accent, "Pan follows");

    for (int to = 0; to < kBuses; ++to)
    {
        assigns[(size_t) to] = make<rkfx::ToggleSwitch> (s.param (busAssignId (b, to)), accent, o.busDisplayName (to));
        sends[(size_t) to] = make<rkfx::Knob> (s.param (busSendId (b, to)), o.busDisplayName (to));
    }
    refresh();
}

void RKMixEditor::BusPanel::refresh()
{
    if (! name.isBeingEdited())
        name.setText (owner.mixer.getBusName (bus), juce::dontSendNotification);

    const auto t = owner.mixer.getBusType (bus);
    const auto stereo = owner.mixer.isBusStereo (bus);
    toMain->setVisible (t != BusType::aux);
    pan->setVisible (t != BusType::aux);
    defaultTap->setVisible (t == BusType::aux);
    panFollows->setVisible (t == BusType::aux && stereo);
    sendsOnFader.setVisible (t != BusType::group);
    sendsOnFader.setToggleState (owner.sendsOnFader == bus, juce::dontSendNotification);

    // What this bus may feed: later groups (a group), Aux and FX buses (a group), Aux buses (an FX bus).
    for (int to = 0; to < kBuses; ++to)
    {
        assigns[(size_t) to]->setVisible (owner.mixer.canAssign (bus, to));
        sends[(size_t) to]->setVisible (owner.mixer.canSend (bus, to));
    }
    resized();
    repaint();
}

void RKMixEditor::BusPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kPanelHeader).reduced (4, 2);
    header.removeFromLeft (64);
    sendsOnFader.setBounds (header.removeFromRight (130));
    header.removeFromRight (16);
    placeSwatches (header, swatches);
    header.removeFromRight (12);
    name.setBounds (header.withWidth (juce::jmin (header.getWidth(), 300)));
    r.removeFromTop (4);

    setupArea = r.removeFromTop (kRowA);
    r.removeFromTop (kGap);
    {
        auto a = inner (setupArea);
        type->setBounds (a.removeFromLeft (190).withHeight (kKnobH).withSizeKeepingCentre (190, 44));
        a.removeFromLeft (12);
        format->setBounds (a.removeFromLeft (130).withHeight (kKnobH).withSizeKeepingCentre (130, 44));
        a.removeFromLeft (12);
        auto out = a.removeFromLeft (130).withHeight (kKnobH);
        output->setBounds (out.withSizeKeepingCentre (130, kSwitchH).translated (0, 8));
        a.removeFromLeft (16);
        if (toMain->isVisible())
        {
            toMain->setBounds (switchIn (a, 60));
            a.removeFromLeft (12);
            pan->setBounds (a.removeFromLeft (kKnobW).withHeight (kKnobH));
        }
        else
        {
            auto tapArea = a.removeFromLeft (130).withHeight (kKnobH);
            defaultTap->setBounds (tapArea.withSizeKeepingCentre (130, kSwitchH).translated (0, 8));
            a.removeFromLeft (12);
            panFollows->setBounds (switchIn (a, 100));
        }
    }

    routingArea = r;
    auto a = inner (routingArea);
    const auto width = (a.getWidth() - (kBuses - 1) * 2) / kBuses;
    for (int to = 0; to < kBuses; ++to)
    {
        auto column = a.removeFromLeft (width);
        a.removeFromLeft (2);
        sends[(size_t) to]->setBounds (column.removeFromTop (kKnobH));
        assigns[(size_t) to]->setBounds (sends[(size_t) to]->getBounds().withSizeKeepingCentre (column.getWidth(), kSwitchH));
    }
}

void RKMixEditor::BusPanel::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (14.0f));
    g.drawText ("Bus " + num (bus), 4, 2, 64, 26, juce::Justification::centredLeft);
    if (name.getText().isEmpty() && ! name.isBeingEdited())
    {
        g.setFont (rkfx::Fonts::regular (14.0f));
        g.drawText ("Double-click to name it", name.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft);
    }
    paintSwatches (g, swatches, owner.mixer.getBusColour (bus), MixGui::stripColour);

    rkfx::drawPanel (g, setupArea, owner.panelColour(), "Setup");
    const auto t = owner.mixer.getBusType (bus);
    rkfx::drawPanel (g, routingArea, owner.panelColour(),
               t == BusType::group ? "Feeds: later groups (assign) and Aux / FX buses (send)"
                                   : t == BusType::fx ? "Feeds: Aux buses (send), e.g. the reverb into the in-ears" : "Feeds");

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    auto setup = inner (setupArea);
    g.drawText ("Output", setup.withX (setup.getX() + 332).withWidth (130).withHeight (24), juce::Justification::centred);
    if (t == BusType::aux)
    {
        g.drawText ("Default tap", setup.withX (setup.getX() + 478).withWidth (130).withHeight (24), juce::Justification::centred);
        g.drawFittedText ("An Aux bus feeds only its own output: an in-ear or monitor mix, a recorder feed. "
                          "Its sends take each channel at the tap set here, unless a send sets its own.",
                          inner (routingArea).reduced (4), juce::Justification::topLeft, 3);
    }
}

void RKMixEditor::BusPanel::mouseDown (const juce::MouseEvent& e)
{
    if (const auto i = swatchAt (swatches, e.getPosition()); i >= 0)
        owner.mixer.setBusColour (bus, i);
}

//==============================================================================
// Routing page: the main and mono outputs, and what every output carries

RKMixEditor::RoutingPanel::RoutingPanel (RKMixEditor& o) : Panel (o)
{
    auto& s = o.mixer.getSettings();
    mainOutput = std::make_unique<ChoiceBox> (s.param ("main_out"), targetName, kFirstPairTarget);
    monoOutput = std::make_unique<ChoiceBox> (s.param ("mono_out"), targetName, kFirstPairTarget);
    addAndMakeVisible (*mainOutput);
    addAndMakeVisible (*monoOutput);

    balance = std::make_unique<rkfx::Knob> (s.param ("main_bal"), "Balance");
    addAndMakeVisible (*balance);

    for (int out = 0; out < kChannels; ++out)
    {
        auto& label = outputNames[(size_t) out];
        styleName (label, 11.5f);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setTooltip ("Double-click to name this output");
        label.onTextChange = [this, out] { owner.mixer.setOutputName (out, outputNames[(size_t) out].getText()); };
        addAndMakeVisible (label);
    }
}

void RKMixEditor::RoutingPanel::refresh()
{
    sources.fill ({});
    sourceCount.fill (0);
    const auto add = [this] (int target, const juce::String& what)
    {
        const auto first = firstOutputOf (target);
        if (first < 0)
            return;
        for (int out = first; out < first + (isPairTarget (target) ? 2 : 1); ++out)
        {
            sources[(size_t) out] += (sources[(size_t) out].isEmpty() ? "" : " + ") + what;
            ++sourceCount[(size_t) out];
        }
    };

    const auto& s = owner.mixer.getSettings();
    add (s.choice ("main_out"), "Main");
    add (s.choice ("mono_out"), "Mono");
    for (int bus = 0; bus < kBuses; ++bus)
        add (s.choice (busId (bus, "out")), owner.busDisplayName (bus));

    for (int out = 0; out < kChannels; ++out)
        if (! outputNames[(size_t) out].isBeingEdited())
            outputNames[(size_t) out].setText (owner.mixer.getOutputName (out), juce::dontSendNotification);
    repaint();
}

void RKMixEditor::RoutingPanel::resized()
{
    auto r = getLocalBounds();
    mainArea = r.removeFromLeft (290);
    r.removeFromLeft (kGap);
    outputsArea = r;

    auto a = inner (mainArea).reduced (4, 4);
    auto row = a.removeFromTop (26);
    row.removeFromLeft (100);
    mainOutput->setBounds (row);
    a.removeFromTop (8);
    row = a.removeFromTop (26);
    row.removeFromLeft (100);
    monoOutput->setBounds (row);
    a.removeFromTop (8);
    balance->setBounds (a.removeFromTop (kKnobH).withWidth (kKnobW).withX (a.getX() + 100));

    auto grid = inner (outputsArea);
    const auto cellW = grid.getWidth() / 4, cellH = grid.getHeight() / 8;
    for (int out = 0; out < kChannels; ++out)
    {
        const auto cell = juce::Rectangle<int> (grid.getX() + (out / 8) * cellW, grid.getY() + (out % 8) * cellH, cellW, cellH);
        outputNames[(size_t) out].setBounds (cell.withTrimmedLeft (34).withHeight (cellH / 2 + 2).reduced (0, 1));
    }
}

void RKMixEditor::RoutingPanel::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, mainArea, owner.panelColour(), "Main and mono");
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::regular (13.0f));
    auto a = inner (mainArea).reduced (4, 4);
    g.drawText ("Main output", a.removeFromTop (26), juce::Justification::centredLeft);
    a.removeFromTop (8);
    g.drawText ("Mono output", a.removeFromTop (26), juce::Justification::centredLeft);
    a.removeFromTop (8 + kKnobH + 6);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawFittedText ("The mono sum is (L + R) / 2 of the main after its fader. A stereo source on a single output "
                      "is summed the same way; several sources on one output add up.",
                      a, juce::Justification::topLeft, 5);

    rkfx::drawPanel (g, outputsArea, owner.panelColour(), "Outputs");
    auto grid = inner (outputsArea);
    const auto cellW = grid.getWidth() / 4, cellH = grid.getHeight() / 8;
    for (int out = 0; out < kChannels; ++out)
    {
        const auto cell = juce::Rectangle<int> (grid.getX() + (out / 8) * cellW, grid.getY() + (out % 8) * cellH, cellW, cellH);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (11.5f));
        g.drawText (juce::String (out + 1), cell.withWidth (30).withHeight (cellH / 2 + 2), juce::Justification::centredRight);
        if (outputNames[(size_t) out].getText().isEmpty() && ! outputNames[(size_t) out].isBeingEdited())
        {
            g.setFont (rkfx::Fonts::regular (11.5f));
            g.drawText ("Out " + juce::String (out + 1), outputNames[(size_t) out].getBounds().withTrimmedLeft (3), juce::Justification::centredLeft);
        }

        // Several sources on one output add up: shown in the accent.
        const auto count = sourceCount[(size_t) out];
        g.setColour (count > 1 ? owner.accent().brighter (0.4f) : count == 1 ? rkfx::Palette::text : rkfx::Palette::knobTrack);
        g.setFont (rkfx::Fonts::regular (11.0f));
        g.drawFittedText (count == 0 ? juce::String ("silent") : sources[(size_t) out],
                          cell.withTrimmedLeft (37).withTrimmedTop (cellH / 2).withTrimmedRight (4), juce::Justification::centredLeft, 1);
    }
}
