#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kVocoderWidth = 420;
}

RKVocodeEditor::RKVocodeEditor (RKVocodeProcessor& p)
    : rkfx::EditorBase (p, 820, 470),
      vocoder (p),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    bands    = std::make_unique<rkfx::SegmentedSwitch> (param ("bands"), accent(), juce::StringArray {}, "Bands");
    carrier  = std::make_unique<rkfx::SegmentedSwitch> (param ("carrier"), accent());
    wave     = std::make_unique<rkfx::SegmentedSwitch> (param ("osc_wave"), accent(), juce::StringArray { "Saw", "Pulse", "Noise", "Saw+N" });
    mode     = std::make_unique<rkfx::SegmentedSwitch> (param ("osc_mode"), accent());
    freqLow  = std::make_unique<Knob> (param ("freq_low"), "Low");
    freqHigh = std::make_unique<Knob> (param ("freq_high"), "High");
    formant  = std::make_unique<Knob> (param ("formant"), "Formant");
    attack   = std::make_unique<Knob> (param ("attack"), "Attack");
    release  = std::make_unique<Knob> (param ("release"), "Release");
    unvoiced = std::make_unique<Knob> (param ("unvoiced"), "Unvoiced");
    modMix   = std::make_unique<Knob> (param ("mod_mix"), "Mod Mix");
    note     = std::make_unique<Knob> (param ("osc_note"), "Note");
    voices   = std::make_unique<Knob> (param ("osc_voices"), "Voices");
    glide    = std::make_unique<Knob> (param ("osc_glide"), "Glide");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             bands.get(), carrier.get(), wave.get(), mode.get(), freqLow.get(), freqHigh.get(), formant.get(),
             attack.get(), release.get(), unvoiced.get(), modMix.get(), note.get(), voices.get(), glide.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
}

void RKVocodeEditor::onParameterChange()
{
    const auto s = vocoder.currentSettings();
    const auto internal = s.carrier == VocodeEngine::Carrier::internal;
    const auto midi = s.pitchMode == VocodeEngine::PitchMode::midi;

    wave->setAlpha (internal ? 1.0f : 0.45f);
    mode->setAlpha (internal ? 1.0f : 0.45f);
    note->setDimmed (! internal || midi);
    voices->setDimmed (! internal || ! midi);
    glide->setDimmed (! internal || ! midi);
}

juce::Rectangle<int> RKVocodeEditor::vocoderArea() const { return contentArea().withWidth (kVocoderWidth); }
juce::Rectangle<int> RKVocodeEditor::carrierArea() const { return contentArea().withTrimmedLeft (kVocoderWidth + kGap); }

void RKVocodeEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = vocoderArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        bands->setBounds (area.removeFromTop (44).withWidth (220));
        area.removeFromTop (6);
        auto row1 = area.removeFromTop (Knob::kDefaultHeight);
        const auto quarter = row1.getWidth() / 4;
        for (auto* k : { freqLow.get(), freqHigh.get(), formant.get(), unvoiced.get() })
            k->setBounds (row1.removeFromLeft (quarter));
        auto row2 = area.removeFromTop (Knob::kDefaultHeight);
        for (auto* k : { attack.get(), release.get(), modMix.get() })
            k->setBounds (row2.removeFromLeft (quarter));
    }

    {
        auto area = carrierArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        carrier->setBounds (area.removeFromTop (26));
        area.removeFromTop (6);
        wave->setBounds (area.removeFromTop (26));
        area.removeFromTop (6);
        mode->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        note->setBounds (knobs.removeFromLeft (third));
        voices->setBounds (knobs.removeFromLeft (third));
        glide->setBounds (knobs);
    }
}

void RKVocodeEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, vocoderArea(), "Vocoder");
    drawSection (g, carrierArea(), "Carrier");
}
