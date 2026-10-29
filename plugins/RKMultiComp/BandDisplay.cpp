#include "BandDisplay.h"

#include "gui/LookAndFeel.h"
#include "plugin/ParamBuilder.h"

#include "Parameters.h"

#include <cmath>

using rkfx::Palette::background;
using rkfx::Palette::outline;
using rkfx::Palette::text;
using rkfx::Palette::textDim;

namespace
{
    constexpr double kMinHz = 20.0, kMaxHz = 20000.0;
    constexpr float kHitDistance = 5.0f;
    constexpr float kFineDrag = 0.2f;          // Shift-drag speed, as on the knobs
    constexpr float kLevelFallDbPerSec = 30.0f;
    constexpr float kReductionFallDbPerSec = 20.0f;
}

BandDisplay::BandDisplay (std::function<MultiCompEngine::Settings()> settingsSource, Parameters p, juce::Colour accentColour,
                          std::function<void (MultiCompEngine::Band)> selectCallback)
    : getSettings (std::move (settingsSource)), params (p), accent (accentColour), onSelect (std::move (selectCallback))
{
    shownLevel.fill (kMinDb);
    shownReduction.fill (0.0f);

    // The grid and the lines are cached: the live columns repaint over them
    // without redrawing them.
    grid.setBufferedToImage (true);
    lines.setBufferedToImage (true);

    addAndMakeVisible (grid);
    addAndMakeVisible (columns);
    addAndMakeVisible (lines);

    refresh();
}

void BandDisplay::resized()
{
    for (auto* layer : { &grid, &columns, &lines })
        layer->setBounds (getLocalBounds());
}

void BandDisplay::refresh()
{
    settings = getSettings();
    numBands = MultiCompEngine::bandsInUse (settings, bandList);
    lines.repaint();
    columns.repaint();
}

void BandDisplay::setSelectedBand (MultiCompEngine::Band band)
{
    if (band != selected)
    {
        selected = band;
        lines.repaint();
    }
}

void BandDisplay::setMeter (MultiCompEngine::Band band, MultiCompEngine::Meter meter, float seconds)
{
    const auto i = (size_t) band;

    // Instant rise, gentle fall.
    const auto level = juce::jmax (juce::jmax (kMinDb, meter.levelDb), shownLevel[i] - kLevelFallDbPerSec * seconds);
    const auto reduction = juce::jmin (meter.reductionDb, juce::jmin (0.0f, shownReduction[i] + kReductionFallDbPerSec * seconds));

    if (std::abs (level - shownLevel[i]) > 0.1f || std::abs (reduction - shownReduction[i]) > 0.05f)
    {
        shownLevel[i] = level;
        shownReduction[i] = reduction;
        columns.repaint();
    }
}

//==============================================================================
float BandDisplay::xFor (double hz) const
{
    return (float) (std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz)) * (float) getWidth();
}

double BandDisplay::hzFor (float x) const
{
    return kMinHz * std::pow (kMaxHz / kMinHz, (double) x / juce::jmax (1, getWidth()));
}

float BandDisplay::yFor (float db) const
{
    return (float) getHeight() * juce::jlimit (0.0f, 1.0f, db / kMinDb);
}

std::array<float, MultiCompEngine::kNumBands - 1> BandDisplay::crossoverHz() const
{
    const auto x = MultiCompEngine::crossoversInUse (settings);
    if (numBands == 4)
        return { x.low, x.mid, x.high };
    return { x.low, x.high, x.high };
}

juce::Range<float> BandDisplay::regionOf (int index) const
{
    const auto xo = crossoverHz();
    const auto left = index == 0 ? 0.0f : xFor (xo[(size_t) index - 1]);
    const auto right = index == numBands - 1 ? (float) getWidth() : xFor (xo[(size_t) index]);
    return { left, right };
}

int BandDisplay::regionAt (float x) const
{
    for (int i = 0; i < numBands - 1; ++i)
        if (x < regionOf (i).getEnd())
            return i;
    return numBands - 1;
}

BandDisplay::Hit BandDisplay::findTarget (juce::Point<float> pos) const
{
    // Crossover lines first: they are thin and run the full height.
    const auto xo = crossoverHz();
    juce::RangedAudioParameter* crossoverParams[] = { params.low, numBands == 4 ? params.mid : params.high, params.high };
    for (int i = 0; i < numBands - 1; ++i)
        if (std::abs (pos.x - xFor (xo[(size_t) i])) <= kHitDistance)
            return { Target::crossover, crossoverParams[i], MultiCompEngine::low };

    const auto index = regionAt (pos.x);
    const auto band = bandList[(size_t) index];
    if (std::abs (pos.y - yFor (MultiCompEngine::thresholdDb (settings, band))) <= kHitDistance)
        return { Target::threshold, params.thresholds[(size_t) band], band };

    return { Target::none, nullptr, band };
}

//==============================================================================
void BandDisplay::mouseMove (const juce::MouseEvent& e)
{
    switch (findTarget (e.position).target)
    {
        case Target::crossover: setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Target::threshold: setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case Target::none:      setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void BandDisplay::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void BandDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto hit = findTarget (e.position);

    if (hit.target != Target::crossover && onSelect != nullptr)
        onSelect (hit.band);

    if (hit.param == nullptr)
        return;

    dragging = hit;
    lastDragPos = hit.target == Target::crossover ? e.position.x : e.position.y;
    dragging.param->beginChangeGesture();
}

void BandDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging.param == nullptr)
        return;

    const auto speed = e.mods.isShiftDown() ? kFineDrag : 1.0f;
    auto* p = dragging.param;
    const auto current = p->convertFrom0to1 (p->getValue());
    float value;

    if (dragging.target == Target::crossover)
    {
        // Moves in octaves from the line where it is drawn (a crossover pushed
        // up by the one below starts from where it is used).
        const auto shown = p == params.low ? crossoverHz()[0]
                         : p == params.mid ? MultiCompEngine::crossoversInUse (settings).mid
                                           : MultiCompEngine::crossoversInUse (settings).high;
        const auto from = juce::jmax (current, shown);
        const auto octavesPerPixel = (float) (std::log2 (kMaxHz / kMinHz) / juce::jmax (1, getWidth()));
        value = from * std::exp2 ((e.position.x - lastDragPos) * octavesPerPixel * speed);
        lastDragPos = e.position.x;
    }
    else
    {
        const auto dbPerPixel = -kMinDb / (float) juce::jmax (1, getHeight());
        value = current - (e.position.y - lastDragPos) * dbPerPixel * speed;
        lastDragPos = e.position.y;
    }

    const auto& range = p->getNormalisableRange();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (range.start, range.end, value)));
}

void BandDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging.param != nullptr)
        dragging.param->endChangeGesture();
    dragging = {};
}

void BandDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* p = findTarget (e.position).param)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

//==============================================================================
void BandDisplay::paintGrid (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (background);
    g.fillRoundedRectangle (bounds, rkfx::Style::kPanelRadius);

    g.setFont (rkfx::Fonts::regular (11.0f));

    struct GridLine { double hz; bool major; };
    for (const auto line : { GridLine { 50.0, false }, GridLine { 100.0, true }, GridLine { 200.0, false },
                             GridLine { 500.0, false }, GridLine { 1000.0, true }, GridLine { 2000.0, false },
                             GridLine { 5000.0, false }, GridLine { 10000.0, true } })
    {
        const auto x = xFor (line.hz);
        g.setColour (outline.withAlpha (line.major ? 0.9f : 0.45f));
        g.drawVerticalLine ((int) x, 0.0f, bounds.getBottom());

        if (line.major)
        {
            g.setColour (textDim);
            g.drawText (line.hz >= 1000.0 ? juce::String ((int) (line.hz / 1000.0)) + "k" : juce::String ((int) line.hz),
                        (int) x + 3, getHeight() - 15, 30, 13, juce::Justification::left);
        }
    }

    for (float db = -12.0f; db > kMinDb; db -= 12.0f)
    {
        const auto y = yFor (db);
        g.setColour (outline.withAlpha (0.45f));
        g.drawHorizontalLine ((int) y, 0.0f, bounds.getRight());
        g.setColour (textDim);
        g.drawText (juce::String ((int) db), 3, (int) y - 13, 30, 12, juce::Justification::left);
    }
}

void BandDisplay::paintColumns (juce::Graphics& g)
{
    g.setFont (rkfx::Fonts::regular (11.0f));

    for (int i = 0; i < numBands; ++i)
    {
        const auto band = (size_t) bandList[(size_t) i];
        const auto region = regionOf (i);
        const auto inset = juce::jmin (14.0f, region.getLength() * 0.12f);
        const auto left = region.getStart() + inset, width = region.getLength() - 2.0f * inset;

        const auto level = shownLevel[band];
        const auto reduction = shownReduction[band];
        const auto bottom = (float) getHeight();

        if (level > kMinDb)
        {
            const auto top = yFor (level), compressedTop = yFor (level + reduction);
            g.setColour (accent.withAlpha (0.22f));
            g.fillRect (left, compressedTop, width, bottom - compressedTop);
            g.setColour (accent.withAlpha (0.85f));
            g.fillRect (left, top, width, compressedTop - top);
        }

        g.setColour (reduction < -0.05f ? text : textDim);
        g.drawText (juce::String (reduction, 1) + " dB",
                    juce::Rectangle<float> (region.getStart(), 20.0f, region.getLength(), 14.0f),
                    juce::Justification::centred);
    }
}

void BandDisplay::paintLines (juce::Graphics& g)
{
    const auto height = (float) getHeight();

    for (int i = 0; i < numBands; ++i)
    {
        const auto band = bandList[(size_t) i];
        const auto& b = settings.bands[(size_t) band];
        const auto region = regionOf (i);
        const auto area = juce::Rectangle<float> (region.getStart(), 0.0f, region.getLength(), height);

        if (b.solo)
        {
            g.setColour (accent.withAlpha (0.08f));
            g.fillRect (area);
        }

        if (band == selected)
        {
            g.setColour (accent.withAlpha (0.6f));
            g.drawRoundedRectangle (area.reduced (1.5f), rkfx::Style::kPanelRadius, 1.5f);
        }

        // Band name (and state) at the top.
        g.setFont (rkfx::Fonts::bold (12.0f));
        g.setColour (b.on ? text : textDim);
        auto name = MultiCompParams::displayName (band, settings.fourBands);
        if (! b.on)    name << " (off)";
        else if (b.solo) name << " (solo)";
        g.drawText (name, area.withHeight (20.0f).withTrimmedTop (4.0f), juce::Justification::centred);

        // Threshold line, with its value.
        const auto thresholdDb = MultiCompEngine::thresholdDb (settings, band);
        const auto y = yFor (thresholdDb);
        const auto lineColour = b.on ? accent : textDim.withAlpha (0.7f);
        g.setColour (lineColour);
        g.fillRect (region.getStart() + 4.0f, y - 1.0f, region.getLength() - 8.0f, 2.0f);

        g.setFont (rkfx::Fonts::regular (11.0f));
        const auto labelY = y < 34.0f ? y + 3.0f : y - 15.0f;
        g.drawText (juce::String (thresholdDb, 1) + " dB",
                    juce::Rectangle<float> (region.getStart() + 6.0f, labelY, region.getLength() - 12.0f, 12.0f),
                    juce::Justification::right);
    }

    // Crossovers, labelled with the frequency in use.
    const auto xo = crossoverHz();
    for (int i = 0; i < numBands - 1; ++i)
    {
        const auto x = xFor (xo[(size_t) i]);
        g.setColour (text.withAlpha (0.75f));
        g.fillRect (x - 0.75f, 0.0f, 1.5f, height);
        g.setFont (rkfx::Fonts::regular (11.0f));
        g.drawText (rkfx::Units::frequency (xo[(size_t) i]),
                    juce::Rectangle<float> (x + 4.0f, height - 32.0f, 60.0f, 13.0f), juce::Justification::left);
    }
}
