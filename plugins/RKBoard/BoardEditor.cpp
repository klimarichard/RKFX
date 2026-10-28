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

void BoardStrip::setSelected (int id)
{
    if (selected != id)
    {
        selected = id;
        repaint();
    }
}

juce::Rectangle<int> BoardStrip::tileBounds (int position) const
{
    // Centred row of up to 12 tiles (+ the add tile).
    const auto count = juce::jmin ((int) board.getChain().size() + 1, BoardProcessor::kMaxPedals);
    const auto rowWidth = count * kTileWidth + (count - 1) * kTileGap;
    const auto x0 = (getWidth() - rowWidth) / 2;
    return { x0 + position * (kTileWidth + kTileGap), (getHeight() - kTileHeight) / 2, kTileWidth, kTileHeight };
}

juce::Rectangle<int> BoardStrip::footswitchBounds (juce::Rectangle<int> tile) const
{
    return tile.removeFromBottom (48).withSizeKeepingCentre (34, 34);
}

int BoardStrip::tileAt (juce::Point<int> pos) const
{
    const auto chain = board.getChain();
    for (int i = 0; i < (int) chain.size(); ++i)
        if (tileBounds (i).contains (pos))
            return chain[(size_t) i];

    if ((int) chain.size() < BoardProcessor::kMaxPedals && tileBounds ((int) chain.size()).contains (pos))
        return -1;

    return 0;
}

int BoardStrip::insertionIndex (int x) const
{
    // Number of tiles whose centre lies left of x (in the chain before the move).
    const auto chain = board.getChain();
    int index = 0;
    for (int i = 0; i < (int) chain.size(); ++i)
        if (tileBounds (i).getCentreX() < x)
            index = i + 1;
    return index;
}

void BoardStrip::paint (juce::Graphics& g)
{
    const auto chain = board.getChain();

    // The signal line through the row.
    if (! chain.empty())
    {
        const auto first = tileBounds (0), last = tileBounds ((int) chain.size() - 1);
        g.setColour (Palette::outline);
        g.fillRect (juce::Rectangle<int> (first.getCentreX(), first.getY() + 30, last.getCentreX() - first.getCentreX(), 2));
    }

    for (int i = 0; i < (int) chain.size(); ++i)
    {
        const auto id = chain[(size_t) i];
        if (dragging && id == pressedId)
        {
            // Its old place stays as an outline while it is dragged.
            g.setColour (Palette::outline);
            g.drawRoundedRectangle (tileBounds (i).toFloat().reduced (1.0f), Style::kPanelRadius, 1.0f);
            continue;
        }
        paintTile (g, tileBounds (i), id, false);
    }

    if ((int) chain.size() < BoardProcessor::kMaxPedals)
        paintAddTile (g, tileBounds ((int) chain.size()));

    if (dragging)
    {
        // Insertion marker, then the dragged tile under the mouse.
        const auto index = insertionIndex (dragX);
        const auto x = index < (int) chain.size() ? tileBounds (index).getX() - kTileGap / 2
                                                  : tileBounds ((int) chain.size() - 1).getRight() + kTileGap / 2;
        g.setColour (Palette::boardAccent);
        g.fillRect (x - 1, tileBounds (0).getY(), 3, kTileHeight);

        auto ghost = tileBounds (0).withX (dragX - kTileWidth / 2).translated (0, -4);
        paintTile (g, ghost, pressedId, true);
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
    if (id == hoverId && ! ghost)
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
    g.setFont (Fonts::bold (13.0f));
    g.drawFittedText (shortNameOf (*pedal), bounds.withTrimmedTop (10).removeFromTop (20).reduced (4, 0),
                      juce::Justification::centred, 1);

    // LED
    const auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ tile.getCentreX(), tile.getY() + 46.0f });
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
    g.fillEllipse (sw.reduced (5.0f));
    g.setColour (Palette::outline);
    g.drawEllipse (sw, 1.0f);
}

void BoardStrip::paintAddTile (juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    const auto tile = bounds.toFloat().reduced (1.0f);
    const float dashes[] = { 4.0f, 4.0f };
    juce::Path outline, dashed;
    outline.addRoundedRectangle (tile, Style::kPanelRadius);
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);

    g.setColour (hoverId == -1 ? Palette::text : Palette::textDim);
    g.fillPath (dashed);
    g.setFont (Fonts::regular (26.0f));
    g.drawText ("+", bounds.withTrimmedBottom (24), juce::Justification::centred);
    g.setFont (Fonts::regular (12.0f));
    g.drawText ("Add pedal", bounds.withTrimmedTop (bounds.getHeight() - 40).withTrimmedBottom (14),
                juce::Justification::centred);
}

void BoardStrip::mouseMove (const juce::MouseEvent& e)
{
    const auto id = tileAt (e.getPosition());
    if (id != hoverId)
    {
        hoverId = id;
        repaint();
    }
}

void BoardStrip::mouseExit (const juce::MouseEvent&)
{
    hoverId = 0;
    repaint();
}

void BoardStrip::mouseDown (const juce::MouseEvent& e)
{
    pressedId = tileAt (e.getPosition());
    dragging = false;

    if (pressedId == -1)
    {
        showAddMenu (tileBounds ((int) board.getChain().size()));
        pressedId = 0;
        return;
    }

    if (pressedId > 0 && e.mods.isPopupMenu())
    {
        showTileMenu (pressedId);
        pressedId = 0;
    }
}

void BoardStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (pressedId <= 0)
        return;

    if (! dragging && e.getDistanceFromDragStart() > 5 && board.getChain().size() > 1)
        dragging = true;

    if (dragging)
    {
        dragX = e.x;
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
        board.movePedal (id, insertionIndex (e.x));
        repaint();
        return;
    }

    // A click: the footswitch toggles bypass, anywhere else opens the pedal.
    const auto chain = board.getChain();
    const auto position = (int) std::distance (chain.begin(), std::find (chain.begin(), chain.end(), id));
    if (footswitchBounds (tileBounds (position)).contains (e.getPosition()))
        board.setPedalBypassed (id, ! board.isPedalBypassed (id));
    else if (onSelect)
        onSelect (id);

    repaint();
}

void BoardStrip::showAddMenu (juce::Rectangle<int> target)
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
                        [safe] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            const auto id = safe->board.addPedal (pedalTypes()[(size_t) result - 1].name);
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
    pedalEditor.reset();
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> BoardEditor::panelArea() const
{
    return getLocalBounds().withTrimmedTop (kHeaderHeight + BoardStrip::kTileHeight + 32).reduced (kMargin, 0).withTrimmedBottom (kMargin);
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
    if (pedalEditor == nullptr)
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

    strip.setBounds (getLocalBounds().withTrimmedTop (kHeaderHeight).removeFromTop (BoardStrip::kTileHeight + 32));
    layoutPedalEditor();
}

void BoardEditor::showPedal (int id)
{
    if (id == shownPedal && (pedalEditor != nullptr || id == 0))
        return;

    pedalEditor.reset();
    shownPedal = 0;

    if (auto* pedal = board.getPedal (id))
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

void BoardEditor::layoutPedalEditor()
{
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
        if (shownPedal != 0 && board.getPedal (shownPedal) == nullptr)
            showPedal (0);

        strip.repaint();
        repaint (panelArea());
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
