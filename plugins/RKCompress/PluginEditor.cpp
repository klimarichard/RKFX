#include "PluginEditor.h"
#include "PluginProcessor.h"


namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kCurveWidth = 290;
    constexpr int kCompressorWidth = 262;
}

RKCompressEditor::RKCompressEditor (RKCompressProcessor& p)
    : rkfx::EditorBase (p, 760, 470),
      comp (p),
      curve ([this] (float in) { return CompressorEngine::transferDb (comp.currentSettings(), in); }, accent()),
      meter (accent()),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    threshold   = std::make_unique<Knob> (param ("threshold"), "Threshold");
    ratio       = std::make_unique<Knob> (param ("ratio"), "Ratio");
    knee        = std::make_unique<Knob> (param ("knee"), "Knee");
    attack      = std::make_unique<Knob> (param ("attack"), "Attack");
    release     = std::make_unique<Knob> (param ("release"), "Release");
    scHpf       = std::make_unique<Knob> (param ("sc_hpf"), "High-Pass");
    link        = std::make_unique<Knob> (param ("link"), "Link");
    makeup      = std::make_unique<Knob> (param ("makeup"), "Makeup");

    releaseAuto = std::make_unique<rkfx::ToggleSwitch> (param ("release_auto"), accent(), "Auto");
    makeupAuto  = std::make_unique<rkfx::ToggleSwitch> (param ("makeup_auto"), accent(), "Auto");
    listen      = std::make_unique<rkfx::ToggleSwitch> (param ("sc_listen"), rkfx::Palette::warning, "Listen");

    detect = std::make_unique<rkfx::SegmentedSwitch> (param ("detect"), accent());
    source = std::make_unique<rkfx::SegmentedSwitch> (param ("sc_source"), accent(), juce::StringArray { "Int", "Ext" });

    addToCanvas (curve);
    addToCanvas (meter);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             threshold.get(), ratio.get(), knee.get(), attack.get(), release.get(), scHpf.get(), link.get(), makeup.get(),
             releaseAuto.get(), makeupAuto.get(), listen.get(), detect.get(), source.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKCompressEditor::onParameterChange()
{
    const auto s = comp.currentSettings();
    release->setDimmed (s.autoRelease);
    makeup->setDimmed (s.autoMakeup);
    curve.setListening (s.listen);
    curve.refresh();
}

void RKCompressEditor::updateMeters()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;

    meter.setReduction (comp.takeGainReductionDb(), elapsed);
    curve.setInputLevel (comp.takeInputLevelDb());
}

juce::Rectangle<int> RKCompressEditor::curveSection() const
{
    return contentArea().withWidth (kCurveWidth);
}

juce::Rectangle<int> RKCompressEditor::compressorSection() const
{
    return contentArea().withTrimmedLeft (kCurveWidth + kGap).withWidth (kCompressorWidth);
}

juce::Rectangle<int> RKCompressEditor::sidechainSection() const
{
    return contentArea().withTrimmedLeft (kCurveWidth + kCompressorWidth + 2 * kGap);
}

void RKCompressEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = curveSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        area.removeFromBottom (8);
        meter.setBounds (area.removeFromRight (42));
        area.removeFromRight (6);
        const auto side = juce::jmin (area.getWidth(), area.getHeight());
        curve.setBounds (area.withSize (side, side));
    }

    {
        auto area = compressorSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        const auto third = area.getWidth() / 3;

        auto row1 = area.removeFromTop (Knob::kDefaultHeight);
        threshold->setBounds (row1.removeFromLeft (third));
        ratio->setBounds (row1.removeFromLeft (third));
        knee->setBounds (row1);

        area.removeFromTop (8);
        auto row2 = area.removeFromTop (Knob::kDefaultHeight);
        attack->setBounds (row2.removeFromLeft (third));
        release->setBounds (row2.removeFromLeft (third));

        auto switches = row2.reduced (4, 14);
        releaseAuto->setBounds (switches.removeFromTop (26));
        switches.removeFromTop (8);
        detect->setBounds (switches.removeFromTop (26));
    }

    {
        auto area = sidechainSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        source->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        scHpf->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        link->setBounds (knobs);
        area.removeFromTop (8);
        listen->setBounds (area.removeFromTop (26));
    }

    auto global = globalArea();
    makeup->setBounds (global.removeFromLeft (Knob::kDefaultWidth).withSizeKeepingCentre (Knob::kDefaultWidth, Knob::kDefaultHeight));
    global.removeFromLeft (6);
    makeupAuto->setBounds (global.removeFromLeft (52).withSizeKeepingCentre (52, 26));
}

void RKCompressEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, curveSection(), "Transfer");
    drawSection (g, compressorSection(), "Compressor");
    drawSection (g, sidechainSection(), "Sidechain");
}
