#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/LookAndFeel.h"

/** Sizes and helpers shared by the RKMix editor and its panels. */
namespace MixGui::Layout
{
    constexpr int kGap = 8;
    constexpr int kMasterWidth = 200;
    constexpr int kTopHeight = 400;
    constexpr int kTabHeight = 28;
    constexpr int kStripsPerLayer = 16;

    // Strip rows, top to bottom
    constexpr int kColourBar = 5;
    constexpr int kStripNameHeight = 22;
    constexpr int kStripNumberHeight = 14;
    constexpr int kSectionRowHeight = 14;
    constexpr int kPanHeight = 18;
    constexpr int kButtonHeight = 22;
    constexpr int kStripHeadHeight = kColourBar + 2 + kStripNameHeight + kStripNumberHeight + kSectionRowHeight + 4 + kPanHeight + 4;
    constexpr int kMeterWidth = 9;
    constexpr int kInsertRow = 15;

    // Panels
    constexpr int kKnobW = 58, kKnobH = 80;
    constexpr int kSectionTitle = 22;
    constexpr int kSwitchW = 46, kSwitchH = 24;
    constexpr int kPanelHeader = 30;
    constexpr int kRowA = 116;
    constexpr float kEqRangeDb = 18.0f;

    inline void styleName (juce::Label& label, float size = 12.0f)
    {
        label.setEditable (false, true, false);   // double-click to rename
        label.setFont (rkfx::Fonts::bold (size));
        label.setJustificationType (juce::Justification::centred);
        label.setMinimumHorizontalScale (0.6f);
        label.setColour (juce::Label::textColourId, rkfx::Palette::text);
        label.setColour (juce::Label::textWhenEditingColourId, rkfx::Palette::text);
        label.setTooltip ("Click to select, double-click to rename, right-click for its colour");
    }

    /** The 8 colour swatches of a panel header, right to left from @p header's right edge. */
    inline void placeSwatches (juce::Rectangle<int>& header, std::array<juce::Rectangle<int>, 8>& swatches)
    {
        for (int i = (int) swatches.size() - 1; i >= 0; --i)
        {
            swatches[(size_t) i] = header.removeFromRight (20).reduced (2, 3);
            header.removeFromRight (2);
        }
    }

    inline void paintSwatches (juce::Graphics& g, const std::array<juce::Rectangle<int>, 8>& swatches, int current,
                               const std::function<juce::Colour (int)>& colourOf)
    {
        for (int i = 0; i < (int) swatches.size(); ++i)
        {
            const auto r = swatches[(size_t) i].toFloat();
            g.setColour (colourOf (i));
            g.fillRoundedRectangle (r, 3.0f);
            if (i == current)
            {
                g.setColour (rkfx::Palette::text);
                g.drawRoundedRectangle (r.expanded (1.5f), 3.0f, 1.5f);
            }
        }
    }

    inline int swatchAt (const std::array<juce::Rectangle<int>, 8>& swatches, juce::Point<int> p)
    {
        for (int i = 0; i < (int) swatches.size(); ++i)
            if (swatches[(size_t) i].expanded (2).contains (p))
                return i;
        return -1;
    }
}
