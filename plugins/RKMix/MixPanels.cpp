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

    constexpr int kDirectLabelWidth = 70, kDirectAtWidth = 24;   // the sends page's "Direct out [..] at [..]"

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
    : nameOf (itemName), numValues ((int) param.getNormalisableRange().end + 1)
{
    const auto numItems = numValues;
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

void RKMixEditor::ChoiceBox::filter (const std::function<bool (int)>& offered)
{
    clear (juce::dontSendNotification);
    for (int v = 0; v < numValues; ++v)
        if (v == 0 || offered (v))
            addItem (nameOf (v), v + 1);
    attachment->sendInitialUpdate();
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
    soloSafe = make<rkfx::ToggleSwitch> (param ("solosafe"), accent, "Solo safe");

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
        auto column = a.removeFromLeft (64).withHeight (kKnobH).withSizeKeepingCentre (64, 2 * kSwitchH + 8);
        toMain->setBounds (column.removeFromTop (kSwitchH));
        soloSafe->setBounds (column.removeFromBottom (kSwitchH));
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
        compMeter->setBounds (a.removeFromLeft (40).withTrimmedBottom (4));
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
        c.pan = make<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        auto& panParam = s.param (channelSendPanId (ch, bus));
        c.panAttachment = std::make_unique<juce::SliderParameterAttachment> (panParam, *c.pan);
        c.pan->setDoubleClickReturnValue (true, 0.0);
        c.pan->setPopupDisplayEnabled (true, true, nullptr);
        c.pan->setTooltip ("This send's pan in the bus (the bus's Pan is Own; double-click: centre)");
        rkfx::tagParameter (*c.pan, panParam.getParameterID());
        c.tap = make<MixGui::OptionList> (s.param (channelTapId (ch, bus)),
                                          juce::StringArray { "Default", "Input", "Pre-EQ", "Pre-Comp", "Pre-Ins", "Pre-Fader", "Post-Fader" },
                                          o.accent());
        c.tap->setTooltip ("Where this send takes the channel's signal: the bus's default (dot) unless set here");
    }

    directOut = make<ChoiceBox> (s.param (channelId (ch, "direct_out")), targetName, kFirstPairTarget);
    directOut->setTooltip ("Direct out: this channel straight to an output (as well as everywhere else it goes)");
    directTap = make<ChoiceBox> (s.param (channelId (ch, "direct_tap")), [] (int v) { return tapName ((Tap) v); });
    directTap->setTooltip ("Where the direct out takes the channel. Before Post-Fader it ignores mute and solo; "
                           "Post-Fader follows the fader and mute. Never panned.");
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
        c.pan->setVisible (owner.hasOwnPans (bus));
        c.pan->setColour (juce::Slider::trackColourId, MixGui::stripColour (owner.mixer.getBusColour (bus)));
        c.tap->setVisible (type == BusType::aux);
        c.tap->setMarked (1 + owner.mixer.getSettings().choice (busId (bus, "tap")));
    }

    // A mono channel goes to one output, a stereo pair to an output pair.
    const auto stereo = owner.isStereo (channel);
    directOut->filter ([stereo] (int target) { return isPairTarget (target) == stereo; });
    directTap->setEnabled (owner.mixer.getSettings().choice (channelId (channel, "direct_out")) != kTargetOff);
    repaint();
}

void RKMixEditor::SendsPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kPanelHeader).reduced (4, 3);
    directTap->setBounds (header.removeFromRight (120));
    header.removeFromRight (kDirectAtWidth);
    directOut->setBounds (header.removeFromRight (120));
    r.removeFromTop (4);
    const auto width = (r.getWidth() - (kBuses - 1) * 4) / kBuses;
    for (int bus = 0; bus < kBuses; ++bus)
    {
        auto& c = columns[(size_t) bus];
        c.area = r.removeFromLeft (width);
        r.removeFromLeft (4);
        auto a = c.area.withTrimmedTop (48).reduced (2, 0);
        c.send->setBounds (a.removeFromTop (kKnobH));
        c.assign->setBounds (c.send->getBounds().withSizeKeepingCentre (a.getWidth(), kSwitchH));
        c.pan->setBounds (a.removeFromTop (22));
        a.removeFromTop (6);
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
    const auto label = [&] (const juce::String& text, juce::Rectangle<int> area)
    {
        g.drawText (text, area.withY (0).withHeight (kPanelHeader), juce::Justification::centred);
    };
    label ("Direct out", directOut->getBounds().withX (directOut->getX() - kDirectLabelWidth).withWidth (kDirectLabelWidth));
    label ("at", directOut->getBounds().withX (directOut->getRight()).withWidth (kDirectAtWidth));
    g.drawFittedText ("Groups after the fader, Aux sends at their tap, FX sends after the fader.",
                      getLocalBounds().removeFromTop (kPanelHeader).withTrimmedLeft (280)
                          .withRight (directOut->getX() - kDirectLabelWidth - 12),
                      juce::Justification::centredRight, 1);

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
// Processing: the EQ, compressor and limiter of a bus, the main or the mono sum

namespace
{
    constexpr int kProcKnobW = 56, kProcKnobH = 75;
}

rkfx::Knob* RKMixEditor::ProcessingPanel::knob (const char* suffix, const juce::String& label)
{
    return make<rkfx::Knob> (owner.mixer.getSettings().param (chainId (chain, suffix)), label);
}

RKMixEditor::ProcessingPanel::ProcessingPanel (RKMixEditor& o, int c) : Panel (o), chain (c)
{
    auto& s = o.mixer.getSettings();
    const auto accent = o.accent();
    const auto param = [&] (const char* suffix) -> juce::RangedAudioParameter& { return s.param (chainId (chain, suffix)); };

    eqOn = make<rkfx::ToggleSwitch> (param ("eq_on"), accent, "On");
    lowType = make<rkfx::SegmentedSwitch> (param ("eq_low_type"), accent, juce::StringArray { "Shelf", "Bell" }, "Band 1");
    highType = make<rkfx::SegmentedSwitch> (param ("eq_high_type"), accent, juce::StringArray { "Shelf", "Bell" }, "Band 6");
    hpfOn = make<rkfx::ToggleSwitch> (param ("hpf_on"), accent, "HPF");
    hpf = knob ("hpf", "Freq");
    hpfSlope = make<rkfx::SegmentedSwitch> (param ("hpf_slope"), accent, juce::StringArray { "12", "24" });
    lpfOn = make<rkfx::ToggleSwitch> (param ("lpf_on"), accent, "LPF");
    lpf = knob ("lpf", "Freq");
    lpfSlope = make<rkfx::SegmentedSwitch> (param ("lpf_slope"), accent, juce::StringArray { "12", "24" });
    for (int b = 0; b < 6; ++b)
    {
        const auto band = "eq_b" + juce::String (b + 1);
        bands[(size_t) (3 * b)] = knob ((band + "_freq").toRawUTF8(), juce::String (b + 1));
        bands[(size_t) (3 * b + 1)] = knob ((band + "_gain").toRawUTF8(), "Gain");
        bands[(size_t) (3 * b + 2)] = knob ((band + "_q").toRawUTF8(), "Q");
    }
    const auto rate = o.sampleRateForDisplay();
    curve = std::make_unique<rkfx::ResponseCurve> ([this, rate] (double hz)
    {
        return BusDsp::responseDb (owner.mixer.chainSettings (chain).eq, hz, rate);
    }, kEqRangeDb, accent);
    addAndMakeVisible (*curve);

    compOn = make<rkfx::ToggleSwitch> (param ("comp_on"), accent, "On");
    compAuto = make<rkfx::ToggleSwitch> (param ("comp_auto"), accent, "Auto rel.");
    const char* compIds[] = { "comp_thr", "comp_ratio", "comp_knee", "comp_att", "comp_rel", "comp_makeup" };
    const char* compNames[] = { "Threshold", "Ratio", "Knee", "Attack", "Release", "Makeup" };
    for (int k = 0; k < 6; ++k)
        comp[(size_t) k] = knob (compIds[k], compNames[k]);
    compMeter = std::make_unique<rkfx::GainReductionMeter> (accent, 24.0f);
    addAndMakeVisible (*compMeter);

    limitOn = make<rkfx::ToggleSwitch> (param ("lim_on"), accent, "On");
    limit[0] = knob ("lim_ceiling", "Ceiling");
    limit[1] = knob ("lim_rel", "Release");
    limitMeter = std::make_unique<rkfx::GainReductionMeter> (accent, 24.0f);
    addAndMakeVisible (*limitMeter);

    refresh();
}

void RKMixEditor::ProcessingPanel::refresh()
{
    const auto& s = owner.mixer.getSettings();
    const auto flag = [&] (const char* suffix) { return s.flag (chainId (chain, suffix)); };
    const auto eq = flag ("eq_on");

    for (auto* k : bands)
        k->setDimmed (! eq);
    hpf->setDimmed (! (eq && flag ("hpf_on")));
    lpf->setDimmed (! (eq && flag ("lpf_on")));
    if (eq)
    {
        bands[2]->setDimmed (s.choice (chainId (chain, "eq_low_type")) == 0);    // a shelf has no Q
        bands[17]->setDimmed (s.choice (chainId (chain, "eq_high_type")) == 0);
    }
    for (auto* k : comp)
        k->setDimmed (! flag ("comp_on"));
    comp[4]->setDimmed (! flag ("comp_on") || flag ("comp_auto"));
    for (auto* k : limit)
        k->setDimmed (! flag ("lim_on"));

    curve->refresh();
    repaint();
}

void RKMixEditor::ProcessingPanel::updateMeters (float compDb, float limitDb, float seconds)
{
    compMeter->setReduction (compDb, seconds);
    limitMeter->setReduction (limitDb, seconds);
}

void RKMixEditor::ProcessingPanel::resized()
{
    auto r = getLocalBounds();
    limitArea = r.removeFromRight (122);
    r.removeFromRight (kGap);
    compArea = r.removeFromRight (246);
    r.removeFromRight (kGap);
    eqArea = r;

    const auto column = [] (juce::Rectangle<int>& a, int width) { auto c = a.removeFromLeft (width); a.removeFromLeft (4); return c; };

    // EQ: switches, curve, HPF, 6 bands (Freq, Gain, Q stacked), LPF
    auto a = inner (eqArea);
    auto switches = column (a, 64);
    eqOn->setBounds (switches.removeFromTop (kSwitchH).withWidth (kSwitchW));
    switches.removeFromTop (10);
    lowType->setBounds (switches.removeFromTop (44));
    switches.removeFromTop (8);
    highType->setBounds (switches.removeFromTop (44));
    curve->setBounds (column (a, juce::jmax (100, a.getWidth() - 8 * (kProcKnobW + 4))).reduced (0, 4));

    auto hpfColumn = column (a, kProcKnobW);
    hpfOn->setBounds (hpfColumn.removeFromTop (kSwitchH));
    hpf->setBounds (hpfColumn.removeFromTop (kProcKnobH));
    hpfSlope->setBounds (hpfColumn.removeFromTop (24));
    for (int b = 0; b < 6; ++b)
    {
        auto band = column (a, kProcKnobW);
        for (int k = 0; k < 3; ++k)
            bands[(size_t) (3 * b + k)]->setBounds (band.removeFromTop (kProcKnobH));
    }
    auto lpfColumn = column (a, kProcKnobW);
    lpfOn->setBounds (lpfColumn.removeFromTop (kSwitchH));
    lpf->setBounds (lpfColumn.removeFromTop (kProcKnobH));
    lpfSlope->setBounds (lpfColumn.removeFromTop (24));

    // Compressor: On and Auto, two columns of knobs, gain reduction
    a = inner (compArea);
    auto compSwitches = column (a, 64);
    compOn->setBounds (compSwitches.removeFromTop (kSwitchH).withWidth (kSwitchW));
    compSwitches.removeFromTop (8);
    compAuto->setBounds (compSwitches.removeFromTop (kSwitchH));
    for (int c = 0; c < 2; ++c)
    {
        auto knobs = column (a, kProcKnobW);
        for (int k = 0; k < 3; ++k)
            comp[(size_t) (3 * c + k)]->setBounds (knobs.removeFromTop (kProcKnobH));
    }
    compMeter->setBounds (a.withWidth (40).withTrimmedBottom (4));

    // Limiter: On, Ceiling, Release, gain reduction
    a = inner (limitArea);
    auto limitKnobs = column (a, kProcKnobW);
    limitOn->setBounds (limitKnobs.removeFromTop (kSwitchH));
    limitKnobs.removeFromTop (4);
    limit[0]->setBounds (limitKnobs.removeFromTop (kProcKnobH));
    limit[1]->setBounds (limitKnobs.removeFromTop (kProcKnobH));
    limitMeter->setBounds (a.withWidth (40).withTrimmedBottom (4));
}

void RKMixEditor::ProcessingPanel::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, eqArea, owner.panelColour(), "EQ");
    rkfx::drawPanel (g, compArea, owner.panelColour(), "Compressor");
    rkfx::drawPanel (g, limitArea, owner.panelColour(), "Limiter");
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
    defaultTap = make<ChoiceBox> (param ("tap"), [] (int v) { return tapName ((Tap) v); });
    defaultTap->setTooltip ("Where this bus's sends take the channels, unless a send says otherwise");
    panMode = make<rkfx::SegmentedSwitch> (param ("panmode"), accent, juce::StringArray { "Own", "Follow", "Mirror" }, "Pan");
    soloSafe = make<rkfx::SegmentedSwitch> (param ("solosafe"), accent, juce::StringArray { "Auto", "On", "Off" });

    auto& panParam = param ("pan");
    pan.setSliderStyle (juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    panAttachment = std::make_unique<juce::SliderParameterAttachment> (panParam, pan);
    pan.setDoubleClickReturnValue (true, 0.0);
    rkfx::tagParameter (pan, panParam.getParameterID());
    addAndMakeVisible (pan);

    processing = std::make_unique<ProcessingPanel> (o, b);
    addAndMakeVisible (*processing);
    refresh();
}

void RKMixEditor::BusPanel::refresh()
{
    if (! name.isBeingEdited())
        name.setText (owner.mixer.getBusName (bus), juce::dontSendNotification);

    const auto t = owner.mixer.getBusType (bus);
    const auto stereo = owner.mixer.isBusStereo (bus);
    // A mono bus goes to one output, a stereo bus to an output pair.
    output->filter ([stereo] (int target) { return isPairTarget (target) == stereo; });
    toMain->setVisible (t != BusType::aux);
    pan.setVisible (t != BusType::aux);
    defaultTap->setVisible (t == BusType::aux);
    panMode->setVisible (t == BusType::aux && stereo);
    sendsOnFader.setVisible (t != BusType::group);
    sendsOnFader.setToggleState (owner.sendsOnFader == bus, juce::dontSendNotification);
    processing->refresh();
    resized();
    repaint();
}

void RKMixEditor::BusPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kPanelHeader).reduced (4, 2);
    header.removeFromLeft (64);
    sendsOnFader.setBounds (header.removeFromRight (130));
    header.removeFromRight (12);
    soloSafe->setBounds (header.removeFromRight (140));
    header.removeFromRight (70);   // the "Solo safe" label
    placeSwatches (header, swatches);
    header.removeFromRight (12);
    name.setBounds (header.withWidth (juce::jmin (header.getWidth(), 260)));
    r.removeFromTop (4);

    setupArea = r.removeFromTop (70);
    r.removeFromTop (kGap);
    processing->setBounds (r);

    auto a = setupArea.withTrimmedTop (16).reduced (8, 2);
    type->setBounds (a.removeFromLeft (190).withSizeKeepingCentre (190, 44));
    a.removeFromLeft (12);
    format->setBounds (a.removeFromLeft (130).withSizeKeepingCentre (130, 44));
    a.removeFromLeft (12);
    output->setBounds (a.removeFromLeft (130).withSizeKeepingCentre (130, kSwitchH).translated (0, 8));
    a.removeFromLeft (16);
    if (toMain->isVisible())
    {
        toMain->setBounds (a.removeFromLeft (60).withSizeKeepingCentre (60, kSwitchH).translated (0, 8));
        a.removeFromLeft (16);
        pan.setBounds (a.removeFromLeft (200).withSizeKeepingCentre (200, kSwitchH).translated (0, 8));
    }
    else
    {
        defaultTap->setBounds (a.removeFromLeft (130).withSizeKeepingCentre (130, kSwitchH).translated (0, 8));
        a.removeFromLeft (12);
        panMode->setBounds (a.removeFromLeft (210).withSizeKeepingCentre (210, 44));
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

    rkfx::drawPanel (g, setupArea, owner.panelColour());
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    const auto label = [&] (juce::Component& c, const juce::String& text)
    {
        if (c.isVisible())
            g.drawText (text, c.getBounds().withY (c.getY() - 20).withHeight (18), juce::Justification::centred);
    };
    label (*output, "Output");
    label (*defaultTap, "Default tap");
    label (pan, "Pan into the main");
    g.drawText ("Solo safe", soloSafe->getBounds().translated (-62, 0).withWidth (60), juce::Justification::centredRight);
}

void RKMixEditor::BusPanel::mouseDown (const juce::MouseEvent& e)
{
    if (const auto i = swatchAt (swatches, e.getPosition()); i >= 0)
        owner.mixer.setBusColour (bus, i);
}

//==============================================================================
// A bus's sends page: what it feeds

RKMixEditor::BusSendsPanel::BusSendsPanel (RKMixEditor& o, int b) : Panel (o), bus (b)
{
    auto& s = o.mixer.getSettings();
    for (int to = 0; to < kBuses; ++to)
    {
        assigns[(size_t) to] = make<rkfx::ToggleSwitch> (s.param (busAssignId (b, to)), o.accent(), "Assign");
        sends[(size_t) to] = make<rkfx::Knob> (s.param (busSendId (b, to)), "Send");
    }
    refresh();
}

void RKMixEditor::BusSendsPanel::refresh()
{
    for (int to = 0; to < kBuses; ++to)
    {
        assigns[(size_t) to]->setVisible (owner.mixer.canAssign (bus, to));
        sends[(size_t) to]->setVisible (owner.mixer.canSend (bus, to));
    }
    repaint();
}

void RKMixEditor::BusSendsPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kPanelHeader + 4);
    const auto width = (r.getWidth() - (kBuses - 1) * 4) / kBuses;
    for (int to = 0; to < kBuses; ++to)
    {
        columns[(size_t) to] = r.removeFromLeft (width);
        r.removeFromLeft (4);
        auto a = columns[(size_t) to].withTrimmedTop (48).reduced (2, 0);
        sends[(size_t) to]->setBounds (a.removeFromTop (kKnobH));
        assigns[(size_t) to]->setBounds (sends[(size_t) to]->getBounds().withSizeKeepingCentre (a.getWidth(), kSwitchH));
    }
}

void RKMixEditor::BusSendsPanel::paint (juce::Graphics& g)
{
    const auto t = owner.mixer.getBusType (bus);
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (owner.busDisplayName (bus) + " - feeds", 6, 2, 400, kPanelHeader, juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawText (t == BusType::group ? "A group feeds later groups (assign) and Aux and FX buses (send), after its fader."
                : t == BusType::fx ? "An FX bus feeds Aux buses (send), after its fader: the reverb into the in-ears."
                                   : "An Aux bus feeds only its own output.",
                getLocalBounds().removeFromTop (kPanelHeader).withTrimmedLeft (300).withTrimmedRight (6), juce::Justification::centredRight);

    for (int to = 0; to < kBuses; ++to)
    {
        const auto area = columns[(size_t) to];
        const auto usable = owner.mixer.canAssign (bus, to) || owner.mixer.canSend (bus, to);
        rkfx::drawPanel (g, area, usable ? owner.panelColour() : owner.panelColour().darker (0.3f));
        g.setColour (MixGui::stripColour (owner.mixer.getBusColour (to)).withMultipliedAlpha (usable ? 1.0f : 0.4f));
        g.fillRoundedRectangle (area.withHeight (4).reduced (4, 0).toFloat().withY ((float) area.getY() + 2.0f), 2.0f);
        g.setColour (usable ? rkfx::Palette::text : rkfx::Palette::knobTrack);
        g.setFont (rkfx::Fonts::bold (11.5f));
        g.drawFittedText (owner.busDisplayName (to), area.withHeight (22).withY (area.getY() + 8).reduced (2, 0),
                          juce::Justification::centred, 1);
        g.setFont (rkfx::Fonts::regular (10.5f));
        g.drawText (to == bus ? juce::String ("this bus") : busTypeName (owner.mixer.getBusType (to)),
                    area.withHeight (14).withY (area.getY() + 28), juce::Justification::centred);
    }
}

//==============================================================================
// Main page: the main or the mono sum

RKMixEditor::MainPanel::MainPanel (RKMixEditor& o, int c) : Panel (o), chain (c)
{
    auto& s = o.mixer.getSettings();
    for (auto* b : { &mainButton, &monoButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    mainButton.setToggleState (chain == kMainChain, juce::dontSendNotification);
    monoButton.setToggleState (chain == kMonoChain, juce::dontSendNotification);
    mainButton.onClick = [this] { owner.selectMain (kMainChain); };
    monoButton.onClick = [this] { owner.selectMain (kMonoChain); };

    output = std::make_unique<ChoiceBox> (s.param (chain == kMainChain ? "main_out" : "mono_out"), targetName, kFirstPairTarget);
    addAndMakeVisible (*output);
    if (chain == kMainChain)
    {
        balance = std::make_unique<rkfx::Knob> (s.param ("main_bal"), "Balance");
        addAndMakeVisible (*balance);
    }

    processing = std::make_unique<ProcessingPanel> (o, chain);
    addAndMakeVisible (*processing);
    refresh();
}

void RKMixEditor::MainPanel::refresh()
{
    processing->refresh();
    repaint();
}

void RKMixEditor::MainPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kPanelHeader).reduced (4, 2);
    mainButton.setBounds (header.removeFromLeft (90));
    header.removeFromLeft (6);
    monoButton.setBounds (header.removeFromLeft (90));
    r.removeFromTop (4);

    setupArea = r.removeFromTop (70);
    r.removeFromTop (kGap);
    processing->setBounds (r);

    auto a = setupArea.reduced (8, 4);
    a.removeFromLeft (90);
    output->setBounds (a.removeFromLeft (140).withSizeKeepingCentre (140, kSwitchH));
    a.removeFromLeft (24);
    if (balance != nullptr)
        balance->setBounds (a.removeFromLeft (kKnobW).withHeight (setupArea.getHeight()).withY (setupArea.getY()));
}

void RKMixEditor::MainPanel::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, setupArea, owner.panelColour());
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::regular (13.0f));
    g.drawText ("Output", setupArea.reduced (12, 4).withWidth (80), juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawFittedText (chain == kMainChain ? "EQ -> Comp -> inserts -> fader, balance -> Limit -> output"
                                          : "(L + R) / 2 of the main -> EQ -> Comp -> inserts -> fader -> Limit -> output. "
                                            "Its EQ's low-pass makes a sub feed.",
                      setupArea.reduced (12, 4).withTrimmedLeft (balance != nullptr ? 330 : 260), juce::Justification::centredLeft, 2);
}

//==============================================================================
// Talk / Osc page

RKMixEditor::MonitorPanel::MonitorPanel (RKMixEditor& o) : Panel (o)
{
    auto& s = o.mixer.getSettings();
    const auto accent = o.accent();

    talkSource = std::make_unique<ChoiceBox> (s.param ("talk_src"), [] (int v) { return v == 0 ? juce::String ("Off") : "In " + juce::String (v); }, 1);
    addAndMakeVisible (*talkSource);
    talkLevel = make<rkfx::Knob> (s.param ("talk_level"), "Level");
    talkLatch = make<rkfx::ToggleSwitch> (s.param ("talk_latch"), accent, "Latch");
    talkMain = make<rkfx::ToggleSwitch> (s.param ("talk_main"), accent, "Main");

    oscOn = make<rkfx::ToggleSwitch> (s.param ("osc_on"), rkfx::Palette::warning, "On");
    oscType = make<rkfx::SegmentedSwitch> (s.param ("osc_type"), accent, juce::StringArray { "Sine", "Pink" });
    oscFreq = make<rkfx::Knob> (s.param ("osc_freq"), "Freq");
    oscLevel = make<rkfx::Knob> (s.param ("osc_level"), "Level");
    oscMain = make<rkfx::ToggleSwitch> (s.param ("osc_main"), accent, "Main");
    oscOutput = std::make_unique<ChoiceBox> (s.param ("osc_out"), targetName, kFirstPairTarget);
    addAndMakeVisible (*oscOutput);

    for (int bus = 0; bus < kBuses; ++bus)
    {
        talkTo[(size_t) bus] = make<rkfx::ToggleSwitch> (s.param ("talk_to" + num (bus)), accent, num (bus));
        oscTo[(size_t) bus] = make<rkfx::ToggleSwitch> (s.param ("osc_to" + num (bus)), accent, num (bus));
    }
    refresh();
}

void RKMixEditor::MonitorPanel::refresh()
{
    // Talkback reaches Aux buses only (monitor mixes); the others' switches are dimmed out.
    for (int bus = 0; bus < kBuses; ++bus)
    {
        talkTo[(size_t) bus]->setAlpha (owner.mixer.getBusType (bus) == BusType::aux ? 1.0f : 0.35f);
    }
    oscFreq->setDimmed (owner.mixer.getSettings().choice ("osc_type") == 1);
    repaint();
}

void RKMixEditor::MonitorPanel::resized()
{
    auto r = getLocalBounds();
    talkArea = r.removeFromTop (r.getHeight() / 2 - 4);
    r.removeFromTop (kGap);
    oscArea = r;

    const auto busGrid = [] (juce::Rectangle<int> area, std::array<rkfx::ToggleSwitch*, mix::kBuses>& switches)
    {
        const auto w = (area.getWidth() - 7 * 4) / 8;
        for (int bus = 0; bus < kBuses; ++bus)
            switches[(size_t) bus]->setBounds (area.getX() + (bus % 8) * (w + 4), area.getY() + (bus / 8) * (kSwitchH + 6), w, kSwitchH);
    };

    auto a = inner (talkArea);
    auto controls = a.removeFromLeft (420);
    talkSource->setBounds (controls.removeFromLeft (110).withSizeKeepingCentre (110, kSwitchH).translated (0, 8));
    controls.removeFromLeft (12);
    talkLevel->setBounds (controls.removeFromLeft (kKnobW).withHeight (kKnobH));
    controls.removeFromLeft (12);
    talkLatch->setBounds (controls.removeFromLeft (70).withSizeKeepingCentre (70, kSwitchH));
    a.removeFromLeft (16);
    auto dest = a.withTrimmedTop (18);
    talkMain->setBounds (dest.removeFromRight (60).withHeight (kSwitchH));
    dest.removeFromRight (12);
    busGrid (dest, talkTo);

    a = inner (oscArea);
    controls = a.removeFromLeft (420);
    oscOn->setBounds (controls.removeFromLeft (50).withSizeKeepingCentre (50, kSwitchH));
    controls.removeFromLeft (8);
    oscType->setBounds (controls.removeFromLeft (100).withSizeKeepingCentre (100, kSwitchH));
    controls.removeFromLeft (8);
    oscFreq->setBounds (controls.removeFromLeft (kKnobW).withHeight (kKnobH));
    controls.removeFromLeft (4);
    oscLevel->setBounds (controls.removeFromLeft (kKnobW).withHeight (kKnobH));
    controls.removeFromLeft (8);
    oscOutput->setBounds (controls.removeFromLeft (110).withSizeKeepingCentre (110, kSwitchH).translated (0, 8));
    a.removeFromLeft (16);
    dest = a.withTrimmedTop (18);
    oscMain->setBounds (dest.removeFromRight (60).withHeight (kSwitchH));
    dest.removeFromRight (12);
    busGrid (dest, oscTo);
}

void RKMixEditor::MonitorPanel::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, talkArea, owner.panelColour(), "Talkback");
    rkfx::drawPanel (g, oscArea, owner.panelColour(), "Oscillator");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.5f));
    g.drawText ("Source (input)", talkSource->getBounds().withY (talkSource->getY() - 18).withHeight (16), juce::Justification::centredLeft);
    g.drawText ("Straight to", oscOutput->getBounds().withY (oscOutput->getY() - 18).withHeight (16), juce::Justification::centredLeft);
    const auto destTitle = [&] (juce::Rectangle<int> area, const juce::String& text)
    {
        auto a = inner (area);
        a.removeFromLeft (436);
        g.drawText (text, a.withHeight (16), juce::Justification::centredLeft);
    };
    destTitle (talkArea, "Into the Aux buses (and the main): high-passed at 100 Hz; never on when a mix loads");
    destTitle (oscArea, "Into buses, the main, or straight to an output; never on when a mix loads");
}

//==============================================================================
// Scenes page

RKMixEditor::ScenesPanel::ScenesPanel (RKMixEditor& o) : Panel (o)
{
    auto& s = o.mixer.getSettings();
    glide = make<rkfx::Knob> (s.param ("scene_glide"), "Glide");
    for (int ch = 0; ch < kChannels; ++ch)
        channelSafe[(size_t) ch] = make<rkfx::ToggleSwitch> (s.param (channelId (ch, "recallsafe")), rkfx::Palette::warning, num (ch));
    for (int bus = 0; bus < kBuses; ++bus)
        busSafe[(size_t) bus] = make<rkfx::ToggleSwitch> (s.param (busId (bus, "recallsafe")), rkfx::Palette::warning, "B" + num (bus));

    // The recall scope of one scene (the active one, unless another is picked).
    for (int i = 0; i < MixScenes::kNumScenes; ++i)
        scopeScene.addItem ("Scene " + juce::String (i + 1), i + 1);
    scopeScene.onChange = [this] { editScope (scopeScene.getSelectedId() - 1); };
    scopeScene.setTooltip ("The scene whose recall scope is shown");
    addAndMakeVisible (scopeScene);

    const auto toggle = [this] (juce::TextButton& b, const juce::String& tooltip)
    {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, owner.accent());
        b.setTooltip (tooltip);
        b.onClick = [this] { applyScope(); };
        addAndMakeVisible (b);
    };
    using Scope = MixScenes::Scope;
    const char* categoryTips[] = {
        "Channel, bus, main and mono faders and mutes",
        "Channel and bus pans, the main's balance",
        "Trim, polarity, delay, high-pass, gate, EQ and compressor of every channel",
        "The EQ, compressor and limiter of the buses, the main and the mono sum",
        "Inserts on or off, and the scene-controlled insert knobs",
        "Group assigns and Main switches",
    };
    for (int c = 0; c < Scope::numCategories; ++c)
    {
        categoryButtons[(size_t) c].setButtonText (Scope::name ((Scope::Category) c));
        toggle (categoryButtons[(size_t) c], categoryTips[c]);
    }
    for (int bus = 0; bus < kBuses; ++bus)
        toggle (sendButtons[(size_t) bus], {});

    everything.setTooltip ("Recall everything (the default)");
    everything.onClick = [this]
    {
        owner.mixer.getScenes().setScope (scopeFor, {});
        owner.refreshScenes();
    };
    addAndMakeVisible (everything);
    copyToAll.setTooltip ("Give every scene this recall scope");
    copyToAll.onClick = [this]
    {
        auto& scenes = owner.mixer.getScenes();
        const auto scope = scenes.getScope (scopeFor);
        for (int i = 0; i < MixScenes::kNumScenes; ++i)
            scenes.setScope (i, scope);
        owner.refreshScenes();
    };
    addAndMakeVisible (copyToAll);
    refresh();
}

void RKMixEditor::ScenesPanel::editScope (int scene)
{
    scopeFor = juce::jlimit (0, MixScenes::kNumScenes - 1, scene);
    refresh();
}

void RKMixEditor::ScenesPanel::applyScope()
{
    MixScenes::Scope scope;
    for (int c = 0; c < MixScenes::Scope::numCategories; ++c)
        scope.set ((MixScenes::Scope::Category) c, categoryButtons[(size_t) c].getToggleState());
    for (int bus = 0; bus < kBuses; ++bus)
        scope.setSendsTo (bus, sendButtons[(size_t) bus].getToggleState());
    owner.mixer.getScenes().setScope (scopeFor, scope);
    owner.refreshScenes();
}

void RKMixEditor::ScenesPanel::refresh()
{
    // A recall shows the scope of the scene now active.
    const auto current = owner.mixer.getScenes().getCurrent();
    if (current != lastCurrent)
    {
        lastCurrent = current;
        scopeFor = current;
    }
    scopeScene.setSelectedId (scopeFor + 1, juce::dontSendNotification);

    const auto scope = owner.mixer.getScenes().getScope (scopeFor);
    for (int c = 0; c < MixScenes::Scope::numCategories; ++c)
        categoryButtons[(size_t) c].setToggleState (scope.has ((MixScenes::Scope::Category) c), juce::dontSendNotification);
    for (int bus = 0; bus < kBuses; ++bus)
    {
        auto& b = sendButtons[(size_t) bus];
        b.setToggleState (scope.sendsTo (bus), juce::dontSendNotification);
        b.setButtonText ("B" + num (bus));
        b.setTooltip ("Send levels (and Own send pans) to " + owner.busDisplayName (bus) + ", from the channels and buses");
        b.setEnabled (owner.mixer.getBusType (bus) != BusType::group);   // nothing sends to a group
    }
    everything.setEnabled (! scope.isEverything());
    repaint();
}

void RKMixEditor::ScenesPanel::resized()
{
    auto r = getLocalBounds();
    glideArea = r.removeFromLeft (220);
    r.removeFromLeft (kGap);
    safeArea = r.removeFromTop (3 * (kSwitchH + 6) + 28 + 44);
    r.removeFromTop (kGap);
    scopeArea = r;

    glide->setBounds (inner (glideArea).removeFromTop (kKnobH).withWidth (kKnobW));

    auto a = inner (safeArea).withTrimmedTop (20);
    const auto w = (a.getWidth() - 15 * 4) / 16;
    for (int ch = 0; ch < kChannels; ++ch)
        channelSafe[(size_t) ch]->setBounds (a.getX() + (ch % 16) * (w + 4), a.getY() + (ch / 16) * (kSwitchH + 6), w, kSwitchH);
    const auto busTop = a.getY() + 2 * (kSwitchH + 6) + 28;
    for (int bus = 0; bus < kBuses; ++bus)
        busSafe[(size_t) bus]->setBounds (a.getX() + bus * (w + 4), busTop, w, kSwitchH);

    // Recall scope: the scene, then the kinds of values, then the sends by bus.
    auto s = inner (scopeArea);
    auto top = s.removeFromTop (kSwitchH);
    scopeScene.setBounds (top.removeFromLeft (110));
    top.removeFromLeft (8);
    everything.setBounds (top.removeFromLeft (100));
    copyToAll.setBounds (top.removeFromRight (150));
    s.removeFromTop (10);
    auto row = s.removeFromTop (kSwitchH);
    const auto cw = (row.getWidth() - (MixScenes::Scope::numCategories - 1) * 4) / MixScenes::Scope::numCategories;
    for (auto& b : categoryButtons)
    {
        b.setBounds (row.removeFromLeft (cw));
        row.removeFromLeft (4);
    }
    s.removeFromTop (24);
    row = s.removeFromTop (kSwitchH);
    for (int bus = 0; bus < kBuses; ++bus)
        sendButtons[(size_t) bus].setBounds (row.getX() + bus * (w + 4), row.getY(), w, kSwitchH);
}

void RKMixEditor::ScenesPanel::paint (juce::Graphics& g)
{
    rkfx::drawPanel (g, glideArea, owner.panelColour(), "Scenes");
    rkfx::drawPanel (g, safeArea, owner.panelColour(), "Recall safe: a scene leaves these channels and buses alone");

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    auto text = inner (glideArea).withTrimmedTop (kKnobH + 8);
    g.drawFittedText ("16 scenes, in the bottom row: click to recall, right-click to copy. A scene holds the mix's values: "
                      "faders, mutes, pans, sends, the strips' and buses' settings, inserts on or off, and the insert knobs "
                      "made scene controlled (right-click one). Never the structure: links, bus types, outputs, tap points, "
                      "which insert is where. Edits made while a scene is active go into it. Continuous values glide; "
                      "switches change at once.",
                      text, juce::Justification::topLeft, 12);

    auto a = inner (safeArea);
    g.drawText ("Channels", a.withHeight (18), juce::Justification::centredLeft);
    g.drawText ("Buses", a.withY (busSafe[0]->getY() - 20).withHeight (18), juce::Justification::centredLeft);

    rkfx::drawPanel (g, scopeArea, owner.panelColour(), "Recall scope: what recalling the scene changes (it always stores everything)");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    const auto scope = owner.mixer.getScenes().getScope (scopeFor);
    g.drawFittedText (scope.describe ([this] (int bus) { return owner.busDisplayName (bus); }),
                      everything.getBounds().withX (everything.getRight() + 12).withRight (copyToAll.getX() - 12),
                      juce::Justification::centredLeft, 1);
    g.drawText ("Sends to", sendButtons[0].getBounds().withY (sendButtons[0].getY() - 20).withHeight (18).withWidth (200),
                juce::Justification::centredLeft);
}

//==============================================================================
// Assign page

RKMixEditor::AssignPanel::AssignPanel (RKMixEditor& o) : Panel (o)
{
    for (int a = 0; a < MixAssigns::kNumAssigns; ++a)
    {
        auto row = std::make_unique<Row>();
        for (auto* slider : { &row->min, &row->max })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider->setRange (0.0, 100.0, 1.0);
            slider->setPopupDisplayEnabled (true, true, nullptr);
            slider->setTextValueSuffix (" %");
            slider->onValueChange = [this, a] { applyRange (a); };
            addAndMakeVisible (*slider);
        }
        row->min.setTooltip ("The control's value at the Assign's minimum");
        row->max.setTooltip ("The control's value at the Assign's maximum");
        row->invert.onClick = [this, a] { applyRange (a); };
        row->clear.setTooltip ("Remove this assignment");
        row->clear.onClick = [this, a] { owner.mixer.getAssigns().clear (a); refresh(); };
        row->learn.setTooltip ("MIDI learn: a controller (an expression pedal) for this Assign");
        row->learn.onClick = [this, a] { owner.showLearnMenu (MixLearn::firstAssign + a, rows[(size_t) a]->learn); };
        addAndMakeVisible (row->invert);
        addAndMakeVisible (row->clear);
        addChildComponent (row->learn);
        rows[(size_t) a] = std::move (row);
    }
}

void RKMixEditor::AssignPanel::applyRange (int a)
{
    const auto& row = *rows[(size_t) a];
    owner.mixer.getAssigns().setRange (a, (float) row.min.getValue() * 0.01f, (float) row.max.getValue() * 0.01f,
                                      row.invert.getToggleState());
}

void RKMixEditor::AssignPanel::refresh()
{
    auto& assigns = owner.mixer.getAssigns();
    for (int a = 0; a < MixAssigns::kNumAssigns; ++a)
    {
        auto& row = *rows[(size_t) a];
        const auto t = assigns.get (a);
        const auto used = t.kind != MixAssigns::Target::Kind::none;
        row.min.setValue (t.min * 100.0, juce::dontSendNotification);
        row.max.setValue (t.max * 100.0, juce::dontSendNotification);
        row.invert.setToggleState (t.invert, juce::dontSendNotification);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &row.min, &row.max, &row.invert, &row.clear })
            c->setVisible (used);

        // Standalone: the controller learned to this Assign.
        if (auto* map = owner.mixer.getMidiMap())
        {
            const auto mapping = map->find (MixLearn::firstAssign + a);
            row.learn.setButtonText (owner.mixer.getLearningTarget() == MixLearn::firstAssign + a ? juce::String ("...")
                                     : mapping.has_value() ? "CC " + juce::String (mapping->cc) : juce::String ("MIDI"));
            row.learn.setVisible (true);
        }
    }
    repaint();
}

void RKMixEditor::AssignPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (22);
    const auto columnWidth = (r.getWidth() - kGap) / 2;
    const auto rowHeight = r.getHeight() / 16;
    for (int a = 0; a < MixAssigns::kNumAssigns; ++a)
    {
        auto& row = *rows[(size_t) a];
        row.area = juce::Rectangle<int> (r.getX() + (a / 16) * (columnWidth + kGap), r.getY() + (a % 16) * rowHeight, columnWidth, rowHeight);
        auto line = row.area.reduced (4, 1);
        if (owner.mixer.isStandalone())
        {
            row.learn.setBounds (line.removeFromRight (48));
            line.removeFromRight (4);
        }
        row.clear.setBounds (line.removeFromRight (22));
        line.removeFromRight (4);
        row.invert.setBounds (line.removeFromRight (50));
        line.removeFromRight (4);
        row.max.setBounds (line.removeFromRight (70));
        row.min.setBounds (line.removeFromRight (70));
    }
}

void RKMixEditor::AssignPanel::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawText ("Assign 1-32 are host parameters. Right-click any control (or an insert's) to assign it. "
                "Min and max: the control's range; Inv: upside down (expression pedals).",
                getLocalBounds().removeFromTop (20).reduced (6, 0), juce::Justification::centredLeft);

    auto& assigns = owner.mixer.getAssigns();
    for (int a = 0; a < MixAssigns::kNumAssigns; ++a)
    {
        const auto& row = *rows[(size_t) a];
        if (a % 2 == 0)
        {
            g.setColour (owner.panelColour());
            g.fillRect (row.area);
        }
        auto text = row.area.reduced (6, 0);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (11.5f));
        g.drawText (juce::String (a + 1), text.removeFromLeft (24), juce::Justification::centredLeft);
        const auto what = assigns.describe (a);
        g.setColour (what.isNotEmpty() ? rkfx::Palette::text : rkfx::Palette::knobTrack);
        g.setFont (rkfx::Fonts::regular (11.5f));
        g.drawFittedText (what.isNotEmpty() ? what : juce::String ("-"), text.withRight (row.min.isVisible() ? row.min.getX() - 6 : text.getRight()),
                          juce::Justification::centredLeft, 1);
    }
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

    midiThru = std::make_unique<rkfx::ToggleSwitch> (s.param ("midi_thru"), o.accent(), "MIDI thru");
    addAndMakeVisible (*midiThru);

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
    for (int ch = 0; ch < kChannels; ++ch)
        if (! (ch % 2 == 1 && owner.isLinked (ch / 2)))
            add (s.choice (channelId (ch, "direct_out")), owner.displayName (ch) + " direct");

    for (int out = 0; out < kChannels; ++out)
        if (! outputNames[(size_t) out].isBeingEdited())
            outputNames[(size_t) out].setText (owner.mixer.getOutputName (out), juce::dontSendNotification);
    repaint();
}

void RKMixEditor::RoutingPanel::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (290);
    r.removeFromLeft (kGap);
    outputsArea = r;
    mainArea = left.removeFromTop (180);
    left.removeFromTop (kGap);
    midiArea = left;

    auto a = inner (mainArea).reduced (4, 4);
    auto row = a.removeFromTop (26);
    row.removeFromLeft (100);
    mainOutput->setBounds (row);
    a.removeFromTop (8);
    row = a.removeFromTop (26);
    row.removeFromLeft (100);
    monoOutput->setBounds (row);
    a.removeFromTop (4);
    balance->setBounds (a.removeFromTop (kKnobH).withWidth (kKnobW).withX (a.getX() + 100));

    midiThru->setBounds (inner (midiArea).reduced (4, 2).removeFromTop (kSwitchH).removeFromRight (90));

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

    // MIDI out: the inserts that send MIDI, and what clashes.
    rkfx::drawPanel (g, midiArea, owner.panelColour(), "MIDI out");
    auto m = inner (midiArea).reduced (4, 2);
    m.removeFromTop (kSwitchH + 6);
    auto& rack = owner.mixer.getRack();
    const auto slots = rack.midiSlots();
    g.setFont (rkfx::Fonts::regular (11.5f));
    if (slots.empty())
    {
        g.setColour (rkfx::Palette::textDim);
        g.drawFittedText ("RKTrigger and RKFollow inserts send MIDI out of RKMix. None yet.", m, juce::Justification::topLeft, 3);
    }
    for (const auto& slot : slots)
    {
        if (m.getHeight() < 14)
            break;
        g.setColour (rack.isInConflict (slot) ? rkfx::Palette::warning : rkfx::Palette::text);
        g.drawFittedText (owner.slotName (slot) + ": " + rack.footprintOf (slot).describe(), m.removeFromTop (16),
                          juce::Justification::centredLeft, 1);
    }
    for (const auto& c : rack.getConflicts())
    {
        if (m.getHeight() < 14)
            break;
        g.setColour (rkfx::Palette::warning);
        g.drawFittedText ("Clash: " + c.where.joinIntoString (", "), m.removeFromTop (16), juce::Justification::centredLeft, 1);
    }

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
