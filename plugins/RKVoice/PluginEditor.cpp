#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kGap = 10;
    constexpr int kHarmonyWidth = 470;
    constexpr int kTopHeight = kTitleHeight + kSwitchHeight + 8 + rkfx::Knob::kDefaultHeight + 4;
    constexpr int kStatusHeight = 34;

    juce::String noteName (int note)
    {
        return note < 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) : juce::MidiMessage::getMidiNoteName (note, true, true, 4);
    }

    juce::String chordName (int classes)
    {
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        juce::StringArray held;
        for (int pc = 0; pc < 12; ++pc)
            if ((classes >> pc) & 1)
                held.add (names[pc]);
        return held.joinIntoString (" ");
    }
}

RKVoiceEditor::RKVoiceEditor (RKVoiceProcessor& p)
    : rkfx::EditorBase (p, 760, 620),
      voice (p),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const juce::String& id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode        = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray { "Chord", "Scale" });
    range       = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent(), juce::StringArray { "Voice", "Bass", "Guitar", "Violin" });
    chordTones  = std::make_unique<Knob> (param ("chord_tones"), "Chord Tones");
    noChord     = std::make_unique<Knob> (param ("no_chord"), "No Chord");
    midiChannel = std::make_unique<Knob> (param ("midi_channel"), "MIDI Ch");
    key         = std::make_unique<Knob> (param ("key"), "Key");
    scale       = std::make_unique<Knob> (param ("scale"), "Scale");
    threshold   = std::make_unique<Knob> (param ("threshold"), "Threshold");
    dry         = std::make_unique<Knob> (param ("dry"), "Dry");
    detune      = std::make_unique<Knob> (param ("detune"), "Detune");
    humanize    = std::make_unique<Knob> (param ("humanize"), "Humanize");
    glide       = std::make_unique<Knob> (param ("glide"), "Glide");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             mode.get(), range.get(), chordTones.get(), noChord.get(), midiChannel.get(), key.get(), scale.get(),
             threshold.get(), dry.get(), detune.get(), humanize.get(), glide.get() })
        addToCanvas (*c);

    for (int v = 0; v < VoiceEngine::kNumVoices; ++v)
    {
        auto& k = voices[(size_t) v];
        k.position = std::make_unique<Knob> (param (VoiceParams::voiceId (v + 1, "position")), "In a Chord");
        k.interval = std::make_unique<Knob> (param (VoiceParams::voiceId (v + 1, "interval")), "In the Scale");
        k.level    = std::make_unique<Knob> (param (VoiceParams::voiceId (v + 1, "level")), "Level");
        k.pan      = std::make_unique<Knob> (param (VoiceParams::voiceId (v + 1, "pan")), "Pan");
        for (auto* c : { k.position.get(), k.interval.get(), k.level.get(), k.pan.get() })
            addToCanvas (*c);
    }

    finishSetup();
    onParameterChange();
    refresher.startTimerHz (15);   // the status line
}

void RKVoiceEditor::onParameterChange()
{
    // Dim what isn't in use: the chord settings in Scale mode; the scale (key,
    // scale, intervals) in Chord mode unless it stands in for a missing chord.
    const auto s = voice.currentSettings();
    const auto chord = s.mode == VoiceEngine::Mode::chord;
    const auto scaleUsed = ! chord || s.noChord == VoiceEngine::NoChord::scale;

    chordTones->setDimmed (! chord);
    noChord->setDimmed (! chord);
    midiChannel->setDimmed (! chord);
    key->setDimmed (! scaleUsed);
    scale->setDimmed (! scaleUsed);
    for (auto& k : voices)
    {
        k.position->setDimmed (! chord);
        k.interval->setDimmed (! scaleUsed);
    }
}

juce::Rectangle<int> RKVoiceEditor::harmonyArea() const { return contentArea().removeFromTop (kTopHeight).withWidth (kHarmonyWidth); }
juce::Rectangle<int> RKVoiceEditor::inputArea() const   { return contentArea().removeFromTop (kTopHeight).withTrimmedLeft (kHarmonyWidth + kGap); }
juce::Rectangle<int> RKVoiceEditor::statusArea() const  { return contentArea().withTrimmedTop (kTopHeight + kGap).withHeight (kStatusHeight); }
juce::Rectangle<int> RKVoiceEditor::voicesArea() const  { return contentArea().withTrimmedTop (kTopHeight + kGap + kStatusHeight + kGap); }

juce::Rectangle<int> RKVoiceEditor::voiceColumn (int v) const
{
    auto area = voicesArea().reduced (8, 0).withTrimmedTop (kTitleHeight);
    const auto width = area.getWidth() / VoiceEngine::kNumVoices;
    return area.withX (area.getX() + v * width).withWidth (width);
}

void RKVoiceEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = harmonyArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        mode->setBounds (area.removeFromTop (kSwitchHeight).withWidth (170));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = knobs.getWidth() / 5;
        for (auto* k : { chordTones.get(), noChord.get(), midiChannel.get(), key.get(), scale.get() })
            k->setBounds (knobs.removeFromLeft (width));
    }

    {
        auto area = inputArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        range->setBounds (area.removeFromTop (kSwitchHeight));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = knobs.getWidth() / 2;
        threshold->setBounds (knobs.removeFromLeft (width));
        dry->setBounds (knobs);
    }

    for (int v = 0; v < VoiceEngine::kNumVoices; ++v)
    {
        auto column = voiceColumn (v).withTrimmedTop (18);
        auto& k = voices[(size_t) v];
        auto top = column.removeFromTop (Knob::kDefaultHeight);
        k.position->setBounds (top.removeFromLeft (top.getWidth() / 2));
        k.interval->setBounds (top);
        auto bottom = column.removeFromTop (Knob::kDefaultHeight);
        k.level->setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
        k.pan->setBounds (bottom);
    }

    auto global = globalArea();
    for (auto* k : { detune.get(), humanize.get(), glide.get() })
        k->setBounds (global.removeFromLeft (Knob::kDefaultWidth + 8).withSizeKeepingCentre (Knob::kDefaultWidth, Knob::kDefaultHeight));
}

void RKVoiceEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, harmonyArea(), "Harmony");
    drawSection (g, inputArea(), "Input");
    drawSection (g, voicesArea(), "Voices");

    for (int v = 0; v < VoiceEngine::kNumVoices; ++v)
    {
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (12.0f));
        g.drawText ("Voice " + juce::String (v + 1), voiceColumn (v).removeFromTop (18), juce::Justification::centred);
    }

    // What's happening: the sung note, the chord in use, each voice's note.
    const auto& engine = voice.getEngine();
    auto status = statusArea();
    g.setColour (rkfx::Palette::panel);
    g.fillRoundedRectangle (status.toFloat(), 6.0f);
    status = status.reduced (12, 0);

    const auto sung = engine.getSungNote();
    const auto classes = engine.getChordPitchClasses();
    const auto s = voice.currentSettings();
    juce::String chord = s.mode == VoiceEngine::Mode::scale ? juce::String ("Scale")
                         : classes != 0 ? chordName (classes)
                         : s.noChord == VoiceEngine::NoChord::scale ? juce::String ("no chord: scale")
                         : juce::String ("no chord");

    auto field = [&] (const juce::String& label, const juce::String& value, int width, bool on)
    {
        auto area = status.removeFromLeft (width);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (11.0f));
        const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 6;
        g.drawText (label, area.removeFromLeft (labelWidth), juce::Justification::centredLeft);
        g.setColour (on ? rkfx::Palette::text : rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (14.0f));
        g.drawText (value, area, juce::Justification::centredLeft, true);
    };
    field ("Singing", noteName (sung), 120, sung >= 0);
    field ("Chord", chord, 210, classes != 0 || s.mode == VoiceEngine::Mode::scale);
    for (int v = 0; v < VoiceEngine::kNumVoices; ++v)
    {
        const auto n = engine.getVoiceNote (v);
        field (juce::String (v + 1), noteName (n), (status.getWidth() + 1) / (VoiceEngine::kNumVoices - v), n >= 0);
    }
}
