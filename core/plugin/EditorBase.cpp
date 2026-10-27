#include "EditorBase.h"

namespace rkfx
{

namespace
{
    constexpr int kUtilityKnobWidth = 72;
    constexpr int kBypassWidth = 76;

    /** Adds the .json files and sub-folders of @p folder to @p menu, sub-folders
        as sub-menus. Item IDs index into @p files. */
    void addPresetItems (juce::PopupMenu& menu, const juce::File& folder, std::vector<juce::File>& files,
                         const juce::File& current)
    {
        auto entries = folder.findChildFiles (juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles,
                                              false, "*");
        std::sort (entries.begin(), entries.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });

        for (const auto& f : entries)
        {
            if (f.isDirectory())
            {
                juce::PopupMenu sub;
                addPresetItems (sub, f, files, current);
                if (sub.getNumItems() > 0)
                    menu.addSubMenu (f.getFileName(), sub);
            }
        }

        for (const auto& f : entries)
        {
            if (f.existsAsFile() && f.hasFileExtension ("json"))
            {
                files.push_back (f);
                menu.addItem ((int) files.size(), f.getFileNameWithoutExtension(), true, f == current);
            }
        }
    }

    enum MenuCommands
    {
        resetCommand = 100000,
        revealCommand,
        libraryCommand
    };
}

EditorBase::EditorBase (ProcessorBase& p, int width, int height)
    : juce::AudioProcessorEditor (p),
      proc (p),
      lookAndFeel (p.getConfig().accent),
      baseWidth (width),
      baseHeight (height)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    canvas.setBounds (0, 0, baseWidth, baseHeight);
    addAndMakeVisible (canvas);

    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick   = [this] { save(); };
    saveAsButton.onClick = [this] { saveAs(); };
    canvas.addAndMakeVisible (presetButton);
    canvas.addAndMakeVisible (saveButton);
    canvas.addAndMakeVisible (saveAsButton);

    noteLabel.setFont (Fonts::regular (12.0f));
    noteLabel.setJustificationType (juce::Justification::centredRight);
    noteLabel.setMinimumHorizontalScale (0.7f);
    canvas.addAndMakeVisible (noteLabel);

    if (auto* mix = proc.findParam ("mix"))
    {
        mixKnob = std::make_unique<Knob> (*mix, "Dry/Wet");
        canvas.addAndMakeVisible (*mixKnob);
    }

    outputKnob = std::make_unique<Knob> (*proc.findParam ("output"), "Output");
    canvas.addAndMakeVisible (*outputKnob);

    bypassSwitch = std::make_unique<ToggleSwitch> (*proc.findParam ("bypass"), accent(), "Bypass");
    canvas.addAndMakeVisible (*bypassSwitch);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    refreshPresetButton();
    startTimerHz (10);
}

EditorBase::~EditorBase()
{
    setLookAndFeel (nullptr);
}

void EditorBase::finishSetup()
{
    layoutBase();
    layoutContent();
    setSize (baseWidth, baseHeight);
}

//==============================================================================
juce::Rectangle<int> EditorBase::contentArea() const
{
    return juce::Rectangle<int> (0, 0, baseWidth, baseHeight)
               .withTrimmedTop (kHeaderHeight)
               .withTrimmedBottom (kBottomHeight)
               .reduced (kMargin);
}

juce::Rectangle<int> EditorBase::globalArea() const
{
    auto row = juce::Rectangle<int> (0, baseHeight - kBottomHeight, baseWidth, kBottomHeight).reduced (kMargin, 8);
    const auto utilityWidth = (mixKnob != nullptr ? 2 : 1) * kUtilityKnobWidth + kBypassWidth + 2 * kMargin;
    return row.withTrimmedRight (utilityWidth + kMargin);
}

void EditorBase::layoutBase()
{
    // Header: preset controls on the right.
    auto header = juce::Rectangle<int> (0, 0, baseWidth, kHeaderHeight).reduced (kMargin, 8);
    saveAsButton.setBounds (header.removeFromRight (70));
    header.removeFromRight (6);
    saveButton.setBounds (header.removeFromRight (54));
    header.removeFromRight (6);
    presetButton.setBounds (header.removeFromRight (juce::jmin (200, header.getWidth() / 2)));
    header.removeFromRight (8);
    header.removeFromLeft (140); // plugin name
    noteLabel.setBounds (header);

    // Bottom row: utility controls, always in the same spot.
    auto row = juce::Rectangle<int> (0, baseHeight - kBottomHeight, baseWidth, kBottomHeight).reduced (kMargin, 8);
    bypassSwitch->setBounds (row.removeFromRight (kBypassWidth).withSizeKeepingCentre (kBypassWidth, 30));
    row.removeFromRight (kMargin);
    outputKnob->setBounds (row.removeFromRight (kUtilityKnobWidth).withSizeKeepingCentre (kUtilityKnobWidth, Knob::kDefaultHeight));
    if (mixKnob != nullptr)
    {
        row.removeFromRight (kMargin);
        mixKnob->setBounds (row.removeFromRight (kUtilityKnobWidth).withSizeKeepingCentre (kUtilityKnobWidth, Knob::kDefaultHeight));
    }
}

void EditorBase::resized()
{
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
}

//==============================================================================
void EditorBase::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
}

void EditorBase::paintCanvas (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Header
    g.setColour (Palette::panel);
    g.fillRect (0, 0, baseWidth, kHeaderHeight);
    g.setColour (accent());
    g.fillRect (0, kHeaderHeight - 2, baseWidth, 2);

    g.setColour (Palette::text);
    g.setFont (Fonts::bold (18.0f));
    g.drawText (proc.getConfig().name, kMargin, 0, 200, kHeaderHeight, juce::Justification::centredLeft);

    // Bottom row
    g.setColour (Palette::panel);
    g.fillRect (0, baseHeight - kBottomHeight, baseWidth, kBottomHeight);
    g.setColour (Palette::outline);
    g.fillRect (0, baseHeight - kBottomHeight, baseWidth, 1);

    paintContent (g);
}

void EditorBase::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);

    if (title.isNotEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::bold (12.5f));
        g.drawText (title.toUpperCase(), area.reduced (10, 6).removeFromTop (16), juce::Justification::centredLeft);
    }
}

//==============================================================================
void EditorBase::timerCallback()
{
    if (proc.getPresetStateCounter() != lastPresetCounter)
        refreshPresetButton();
}

void EditorBase::refreshPresetButton()
{
    lastPresetCounter = proc.getPresetStateCounter();
    presetButton.setButtonText (proc.getPresetName() + (proc.isPresetModified() ? " *" : "")
                                + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
}

void EditorBase::showNote (const juce::String& text, bool isError)
{
    noteLabel.setColour (juce::Label::textColourId, isError ? Palette::warning : Palette::textDim);
    noteLabel.setText (text, juce::dontSendNotification);
    noteLabel.setTooltip (text);
}

void EditorBase::showPresetMenu()
{
    showNote ({}, false);

    auto& library = proc.getLibrary();
    const auto folder = library.getPresetFolder (proc.getConfig().name);

    auto files = std::make_shared<std::vector<juce::File>>();
    juce::PopupMenu menu;
    addPresetItems (menu, folder, *files, proc.getPresetFile());

    if (files->empty())
        menu.addItem (-1, "No presets yet", false);

    menu.addSeparator();
    menu.addItem (resetCommand, "Reset to defaults");
    menu.addItem (revealCommand, "Show presets folder");
    menu.addItem (libraryCommand, "Change library folder...");

    juce::Component::SafePointer<EditorBase> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton),
                        [safe, files, folder] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            auto& self = *safe;

                            if (result == resetCommand)
                            {
                                self.proc.resetToDefaults();
                            }
                            else if (result == revealCommand)
                            {
                                folder.createDirectory();
                                folder.revealToUser();
                            }
                            else if (result == libraryCommand)
                            {
                                self.chooseLibraryFolder();
                            }
                            else if (result > 0 && result <= (int) files->size())
                            {
                                const auto r = self.proc.loadPreset ((*files)[(size_t) result - 1]);
                                self.showNote (r.message, ! r.ok);
                            }

                            self.refreshPresetButton();
                        });
}

void EditorBase::save()
{
    showNote ({}, false);

    const auto file = proc.getPresetFile();
    if (! file.existsAsFile())
    {
        saveAs();
        return;
    }

    juce::Component::SafePointer<EditorBase> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Save preset")
                                      .withMessage ("Overwrite \"" + file.getFileNameWithoutExtension() + "\"?")
                                      .withButton ("Overwrite")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe, file] (int result)
                                  {
                                      if (safe == nullptr || result != 1)
                                          return;

                                      const auto r = safe->proc.savePreset (file);
                                      safe->showNote (r.ok ? "Saved" : r.message, ! r.ok);
                                      safe->refreshPresetButton();
                                  });
}

void EditorBase::saveAs()
{
    showNote ({}, false);

    const auto folder = proc.getLibrary().getPresetFolder (proc.getConfig().name);
    folder.createDirectory();

    const auto current = proc.getPresetFile();
    const auto start = current.existsAsFile() ? current
                                               : folder.getChildFile (juce::File::createLegalFileName (proc.getPresetName()) + ".json");

    chooser = std::make_unique<juce::FileChooser> ("Save preset as", start, "*.json");

    juce::Component::SafePointer<EditorBase> safe (this);
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

                              const auto r = safe->proc.savePreset (file);
                              safe->showNote (r.ok ? "Saved" : r.message, ! r.ok);
                              safe->refreshPresetButton();
                          });
}

void EditorBase::chooseLibraryFolder()
{
    auto& library = proc.getLibrary();
    chooser = std::make_unique<juce::FileChooser> ("Choose the RKFX preset library folder", library.getLibraryFolder());

    juce::Component::SafePointer<EditorBase> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;

                              const auto folder = fc.getResult();
                              if (folder == juce::File())
                                  return;

                              const auto ok = safe->proc.getLibrary().setLibraryFolder (folder);
                              safe->showNote (ok ? "Library: " + folder.getFullPathName()
                                                 : "Could not change the library folder", ! ok);
                          });
}

} // namespace rkfx
