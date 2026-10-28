#include "BoardEditor.h"

#include "plugin/EditorBase.h"

#include <limits>
#include <tuple>

namespace rkboard
{

using namespace rkfx;

namespace
{
    juce::Colour familyOf (const rkfx::ProcessorBase& pedal)
    {
        return Palette::familyColour (pedal.getConfig().family);
    }

    juce::String shortNameOf (const rkfx::ProcessorBase& pedal)
    {
        if (const auto* type = findPedalType (pedal.getConfig().name))
            return type->shortName;
        return pedal.getConfig().name;
    }

    enum MenuCommands
    {
        presetFileBase = 1,         // + file index (from addPresetMenuItems)
        factoryBase = 50000,        // + factory preset index
        resetCommand = 100000,
        revealCommand
    };
}

//==============================================================================
BoardStrip::BoardStrip (BoardProcessor& b) : board (b)
{
}

void BoardStrip::setSelected (int item)
{
    if (selected != item)
    {
        selected = item;
        repaint();
    }
}

int BoardStrip::keyOf (const Item& item)
{
    switch (item.kind)
    {
        case Item::Kind::pedal: return item.id;
        case Item::Kind::add:   return 100 + (int) item.section;
        case Item::Kind::split: return kSplitNode;
        case Item::Kind::merge: return kMergeNode;
    }
    return 0;
}

BoardStrip::Layout BoardStrip::computeLayout (int width) const
{
    constexpr int gap = 6, addWidth = 30, nodeWidth = 34, margin = 12, minTileWidth = 44;
    constexpr int tallHeight = 2 * kTileHeight + kLaneGap;
    const auto maxRowWidth = juce::jmax (200, width - 2 * margin);

    // Blocks flow like words: a pedal, a "+", or the split with both paths
    // and the merge (which stays together).
    struct Block
    {
        std::vector<Item> items;     // positioned relative to the block
        std::vector<bool> inFlow;
        int width = 0;
        bool tall = false;
    };
    std::vector<Block> blocks;

    const auto addSingles = [&] (const std::vector<int>& ids, Section section)
    {
        for (int i = 0; i < (int) ids.size(); ++i)
            blocks.push_back ({ { { Item::Kind::pedal, ids[(size_t) i], section, i, { 0, 0, kTileWidth, kTileHeight } } }, { true }, kTileWidth, false });
        blocks.push_back ({ { { Item::Kind::add, 0, section, (int) ids.size(), { 0, 0, addWidth, kTileHeight } } }, { true }, addWidth, false });
    };

    addSingles (board.getSection (Section::pre), Section::pre);

    {
        const auto& pathA = board.getSection (Section::pathA);
        const auto& pathB = board.getSection (Section::pathB);
        const auto lanes = (int) juce::jmax (pathA.size(), pathB.size());

        // Paths too long for one row: their tiles get narrower.
        const auto laneRoom = maxRowWidth - 2 * (nodeWidth + gap) - (addWidth + gap);
        const auto tileWidth = lanes > 0 ? juce::jlimit (minTileWidth, kTileWidth, laneRoom / lanes - gap) : kTileWidth;

        Block block;
        block.tall = true;
        block.items.push_back ({ Item::Kind::split, 0, Section::pre, 0, { 0, 0, nodeWidth, tallHeight } });
        block.inFlow.push_back (true);

        const auto x = nodeWidth + gap;
        for (const auto& [ids, section, y] : { std::tuple { &pathA, Section::pathA, 0 },
                                               std::tuple { &pathB, Section::pathB, kTileHeight + kLaneGap } })
        {
            for (int i = 0; i < (int) ids->size(); ++i)
            {
                block.items.push_back ({ Item::Kind::pedal, (*ids)[(size_t) i], section, i, { x + i * (tileWidth + gap), y, tileWidth, kTileHeight } });
                block.inFlow.push_back (false);
            }
            block.items.push_back ({ Item::Kind::add, 0, section, (int) ids->size(), { x + (int) ids->size() * (tileWidth + gap), y, addWidth, kTileHeight } });
            block.inFlow.push_back (false);
        }

        const auto mergeX = x + lanes * (tileWidth + gap) + addWidth + gap;
        block.items.push_back ({ Item::Kind::merge, 0, Section::post, 0, { mergeX, 0, nodeWidth, tallHeight } });
        block.inFlow.push_back (true);
        block.width = mergeX + nodeWidth;
        blocks.push_back (std::move (block));
    }

    addSingles (board.getSection (Section::post), Section::post);

    // Into rows. A "+" never starts a row on its own: the block before it
    // comes along.
    std::vector<std::vector<int>> rows (1);
    const auto widthOf = [&] (const std::vector<int>& row)
    {
        auto w = 0;
        for (const auto b : row)
            w += (w > 0 ? gap : 0) + blocks[(size_t) b].width;
        return w;
    };

    for (int b = 0; b < (int) blocks.size(); ++b)
    {
        auto& row = rows.back();
        const auto fits = row.empty() || widthOf (row) + gap + blocks[(size_t) b].width <= maxRowWidth;
        if (fits)
        {
            row.push_back (b);
            continue;
        }

        const auto isAdd = blocks[(size_t) b].items.front().kind == Item::Kind::add;
        std::vector<int> next;
        if (isAdd && row.size() > 1)
        {
            next.push_back (row.back());
            row.pop_back();
        }
        next.push_back (b);
        rows.push_back (std::move (next));
    }

    std::vector<int> rowWidths;
    for (const auto& row : rows)
        rowWidths.push_back (widthOf (row));

    // Positions: one row is centred, several start at the left margin.
    Layout result;
    auto y = kPadding;
    for (int r = 0; r < (int) rows.size(); ++r)
    {
        auto tall = false;
        for (const auto b : rows[(size_t) r])
            tall = tall || blocks[(size_t) b].tall;

        const Row row { y, tall ? tallHeight : kTileHeight };
        result.rows.push_back (row);

        auto x = rows.size() == 1 ? (width - rowWidths[0]) / 2 : margin;
        for (const auto b : rows[(size_t) r])
        {
            const auto& block = blocks[(size_t) b];
            const auto blockY = row.top + (row.height - (block.tall ? tallHeight : kTileHeight)) / 2;
            for (size_t i = 0; i < block.items.size(); ++i)
            {
                auto item = block.items[i];
                item.bounds.translate (x, blockY);
                if (block.inFlow[i])
                    result.flow.push_back ((int) result.items.size());
                result.items.push_back (item);
                result.rowOf.push_back (r);
            }
            x += block.width + gap;
        }

        y += row.height + kRowGap;
    }
    result.height = y - kRowGap + kPadding;
    return result;
}

int BoardStrip::getPreferredHeight (int width) const
{
    return computeLayout (width).height;
}

void BoardStrip::layoutItems()
{
    layout = computeLayout (getWidth());
}

const BoardStrip::Item* BoardStrip::itemAt (juce::Point<int> pos) const
{
    for (const auto& item : layout.items)
        if (item.bounds.contains (pos))
            return &item;
    return nullptr;
}

const BoardStrip::Item* BoardStrip::find (Item::Kind kind, int id) const
{
    for (const auto& item : layout.items)
        if (item.kind == kind && item.id == id)
            return &item;
    return nullptr;
}

juce::Rectangle<int> BoardStrip::footswitchBounds (juce::Rectangle<int> tile) const
{
    return tile.removeFromBottom (40).withSizeKeepingCentre (28, 28);
}

BoardStrip::Drop BoardStrip::dropAt (juce::Point<int> pos) const
{
    // The nearest gap before or after a tile (or before a "+"), on the row
    // and path the mouse is over.
    Drop best { Section::pre, 0, {} };
    auto bestScore = std::numeric_limits<int>::max();

    const auto consider = [&] (const Item& item, int index, int x)
    {
        const auto vertical = item.bounds.expanded (0, kLaneGap / 2);
        const auto dy = pos.y < vertical.getY() ? vertical.getY() - pos.y
                      : pos.y > vertical.getBottom() ? pos.y - vertical.getBottom() : 0;
        const auto score = dy * 1000 + std::abs (pos.x - x);
        if (score < bestScore)
        {
            bestScore = score;
            best = { item.section, index, { x - 1, item.bounds.getY(), 3, kTileHeight } };
        }
    };

    for (const auto& item : layout.items)
    {
        if (item.kind != Item::Kind::pedal && item.kind != Item::Kind::add)
            continue;
        consider (item, item.index, item.bounds.getX() - 3);
        if (item.kind == Item::Kind::pedal)
            consider (item, item.index + 1, item.bounds.getRight() + 3);
    }

    return best;
}

void BoardStrip::paint (juce::Graphics& g)
{
    layoutItems();
    const auto& items = layout.items;

    // Signal lines (under the tiles): along each row, through both paths,
    // and a cable from the end of a row to the start of the next.
    const auto active = board.isSplitActive();
    for (size_t k = 0; k + 1 < layout.flow.size(); ++k)
    {
        const auto& a = items[(size_t) layout.flow[k]];
        const auto& b = items[(size_t) layout.flow[k + 1]];

        if (a.kind == Item::Kind::split && b.kind == Item::Kind::merge)
        {
            g.setColour (active ? Palette::outline : Palette::outline.withAlpha (0.4f));
            for (const auto y : { a.bounds.getY() + kTileHeight / 2, a.bounds.getY() + kTileHeight + kLaneGap + kTileHeight / 2 })
                g.fillRect (a.bounds.getRight(), y - 1, b.bounds.getX() - a.bounds.getRight(), 2);
            continue;
        }

        g.setColour (Palette::outline);
        const auto rowA = layout.rowOf[(size_t) layout.flow[k]], rowB = layout.rowOf[(size_t) layout.flow[k + 1]];
        if (rowA == rowB)
        {
            g.fillRect (a.bounds.getRight(), a.bounds.getCentreY() - 1, b.bounds.getX() - a.bounds.getRight(), 2);
        }
        else
        {
            const auto& row = layout.rows[(size_t) rowA];
            const auto between = (float) (row.top + row.height + kRowGap / 2);
            const auto out = (float) a.bounds.getRight() + 8.0f, in = (float) b.bounds.getX() - 8.0f;

            juce::Path cable;
            cable.startNewSubPath ((float) a.bounds.getRight(), (float) a.bounds.getCentreY());
            cable.lineTo (out, (float) a.bounds.getCentreY());
            cable.lineTo (out, between);
            cable.lineTo (in, between);
            cable.lineTo (in, (float) b.bounds.getCentreY());
            cable.lineTo ((float) b.bounds.getX(), (float) b.bounds.getCentreY());
            g.strokePath (cable.createPathWithRoundedCorners (6.0f), juce::PathStrokeType (2.0f));
        }
    }

    const auto full = (int) board.getChain().size() >= BoardProcessor::kMaxPedals;

    for (const auto& item : items)
    {
        switch (item.kind)
        {
            case Item::Kind::pedal:
                if (dragging && item.id == pressedId)
                {
                    // Its old place stays as an outline while it is dragged.
                    g.setColour (Palette::outline);
                    g.drawRoundedRectangle (item.bounds.toFloat().reduced (1.0f), Style::kPanelRadius, 1.0f);
                }
                else
                {
                    paintTile (g, item.bounds, item.id, false);
                }
                break;

            case Item::Kind::add:
                if (! full || dragging)
                    paintAddTile (g, item);
                break;

            case Item::Kind::split:
            case Item::Kind::merge:
                paintNode (g, item);
                break;
        }
    }

    if (dragging)
    {
        const auto drop = dropAt (dragPos);
        g.setColour (Palette::boardAccent);
        g.fillRect (drop.marker);

        if (const auto* dragged = find (Item::Kind::pedal, pressedId))
            paintTile (g, dragged->bounds.withCentre (dragPos), pressedId, true);
    }
}

void BoardStrip::paintTile (juce::Graphics& g, juce::Rectangle<int> bounds, int id, bool ghost) const
{
    const auto* pedal = board.getPedal (id);
    if (pedal == nullptr)
        return;

    const auto family = familyOf (*pedal);
    const auto on = ! board.isPedalBypassed (id);
    const auto tile = bounds.toFloat();

    // Body in the family tint; a bypassed pedal is darker.
    auto body = Palette::knobBody.interpolatedWith (family, on ? 0.32f : 0.14f);
    if (id == hoverKey && ! ghost)
        body = body.brighter (0.06f);
    g.setColour (ghost ? body.withAlpha (0.85f) : body);
    g.fillRoundedRectangle (tile, Style::kPanelRadius);

    // Family stripe along the top
    g.setColour (family.withAlpha (on ? 1.0f : 0.45f));
    g.fillRoundedRectangle (tile.withHeight (5.0f), 2.5f);

    if (id == selected)
    {
        g.setColour (Palette::boardAccent);
        g.drawRoundedRectangle (tile.reduced (1.0f), Style::kPanelRadius, 2.0f);
    }

    // Name
    g.setColour (on ? Palette::text : Palette::textDim);
    g.setFont (Fonts::bold (bounds.getWidth() < 60 ? 11.5f : 13.0f));
    g.drawFittedText (shortNameOf (*pedal), bounds.withTrimmedTop (9).removeFromTop (18).reduced (3, 0),
                      juce::Justification::centred, 1, 0.8f);

    // LED
    const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ tile.getCentreX(), tile.getY() + 36.0f });
    if (on)
    {
        g.setColour (family.brighter (0.4f).withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
        g.setColour (family.brighter (0.5f));
    }
    else
    {
        g.setColour (Palette::background);
    }
    g.fillEllipse (led);

    // Footswitch
    const auto sw = footswitchBounds (bounds).toFloat();
    g.setColour (Palette::knobTrack);
    g.fillEllipse (sw);
    g.setColour (Palette::knobBody.brighter (0.15f));
    g.fillEllipse (sw.reduced (4.0f));
    g.setColour (Palette::outline);
    g.drawEllipse (sw, 1.0f);
}

void BoardStrip::paintAddTile (juce::Graphics& g, const Item& item) const
{
    const auto tile = item.bounds.toFloat().reduced (1.0f);
    const float dashes[] = { 4.0f, 4.0f };
    juce::Path outline, dashed;
    outline.addRoundedRectangle (tile, Style::kCornerRadius);
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);

    g.setColour (hoverKey == keyOf (item) ? Palette::text : Palette::textDim.withAlpha (0.7f));
    g.fillPath (dashed);
    g.setFont (Fonts::regular (20.0f));
    g.drawText ("+", item.bounds, juce::Justification::centred);
}

void BoardStrip::paintNode (juce::Graphics& g, const Item& item) const
{
    const auto active = board.isSplitActive();
    const auto node = item.bounds.toFloat();
    const auto isSplit = item.kind == Item::Kind::split;

    g.setColour (Palette::panel.brighter (hoverKey == keyOf (item) ? 0.08f : 0.0f));
    g.fillRoundedRectangle (node, Style::kCornerRadius);
    g.setColour (keyOf (item) == selected ? Palette::boardAccent : active ? Palette::textDim : Palette::outline);
    g.drawRoundedRectangle (node.reduced (0.5f), Style::kCornerRadius, keyOf (item) == selected ? 2.0f : 1.0f);

    // Name written upwards, and the split mode under it.
    juce::String text = isSplit ? "SPLIT" : "MERGE";
    if (isSplit)
        switch (board.getRouting().getSplitMode())
        {
            case SplitMode::full:      break;
            case SplitMode::frequency: text << "  " << juce::String::fromUTF8 ("\xc2\xb7") << "  FREQ"; break;
            case SplitMode::stereo:    text << "  " << juce::String::fromUTF8 ("\xc2\xb7") << "  L/R"; break;
        }

    // Which lane is which, at the ends of the node.
    g.setColour (active ? Palette::textDim : Palette::outline);
    g.setFont (Fonts::bold (11.0f));
    g.drawText ("A", item.bounds.withHeight (20), juce::Justification::centred);
    g.drawText ("B", item.bounds.withTrimmedTop (item.bounds.getHeight() - 20), juce::Justification::centred);

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, node.getCentreX(), node.getCentreY()));
    g.setColour (active ? Palette::text : Palette::textDim);
    g.setFont (Fonts::bold (12.0f));
    g.drawText (text, juce::Rectangle<float> (node.getHeight(), node.getWidth()).withCentre (node.getCentre()),
                juce::Justification::centred);
}

void BoardStrip::mouseMove (const juce::MouseEvent& e)
{
    const auto* item = itemAt (e.getPosition());
    const auto key = item != nullptr ? keyOf (*item) : 0;
    if (key != hoverKey)
    {
        hoverKey = key;
        repaint();
    }
}

void BoardStrip::mouseExit (const juce::MouseEvent&)
{
    hoverKey = 0;
    repaint();
}

void BoardStrip::mouseDown (const juce::MouseEvent& e)
{
    layoutItems();
    dragging = false;
    pressedId = 0;

    const auto* item = itemAt (e.getPosition());
    if (item == nullptr)
        return;

    switch (item->kind)
    {
        case Item::Kind::add:
            if ((int) board.getChain().size() < BoardProcessor::kMaxPedals)
                showAddMenu (item->section, item->bounds);
            break;

        case Item::Kind::split:
        case Item::Kind::merge:
            if (onSelect)
                onSelect (keyOf (*item));
            break;

        case Item::Kind::pedal:
            if (e.mods.isPopupMenu())
                showTileMenu (item->id);
            else
                pressedId = item->id;
            break;
    }
}

void BoardStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (pressedId <= 0)
        return;

    if (! dragging && e.getDistanceFromDragStart() > 5)
        dragging = true;

    if (dragging)
    {
        dragPos = e.getPosition();
        repaint();
    }
}

void BoardStrip::mouseUp (const juce::MouseEvent& e)
{
    const auto id = pressedId;
    pressedId = 0;

    if (id <= 0)
        return;

    if (dragging)
    {
        dragging = false;
        const auto drop = dropAt (e.getPosition());
        board.movePedal (id, drop.section, drop.index);
        repaint();
        return;
    }

    // A click: the footswitch toggles bypass, anywhere else opens the pedal.
    if (const auto* item = find (Item::Kind::pedal, id))
    {
        if (footswitchBounds (item->bounds).contains (e.getPosition()))
            board.setPedalBypassed (id, ! board.isPedalBypassed (id));
        else if (onSelect)
            onSelect (id);
    }

    repaint();
}

void BoardStrip::showAddMenu (Section section, juce::Rectangle<int> target)
{
    juce::PopupMenu menu;
    const auto& types = pedalTypes();

    std::optional<Palette::Family> family;
    for (int i = 0; i < (int) types.size(); ++i)
    {
        if (family != types[(size_t) i].family)
        {
            family = types[(size_t) i].family;
            menu.addSectionHeader (familyName (*family));
        }
        menu.addItem (i + 1, types[(size_t) i].shortName);
    }

    juce::Component::SafePointer<BoardStrip> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (target)),
                        [safe, section] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            const auto id = safe->board.addPedal (pedalTypes()[(size_t) result - 1].name, section);
                            if (id > 0 && safe->onSelect)
                                safe->onSelect (id);
                            safe->repaint();
                        });
}

void BoardStrip::showTileMenu (int id)
{
    juce::PopupMenu menu;
    const auto full = (int) board.getChain().size() >= BoardProcessor::kMaxPedals;
    menu.addItem (1, "Duplicate", ! full);
    menu.addItem (2, "Remove");
    if (board.canHaveTrails (id))
    {
        menu.addSeparator();
        menu.addItem (3, "Trails (echoes ring out when bypassed)", true, board.getTrails (id));
    }

    // The footswitch on an Assign slot (a host widget or footswitch).
    constexpr int assignBase = 1000, removeAssign = 1100;
    const auto current = board.findAssignment (id, "bypass");
    juce::PopupMenu slots;
    for (int slot = 0; slot < BoardProcessor::kNumAssigns; ++slot)
    {
        auto text = "Assign " + juce::String (slot + 1);
        if (slot != current)
            if (const auto used = board.describeAssignment (slot); used.isNotEmpty())
                text << "   (" << used << ")";
        slots.addItem (assignBase + slot, text, true, slot == current);
    }
    menu.addSeparator();
    menu.addSubMenu ("Assign On/Off to", slots);
    if (current >= 0)
        menu.addItem (removeAssign, "Remove Assign " + juce::String (current + 1));

    if (addTileMenuItems)
        addTileMenuItems (menu, id);

    juce::Component::SafePointer<BoardStrip> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safe, id] (int result)
                        {
                            if (safe == nullptr)
                                return;

                            if (result == 1)
                            {
                                const auto copy = safe->board.duplicatePedal (id);
                                if (copy > 0 && safe->onSelect)
                                    safe->onSelect (copy);
                            }
                            else if (result == 2)
                            {
                                safe->board.removePedal (id);
                            }
                            else if (result == 3)
                            {
                                safe->board.setTrails (id, ! safe->board.getTrails (id));
                            }
                            else if (result >= assignBase && result < assignBase + BoardProcessor::kNumAssigns)
                            {
                                safe->board.assign (result - assignBase, id, "bypass");
                            }
                            else if (result == removeAssign)
                            {
                                safe->board.clearAssignment (safe->board.findAssignment (id, "bypass"));
                            }
                            else if (result >= 100 && safe->onTileMenuResult)
                            {
                                safe->onTileMenuResult (result, id);
                            }
                            safe->repaint();
                        });
}

//==============================================================================
BoardEditor::BoardEditor (BoardProcessor& p)
    : juce::AudioProcessorEditor (p), board (p)
{
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick   = [this] { save(); };
    saveAsButton.onClick = [this] { saveAs(); };
    addAndMakeVisible (presetButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (saveAsButton);

    assignButton.setTooltip ("The assignable parameters (Assign 1 to 32) and what they drive.");
    assignButton.setClickingTogglesState (false);
    assignButton.onClick = [this] { showPedal (shownPedal == BoardStrip::kAssignView ? 0 : BoardStrip::kAssignView); };
    addAndMakeVisible (assignButton);

    sceneLabel.setText ("Scene", juce::dontSendNotification);
    sceneLabel.setFont (Fonts::regular (13.0f));
    sceneLabel.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (sceneLabel);

    for (int i = 0; i < BoardProcessor::kNumScenes; ++i)
    {
        auto& button = sceneButtons[(size_t) i];
        button.setButtonText (juce::String (i + 1));
        button.setTooltip ("Scene " + juce::String (i + 1) + " (right-click to copy it to another scene)");
        button.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                                  | (i < BoardProcessor::kNumScenes - 1 ? juce::Button::ConnectedOnRight : 0));
        button.onClick = [this, i] { board.setScene (i); refreshSceneButtons(); };
        button.addMouseListener (this, false);   // right-click: copy menu
        addAndMakeVisible (button);
    }
    refreshSceneButtons();

    // Standalone: tempo field, tap tempo, tuner (in a host, the host has these).
    tempoField.setEditable (true);
    tempoField.setJustificationType (juce::Justification::centred);
    tempoField.setColour (juce::Label::backgroundColourId, Palette::background);
    tempoField.setColour (juce::Label::outlineColourId, Palette::outline);
    tempoField.setTooltip ("Tempo for the pedals (20 to 300 BPM). Right-click Tap to follow MIDI clock.");
    tempoField.onTextChange = [this] { board.setTempo (tempoField.getText().getDoubleValue()); refreshTempo(); };
    tempoUnit.setText ("BPM", juce::dontSendNotification);
    tempoUnit.setFont (Fonts::regular (12.0f));
    tempoUnit.setColour (juce::Label::textColourId, Palette::textDim);
    tapButton.setTooltip ("Tap the tempo (right-click: MIDI learn, follow MIDI clock).");
    tapButton.onClick = [this] { board.tapTempo(); refreshTempo(); };
    tunerButton.setTooltip ("Tuner (right-click: MIDI learn).");
    tunerButton.onClick = [this] { board.setTunerActive (! board.isTunerActive()); syncTunerView(); };
    for (auto* c : std::initializer_list<juce::Component*> { &tempoField, &tempoUnit, &tapButton, &tunerButton })
    {
        c->addMouseListener (this, false);   // right-click menus
        addChildComponent (c);
        c->setVisible (board.isStandalone());
    }
    refreshTempo();

    noteLabel.setFont (Fonts::regular (12.0f));
    noteLabel.setJustificationType (juce::Justification::centredRight);
    noteLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (noteLabel);

    const auto setUpLevel = [this] (juce::Label& label, const juce::String& text, juce::Slider& slider,
                                    juce::RangedAudioParameter& param, std::unique_ptr<juce::SliderParameterAttachment>& attachment)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Fonts::regular (13.0f));
        label.setColour (juce::Label::textColourId, Palette::textDim);
        label.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        slider.setDoubleClickReturnValue (true, 0.0);
        attachment = std::make_unique<juce::SliderParameterAttachment> (param, slider);
        slider.setTextValueSuffix (" dB");
        addAndMakeVisible (slider);
    };
    setUpLevel (inputLabel, "In", inputSlider, board.getInputParam(), inputAttachment);
    setUpLevel (outputLabel, "Out", outputSlider, board.getOutputParam(), outputAttachment);

    strip.onSelect = [this] (int id) { showPedal (id); };
    strip.addTileMenuItems = [this] (juce::PopupMenu& menu, int id)
    {
        if (! board.isStandalone())
            return;
        menu.addSeparator();
        menu.addSectionHeader ("MIDI footswitch (pedal on / off)");
        addLearnItems (menu, LearnTarget::firstBypass + id - 1, 100);
    };
    strip.onTileMenuResult = [this] (int result, int id) { handleLearnResult (result, LearnTarget::firstBypass + id - 1, 100); };
    addAndMakeVisible (strip);

    // A pedal's editor must go before the pedal itself.
    board.pedalAboutToBeRemoved = [this] (int id)
    {
        if (id == shownPedal)
            showPedal (0);
    };

    // Last, so every child added above gets the change.
    setLookAndFeel (&lookAndFeel);

    updateSize();

    // Start with the first pedal open, if any.
    if (const auto chain = board.getChain(); ! chain.empty())
        showPedal (chain.front());

    refreshPresetButton();
    startTimerHz (15);
}

BoardEditor::~BoardEditor()
{
    board.pedalAboutToBeRemoved = nullptr;
    closePedalEditor();
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> BoardEditor::panelArea() const
{
    return getLocalBounds().withTrimmedTop (kHeaderHeight + kSceneBarHeight + stripHeight() + 8).reduced (kMargin, 0).withTrimmedBottom (kMargin);
}

int BoardEditor::stripHeight() const
{
    return strip.getPreferredHeight (kWidth);
}

void BoardEditor::updateSize()
{
    // Taller by a row as the board wraps; on a screen too small for that,
    // the pedal's controls get less room (they scale down).
    auto height = kHeaderHeight + kSceneBarHeight + stripHeight() + 8 + kPanelHeight + kMargin;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        height = juce::jmin (height, juce::jmax (640, (int) display->userBounds.getHeight() - 60));

    if (getWidth() != kWidth || getHeight() != height)
        setSize (kWidth, height);
    else
        resized();
}

void BoardEditor::paint (juce::Graphics& g)
{
    // Untinted: a white tint would only lighten the panels (and hide the
    // header buttons); the pedals carry the colour.
    g.fillAll (Palette::background);

    // Header band with the accent line along its bottom, as in every RKFX plugin.
    g.setColour (Palette::panel);
    g.fillRect (0, 0, getWidth(), kHeaderHeight);
    g.setColour (Palette::boardAccent);
    g.fillRect (0, kHeaderHeight - 2, getWidth(), 2);

    g.setColour (Palette::text);
    g.setFont (Fonts::bold (18.0f));
    g.drawText ("RKBoard", kMargin, 0, 120, kHeaderHeight - 2, juce::Justification::centredLeft);

    const auto panel = panelArea();
    if (pedalEditor == nullptr && routingPanel == nullptr && assignPanel == nullptr && tunerPanel == nullptr)
    {
        drawPanel (g, panel, Palette::panel);
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (15.0f));
        g.drawText (board.getChain().empty() ? "Click + to add a pedal." : "Click a pedal to show its controls.",
                    panel, juce::Justification::centred);
    }
}

void BoardEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (kHeaderHeight).withTrimmedBottom (2).reduced (kMargin, 8);
    header.removeFromLeft (110);   // the name, painted

    presetButton.setBounds (header.removeFromLeft (200));
    header.removeFromLeft (4);
    saveButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (4);
    saveAsButton.setBounds (header.removeFromLeft (70));
    header.removeFromLeft (12);
    assignButton.setBounds (header.removeFromLeft (70));

    outputSlider.setBounds (header.removeFromRight (150));
    outputLabel.setBounds (header.removeFromRight (34));
    header.removeFromRight (8);
    inputSlider.setBounds (header.removeFromRight (150));
    inputLabel.setBounds (header.removeFromRight (24));
    header.removeFromRight (8);
    noteLabel.setBounds (header);

    auto sceneBar = getLocalBounds().withTrimmedTop (kHeaderHeight).removeFromTop (kSceneBarHeight).reduced (kMargin, 5);
    const auto buttonWidth = 40;
    tunerButton.setBounds (sceneBar.removeFromRight (70));
    auto tempoArea = sceneBar.removeFromLeft (150);
    tempoField.setBounds (tempoArea.removeFromLeft (56));
    tempoUnit.setBounds (tempoArea.removeFromLeft (36));
    tapButton.setBounds (tempoArea.removeFromLeft (50));
    sceneBar = sceneBar.withSizeKeepingCentre (54 + BoardProcessor::kNumScenes * buttonWidth, sceneBar.getHeight());
    sceneLabel.setBounds (sceneBar.removeFromLeft (54));
    for (auto& button : sceneButtons)
        button.setBounds (sceneBar.removeFromLeft (buttonWidth));

    strip.setBounds (getLocalBounds().withTrimmedTop (kHeaderHeight + kSceneBarHeight).removeFromTop (stripHeight()));
    layoutPedalEditor();
}

void BoardEditor::showPedal (int id)
{
    if (id == shownPedal && (pedalEditor != nullptr || routingPanel != nullptr || assignPanel != nullptr
                             || tunerPanel != nullptr || id == 0))
        return;

    closePedalEditor();

    if (id == BoardStrip::kTunerView)
    {
        tunerPanel = std::make_unique<TunerPanel> (board);
        shownPedal = id;
        addAndMakeVisible (*tunerPanel);
        layoutPedalEditor();
    }
    else if (id == BoardStrip::kAssignView)
    {
        assignPanel = std::make_unique<AssignPanel> (board);
        assignPanel->onRowMenu = [this] (int slot, juce::Component& row)
        {
            showLearnMenu (LearnTarget::firstAssign + slot, row);
        };
        shownPedal = id;
        addAndMakeVisible (*assignPanel);
        layoutPedalEditor();
    }
    else if (id == BoardStrip::kSplitNode || id == BoardStrip::kMergeNode)
    {
        routingPanel = std::make_unique<RoutingPanel> (board, id == BoardStrip::kSplitNode ? RoutingPanel::Node::split
                                                                                           : RoutingPanel::Node::merge);
        shownPedal = id;
        addAndMakeVisible (*routingPanel);
        layoutPedalEditor();
    }
    else if (auto* pedal = board.getPedal (id))
    {
        pedalEditor.reset (pedal->createEditorAndMakeActive());
        if (pedalEditor != nullptr)
        {
            shownPedal = id;
            pedalBaseSize = pedalEditor->getLocalBounds();
            pedalEditor->setResizable (false, false);   // the board sizes it
            pedalEditor->addMouseListener (this, true);   // right-click on a knob: assign
            addAndMakeVisible (*pedalEditor);
            layoutPedalEditor();
        }
    }

    strip.setSelected (shownPedal);
    repaint();
}

void BoardEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || e.originalComponent == nullptr)
        return;

    for (int i = 0; i < BoardProcessor::kNumScenes; ++i)
        if (e.originalComponent == &sceneButtons[(size_t) i])
        {
            showSceneMenu (i);
            return;
        }

    if (e.originalComponent == &tapButton || e.originalComponent == &tempoField)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Follow MIDI clock", true, board.isFollowingClock());
        menu.addSeparator();
        addLearnItems (menu, LearnTarget::tap, 10);

        juce::Component::SafePointer<BoardEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (tapButton), [safe] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            if (result == 1)
                safe->board.setFollowClock (! safe->board.isFollowingClock());
            else
                safe->handleLearnResult (result, LearnTarget::tap, 10);
            safe->refreshTempo();
        });
        return;
    }

    if (e.originalComponent == &tunerButton)
    {
        showLearnMenu (LearnTarget::tuner, tunerButton);
        return;
    }

    if (pedalEditor == nullptr)
        return;

    auto* knob = dynamic_cast<rkfx::Knob*> (e.originalComponent);
    if (knob == nullptr)
        knob = e.originalComponent->findParentComponentOfClass<rkfx::Knob>();

    if (knob != nullptr && pedalEditor->isParentOf (knob))
        showAssignMenu (shownPedal, knob->getParameter());
}

void BoardEditor::showAssignMenu (int pedalId, juce::RangedAudioParameter& param)
{
    const auto paramId = param.getParameterID();
    const auto current = board.findAssignment (pedalId, paramId);

    juce::PopupMenu slots;
    for (int slot = 0; slot < BoardProcessor::kNumAssigns; ++slot)
    {
        auto text = "Assign " + juce::String (slot + 1);
        if (slot != current)
            if (const auto used = board.describeAssignment (slot); used.isNotEmpty())
                text << "   (" << used << ")";
        slots.addItem (slot + 1, text, true, slot == current);
    }

    const auto sceneControlled = board.isSceneControlled (pedalId, paramId);

    juce::PopupMenu menu;
    menu.addSectionHeader (param.getName (40));
    menu.addSubMenu ("Assign to", slots);
    if (current >= 0)
        menu.addItem (1000, "Remove Assign " + juce::String (current + 1));
    menu.addSeparator();
    menu.addItem (1002, "Scene controlled", true, sceneControlled);
    menu.addSeparator();
    menu.addItem (1001, "Show assignments");

    juce::Component::SafePointer<BoardEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safe, pedalId, paramId, current, sceneControlled] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result == 1002)
                            {
                                if (! safe->board.setSceneControlled (pedalId, paramId, ! sceneControlled))
                                    safe->showNote ("All 48 scene knobs are in use", true);
                            }
                            else if (result == 1000)
                                safe->board.clearAssignment (current);
                            else if (result == 1001)
                                safe->showPedal (BoardStrip::kAssignView);
                            else if (result >= 1 && result <= BoardProcessor::kNumAssigns)
                                safe->board.assign (result - 1, pedalId, paramId);
                        });
}

void BoardEditor::showSceneMenu (int scene)
{
    juce::PopupMenu copyTo;
    for (int i = 0; i < BoardProcessor::kNumScenes; ++i)
        copyTo.addItem (i + 1, "Scene " + juce::String (i + 1), i != scene);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Scene " + juce::String (scene + 1));
    menu.addSubMenu ("Copy to", copyTo);
    if (board.isStandalone())
    {
        menu.addSeparator();
        menu.addSectionHeader ("Scene selector (CC value 0-15 = scene 1-16)");
        addLearnItems (menu, LearnTarget::scene, 100);
    }

    juce::Component::SafePointer<BoardEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sceneButtons[(size_t) scene]),
                        [safe, scene] (int result)
                        {
                            if (safe != nullptr && safe->handleLearnResult (result, LearnTarget::scene, 100))
                                return;

                            if (safe != nullptr && result > 0)
                            {
                                safe->board.copyScene (scene, result - 1);
                                safe->showNote ("Scene " + juce::String (scene + 1) + " copied to scene " + juce::String (result), false);
                            }
                        });
}

void BoardEditor::addLearnItems (juce::PopupMenu& menu, int target, int base) const
{
    auto* map = const_cast<BoardProcessor&> (board).getMidiMap();
    if (! board.isStandalone() || map == nullptr)
        return;

    const auto mapping = map->find (target);
    menu.addItem (base, board.getLearningTarget() == target ? "MIDI learn (move a controller...)" : "MIDI learn");
    if (mapping)
    {
        menu.addItem (base + 1, "Clear MIDI (CC " + juce::String (mapping->cc) + ", channel " + juce::String (mapping->channel) + ")");
        if (LearnTarget::isSwitch (target))
        {
            menu.addItem (base + 2, "Switch toggles", true, ! mapping->momentary);
            menu.addItem (base + 3, "Switch momentary (on while held)", true, mapping->momentary);
        }
    }
}

bool BoardEditor::handleLearnResult (int result, int target, int base)
{
    auto* map = board.getMidiMap();
    if (map == nullptr || result < base || result > base + 3)
        return false;

    if (result == base)
    {
        board.startLearning (target);
        showNote ("MIDI learn: move a controller for " + LearnTarget::describe (target), false);
    }
    else if (result == base + 1)
    {
        map->clear (target);
        showNote ({}, false);
    }
    else if (auto mapping = map->find (target))
    {
        mapping->momentary = result == base + 3;
        map->set (*mapping);
    }
    return true;
}

void BoardEditor::showLearnMenu (int target, juce::Component& near)
{
    if (! board.isStandalone())
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader (LearnTarget::describe (target));
    addLearnItems (menu, target, 1);

    juce::Component::SafePointer<BoardEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (near), [safe, target] (int result)
    {
        if (safe != nullptr)
            safe->handleLearnResult (result, target, 1);
    });
}

void BoardEditor::syncTunerView()
{
    // The tuner opens and closes with its state (also from a footswitch).
    if (board.isTunerActive() && shownPedal != BoardStrip::kTunerView)
    {
        viewBeforeTuner = shownPedal;
        showPedal (BoardStrip::kTunerView);
    }
    else if (! board.isTunerActive() && shownPedal == BoardStrip::kTunerView)
    {
        showPedal (viewBeforeTuner);
    }
}

void BoardEditor::refreshTempo()
{
    if (! board.isStandalone() || tempoField.isBeingEdited())
        return;

    const auto bpm = board.getTempo();
    if (! juce::approximatelyEqual (bpm, shownTempo))
    {
        shownTempo = bpm;
        tempoField.setText (juce::String (bpm, 1), juce::dontSendNotification);
    }
    tempoField.setColour (juce::Label::textColourId, board.isFollowingClock() ? Palette::textDim : Palette::text);
    tapButton.setEnabled (! board.isFollowingClock());
}

void BoardEditor::refreshSceneButtons()
{
    for (int i = 0; i < BoardProcessor::kNumScenes; ++i)
        sceneButtons[(size_t) i].setToggleState (i == board.getScene(), juce::dontSendNotification);
}

void BoardEditor::closePedalEditor()
{
    // As a host does: the pedal forgets its editor before it is deleted, or it
    // refuses to make a new one.
    if (pedalEditor != nullptr)
        if (auto* pedal = board.getPedal (shownPedal))
            pedal->editorBeingDeleted (pedalEditor.get());

    pedalEditor.reset();
    routingPanel.reset();
    assignPanel.reset();
    tunerPanel.reset();
    shownPedal = 0;
}

void BoardEditor::layoutPedalEditor()
{
    // A panel built before it joined the window restyles with the board's look.
    for (auto* panel : std::initializer_list<juce::Component*> { routingPanel.get(), assignPanel.get(), tunerPanel.get() })
        if (panel != nullptr && &panel->getLookAndFeel() == &lookAndFeel && ! panel->getProperties().contains ("styled"))
        {
            panel->getProperties().set ("styled", true);
            panel->sendLookAndFeelChange();
        }

    if (routingPanel != nullptr)
        routingPanel->setBounds (panelArea());
    if (assignPanel != nullptr)
        assignPanel->setBounds (panelArea());
    if (tunerPanel != nullptr)
        tunerPanel->setBounds (panelArea().withHeight (juce::jmin (panelArea().getHeight(), 360)));
    tunerButton.setToggleState (shownPedal == BoardStrip::kTunerView, juce::dontSendNotification);
    assignButton.setToggleState (shownPedal == BoardStrip::kAssignView, juce::dontSendNotification);

    if (pedalEditor == nullptr)
        return;

    // The pedal's editor at its own size, scaled down only when it doesn't fit
    // (RKFX editors scale their whole canvas with the width).
    const auto baseWidth = pedalBaseSize.getWidth(), baseHeight = pedalBaseSize.getHeight();

    const auto area = panelArea();
    const auto scale = juce::jmin (1.0, area.getWidth() / (double) baseWidth, area.getHeight() / (double) baseHeight);
    const auto size = juce::Rectangle<int> (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
    pedalEditor->setBounds (size.withCentre (area.getCentre()).withY (area.getY()));
}

//==============================================================================
void BoardEditor::timerCallback()
{
    // MIDI learn finished (the audio thread heard a controller).
    if (const auto learning = board.getLearningTarget(); learning != lastLearning)
    {
        if (learning < 0 && lastLearning >= 0 && board.getMidiMap() != nullptr)
            if (const auto m = board.getMidiMap()->find (lastLearning))
                showNote (LearnTarget::describe (lastLearning) + ": CC " + juce::String (m->cc)
                          + ", channel " + juce::String (m->channel), false);
        lastLearning = learning;
    }

    if (board.isFollowingClock())
        refreshTempo();

    if (board.getPresetStateCounter() != lastPresetCounter)
        refreshPresetButton();

    if (board.getBoardCounter() != lastBoardCounter)
    {
        lastBoardCounter = board.getBoardCounter();

        // The shown pedal may have gone (a board preset or host state loaded).
        if (shownPedal > 0 && board.getPedal (shownPedal) == nullptr)
            showPedal (0);

        syncTunerView();

        // A row more or less: the window follows.
        if (strip.getHeight() != stripHeight())
            updateSize();

        strip.repaint();
        refreshSceneButtons();
        refreshTempo();
        repaint (panelArea());
        if (routingPanel != nullptr)
            routingPanel->repaint();
        if (assignPanel != nullptr)
            assignPanel->refresh();
    }
}

void BoardEditor::refreshPresetButton()
{
    lastPresetCounter = board.getPresetStateCounter();
    presetButton.setButtonText (board.getPresetName() + (board.isPresetModified() ? " *" : "")
                                + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
}

void BoardEditor::showNote (const juce::String& text, bool isError)
{
    noteLabel.setColour (juce::Label::textColourId, isError ? Palette::warning : Palette::textDim);
    noteLabel.setText (text, juce::dontSendNotification);
    noteLabel.setTooltip (text);
}

void BoardEditor::showPresetMenu()
{
    showNote ({}, false);

    const auto folder = board.getLibrary().getPresetFolder ("RKBoard");
    auto files = std::make_shared<std::vector<juce::File>>();

    juce::PopupMenu menu;

    // Factory presets first, in their own sub-menu (read-only).
    const auto& factory = board.getFactoryPresets();
    if (! factory.empty())
    {
        juce::PopupMenu factoryMenu;
        for (int i = 0; i < (int) factory.size(); ++i)
            factoryMenu.addItem (factoryBase + i, factory[(size_t) i].name, true,
                                 board.getPresetFile() == juce::File() && board.getPresetName() == factory[(size_t) i].name);
        menu.addSubMenu ("Factory", factoryMenu);
        menu.addSeparator();
    }

    addPresetMenuItems (menu, folder, *files, board.getPresetFile());
    if (files->empty())
        menu.addItem (-1, "No board presets of your own yet", false);

    menu.addSeparator();
    menu.addItem (resetCommand, "Empty board");
    menu.addItem (revealCommand, "Show presets folder");

    juce::Component::SafePointer<BoardEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton),
                        [safe, files, folder] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            auto& self = *safe;

                            if (result == resetCommand)
                            {
                                self.board.resetBoard();
                            }
                            else if (result == revealCommand)
                            {
                                folder.createDirectory();
                                folder.revealToUser();
                            }
                            else if (result >= factoryBase && result < resetCommand)
                            {
                                const auto r = self.board.loadFactoryPreset (result - factoryBase);
                                self.showNote (r.message, ! r.ok);

                                if (const auto chain = self.board.getChain(); ! chain.empty())
                                    self.showPedal (chain.front());
                            }
                            else if (result >= presetFileBase && result <= (int) files->size())
                            {
                                const auto r = self.board.loadBoardPreset ((*files)[(size_t) result - 1]);
                                self.showNote (r.message, ! r.ok);

                                if (const auto chain = self.board.getChain(); ! chain.empty())
                                    self.showPedal (chain.front());
                            }

                            self.refreshPresetButton();
                        });
}

void BoardEditor::save()
{
    showNote ({}, false);

    const auto file = board.getPresetFile();
    if (! file.existsAsFile())
    {
        saveAs();
        return;
    }

    juce::Component::SafePointer<BoardEditor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Save board")
                                      .withMessage ("Overwrite \"" + file.getFileNameWithoutExtension() + "\"?")
                                      .withButton ("Overwrite")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe, file] (int result)
                                  {
                                      if (safe == nullptr || result != 1)
                                          return;

                                      const auto r = safe->board.saveBoardPreset (file);
                                      safe->showNote (r.ok ? "Saved" : r.message, ! r.ok);
                                      safe->refreshPresetButton();
                                  });
}

void BoardEditor::saveAs()
{
    showNote ({}, false);

    const auto folder = board.getLibrary().getPresetFolder ("RKBoard");
    folder.createDirectory();

    const auto current = board.getPresetFile();
    const auto start = current.existsAsFile() ? current
                                               : folder.getChildFile (juce::File::createLegalFileName (board.getPresetName()) + ".json");

    chooser = std::make_unique<juce::FileChooser> ("Save board as", start, "*.json");

    juce::Component::SafePointer<BoardEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;

                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;

                              if (! file.hasFileExtension ("json"))
                                  file = file.withFileExtension ("json");

                              const auto r = safe->board.saveBoardPreset (file);
                              safe->showNote (r.ok ? "Saved" : r.message, ! r.ok);
                              safe->refreshPresetButton();
                          });
}

} // namespace rkboard
