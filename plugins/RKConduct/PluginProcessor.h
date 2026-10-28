#pragma once

#include "plugin/ProcessorBase.h"

#include "ConductEngine.h"
#include "Parameters.h"

/** RKConduct: a MIDI file played by tapping its beats (MIDI effect). */
class RKConductProcessor : public rkfx::ProcessorBase
{
public:
    RKConductProcessor();
    ~RKConductProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    // Message thread.
    /** Loads a MIDI file; returns an error, or empty. */
    juce::String loadFile (const juce::File&);
    juce::String getSongName() const { return songName; }
    const std::vector<ConductEngine::TrackInfo>& getTracks() const noexcept { return tracks; }
    double getSongLength() const noexcept { return songLength; }
    int getSongVersion() const noexcept { return songVersion.load(); }

    bool isTrackOn (int t) const noexcept         { return trackOn[(size_t) t].load(); }
    int getTrackChannel (int t) const noexcept    { return trackChannel[(size_t) t].load(); }
    void setTrackOn (int t, bool on) noexcept     { trackOn[(size_t) t] = on; }
    void setTrackChannel (int t, int ch) noexcept { trackChannel[(size_t) t] = ch; }

    const ConductEngine& getEngine() const noexcept { return engine; }

protected:
    void prepareDsp (double sampleRate, int, int) override { engine.prepare (sampleRate); }
    void resetDsp() override                               { engine.stop(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }

    juce::var getExtraState() const override;
    void setExtraState (const juce::var&) override;

private:
    juce::RangedAudioParameter *tapEvery, *tempo, *fileTempo, *countIn, *loop, *key1, *key2, *tapCC, *tapButton, *stopButton;
    ConductEngine engine;

    juce::MemoryBlock songData;
    juce::String songName;
    std::vector<ConductEngine::TrackInfo> tracks;
    double songLength = 0.0;
    std::atomic<double> songTempo { 120.0 };
    std::atomic<int> songVersion { 0 };
    std::array<std::atomic<bool>, ConductEngine::kMaxTracks> trackOn {};
    std::array<std::atomic<int>, ConductEngine::kMaxTracks> trackChannel {};

    juce::String useSong (const juce::MemoryBlock&, const juce::String& name);

    /** Tap and Stop: each press acts once (counted as the parameter changes). */
    struct ButtonListener : public juce::AudioProcessorParameter::Listener
    {
        explicit ButtonListener (RKConductProcessor& p) : owner (p) {}
        void parameterValueChanged (int index, float value) override;
        void parameterGestureChanged (int, bool) override {}
        RKConductProcessor& owner;
        std::atomic<bool> tapDown { false }, stopDown { false };
    };
    ButtonListener buttonListener { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKConductProcessor)
};
