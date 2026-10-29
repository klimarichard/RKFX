#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kInputsWidth = 330;
    constexpr int kRowHeight = 30;
    constexpr int kChannels = RemapEngine::kChannels;
    constexpr float kPresentDb = -60.0f, kStrongDb = -20.0f;

    void styleName (juce::Label& label, const juce::String& placeholder)
    {
        label.setEditable (false, true, false);   // double-click to edit
        label.setFont (rkfx::Fonts::regular (13.0f));
        label.setColour (juce::Label::textColourId, rkfx::Palette::text);
        label.setColour (juce::Label::textWhenEditingColourId, rkfx::Palette::text);
        label.setTooltip ("Double-click to name it");
        label.setText ({}, juce::dontSendNotification);
        label.getProperties().set ("placeholder", placeholder);
    }
}

//==============================================================================
void RKRemapEditor::Light::setPeak (float peak)
{
    const auto db = juce::Decibels::gainToDecibels (peak, -120.0f);
    const auto next = db >= kStrongDb ? 2 : db >= kPresentDb ? 1 : 0;
    if (next != level)
    {
        level = next;
        repaint();
    }
}

void RKRemapEditor::Light::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (10.0f, 10.0f);
    g.setColour (level == 2 ? accent : level == 1 ? accent.withAlpha (0.4f) : rkfx::Palette::knobBody);
    g.fillEllipse (r);
}

//==============================================================================
RKRemapEditor::InputRow::InputRow (RKRemapEditor& o, int ch) : owner (o), channel (ch), light (o.accent())
{
    styleName (name, "Input " + juce::String (ch + 1));
    name.setText (o.remap.getInputName (ch), juce::dontSendNotification);
    name.onTextChange = [this] { owner.remap.setInputName (channel, name.getText()); };
    addAndMakeVisible (name);
    addAndMakeVisible (light);

    if (ch % 2 == 0)
    {
        link = std::make_unique<rkfx::ToggleSwitch> (*o.remap.findParam (RemapParams::inputLinkId (ch / 2)), o.accent(), "Link");
        addAndMakeVisible (*link);
    }
    else
    {
        pairMode = std::make_unique<rkfx::SegmentedSwitch> (*o.remap.findParam (RemapParams::inputMidSideId (ch / 2)), o.accent());
        addChildComponent (*pairMode);
    }
}

void RKRemapEditor::InputRow::updateLinked()
{
    follower = channel % 2 == 1 && owner.settings.inputLinks[(size_t) (channel / 2)];
    name.setVisible (! follower);
    if (pairMode != nullptr)
        pairMode->setVisible (follower);
    repaint();
}

void RKRemapEditor::InputRow::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    r.removeFromLeft (30);
    if (link != nullptr)
        link->setBounds (r.removeFromRight (48));
    else
        r.removeFromRight (48);
    r.removeFromRight (6);
    light.setBounds (r.removeFromRight (16));
    r.removeFromRight (6);
    name.setBounds (r);
    if (pairMode != nullptr)
        pairMode->setBounds (r.withWidth (juce::jmin (r.getWidth(), 120)));
}

void RKRemapEditor::InputRow::paint (juce::Graphics& g)
{
    const auto linked = owner.settings.inputLinks[(size_t) (channel / 2)];
    if (linked)
    {
        g.setColour (owner.accent().withAlpha (0.08f));
        g.fillRect (getLocalBounds());
    }
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (12.0f));
    g.drawText (juce::String (channel + 1), 4, 0, 30, getHeight(), juce::Justification::centredLeft);
    if (! follower && name.getText().isEmpty() && ! name.isBeingEdited())
    {
        g.setFont (rkfx::Fonts::regular (13.0f));
        g.drawText (name.getProperties()["placeholder"].toString(), name.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft);
    }
}

//==============================================================================
RKRemapEditor::OutputRow::OutputRow (RKRemapEditor& o, int ch) : owner (o), channel (ch), light (o.accent())
{
    using RemapParams::outputId;
    auto& p = o.remap;

    styleName (name, "Output " + juce::String (ch + 1));
    name.setText (p.getOutputName (ch), juce::dontSendNotification);
    name.onTextChange = [this] { owner.remap.setOutputName (channel, name.getText()); };

    // Source: item IDs are the parameter values + 1; only linked pairs are offered.
    auto& sourceParam = *p.findParam (outputId (ch, "source"));
    sourceAttachment = std::make_unique<juce::ParameterAttachment> (sourceParam, [this] (float value)
    {
        source.setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification);
    });
    source.onChange = [this]
    {
        if (source.getSelectedId() > 0)
            sourceAttachment->setValueAsCompleteGesture ((float) (source.getSelectedId() - 1));
    };

    trim.setSliderStyle (juce::Slider::LinearHorizontal);
    trim.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    trimAttachment = std::make_unique<juce::SliderParameterAttachment> (*p.findParam (outputId (ch, "trim")), trim);
    trim.setDoubleClickReturnValue (true, 0.0);

    mute = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (outputId (ch, "mute")), rkfx::Palette::warning, "M");
    polarity = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (outputId (ch, "polarity")), o.accent(),
                                                    juce::String (juce::CharPointer_UTF8 ("\xc3\x98")));
    mono = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (outputId (ch, "mono")), o.accent(), "Mono");
    if (ch % 2 == 0)
        link = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (RemapParams::outputLinkId (ch / 2)), o.accent(), "Link");

    monoNote.setText ("Mono", juce::dontSendNotification);
    monoNote.setFont (rkfx::Fonts::regular (12.0f));
    monoNote.setJustificationType (juce::Justification::centred);
    monoNote.setColour (juce::Label::textColourId, rkfx::Palette::textDim);
    monoNote.setTooltip ("A stereo pair on a single output is always summed to mono");
    addChildComponent (monoNote);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &name, &source, &trim, mute.get(), polarity.get(), mono.get(), &light })
        addAndMakeVisible (*c);
    if (link != nullptr)
        addAndMakeVisible (*link);

    refreshSources();
    sourceAttachment->sendInitialUpdate();
}

void RKRemapEditor::OutputRow::refreshSources()
{
    const auto selected = juce::roundToInt (rkfx::ProcessorBase::valueOf (owner.remap.findParam (RemapParams::outputId (channel, "source"))));
    source.clear (juce::dontSendNotification);
    source.addItem ("Off", 1);
    for (int v = 1; v <= kChannels; ++v)
        source.addItem (owner.sourceText (v), v + 1);
    source.addSeparator();
    for (int pair = 0; pair < RemapEngine::kPairs; ++pair)
    {
        const auto v = RemapEngine::kFirstPairSource + pair;
        if (owner.settings.inputLinks[(size_t) pair] || v == selected)
            source.addItem (owner.sourceText (v), v + 1);
    }
    source.setSelectedId (selected + 1, juce::dontSendNotification);
}

void RKRemapEditor::OutputRow::updateLinked()
{
    // The second output of a linked pair: no controls and no name of its own (the pair has one).
    follower = channel % 2 == 1 && owner.settings.outputLinks[(size_t) (channel / 2)];
    for (juce::Component* c : std::initializer_list<juce::Component*> { &name, &source, &trim, mute.get(), polarity.get() })
        c->setVisible (! follower);

    // Mono: on the left output of a linked pair whose source is a linked input pair.
    const auto& o = owner.settings.outputs[(size_t) channel];
    const auto pairSource = o.source >= RemapEngine::kFirstPairSource
                         && owner.settings.inputLinks[(size_t) juce::jlimit (0, RemapEngine::kPairs - 1, o.source - RemapEngine::kFirstPairSource)];
    const auto linkedOutput = owner.settings.outputLinks[(size_t) (channel / 2)];
    mono->setVisible (! follower && channel % 2 == 0 && linkedOutput && pairSource);

    // A single output fed by a stereo pair sums it to mono: shown, not switchable.
    monoNote.setVisible (! linkedOutput && pairSource);
    repaint();
}

void RKRemapEditor::OutputRow::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    r.removeFromLeft (34);
    if (link != nullptr)
        link->setBounds (r.removeFromRight (48));
    else
        r.removeFromRight (48);
    r.removeFromRight (6);
    light.setBounds (r.removeFromRight (16));
    r.removeFromRight (6);
    mono->setBounds (r.removeFromRight (48));
    monoNote.setBounds (mono->getBounds());
    r.removeFromRight (4);
    polarity->setBounds (r.removeFromRight (28));
    r.removeFromRight (4);
    mute->setBounds (r.removeFromRight (28));
    r.removeFromRight (6);
    trim.setBounds (r.removeFromRight (120));
    r.removeFromRight (6);
    source.setBounds (r.removeFromRight (170));
    r.removeFromRight (6);
    name.setBounds (r);
}

void RKRemapEditor::OutputRow::paint (juce::Graphics& g)
{
    const auto linked = owner.settings.outputLinks[(size_t) (channel / 2)];
    if (linked)
    {
        g.setColour (owner.accent().withAlpha (0.08f));
        g.fillRect (getLocalBounds());
    }
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (12.0f));
    g.drawText (juce::String (channel + 1), 4, 0, 34, getHeight(), juce::Justification::centredLeft);
    g.setFont (rkfx::Fonts::regular (13.0f));
    if (! follower && name.getText().isEmpty() && ! name.isBeingEdited())
        g.drawText (name.getProperties()["placeholder"].toString(), name.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft);
    if (follower)
        g.drawText ("linked with " + juce::String (channel), source.getBounds().getUnion (polarity->getBounds()),
                    juce::Justification::centredLeft);
}

//==============================================================================
void RKRemapEditor::List::resized()
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        rows[i]->setVisible ((int) i < shown);
        rows[i]->setBounds (0, (int) i * rowHeight, getWidth(), rowHeight);
    }
}

//==============================================================================
RKRemapEditor::RKRemapEditor (RKRemapProcessor& p)
    : rkfx::EditorBase (p, 1060, 720),
      remap (p),
      settings (p.currentSettings()),
      watcher (p, [this] { onParameterChange(); })
{
    for (int ch = 0; ch < kChannels; ++ch)
    {
        inputRows.push_back (std::make_unique<InputRow> (*this, ch));
        inputList.addAndMakeVisible (*inputRows.back());
        inputList.rows.push_back (inputRows.back().get());

        outputRows.push_back (std::make_unique<OutputRow> (*this, ch));
        outputList.addAndMakeVisible (*outputRows.back());
        outputList.rows.push_back (outputRows.back().get());
    }

    for (auto* v : { &inputViewport, &outputViewport })
    {
        v->setScrollBarsShown (true, false);
        addToCanvas (*v);
    }
    inputViewport.setViewedComponent (&inputList, false);
    outputViewport.setViewedComponent (&outputList, false);

    for (auto* box : { &inputsShown, &outputsShown })
    {
        for (int n = 1; n <= kChannels; ++n)
            box->addItem (juce::String (n), n);
        addToCanvas (*box);
    }
    inputsShown.onChange = [this] { remap.setInputsShown (inputsShown.getSelectedId()); };
    outputsShown.onChange = [this] { remap.setOutputsShown (outputsShown.getSelectedId()); };

    for (auto* l : { &inputsShownLabel, &outputsShownLabel, &hiddenNote })
    {
        l->setFont (rkfx::Fonts::regular (12.0f));
        l->setColour (juce::Label::textColourId, rkfx::Palette::textDim);
        addToCanvas (*l);
    }
    inputsShownLabel.setText ("Inputs shown", juce::dontSendNotification);
    outputsShownLabel.setText ("Outputs shown", juce::dontSendNotification);
    hiddenNote.setColour (juce::Label::textColourId, rkfx::Palette::warning);

    onParameterChange();
    refreshNamesAndViews();
    finishSetup();
    ++remap.windowsOpen;
    lightTimer.startTimerHz (15);
}

RKRemapEditor::~RKRemapEditor()
{
    --remap.windowsOpen;
    inputViewport.setViewedComponent (nullptr, false);
    outputViewport.setViewedComponent (nullptr, false);
}

juce::String RKRemapEditor::sourceText (int value) const
{
    auto text = RemapParams::sourceName (value);
    if (value <= RemapEngine::kSourceOff)
        return text;

    if (value > kChannels)
    {
        // A pair: its one name.
        if (const auto name = remap.getInputName ((value - RemapEngine::kFirstPairSource) * 2); name.isNotEmpty())
            text << "  " << name;
        return text;
    }

    // One input: in a linked pair, the pair's name and which side ("Keys L", "Keys Mid").
    const auto input = value - 1, pair = input / 2;
    if (settings.inputLinks[(size_t) pair])
    {
        const auto first = input % 2 == 0;
        const auto side = settings.inputMidSide[(size_t) pair] ? (first ? "Mid" : "Side") : (first ? "L" : "R");
        const auto name = remap.getInputName (pair * 2);
        text << "  " << (name.isNotEmpty() ? name + " " : juce::String()) << side;
        return text;
    }

    if (const auto name = remap.getInputName (input); name.isNotEmpty())
        text << "  " << name;
    return text;
}

void RKRemapEditor::onParameterChange()
{
    const auto before = settings;
    settings = remap.currentSettings();

    const auto linksChanged = before.inputLinks != settings.inputLinks || before.inputMidSide != settings.inputMidSide;
    for (auto& row : outputRows)
    {
        if (linksChanged)
            row->refreshSources();
        row->updateLinked();
    }
    for (auto& row : inputRows)
        row->updateLinked();
    updateHiddenNote();
}


void RKRemapEditor::refreshNamesAndViews()
{
    lastNamesVersion = remap.getNamesVersion();

    for (int ch = 0; ch < kChannels; ++ch)
    {
        if (! inputRows[(size_t) ch]->name.isBeingEdited())
            inputRows[(size_t) ch]->name.setText (remap.getInputName (ch), juce::dontSendNotification);
        if (! outputRows[(size_t) ch]->name.isBeingEdited())
            outputRows[(size_t) ch]->name.setText (remap.getOutputName (ch), juce::dontSendNotification);
    }
    for (auto& row : outputRows)
        row->refreshSources();

    inputsShown.setSelectedId (remap.getInputsShown(), juce::dontSendNotification);
    outputsShown.setSelectedId (remap.getOutputsShown(), juce::dontSendNotification);
    inputList.shown = remap.getInputsShown();
    outputList.shown = remap.getOutputsShown();
    inputList.setSize (inputViewport.getMaximumVisibleWidth(), inputList.shown * kRowHeight);
    outputList.setSize (outputViewport.getMaximumVisibleWidth(), outputList.shown * kRowHeight);
    inputList.resized();
    outputList.resized();
    updateHiddenNote();
    repaint();
}

void RKRemapEditor::updateHiddenNote()
{
    // Hidden outputs carrying an input you can see: never unnoticed. (Hidden
    // outputs fed by hidden inputs, such as the straight-through default
    // beyond the shown channels, aren't counted.)
    const auto inputs = remap.getInputsShown();
    int routed = 0;
    for (int k = remap.getOutputsShown(); k < kChannels; ++k)
    {
        const auto route = RemapEngine::routeOf (settings, k);
        const auto shownSource = (route.a >= 0 && route.a < inputs) || (route.b >= 0 && route.b < inputs);
        if (shownSource && ! settings.outputs[(size_t) RemapEngine::leaderOf (settings, k)].mute)
            ++routed;
    }
    hiddenNote.setText (routed == 0 ? juce::String()
                                    : juce::String (routed) + (routed == 1 ? " hidden output carries a shown input"
                                                                           : " hidden outputs carry shown inputs"),
                        juce::dontSendNotification);
}

void RKRemapEditor::updateLights()
{
    if (remap.getNamesVersion() != lastNamesVersion)
        refreshNamesAndViews();

    for (int ch = 0; ch < kChannels; ++ch)
    {
        const auto in = remap.takeInputPeak (ch), out = remap.takeOutputPeak (ch);
        if (ch < inputList.shown)
            inputRows[(size_t) ch]->light.setPeak (in);
        if (ch < outputList.shown)
            outputRows[(size_t) ch]->light.setPeak (out);
    }
}

//==============================================================================
juce::Rectangle<int> RKRemapEditor::inputsArea() const  { return contentArea().withWidth (kInputsWidth); }
juce::Rectangle<int> RKRemapEditor::outputsArea() const { return contentArea().withTrimmedLeft (kInputsWidth + kGap); }

void RKRemapEditor::layoutContent()
{
    inputViewport.setBounds (inputsArea().reduced (6, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (6));
    outputViewport.setBounds (outputsArea().reduced (6, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (6));
    inputList.setSize (inputViewport.getMaximumVisibleWidth(), inputList.shown * kRowHeight);
    outputList.setSize (outputViewport.getMaximumVisibleWidth(), outputList.shown * kRowHeight);

    auto global = globalArea().withSizeKeepingCentre (globalArea().getWidth(), 28);
    inputsShownLabel.setBounds (global.removeFromLeft (90));
    inputsShown.setBounds (global.removeFromLeft (70));
    global.removeFromLeft (24);
    outputsShownLabel.setBounds (global.removeFromLeft (100));
    outputsShown.setBounds (global.removeFromLeft (70));
    global.removeFromLeft (24);
    hiddenNote.setBounds (global);
}

void RKRemapEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, inputsArea(), "Inputs");
    drawSection (g, outputsArea(), "Outputs: source, trim, mute, polarity");
}
