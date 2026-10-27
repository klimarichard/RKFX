#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kCurveHeight = 130;
    constexpr int kGap = 6;
    constexpr int kTitleHeight = 26;
    constexpr int kKnobHeight = 84;

    double displaySampleRate (const juce::AudioProcessor& p)
    {
        return p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0;
    }
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

RKParaEQEditor::RKParaEQEditor (RKParaEQProcessor& p)
    : rkfx::EditorBase (p, 760, 630),
      eq (p),
      curve ([this] (double hz) { return ParaEQEngine::magnitudeDb (eq.currentSettings(), displaySampleRate (eq), hz); },
             18.0f, accent()),
      watcher (p, [this] { curve.refresh(); })
{
    addToCanvas (curve);

    for (const auto& b : ParaEQParams::bands)
    {
        auto strip = std::make_unique<Strip> (p, b, accent());

        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 strip->on.get(), strip->type.get(), strip->slope.get(), strip->freq.get(), strip->gain.get(), strip->q.get() })
            if (c != nullptr)
                addToCanvas (*c);

        strip->onWatcher->sendInitialUpdate();
        if (strip->typeWatcher != nullptr)
            strip->typeWatcher->sendInitialUpdate();

        strips.push_back (std::move (strip));
    }

    finishSetup();
}

juce::Rectangle<int> RKParaEQEditor::curveArea() const
{
    return contentArea().withHeight (kCurveHeight);
}

juce::Rectangle<int> RKParaEQEditor::stripArea (int band) const
{
    const auto area = contentArea().withTrimmedTop (kCurveHeight + 8);
    const auto width = (area.getWidth() - kGap * (ParaEQParams::kNumBands - 1)) / ParaEQParams::kNumBands;
    return { area.getX() + band * (width + kGap), area.getY(), width, area.getHeight() };
}

void RKParaEQEditor::layoutContent()
{
    curve.setBounds (curveArea());

    for (int i = 0; i < (int) strips.size(); ++i)
    {
        auto& s = *strips[(size_t) i];
        auto area = stripArea (i).reduced (6, 0);

        auto title = area.removeFromTop (kTitleHeight);
        s.on->setBounds (title.removeFromRight (34).withSizeKeepingCentre (34, 20));

        auto switchRow = area.removeFromTop (24);
        if (s.type != nullptr)  s.type->setBounds (switchRow);
        if (s.slope != nullptr) s.slope->setBounds (switchRow);
        area.removeFromTop (6);

        for (auto* k : { s.freq.get(), s.gain.get(), s.q.get() })
            if (k != nullptr)
                k->setBounds (area.removeFromTop (kKnobHeight));
    }
}

void RKParaEQEditor::paintContent (juce::Graphics& g)
{
    for (int i = 0; i < ParaEQParams::kNumBands; ++i)
        drawSection (g, stripArea (i), ParaEQParams::bands[i].title);
}
