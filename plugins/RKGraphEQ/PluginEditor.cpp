#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kCurveHeight = 130;

    double displaySampleRate (const juce::AudioProcessor& p)
    {
        return p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0;
    }
}

RKGraphEQEditor::RKGraphEQEditor (RKGraphEQProcessor& p)
    : rkfx::EditorBase (p, 900, 550),
      eq (p),
      curve ([this] (double hz) { return GraphEQEngine::magnitudeDb (eq.currentSettings(), displaySampleRate (eq), hz); },
             12.0f, accent()),
      watcher (p, [this] { curve.refresh(); })
{
    addToCanvas (curve);

    for (int m = 0; m < 3; ++m)
    {
        const auto mode = GraphEQParams::modes[m];
        for (int b = 0; b < GraphEQParams::modeCount (m); ++b)
        {
            auto fader = std::make_unique<rkfx::Fader> (*p.bandParam (m, b),
                                                        GraphEQParams::frequencyLabel (GraphEQEngine::centres (mode)[b]));
            fader->setCompactValue (m == 2);
            addToCanvas (*fader);
            faders[(size_t) m].push_back (std::move (fader));
        }
    }

    bandsSwitch = std::make_unique<rkfx::SegmentedSwitch> (*p.findParam ("bands"), accent(),
                                                           juce::StringArray { "10", "15", "31" }, "Bands");
    addToCanvas (*bandsSwitch);

    flatButton.setTooltip ("Reset every band of the current band count to 0 dB");
    flatButton.onClick = [this] { flatten(); };
    addToCanvas (flatButton);

    modeWatcher = std::make_unique<juce::ParameterAttachment> (*p.findParam ("bands"), [this] (float v)
    {
        showMode (juce::jlimit (0, 2, juce::roundToInt (v)));
    });
    modeWatcher->sendInitialUpdate();

    finishSetup();
}

void RKGraphEQEditor::showMode (int modeIndex)
{
    for (int m = 0; m < 3; ++m)
        for (auto& f : faders[(size_t) m])
            f->setVisible (m == modeIndex);
}

void RKGraphEQEditor::flatten()
{
    const auto mode = eq.activeMode();
    for (int b = 0; b < GraphEQParams::modeCount (mode); ++b)
    {
        auto* param = eq.bandParam (mode, b);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    }
}

juce::Rectangle<int> RKGraphEQEditor::curveArea() const
{
    return contentArea().withHeight (kCurveHeight);
}

juce::Rectangle<int> RKGraphEQEditor::faderArea() const
{
    return contentArea().withTrimmedTop (kCurveHeight + 8).reduced (6, 0);
}

void RKGraphEQEditor::layoutContent()
{
    curve.setBounds (curveArea());

    const auto area = faderArea();
    for (int m = 0; m < 3; ++m)
    {
        const auto count = GraphEQParams::modeCount (m);
        const auto width = (float) area.getWidth() / (float) count;
        const auto faderWidth = juce::jmin ((int) width - 2, 56);

        for (int b = 0; b < count; ++b)
        {
            const auto centreX = area.getX() + juce::roundToInt (width * ((float) b + 0.5f));
            faders[(size_t) m][(size_t) b]->setBounds (centreX - faderWidth / 2, area.getY(), faderWidth, area.getHeight());
        }
    }

    auto global = globalArea();
    bandsSwitch->setBounds (global.removeFromLeft (150).withSizeKeepingCentre (150, 46));
    global.removeFromLeft (rkfx::EditorBase::kMargin);
    flatButton.setBounds (global.removeFromLeft (64).withSizeKeepingCentre (64, 28).translated (0, 9));
}

void RKGraphEQEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, faderArea().expanded (6, 4), {});
}
