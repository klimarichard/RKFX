#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kDisplayHeight = 270;
    constexpr int kTabsHeight = 40;
    constexpr int kBandSectionWidth = 540;
    constexpr int kSwitchHeight = 26;
    constexpr int kTitledSwitchHeight = 44;

    BandDisplay::Parameters displayParameters (RKMultiCompProcessor& p)
    {
        BandDisplay::Parameters d;
        for (const auto& b : MultiCompParams::bands)
            d.thresholds[(size_t) b.band] = p.findParam (MultiCompParams::id (b, "threshold"));
        d.low = p.findParam ("xover_low");
        d.mid = p.findParam ("xover_mid");
        d.high = p.findParam ("xover_high");
        return d;
    }
}

//==============================================================================
void RKMultiCompEditor::BandTabs::setReduction (Band band, float reductionDb, float seconds)
{
    auto& s = shown[(size_t) band];
    const auto next = juce::jmax (-24.0f, juce::jmin (reductionDb, juce::jmin (0.0f, s + 20.0f * seconds)));
    if (std::abs (next - s) > 0.05f)
    {
        s = next;
        repaint();
    }
}

void RKMultiCompEditor::BandTabs::paint (juce::Graphics& g)
{
    const auto selected = owner.selectedBand();

    for (int i = 0; i < owner.numBands; ++i)
    {
        const auto band = owner.bandList[(size_t) i];
        const auto area = owner.tabArea (i).translated (-getX(), -getY()).toFloat();
        const auto isSelected = band == selected;

        g.setColour (owner.panelColour().brighter (isSelected ? 0.12f : 0.0f));
        g.fillRoundedRectangle (area, rkfx::Style::kPanelRadius);
        if (isSelected)
        {
            g.setColour (owner.accent().withAlpha (0.7f));
            g.drawRoundedRectangle (area.reduced (0.75f), rkfx::Style::kPanelRadius, 1.5f);
        }

        g.setColour (owner.settings.bands[(size_t) band].on ? rkfx::Palette::text : rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (13.0f));
        g.drawText (MultiCompParams::displayName (band, owner.settings.fourBands), area.reduced (10.0f, 0.0f),
                    juce::Justification::centredLeft);

        // Gain reduction: a slim bar along the bottom edge, 0 to 24 dB (the
        // value itself is in the display).
        const auto track = area.reduced (8.0f, 0.0f).removeFromBottom (5.0f).withHeight (3.0f);
        g.setColour (rkfx::Palette::background);
        g.fillRoundedRectangle (track, 1.5f);
        g.setColour (owner.accent());
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * juce::jlimit (0.0f, 1.0f, -shown[(size_t) band] / 24.0f)), 1.5f);
    }
}

void RKMultiCompEditor::BandTabs::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition() + getPosition();
    for (int i = 0; i < owner.numBands; ++i)
        if (owner.tabArea (i).contains (pos))
            owner.selectBand (owner.bandList[(size_t) i]);
}

//==============================================================================
RKMultiCompEditor::RKMultiCompEditor (RKMultiCompProcessor& p)
    : rkfx::EditorBase (p, 900, 640),
      comp (p),
      settings (p.currentSettings()),
      display ([this] { return comp.currentSettings(); }, displayParameters (p), accent(),
               [this] (Band b) { selectBand (b); }),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    using MultiCompParams::id;
    auto param = [&p] (const juce::String& paramId) -> juce::RangedAudioParameter& { return *p.findParam (paramId); };

    addToCanvas (display);
    addToCanvas (tabs);

    for (const auto& b : MultiCompParams::bands)
    {
        auto& c = bandControls[(size_t) b.band];
        c.threshold = std::make_unique<Knob> (param (id (b, "threshold")), "Threshold");
        c.ratio     = std::make_unique<Knob> (param (id (b, "ratio")), "Ratio");
        c.attack    = std::make_unique<Knob> (param (id (b, "attack")), "Attack");
        c.release   = std::make_unique<Knob> (param (id (b, "release")), "Release");
        c.gain      = std::make_unique<Knob> (param (id (b, "gain")), "Gain");
        c.on        = std::make_unique<rkfx::ToggleSwitch> (param (id (b, "on")), accent(), "On");
        c.solo      = std::make_unique<rkfx::ToggleSwitch> (param (id (b, "solo")), rkfx::Palette::warning, "Solo");

        for (juce::Component* k : std::initializer_list<juce::Component*> {
                 c.threshold.get(), c.ratio.get(), c.attack.get(), c.release.get(), c.gain.get(), c.on.get(), c.solo.get() })
            addToCanvas (*k);
    }

    xoverLow    = std::make_unique<Knob> (param ("xover_low"), "Low");
    xoverMid    = std::make_unique<Knob> (param ("xover_mid"), "Mid");
    xoverHigh   = std::make_unique<Knob> (param ("xover_high"), "High");
    offset      = std::make_unique<Knob> (param ("offset"), "Offset");
    knee        = std::make_unique<Knob> (param ("knee"), "Knee");
    link        = std::make_unique<Knob> (param ("link"), "Link");
    bands       = std::make_unique<rkfx::SegmentedSwitch> (param ("bands"), accent(), juce::StringArray {}, "Bands");
    detect      = std::make_unique<rkfx::SegmentedSwitch> (param ("detect"), accent(), juce::StringArray {}, "Detection");
    releaseAuto = std::make_unique<rkfx::ToggleSwitch> (param ("release_auto"), accent(), "Auto Release");
    gainAuto    = std::make_unique<rkfx::ToggleSwitch> (param ("gain_auto"), accent(), "Auto Gain");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             xoverLow.get(), xoverMid.get(), xoverHigh.get(), offset.get(), knee.get(), link.get(),
             bands.get(), detect.get(), releaseAuto.get(), gainAuto.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

RKMultiCompEditor::Band RKMultiCompEditor::selectedBand() const
{
    const auto band = (Band) juce::jlimit (0, MultiCompEngine::kNumBands - 1, comp.selectedBand.load());
    // High-Mid isn't there in 3-band mode: its place is taken by Mid.
    return band == MultiCompEngine::highMid && ! settings.fourBands ? MultiCompEngine::lowMid : band;
}

void RKMultiCompEditor::selectBand (Band band)
{
    comp.selectedBand = band;
    const auto shown = selectedBand();

    for (const auto& b : MultiCompParams::bands)
    {
        auto& c = bandControls[(size_t) b.band];
        for (auto* k : { c.threshold.get(), c.ratio.get(), c.attack.get(), c.release.get(), c.gain.get() })
            k->setVisible (b.band == shown);
    }

    display.setSelectedBand (shown);
    tabs.repaint();
    repaint (bandSection());
}

void RKMultiCompEditor::onParameterChange()
{
    const auto previousCount = numBands;
    settings = comp.currentSettings();
    numBands = MultiCompEngine::bandsInUse (settings, bandList);

    for (const auto& b : MultiCompParams::bands)
    {
        const auto& s = settings.bands[(size_t) b.band];
        auto& c = bandControls[(size_t) b.band];
        for (auto* k : { c.threshold.get(), c.ratio.get(), c.attack.get(), c.gain.get() })
            k->setDimmed (! s.on);
        c.release->setDimmed (! s.on || settings.autoRelease);
    }
    xoverMid->setDimmed (! settings.fourBands);

    display.refresh();
    tabs.repaint();

    if (numBands != previousCount)
        layoutContent();
    selectBand (selectedBand());
}

void RKMultiCompEditor::updateMeters()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;

    for (const auto& b : MultiCompParams::bands)
    {
        const auto m = comp.takeMeter (b.band);
        display.setMeter (b.band, m, elapsed);
        tabs.setReduction (b.band, m.reductionDb, elapsed);
    }
}

//==============================================================================
juce::Rectangle<int> RKMultiCompEditor::displayArea() const
{
    return contentArea().withHeight (kDisplayHeight);
}

juce::Rectangle<int> RKMultiCompEditor::tabsArea() const
{
    return contentArea().withTrimmedTop (kDisplayHeight + kGap).withHeight (kTabsHeight);
}

juce::Rectangle<int> RKMultiCompEditor::tabArea (int index) const
{
    const auto area = tabsArea();
    const auto width = (area.getWidth() - kGap * (numBands - 1)) / numBands;
    return { area.getX() + index * (width + kGap), area.getY(), width, area.getHeight() };
}

juce::Rectangle<int> RKMultiCompEditor::bandSection() const
{
    return contentArea().withTrimmedTop (kDisplayHeight + kTabsHeight + 2 * kGap).withWidth (kBandSectionWidth);
}

juce::Rectangle<int> RKMultiCompEditor::crossoverSection() const
{
    return contentArea().withTrimmedTop (kDisplayHeight + kTabsHeight + 2 * kGap).withTrimmedLeft (kBandSectionWidth + kGap);
}

void RKMultiCompEditor::layoutContent()
{
    using rkfx::Knob;

    display.setBounds (displayArea());
    tabs.setBounds (tabsArea());

    for (const auto& b : MultiCompParams::bands)
    {
        auto& c = bandControls[(size_t) b.band];
        int index = -1;
        for (int i = 0; i < numBands; ++i)
            if (bandList[(size_t) i] == b.band)
                index = i;

        c.on->setVisible (index >= 0);
        c.solo->setVisible (index >= 0);
        if (index >= 0)
        {
            auto switches = tabArea (index).reduced (8, 7).removeFromRight (96);
            c.solo->setBounds (switches.removeFromRight (46));
            switches.removeFromRight (4);
            c.on->setBounds (switches);
        }

        auto area = bandSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = row.getWidth() / 5;
        for (auto* k : { c.threshold.get(), c.ratio.get(), c.attack.get(), c.release.get(), c.gain.get() })
            k->setBounds (row.removeFromLeft (width));
    }

    {
        auto area = crossoverSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = row.getWidth() / 3;
        xoverLow->setBounds (row.removeFromLeft (width));
        xoverMid->setBounds (row.removeFromLeft (width));
        xoverHigh->setBounds (row);
    }

    auto global = globalArea();
    auto place = [&global] (juce::Component& c, int width, int height)
    {
        c.setBounds (global.removeFromLeft (width).withSizeKeepingCentre (width, height));
        global.removeFromLeft (6);
    };
    place (*bands, 64, kTitledSwitchHeight);
    place (*offset, Knob::kDefaultWidth, Knob::kDefaultHeight);
    place (*knee, Knob::kDefaultWidth, Knob::kDefaultHeight);
    place (*link, Knob::kDefaultWidth, Knob::kDefaultHeight);
    place (*detect, 100, kTitledSwitchHeight);

    auto toggles = global.removeFromLeft (110).withSizeKeepingCentre (110, 2 * kSwitchHeight + 8);
    releaseAuto->setBounds (toggles.removeFromTop (kSwitchHeight));
    gainAuto->setBounds (toggles.removeFromBottom (kSwitchHeight));
}

void RKMultiCompEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, bandSection(), MultiCompParams::displayName (selectedBand(), settings.fourBands) + " band");
    drawSection (g, crossoverSection(), "Crossovers");
}
