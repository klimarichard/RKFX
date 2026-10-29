#include "RideGraph.h"

#include "gui/LookAndFeel.h"

using rkfx::Palette::background;
using rkfx::Palette::outline;
using rkfx::Palette::text;
using rkfx::Palette::textDim;

namespace
{
    constexpr float kLevelFloorDb = -60.0f;
    constexpr float kGainRangeDb = 18.0f;
    constexpr float kLaneSplit = 0.6f;    // the level lane's share of the height
}

RideGraph::RideGraph (juce::Colour accentColour) : accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
}

void RideGraph::push (const RideEngine::Meter& m)
{
    history[(size_t) next] = { m.levelDb, m.targetDb, m.gainDb, m.holding };
    next = (next + 1) % kPoints;
    repaint();
}

void RideGraph::setSettings (const RideEngine::Settings& s)
{
    settings = s;
    repaint();
}

const RideGraph::Point& RideGraph::at (int age) const
{
    return history[(size_t) ((next + age) % kPoints)];
}

void RideGraph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (background);
    g.fillRoundedRectangle (bounds, rkfx::Style::kPanelRadius);

    auto lanes = bounds.reduced (0.0f, 6.0f);
    const auto levelLane = lanes.removeFromTop (lanes.getHeight() * kLaneSplit);
    lanes.removeFromTop (8.0f);
    const auto gainLane = lanes;

    const auto width = bounds.getWidth();
    auto xFor = [width] (int age) { return width * (float) age / (float) (kPoints - 1); };
    auto levelY = [&levelLane] (float db)
    {
        return levelLane.getY() + levelLane.getHeight() * juce::jlimit (0.0f, 1.0f, db / kLevelFloorDb);
    };
    // The gain lane spans the allowed range (a little more), so small rides still show.
    const auto laneRangeDb = 1.15f * juce::jmax (3.0f, settings.maxBoostDb, settings.maxCutDb);
    auto gainY = [&gainLane, laneRangeDb] (float db)
    {
        return gainLane.getCentreY() - gainLane.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, db / laneRangeDb);
    };

    g.setFont (rkfx::Fonts::regular (11.0f));

    // Level grid
    for (float db = -12.0f; db > kLevelFloorDb; db -= 12.0f)
    {
        const auto y = levelY (db);
        g.setColour (outline.withAlpha (0.45f));
        g.drawHorizontalLine ((int) y, 0.0f, width);
        g.setColour (textDim);
        g.drawText (juce::String ((int) db), 4, (int) y - 13, 30, 12, juce::Justification::left);
    }

    // Input level: one column per point, gray while holding (below the Floor).
    const auto column = width / (float) (kPoints - 1) + 0.5f;
    for (int age = 0; age < kPoints; ++age)
    {
        const auto& p = at (age);
        if (p.levelDb <= kLevelFloorDb)
            continue;
        const auto y = levelY (p.levelDb);
        g.setColour (p.holding ? textDim.withAlpha (0.25f) : accent.withAlpha (0.3f));
        g.fillRect (xFor (age), y, column, levelLane.getBottom() - y);
    }

    // Floor (dotted) and Target (follows the music in Relative mode)
    {
        const auto y = levelY (settings.floorDb);
        g.setColour (textDim);
        for (float x = 0.0f; x < width; x += 8.0f)
            g.fillRect (x, y - 0.5f, 4.0f, 1.0f);
        g.drawText ("Floor", (int) width - 44, (int) y + 2, 40, 12, juce::Justification::right);
    }
    {
        juce::Path target;
        for (int age = 0; age < kPoints; ++age)
        {
            const auto y = levelY (at (age).targetDb);
            if (age == 0) target.startNewSubPath (xFor (age), y);
            else          target.lineTo (xFor (age), y);
        }
        g.setColour (accent);
        g.strokePath (target, juce::PathStrokeType (2.0f));
        g.drawText ("Target", (int) width - 50, (int) levelY (at (kPoints - 1).targetDb) - 15, 46, 12, juce::Justification::right);
    }

    // Gain lane: the allowed range, 0 dB, and the gain.
    {
        const auto top = gainY (settings.maxBoostDb), bottom = gainY (-settings.maxCutDb);
        g.setColour (accent.withAlpha (0.08f));
        g.fillRect (0.0f, top, width, bottom - top);
        g.setColour (outline);
        g.drawHorizontalLine ((int) gainY (0.0f), 0.0f, width);

        g.setColour (textDim);
        g.drawText ("Gain", (int) width - 44, (int) gainLane.getY(), 40, 12, juce::Justification::right);
        g.drawText ("+" + juce::String ((int) settings.maxBoostDb), 4, (int) top - 1, 30, 12, juce::Justification::left);
        g.drawText ("-" + juce::String ((int) settings.maxCutDb), 4, (int) bottom - 12, 30, 12, juce::Justification::left);

        juce::Path gain;
        for (int age = 0; age < kPoints; ++age)
        {
            const auto y = gainY (at (age).gainDb);
            if (age == 0) gain.startNewSubPath (xFor (age), y);
            else          gain.lineTo (xFor (age), y);
        }
        g.setColour (text);
        g.strokePath (gain, juce::PathStrokeType (2.0f));
    }
}

//==============================================================================
RideFader::RideFader (juce::Colour accentColour) : accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
}

void RideFader::setGain (float gainDb)
{
    if (std::abs (gainDb - shown) > 0.05f)
    {
        shown = gainDb;
        repaint();
    }
}

void RideFader::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto valueArea = area.removeFromBottom (16.0f);
    area.removeFromBottom (4.0f);
    const auto track = area.reduced (0.0f, 8.0f);

    auto yFor = [&track] (float db)
    {
        return track.getCentreY() - track.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, db / kGainRangeDb);
    };

    g.setFont (rkfx::Fonts::regular (10.0f));
    for (int db = -18; db <= 18; db += 6)
    {
        const auto y = yFor ((float) db);
        g.setColour (db == 0 ? text.withAlpha (0.6f) : outline);
        g.drawHorizontalLine ((int) y, track.getX(), track.getX() + 8.0f);
        g.setColour (textDim);
        g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db), (int) track.getX() + 10, (int) y - 6, 26, 12,
                    juce::Justification::left);
    }

    // The slot and the cap.
    const auto slotX = track.getRight() - 14.0f;
    g.setColour (background);
    g.fillRoundedRectangle (slotX + 4.0f, track.getY(), 4.0f, track.getHeight(), 2.0f);

    const auto cap = juce::Rectangle<float> (slotX - 4.0f, yFor (shown) - 7.0f, 20.0f, 14.0f);
    g.setColour (rkfx::Palette::knobBody);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (accent);
    g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 4.0f, 2.0f);

    g.setColour (text);
    g.setFont (rkfx::Fonts::regular (11.0f));
    g.drawText ((shown > 0.05f ? "+" : "") + juce::String (shown, 1) + " dB", valueArea, juce::Justification::centred);
}
