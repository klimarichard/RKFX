#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "VoiceEngine.h"

/** RKVoice: harmony voices for a single voice or instrument, placed by a
    held MIDI chord or by a key and scale. */
class RKVoiceProcessor : public rkfx::ProcessorBase
{
public:
    RKVoiceProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    VoiceEngine::Settings currentSettings() const;
    const VoiceEngine& getEngine() const noexcept { return engine; }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *range, *chordTones, *noChord, *midiChannel, *key, *scale,
                               *detune, *humanize, *glide, *dry, *threshold;
    struct VoiceControls { juce::RangedAudioParameter *position, *interval, *level, *pan; };
    std::array<VoiceControls, VoiceEngine::kNumVoices> voiceControls;
    VoiceEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVoiceProcessor)
};
