#include "TransferCurve.h"

#include "gui/LookAndFeel.h"

TransferCurve::TransferCurve (std::function<float (float)> transferDb, juce::Colour accentColour)
    : transfer (std::move (transferDb)), accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
}

juce::Point<float> TransferCurve::pointFor (float inDb, float outDb) const
{
    const auto w = (float) getWidth(), h = (float) getHeight();
    const auto x = (inDb - kMinDb) / -kMinDb * w;
    const auto y = h - (juce::jlimit (kMinDb, 0.0f, outDb) - kMinDb) / -kMinDb * h;
    return { x, y };
}

juce::Rectangle<int> TransferCurve::dotArea() const
{
    if (inputDb <= kMinDb)
        return {};
    return juce::Rectangle<float> (12.0f, 12.0f).withCentre (pointFor (inputDb, transfer (inputDb))).getSmallestIntegerContainer();
}

void TransferCurve::refresh()
{
    curve.clear();
    for (int i = 0; i <= 120; ++i)
    {
        const auto in = kMinDb + -kMinDb * (float) i / 120.0f;
        const auto p = pointFor (in, transfer (in));
        if (i == 0) curve.startNewSubPath (p);
        else        curve.lineTo (p);
    }
    repaint();
}

void TransferCurve::setInputLevel (float newInputDb)
{
    newInputDb = juce::jmax (kMinDb, newInputDb);
    if (std::abs (newInputDb - inputDb) < 0.2f)
        return;

    const auto before = dotArea();
    inputDb = newInputDb;
    repaint (before.getUnion (dotArea()));
}

void TransferCurve::setListening (bool isListening)
{
    if (listening != isListening)
    {
        listening = isListening;
        repaint();
    }
}

void TransferCurve::paint (juce::Graphics& g)
{
    using namespace rkfx;
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::background);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setFont (Fonts::regular (10.5f));
    for (int db = -48; db < 0; db += 12)
    {
        const auto p = pointFor ((float) db, (float) db);
        g.setColour (Palette::outline.withAlpha (0.5f));
        g.drawVerticalLine ((int) p.x, 0.0f, bounds.getBottom());
        g.drawHorizontalLine ((int) p.y, 0.0f, bounds.getRight());
        g.setColour (Palette::textDim);
        g.drawText (juce::String (db), (int) p.x + 2, getHeight() - 14, 30, 12, juce::Justification::left);
    }

    // Unity line (no compression)
    g.setColour (Palette::outline);
    g.drawLine ({ pointFor (kMinDb, kMinDb), pointFor (0.0f, 0.0f) }, 1.0f);

    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (inputDb > kMinDb)
    {
        g.setColour (Palette::text);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (pointFor (inputDb, transfer (inputDb))));
    }

    if (listening)
    {
        g.setColour (Palette::warning);
        g.setFont (Fonts::bold (13.0f));
        g.drawText ("SIDECHAIN LISTEN", getLocalBounds().removeFromTop (24), juce::Justification::centred);
    }
}
