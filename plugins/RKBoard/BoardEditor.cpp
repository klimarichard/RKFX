#include "BoardEditor.h"

#include "plugin/EditorBase.h"

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

void BoardStrip::layoutItems()
{
    constexpr int gap = 6, addWidth = 30, nodeWidth = 34, margin = 12;

    const auto& pre = board.getSection (Section::pre);
    const auto& pathA = board.getSection (Section::pathA);
    const auto& pathB = board.getSection (Section::pathB);
    const auto& post = board.getSection (Section::post);
    const auto lanes = (int) juce::jmax (pathA.size(), pathB.size());
    const auto columns = (int) pre.size() + lanes + (int) post.size();

    // Tiles shrink (down to 44 px) so twelve pedals and the fixed parts fit.
    const auto fixed = 3 * (addWidth + gap) + 2 * (nodeWidth + gap);
    const auto tileWidth = columns > 0 ? juce::jlimit (44, 70, (getWidth() - 2 * margin - fixed) / columns - gap) : 70;
    const auto total = columns * (tileWidth + gap) + fixed - gap;

    const auto laneA = kPadding, laneB = kPadding + kTileHeight + kLaneGap;
    const auto middle = (getHeight() - kTileHeight) / 2;
    auto x = (getWidth() - total) / 2;

    items.clear();
    const auto addSection = [&] (const std::vector<int>& ids, Section section, int startX, int y)
    {
        auto tx = startX;
        for (int i = 0; i < (int) ids.size(); ++i, tx += tileWidth + gap)
            items.push_back ({ Item::Kind::pedal, ids[(size_t) i], section, i, { tx, y, tileWidth, kTileHeight } });
        items.push_back ({ Item::Kind::add, 0, section, (int) ids.size(), { tx, y, addWidth, kTileHeight } });
    };

    addSection (pre, Section::pre, x, middle);
    x += (int) pre.size() * (tileWidth + gap) + addWidth + gap;

    items.push_back ({ Item::Kind::split, 0, Section::pre, 0, { x, laneA, nodeWidth, laneB + kTileHeight - laneA } });
    x += nodeWidth + gap;

    addSection (pathA, Section::pathA, x, laneA);
    addSection (pathB, Section::pathB, x, laneB);
    x += lanes * (tileWidth + gap) + addWidth + gap;

    items.push_back ({ Item::Kind::merge, 0, Section::post, 0, { x, laneA, nodeWidth, laneB + kTileHeight - laneA } });
    x += nodeWidth + gap;

    addSection (post, Section::post, x, middle);
}

const BoardStrip::Item* BoardStrip::itemAt (juce::Point<int> pos) const
{
    for (const auto& item : items)
        if (item.bounds.contains (pos))
            return &item;
    return nullptr;
}

const BoardStrip::Item* BoardStrip::find (Item::Kind kind, int id) const
{
    for (const auto& item : items)
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
    const auto* split = find (Item::Kind::split);
    const auto* merge = find (Item::Kind::merge);
    const auto laneBTop = kPadding + kTileHeight + kLaneGap / 2;

    auto section = Section::pre;
    if (pos.x >= merge->bounds.getX())
        section = Section::post;
    else if (pos.x >= split->bounds.getRight())
        section = pos.y < laneBTop ? Section::pathA : Section::pathB;

    // Before the first tile of the section whose centre is right of the mouse,
    // else just before the section's "+".
    for (const auto& item : items)
        if (item.section == section && (item.kind == Item::Kind::pedal || item.kind == Item::Kind::add)
            && (item.kind == Item::Kind::add || item.bounds.getCentreX() > pos.x))
        {
            const auto marker = juce::Rectangle<int> (item.bounds.getX() - 5, item.bounds.getY(), 3, kTileHeight);
            return { section, item.index, marker };
        }

    return { section, 0, {} };
}

void BoardStrip::paint (juce::Graphics& g)
{
    layoutItems();

    // Signal lines (under the tiles): into the split, through each lane, out of the merge.
    const auto* split = find (Item::Kind::split);
    const auto* merge = find (Item::Kind::merge);
    const auto active = board.isSplitActive();
    const auto middle = getHeight() / 2;
    const auto laneA = kPadding + kTileHeight / 2, laneB = kPadding + kTileHeight + kLaneGap + kTileHeight / 2;

    g.setColour (Palette::outline);
    g.fillRect (items.front().bounds.getX(), middle - 1, split->bounds.getX() - items.front().bounds.getX(), 2);
    g.fillRect (merge->bounds.getRight(), middle - 1, items.back().bounds.getRight() - merge->bounds.getRight(), 2);
    g.setColour (active ? Palette::outline : Palette::outline.withAlpha (0.4f));
    for (const auto y : { laneA, laneB })
        g.fillRect (split->bounds.getRight(), y - 1, merge->bounds.getX() - split->bounds.getRight(), 2);

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
    addAndMakeVisible (strip);

    // A pedal's editor must go before the pedal itself.
    board.pedalAboutToBeRemoved = [this] (int id)
    {
        if (id == shownPedal)
            showPedal (0);
    };

    // Last, so every child added above gets the change.
    setLookAndFeel (&lookAndFeel);

    setSize (kWidth, kHeight);

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
    return getLocalBounds().withTrimmedTop (kHeaderHeight + BoardStrip::kHeight + 8).reduced (kMargin, 0).withTrimmedBottom (kMargin);
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
    if (pedalEditor == nullptr && routingPanel == nullptr)
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

    outputSlider.setBounds (header.removeFromRight (150));
    outputLabel.setBounds (header.removeFromRight (34));
    header.removeFromRight (8);
    inputSlider.setBounds (header.removeFromRight (150));
    inputLabel.setBounds (header.removeFromRight (24));
    header.removeFromRight (8);
    noteLabel.setBounds (header);

    strip.setBounds (getLocalBounds().withTrimmedTop (kHeaderHeight + 4).removeFromTop (BoardStrip::kHeight));
    layoutPedalEditor();
}

void BoardEditor::showPedal (int id)
{
    if (id == shownPedal && (pedalEditor != nullptr || routingPanel != nullptr || id == 0))
        return;

    closePedalEditor();

    if (id == BoardStrip::kSplitNode || id == BoardStrip::kMergeNode)
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
            addAndMakeVisible (*pedalEditor);
            layoutPedalEditor();
        }
    }

    strip.setSelected (shownPedal);
    repaint();
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
    shownPedal = 0;
}

void BoardEditor::layoutPedalEditor()
{
    if (routingPanel != nullptr)
        routingPanel->setBounds (panelArea());

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
    if (board.getPresetStateCounter() != lastPresetCounter)
        refreshPresetButton();

    if (board.getBoardCounter() != lastBoardCounter)
    {
        lastBoardCounter = board.getBoardCounter();

        // The shown pedal may have gone (a board preset or host state loaded).
        if (shownPedal > 0 && board.getPedal (shownPedal) == nullptr)
            showPedal (0);

        strip.repaint();
        repaint (panelArea());
        if (routingPanel != nullptr)
            routingPanel->repaint();
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
    addPresetMenuItems (menu, folder, *files, board.getPresetFile());
    if (files->empty())
        menu.addItem (-1, "No board presets yet", false);

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
