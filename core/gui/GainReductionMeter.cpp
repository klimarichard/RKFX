#include "GainReductionMeter.h"

#include "LookAndFeel.h"

#include <cmath>

namespace rkfx
{

GainReductionMeter::GainReductionMeter (juce::Colour accentColour) : accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
}

void GainReductionMeter::setReduction (float reductionDb, float secondsSinceLast)
{
    // Instant rise, gentle fall back towards 0 dB.
    const auto released = juce::jmin (0.0f, shownDb + 20.0f * secondsSinceLast);
    const auto next = juce::jmax (-kRangeDb, juce::jmin (reductionDb, released));

    if (std::abs (next - shownDb) > 0.05f)
    {
        shownDb = next;
        repaint();
    }
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto labelArea = bounds.removeFromTop (16.0f);
    const auto scaleArea = bounds.removeFromRight (20.0f);
    const auto bar = bounds.reduced (2.0f, 0.0f);

    g.setColour (Palette::textDim);
    g.setFont (Fonts::regular (11.0f));
    g.drawText ("GR", labelArea, juce::Justification::centred);

    g.setColour (Palette::background);
    g.fillRoundedRectangle (bar, 3.0f);

    const auto depth = juce::jlimit (0.0f, 1.0f, -shownDb / kRangeDb);
    g.setColour (accent);
    g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * depth), 3.0f);

    for (int db = 0; db <= (int) kRangeDb; db += 6)
    {
        const auto y = bar.getY() + bar.getHeight() * (float) db / kRangeDb;
        g.setColour (Palette::outline);
        g.drawHorizontalLine ((int) y, bar.getX(), bar.getRight());
        g.setColour (Palette::textDim);
        g.drawText (juce::String (-db), scaleArea.withY (y - 6.0f).withHeight (12.0f), juce::Justification::centredRight);
    }
}

} // namespace rkfx
