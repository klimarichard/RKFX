#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kPreviewHeight = 130;
    constexpr int kSections = 4;
    const char* const kSectionTitles[kSections] = { "Time", "Repeats", "Pitch", "Routing" };
}

//==============================================================================
void RKEchoEditor::Preview::paint (juce::Graphics& g)
{
    using namespace rkfx;
    const auto& s = owner.settings;
    const auto note = owner.echo.lastNote.load() >= 0 ? owner.echo.lastNote.load() : 60;
    const auto velocity = owner.echo.lastNote.load() >= 0 ? owner.echo.lastVelocity.load() : 100;

    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::background);
    g.fillRoundedRectangle (area, Style::kPanelRadius);
    area = area.reduced (16.0f, 10.0f);
    const auto labels = area.removeFromBottom (16.0f);

    std::array<EchoEngine::Repeat, EchoEngine::kMaxRepeats> plan;
    const auto count = EchoEngine::planRepeats (s, note, velocity, 1, 1.0, plan);
    const auto span = (double) juce::jmax (1, s.repeats) + 0.6;
    auto xFor = [&] (double units) { return area.getX() + area.getWidth() * (float) (units / span); };
    const auto barWidth = juce::jmin (26.0f, area.getWidth() / (float) span * 0.4f);

    auto bar = [&] (double units, int vel, int n, juce::Colour colour)
    {
        const auto x = xFor (units);
        const auto h = area.getHeight() * (float) vel / 127.0f;
        g.setColour (colour);
        g.fillRoundedRectangle (x, area.getBottom() - h, barWidth, h, 3.0f);
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (11.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), juce::Rectangle<float> (x - 10.0f, labels.getY(), barWidth + 20.0f, labels.getHeight()),
                    juce::Justification::centred);
    };

    // The note played (dimmed when Original is off), then the repeats.
    bar (0.0, velocity, note, s.original ? Palette::text.withAlpha (0.8f) : Palette::textDim.withAlpha (0.4f));
    for (int k = 0; k < count; ++k)
        bar (plan[(size_t) k].offsetBeats, plan[(size_t) k].velocity, plan[(size_t) k].note, owner.accent());

    g.setColour (Palette::textDim);
    g.setFont (Fonts::regular (11.0f));
    g.drawText (owner.echo.lastNote.load() >= 0 ? "Last note played" : "Play a note (shown: C3)",
                getLocalBounds().toFloat().reduced (16.0f, 8.0f).removeFromTop (14.0f), juce::Justification::topRight);
}

//==============================================================================
RKEchoEditor::RKEchoEditor (RKEchoProcessor& p)
    : rkfx::EditorBase (p, 980, 500),
      echo (p),
      settings (p.currentSettings()),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    sync       = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    scaleSteps = std::make_unique<rkfx::ToggleSwitch> (param ("scale_steps"), accent(), "Scale Steps");
    original   = std::make_unique<rkfx::ToggleSwitch> (param ("original"), accent(), "Original");
    length     = std::make_unique<rkfx::SegmentedSwitch> (param ("length"), accent());
    time       = std::make_unique<Knob> (param ("time"), "Time");
    division   = std::make_unique<Knob> (param ("division"), "Time");
    repeats    = std::make_unique<Knob> (param ("repeats"), "Repeats");
    swing      = std::make_unique<Knob> (param ("swing"), "Swing");
    feedback   = std::make_unique<Knob> (param ("feedback"), "Feedback");
    velMin     = std::make_unique<Knob> (param ("vel_min"), "Min Velocity");
    gate       = std::make_unique<Knob> (param ("gate"), "Gate");
    step       = std::make_unique<Knob> (param ("step"), "Step");
    key        = std::make_unique<Knob> (param ("key"), "Key");
    scale      = std::make_unique<Knob> (param ("scale"), "Scale");
    channel    = std::make_unique<Knob> (param ("channel"), "Echo Channel");
    alternate  = std::make_unique<Knob> (param ("alternate"), "Alternate");

    addToCanvas (preview);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             sync.get(), scaleSteps.get(), original.get(), length.get(), time.get(), division.get(), repeats.get(), swing.get(),
             feedback.get(), velMin.get(), gate.get(), step.get(), key.get(), scale.get(), channel.get(), alternate.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    noteTimer.startTimerHz (15);
}

void RKEchoEditor::onParameterChange()
{
    settings = echo.currentSettings();
    time->setVisible (! settings.sync);
    division->setVisible (settings.sync);
    gate->setDimmed (settings.asPlayed);
    key->setDimmed (! settings.scaleSteps);
    scale->setDimmed (! settings.scaleSteps);
    preview.repaint();
}

void RKEchoEditor::checkNote()
{
    const auto note = echo.lastNote.load();
    if (note != shownNote)
    {
        shownNote = note;
        preview.repaint();
    }
}

juce::Rectangle<int> RKEchoEditor::previewArea() const { return contentArea().withHeight (kPreviewHeight); }

juce::Rectangle<int> RKEchoEditor::sectionArea (int index) const
{
    const auto area = contentArea().withTrimmedTop (kPreviewHeight + kGap);
    const auto width = (area.getWidth() - kGap * (kSections - 1)) / kSections;
    return { area.getX() + index * (width + kGap), area.getY(), width, area.getHeight() };
}

void RKEchoEditor::layoutContent()
{
    using rkfx::Knob;
    preview.setBounds (previewArea());

    auto section = [this] (int index, rkfx::ToggleSwitch* titleSwitch, juce::Component* row, std::initializer_list<Knob*> knobs)
    {
        auto area = sectionArea (index).reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 4);
        if (titleSwitch != nullptr)
            titleSwitch->setBounds (title.removeFromRight (100).withSizeKeepingCentre (100, 22));
        auto switchRow = area.removeFromTop (26);
        if (row != nullptr)
            row->setBounds (switchRow);
        area.removeFromTop (4);
        auto knobRow = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = knobRow.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (knobRow.removeFromLeft (width));
    };

    section (0, sync.get(), nullptr, { time.get(), repeats.get(), swing.get() });
    division->setBounds (time->getBounds());
    section (1, nullptr, length.get(), { feedback.get(), velMin.get(), gate.get() });
    section (2, scaleSteps.get(), nullptr, { step.get(), key.get(), scale.get() });
    section (3, original.get(), nullptr, { channel.get(), alternate.get() });
}

void RKEchoEditor::paintContent (juce::Graphics& g)
{
    for (int i = 0; i < kSections; ++i)
        drawSection (g, sectionArea (i), kSectionTitles[i]);
}
