#include "WahPedal.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKWah";
        c.hasMix = false;
        c.accent = juce::Colour (0xff8e5bd9); // Filter and EQ family, violet
        c.family = rkfx::Palette::Family::filterEQ;
        return c;
    }

    rkfx::ParamBuilder makeParams()
    {
        rkfx::ParamBuilder p;
        p.addPercent   ("position", "Position", 0.0f, 100.0f, 50.0f);
        p.addChoice    ("character", "Character", { "Classic", "Vocal", "Bass" }, { "classic", "vocal", "bass" }, 0);
        p.addFrequency ("freq_low", "Low Frequency", 200.0f, 800.0f, 400.0f);
        p.addFrequency ("freq_high", "High Frequency", 1000.0f, 4000.0f, 2200.0f);
        p.addPercent   ("resonance", "Resonance", 0.0f, 100.0f, 60.0f);
        p.addBool      ("auto_engage", "Auto Engage", false);
        return p;
    }
}

RKWahProcessor::RKWahProcessor()
    : rkfx::ProcessorBase (makeConfig(), makeParams()),
      position (findParam ("position")), character (findParam ("character")), freqLow (findParam ("freq_low")),
      freqHigh (findParam ("freq_high")), resonance (findParam ("resonance")), autoEngage (findParam ("auto_engage"))
{
}

WahEngine::Settings RKWahProcessor::currentSettings() const
{
    WahEngine::Settings s;
    s.position = valueOf (position) / 100.0f;
    s.character = (WahEngine::Character) juce::jlimit (0, 2, juce::roundToInt (valueOf (character)));
    s.lowHz = valueOf (freqLow);
    s.highHz = valueOf (freqHigh);
    s.resonance = valueOf (resonance) / 100.0f;
    s.autoEngage = valueOf (autoEngage) >= 0.5f;
    return s;
}

void RKWahProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
}

void RKWahProcessor::resetDsp()
{
    engine.reset (currentSettings());
}

void RKWahProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.process (buffer, currentSettings());
}

juce::AudioProcessorEditor* RKWahProcessor::createEditor()
{
    return new RKWahEditor (*this);
}

//==============================================================================
RKWahEditor::RKWahEditor (RKWahProcessor& p)
    : rkfx::EditorBase (p, 600, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    position   = std::make_unique<Knob> (param ("position"), "Position");
    freqLow    = std::make_unique<Knob> (param ("freq_low"), "Heel Freq");
    freqHigh   = std::make_unique<Knob> (param ("freq_high"), "Toe Freq");
    resonance  = std::make_unique<Knob> (param ("resonance"), "Resonance");
    character  = std::make_unique<rkfx::SegmentedSwitch> (param ("character"), accent(),
                                                          juce::StringArray { "Classic", "Vocal", "Bass" });
    autoEngage = std::make_unique<rkfx::ToggleSwitch> (param ("auto_engage"), accent(), "Auto Engage");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             position.get(), freqLow.get(), freqHigh.get(), resonance.get(), character.get(), autoEngage.get() })
        addToCanvas (*c);

    finishSetup();
}

void RKWahEditor::layoutContent()
{
    using rkfx::Knob;
    auto area = contentArea().reduced (8, 0).withTrimmedTop (30);

    auto switches = area.removeFromTop (26);
    character->setBounds (switches.removeFromLeft (270));
    autoEngage->setBounds (switches.removeFromRight (110));
    area.removeFromTop (14);

    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto quarter = knobs.getWidth() / 4;
    position->setBounds (knobs.removeFromLeft (quarter));
    freqLow->setBounds (knobs.removeFromLeft (quarter));
    freqHigh->setBounds (knobs.removeFromLeft (quarter));
    resonance->setBounds (knobs);
}

void RKWahEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Wah");
}
