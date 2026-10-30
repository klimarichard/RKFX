#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kMasterWidth = 200;
    constexpr int kTopHeight = 400;
    constexpr int kTabHeight = 28;
    constexpr int kStripsPerLayer = 16;

    // Strip rows, top to bottom
    constexpr int kColourBar = 5;
    constexpr int kStripNameHeight = 22;
    constexpr int kStripNumberHeight = 14;
    constexpr int kSectionRowHeight = 14;
    constexpr int kPanHeight = 18;
    constexpr int kButtonHeight = 22;
    constexpr int kStripHeadHeight = kColourBar + 2 + kStripNameHeight + kStripNumberHeight + kSectionRowHeight + 4 + kPanHeight + 4;
    constexpr int kMeterWidth = 9;

    // Channel page
    constexpr int kKnobW = 58, kKnobH = 80;
    constexpr int kSectionTitle = 22;
    constexpr int kSwitchW = 46, kSwitchH = 24;
    constexpr int kPanelHeader = 30;
    constexpr int kRowA = 116;
    constexpr float kEqRangeDb = 18.0f;

    using namespace mix;

    void styleName (juce::Label& label)
    {
        label.setEditable (false, true, false);   // double-click to rename
        label.setFont (rkfx::Fonts::bold (12.0f));
        label.setJustificationType (juce::Justification::centred);
        label.setMinimumHorizontalScale (0.6f);
        label.setColour (juce::Label::textColourId, rkfx::Palette::text);
        label.setColour (juce::Label::textWhenEditingColourId, rkfx::Palette::text);
        label.setTooltip ("Click to select, double-click to rename, right-click for its colour");
    }

    juce::String keyName (int value)
    {
        return value == 0 ? juce::String ("Self") : "Key: In " + juce::String (value);
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
RKMixEditor::Strip::Strip (RKMixEditor& o, int ch)
    : owner (o), channel (ch),
      fader (*o.mixer.findParam (channelFaderId (ch))),
      meter (o.accent())
{
    styleName (name);
    name.onTextChange = [this] { owner.mixer.setChannelName (channel, name.getText()); };
    name.addMouseListener (this, false);
    addAndMakeVisible (name);

    auto& panParam = o.mixer.getSettings().param (channelId (ch, "pan"));
    pan.setSliderStyle (juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan.setColour (juce::Slider::trackColourId, o.accent().withAlpha (0.5f));
    panAttachment = std::make_unique<juce::SliderParameterAttachment> (panParam, pan);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setPopupDisplayEnabled (true, true, nullptr);
    pan.setTooltip ("Pan (double-click: centre)");
    rkfx::tagParameter (pan, panParam.getParameterID());
    addAndMakeVisible (pan);

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

    const auto& s = owner.mixer.getSettings();
    sectionsOn = { s.flag (channelId (channel, "hpf_on")), s.flag (channelId (channel, "gate_on")),
                   s.flag (channelId (channel, "eq_on")), s.flag (channelId (channel, "comp_on")) };
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
    r.removeFromTop (4);
    pan.setBounds (r.removeFromTop (kPanHeight).reduced (2, 0));
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
    r.removeFromTop (kStripHeadHeight);
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

rkfx::Knob* RKMixEditor::ChannelPanel::knob (int section, const juce::String& suffix, const juce::String& label)
{
    auto* k = make<rkfx::Knob> (owner.mixer.getSettings().param (channelId (channel, suffix.toRawUTF8())), label);
    sections[(size_t) section].knobs.push_back (k);
    return k;
}

RKMixEditor::ChannelPanel::ChannelPanel (RKMixEditor& o, int ch) : owner (o), channel (ch)
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

    styleName (name);
    name.setFont (rkfx::Fonts::bold (16.0f));
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
    for (int i = MixGui::kNumStripColours - 1; i >= 0; --i)
    {
        swatches[(size_t) i] = header.removeFromRight (20).reduced (2, 3);
        header.removeFromRight (2);
    }
    header.removeFromRight (12);
    name.setBounds (header.withWidth (juce::jmin (header.getWidth(), 300)));
    r.removeFromTop (4);

    const auto place = [] (juce::Rectangle<int>& row, int width) { auto a = row.removeFromLeft (width); row.removeFromLeft (kGap); return a; };
    const auto inner = [] (juce::Rectangle<int> a) { return a.withTrimmedTop (kSectionTitle).reduced (8, 2); };
    const auto switchIn = [] (juce::Rectangle<int>& a, int width = kSwitchW)
    {
        return a.removeFromLeft (width).withHeight (kKnobH).withSizeKeepingCentre (width, kSwitchH);
    };

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
        a.removeFromLeft (4);
        compMeter->setBounds (a.removeFromLeft (26).withTrimmedBottom (14));
        for (int row = 0; row < 2; ++row)
        {
            auto line = knobs.removeFromTop (kKnobH);
            for (int k = 0; k < 3; ++k)
                sections[comp].knobs[(size_t) (3 * row + k)]->setBounds (line.removeFromLeft (kKnobW).withHeight (kKnobH));
        }
        a.removeFromLeft (6);
        const auto side = juce::jmin (a.getWidth(), a.getHeight());
        compCurve->setBounds (a.withSizeKeepingCentre (side, side));
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

    for (const auto& section : sections)
        rkfx::drawPanel (g, section.area, owner.panelColour(), section.title);
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
RKMixEditor::RoutingPanel::RoutingPanel (RKMixEditor& o) : owner (o)
{
    auto& s = o.mixer.getSettings();
    mainOutput = std::make_unique<ChoiceBox> (s.param ("main_out"), targetName, kFirstPairTarget);
    monoOutput = std::make_unique<ChoiceBox> (s.param ("mono_out"), targetName, kFirstPairTarget);
    addAndMakeVisible (*mainOutput);
    addAndMakeVisible (*monoOutput);

    balance = std::make_unique<rkfx::Knob> (s.param ("main_bal"), "Balance");
    addAndMakeVisible (*balance);
}

void RKMixEditor::RoutingPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 30);
    auto row = r.removeFromTop (28);
    row.removeFromLeft (110);
    mainOutput->setBounds (row.removeFromLeft (140));
    r.removeFromTop (10);
    row = r.removeFromTop (28);
    row.removeFromLeft (110);
    monoOutput->setBounds (row.removeFromLeft (140));
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
        addToCanvas (*tab);
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
