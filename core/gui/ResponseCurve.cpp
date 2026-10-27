#include "ResponseCurve.h"

#include "LookAndFeel.h"

#include <cmath>

namespace rkfx
{

namespace
{
    constexpr double kMinHz = 20.0, kMaxHz = 20000.0;
}

ResponseCurve::ResponseCurve (std::function<double (double)> magnitudeDb, float rangeDb, juce::Colour accentColour)
    : magnitude (std::move (magnitudeDb)), range (rangeDb), accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
}

float ResponseCurve::xFor (double hz) const
{
    return (float) (std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz)) * (float) getWidth();
}

float ResponseCurve::yFor (double db) const
{
    const auto clamped = juce::jlimit (-(double) range * 1.1, (double) range * 1.1, db);
    return (float) getHeight() * 0.5f * (1.0f - (float) clamped / range);
}

void ResponseCurve::refresh()
{
    curve.clear();

    const auto width = juce::jmax (2, getWidth());
    for (int x = 0; x <= width; x += 2)
    {
        const auto hz = kMinHz * std::pow (kMaxHz / kMinHz, (double) x / width);
        const auto y = yFor (magnitude (hz));
        if (x == 0)
            curve.startNewSubPath ((float) x, y);
        else
            curve.lineTo ((float) x, y);
    }

    repaint();
}

void ResponseCurve::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::background);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setFont (Fonts::regular (11.0f));

    // Frequency grid
    struct GridLine { double hz; bool major; };
    for (const auto line : { GridLine { 50.0, false }, GridLine { 100.0, true }, GridLine { 200.0, false },
                             GridLine { 500.0, false }, GridLine { 1000.0, true }, GridLine { 2000.0, false },
                             GridLine { 5000.0, false }, GridLine { 10000.0, true } })
    {
        const auto hz = line.hz;
        const auto major = line.major;
        const auto x = xFor (hz);
        g.setColour (Palette::outline.withAlpha (major ? 0.9f : 0.45f));
        g.drawVerticalLine ((int) x, 0.0f, bounds.getBottom());

        if (major)
        {
            g.setColour (Palette::textDim);
            g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                        (int) x + 3, getHeight() - 15, 30, 13, juce::Justification::left);
        }
    }

    // Level grid
    const auto step = range > 12.0f ? 6.0f : 3.0f;
    for (float db = -range + step; db < range; db += step)
    {
        const auto y = yFor (db);
        g.setColour (Palette::outline.withAlpha (std::abs (db) < 0.01f ? 1.0f : 0.45f));
        g.drawHorizontalLine ((int) y, 0.0f, bounds.getRight());

        if (std::abs (db) > 0.01f)
        {
            g.setColour (Palette::textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), 3, (int) y - 13, 30, 12, juce::Justification::left);
        }
    }

    // Curve, with a light fill towards 0 dB
    auto fill = curve;
    fill.lineTo (bounds.getRight(), yFor (0.0));
    fill.lineTo (0.0f, yFor (0.0));
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.15f));
    g.fillPath (fill);

    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
ParameterWatcher::ParameterWatcher (juce::AudioProcessor& p, std::function<void()> onChange)
    : processor (p), callback (std::move (onChange))
{
    for (auto* param : processor.getParameters())
        param->addListener (this);
}

ParameterWatcher::~ParameterWatcher()
{
    cancelPendingUpdate();
    for (auto* param : processor.getParameters())
        param->removeListener (this);
}

} // namespace rkfx
