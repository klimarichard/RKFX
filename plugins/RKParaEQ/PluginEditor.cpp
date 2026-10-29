#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
    constexpr int kCurveHeight = 130;
    constexpr int kGap = 6;
    constexpr int kTitleHeight = 26;
    constexpr int kKnobHeight = 84;
    constexpr int kSwitchHeight = 24;
    constexpr int kDynamicsHeight = 130;
    constexpr float kCurveRangeDb = 18.0f;
    constexpr float kMeterFloorDb = -60.0f;

    double displaySampleRate (const juce::AudioProcessor& p)
    {
        return p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0;
    }

    /** Strip index (HP, the five gain bands, LP) of an engine band. */
    int stripOf (int band) { return band + 1; }
}

RKParaEQEditor::Strip::Strip (rkfx::ProcessorBase& p, const ParaEQParams::BandInfo& b, juce::Colour accent)
{
    using ParaEQParams::id;

    on = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (id (b, "on")), accent, "On");
    freq = std::make_unique<rkfx::Knob> (*p.findParam (id (b, "freq")), "Freq");

    if (b.shelfOrBell)
        type = std::make_unique<rkfx::SegmentedSwitch> (*p.findParam (id (b, "type")), accent);

    if (b.cut)
    {
        slope = std::make_unique<rkfx::SegmentedSwitch> (*p.findParam (id (b, "slope")), accent,
                                                         juce::StringArray { "12", "24", "36", "48" });
    }
    else
    {
        gain = std::make_unique<rkfx::Knob> (*p.findParam (id (b, "gain")), "Gain");
        q = std::make_unique<rkfx::Knob> (*p.findParam (id (b, "q")), "Q");
        dyn = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (id (b, "dyn")), accent, "Dynamic");
    }

    onWatcher = std::make_unique<juce::ParameterAttachment> (*p.findParam (id (b, "on")), [this] (float v)
    {
        isOn = v >= 0.5f;
        updateDimming();
    });

    if (b.shelfOrBell)
        typeWatcher = std::make_unique<juce::ParameterAttachment> (*p.findParam (id (b, "type")), [this] (float v)
        {
            isShelf = v < 0.5f;
            updateDimming();
        });
}

void RKParaEQEditor::Strip::updateDimming()
{
    freq->setDimmed (! isOn);
    if (gain != nullptr) gain->setDimmed (! isOn);
    if (q != nullptr)    q->setDimmed (! isOn || isShelf);
}

//==============================================================================
float RKParaEQEditor::LiveCurve::xFor (double hz) const
{
    return (float) (std::log (hz / 20.0) / std::log (1000.0)) * (float) getWidth();
}

float RKParaEQEditor::LiveCurve::yFor (double db) const
{
    // The same scale as the response curve under it.
    const auto clamped = juce::jlimit (-(double) kCurveRangeDb * 1.1, (double) kCurveRangeDb * 1.1, db);
    return (float) getHeight() * 0.5f * (1.0f - (float) clamped / kCurveRangeDb);
}

void RKParaEQEditor::LiveCurve::setGains (const std::array<float, ParaEQEngine::kBands>& newGains)
{
    gains = newGains;

    for (size_t b = 0; b < gains.size(); ++b)
        if (std::abs (gains[b] - drawn[b]) > 0.1f)
        {
            repaint();
            return;
        }
}

void RKParaEQEditor::LiveCurve::paint (juce::Graphics& g)
{
    const auto& s = owner.settings;
    bool anyDynamic = false;
    for (const auto& b : s.bands)
        anyDynamic = anyDynamic || (b.on && b.dynamic.on);

    drawn = gains;
    if (! anyDynamic)
        return;

    // Each dynamic band's reach: from its Gain to Gain + Range.
    for (const auto& b : s.bands)
    {
        if (! b.on || ! b.dynamic.on)
            continue;

        const auto x = xFor (b.hz);
        const auto y1 = yFor (b.gainDb), y2 = yFor (b.gainDb + b.dynamic.rangeDb);
        g.setColour (owner.accent().withAlpha (0.55f));
        g.fillRect (x - 1.5f, juce::jmin (y1, y2), 3.0f, std::abs (y2 - y1));
        g.fillRect (x - 5.0f, y2 - 1.0f, 10.0f, 2.0f);
    }

    // The EQ right now.
    const ParaEQEngine::Response response (s, displaySampleRate (owner.eq), &gains);
    juce::Path path;
    const auto width = juce::jmax (2, getWidth());
    for (int x = 0; x <= width; x += 2)
    {
        const auto hz = 20.0 * std::pow (1000.0, (double) x / width);
        const auto y = yFor (response.magnitudeDb (hz));
        if (x == 0)
            path.startNewSubPath ((float) x, y);
        else
            path.lineTo ((float) x, y);
    }

    g.setColour (rkfx::Palette::text.withAlpha (0.85f));
    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void RKParaEQEditor::DynamicMeter::setValues (float levelDb, float gainDb, float seconds)
{
    const auto level = juce::jmax (juce::jmax (kMeterFloorDb, levelDb), shownLevel - 30.0f * seconds);
    if (std::abs (level - shownLevel) > 0.1f || std::abs (gainDb - shownGain) > 0.05f)
    {
        shownLevel = level;
        shownGain = gainDb;
        repaint();
    }
}

void RKParaEQEditor::DynamicMeter::paint (juce::Graphics& g)
{
    const auto band = owner.selectedBand();
    const auto& b = owner.settings.bands[(size_t) band];
    const auto active = b.on && b.dynamic.on;
    const auto accent = owner.accent();

    auto area = getLocalBounds().toFloat();
    const auto rowHeight = area.getHeight() / 2.0f;

    auto drawRow = [&] (juce::Rectangle<float> row, const juce::String& label, const juce::String& value)
    {
        g.setFont (rkfx::Fonts::regular (12.0f));
        g.setColour (rkfx::Palette::textDim);
        g.drawText (label, row.removeFromLeft (44.0f), juce::Justification::centredLeft);
        g.setColour (active ? rkfx::Palette::text : rkfx::Palette::textDim);
        g.drawText (value, row.removeFromRight (62.0f), juce::Justification::centredRight);
        return row.reduced (4.0f, 0.0f).withSizeKeepingCentre (row.getWidth() - 8.0f, 10.0f);
    };

    // Level: the detector, with the threshold as a mark.
    {
        const auto bar = drawRow (area.removeFromTop (rowHeight), "Level", juce::String (shownLevel, 1) + " dB");
        auto xFor = [&bar] (float db) { return bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, 1.0f - db / kMeterFloorDb); };

        g.setColour (rkfx::Palette::background);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (accent.withAlpha (active ? 0.6f : 0.25f));
        g.fillRoundedRectangle (bar.withRight (xFor (shownLevel)), 3.0f);

        const auto t = xFor (b.dynamic.thresholdDb);
        g.setColour (rkfx::Palette::text);
        g.fillRect (t - 1.0f, bar.getY() - 4.0f, 2.0f, bar.getHeight() + 8.0f);
    }

    // Gain: centred on 0 dB, ±24 dB.
    {
        const auto bar = drawRow (area, "Gain", (shownGain > 0.0f ? "+" : "") + juce::String (shownGain, 1) + " dB");
        const auto centre = bar.getCentreX();
        const auto x = centre + bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, shownGain / 24.0f);

        g.setColour (rkfx::Palette::background);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (x, centre), bar.getY(), juce::jmax (x, centre), bar.getBottom()));
        g.setColour (rkfx::Palette::outline);
        g.fillRect (centre - 0.5f, bar.getY() - 3.0f, 1.0f, bar.getHeight() + 6.0f);
    }
}

//==============================================================================
RKParaEQEditor::RKParaEQEditor (RKParaEQProcessor& p)
    : rkfx::EditorBase (p, 900, 800),
      eq (p),
      settings (p.currentSettings()),
      curve ([this] (double hz) { return ParaEQEngine::magnitudeDb (eq.currentSettings(), displaySampleRate (eq), hz); },
             kCurveRangeDb, accent()),
      watcher (p, [this] { onParameterChange(); })
{
    using ParaEQParams::id;
    auto param = [&p] (const juce::String& paramId) -> juce::RangedAudioParameter& { return *p.findParam (paramId); };

    addToCanvas (curve);
    addToCanvas (liveCurve);

    // Selectors first, so the strips' controls sit on top of them.
    for (int band = 0; band < ParaEQEngine::kBands; ++band)
    {
        selectors.push_back (std::make_unique<StripSelector> (*this, band));
        addToCanvas (*selectors.back());
    }

    for (const auto& b : ParaEQParams::bands)
    {
        auto strip = std::make_unique<Strip> (p, b, accent());

        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 strip->on.get(), strip->type.get(), strip->slope.get(), strip->freq.get(), strip->gain.get(), strip->q.get(),
                 strip->dyn.get() })
            if (c != nullptr)
                addToCanvas (*c);

        strip->onWatcher->sendInitialUpdate();
        if (strip->typeWatcher != nullptr)
            strip->typeWatcher->sendInitialUpdate();

        strips.push_back (std::move (strip));
    }

    for (int band = 0; band < ParaEQEngine::kBands; ++band)
    {
        const auto& b = ParaEQParams::bands[stripOf (band)];
        auto& c = dynamicControls[(size_t) band];
        c.dynamic   = std::make_unique<rkfx::ToggleSwitch> (param (id (b, "dyn")), accent(), "Dynamic");
        c.threshold = std::make_unique<rkfx::Knob> (param (id (b, "threshold")), "Threshold");
        c.range     = std::make_unique<rkfx::Knob> (param (id (b, "range")), "Range");
        c.ratio     = std::make_unique<rkfx::Knob> (param (id (b, "ratio")), "Ratio");
        c.attack    = std::make_unique<rkfx::Knob> (param (id (b, "attack")), "Attack");
        c.release   = std::make_unique<rkfx::Knob> (param (id (b, "release")), "Release");

        for (juce::Component* k : std::initializer_list<juce::Component*> {
                 c.dynamic.get(), c.threshold.get(), c.range.get(), c.ratio.get(), c.attack.get(), c.release.get() })
            addToCanvas (*k);
    }

    addToCanvas (meter);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

int RKParaEQEditor::selectedBand() const
{
    return juce::jlimit (0, ParaEQEngine::kBands - 1, eq.selectedBand.load());
}

void RKParaEQEditor::selectBand (int band)
{
    eq.selectedBand = band;

    for (int b = 0; b < ParaEQEngine::kBands; ++b)
    {
        auto& c = dynamicControls[(size_t) b];
        for (juce::Component* k : std::initializer_list<juce::Component*> {
                 c.dynamic.get(), c.threshold.get(), c.range.get(), c.ratio.get(), c.attack.get(), c.release.get() })
            k->setVisible (b == band);
    }

    meter.repaint();
    repaint();
}

void RKParaEQEditor::onParameterChange()
{
    settings = eq.currentSettings();

    for (int band = 0; band < ParaEQEngine::kBands; ++band)
    {
        const auto& b = settings.bands[(size_t) band];
        const auto dimmed = ! b.on || ! b.dynamic.on;
        auto& c = dynamicControls[(size_t) band];
        for (auto* k : { c.threshold.get(), c.range.get(), c.ratio.get(), c.attack.get(), c.release.get() })
            k->setDimmed (dimmed);
    }

    curve.refresh();
    liveCurve.repaint();
    meter.repaint();
    selectBand (selectedBand());
}

void RKParaEQEditor::updateMeters()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;

    std::array<float, ParaEQEngine::kBands> gains {};
    for (int band = 0; band < ParaEQEngine::kBands; ++band)
    {
        const auto m = eq.takeMeter (band);
        gains[(size_t) band] = m.gainDb;
        if (band == selectedBand())
            meter.setValues (m.levelDb, m.gainDb, elapsed);
    }
    liveCurve.setGains (gains);
}

//==============================================================================
juce::Rectangle<int> RKParaEQEditor::curveArea() const
{
    return contentArea().withHeight (kCurveHeight);
}

juce::Rectangle<int> RKParaEQEditor::stripArea (int band) const
{
    const auto area = contentArea().withTrimmedTop (kCurveHeight + 8).withTrimmedBottom (kDynamicsHeight + 8);
    const auto width = (area.getWidth() - kGap * (ParaEQParams::kNumBands - 1)) / ParaEQParams::kNumBands;
    return { area.getX() + band * (width + kGap), area.getY(), width, area.getHeight() };
}

juce::Rectangle<int> RKParaEQEditor::dynamicsArea() const
{
    return contentArea().removeFromBottom (kDynamicsHeight);
}

void RKParaEQEditor::layoutContent()
{
    curve.setBounds (curveArea());
    liveCurve.setBounds (curveArea());

    for (int i = 0; i < (int) strips.size(); ++i)
    {
        auto& s = *strips[(size_t) i];
        auto area = stripArea (i).reduced (6, 0);

        auto title = area.removeFromTop (kTitleHeight);
        s.on->setBounds (title.removeFromRight (34).withSizeKeepingCentre (34, 20));

        auto switchRow = area.removeFromTop (kSwitchHeight);
        if (s.type != nullptr)  s.type->setBounds (switchRow);
        if (s.slope != nullptr) s.slope->setBounds (switchRow);
        area.removeFromTop (6);

        for (auto* k : { s.freq.get(), s.gain.get(), s.q.get() })
            if (k != nullptr)
                k->setBounds (area.removeFromTop (kKnobHeight));

        if (s.dyn != nullptr)
        {
            area.removeFromTop (6);
            s.dyn->setBounds (area.removeFromTop (kSwitchHeight));
        }
    }

    for (int band = 0; band < ParaEQEngine::kBands; ++band)
        selectors[(size_t) band]->setBounds (stripArea (stripOf (band)));

    auto area = dynamicsArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    auto row = area.removeFromTop (rkfx::Knob::kDefaultHeight);

    for (int band = 0; band < ParaEQEngine::kBands; ++band)
    {
        auto r = row;
        auto& c = dynamicControls[(size_t) band];
        c.dynamic->setBounds (r.removeFromLeft (90).withSizeKeepingCentre (90, 28));
        r.removeFromLeft (12);
        for (auto* k : { c.threshold.get(), c.range.get(), c.ratio.get(), c.attack.get(), c.release.get() })
            k->setBounds (r.removeFromLeft (rkfx::Knob::kDefaultWidth + 6));
    }

    meter.setBounds (row.withTrimmedLeft (90 + 12 + 5 * (rkfx::Knob::kDefaultWidth + 6) + 16).withSizeKeepingCentre (
        row.getWidth() - (90 + 12 + 5 * (rkfx::Knob::kDefaultWidth + 6) + 16), 56));
}

void RKParaEQEditor::paintContent (juce::Graphics& g)
{
    for (int i = 0; i < ParaEQParams::kNumBands; ++i)
        drawSection (g, stripArea (i), ParaEQParams::bands[i].title);

    const auto band = selectedBand();
    g.setColour (accent().withAlpha (0.6f));
    g.drawRoundedRectangle (stripArea (stripOf (band)).toFloat().reduced (0.75f), rkfx::Style::kPanelRadius, 1.5f);

    drawSection (g, dynamicsArea(), juce::String (ParaEQParams::bands[stripOf (band)].title) + " dynamics");
}
