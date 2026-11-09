#pragma once

#include "MixEngine.h"
#include "MixRecorder.h"

#include <juce_events/juce_events.h>

class RKMixProcessor;

/** RKMix's recorder as the console runs it (docs/design/RKMix Recorder.md):
    what to record (the mix's settings: inputs, the main, Aux buses), the
    take's folder and file names, the Record and Marker host parameters,
    markers at scene recalls, and the safety rules:

    - the inputs are recorded as they arrive, before the trim and polarity;
      a linked pair as one stereo file (or two mono files, a setting of this
      computer);
    - the layout is fixed when Record is pressed; links can't change while
      recording (a link changed anyway is put back);
    - a mix loaded or a session restored while recording ends the take and
      starts a new one on the same block (both session.json note the split);
    - a new sample rate stops it; under a minute of disk left stops it;
      fewer than 10 minutes refuses to start.

    The format (24-bit or float), the folder and pairs-as-mono belong to this
    computer (recorder.json beside the remote's settings), not to the mix. */
class MixRecorderSession : public MixEngine::OutputTaps,
                           private juce::Timer
{
public:
    explicit MixRecorderSession (RKMixProcessor&);
    ~MixRecorderSession() override;

    static constexpr double kMinStartMinutes = 10.0, kStopMinutes = 1.0;

    // Message thread
    juce::Result start();
    void stop();
    bool isRecording() const noexcept { return recorder.isRecording(); }
    void addMarker (const juce::String& label = {});      // empty: "Marker n"
    void sceneRecalled (int scene);
    void mixLoaded();                                    // a mix loaded or a session restored
    void sampleRateChanged();

    double getElapsedSeconds() const noexcept;
    int getDropouts() const noexcept { return recorder.getDropouts(); }
    juce::File getTakeFolder() const { return recorder.getFolder(); }
    juce::String getStatus() const { return status; }    // the last problem ("Not started: ..."), or empty

    /** The take Record would start now: its tracks and their channel count. */
    MixRecorder::Take makeTake() const;
    int countChannels() const;
    /** Free space at the folder, and the time it lasts at the chosen tracks (seconds). */
    double getFreeBytes() const;
    double getSecondsLeft() const;

    // This computer's settings
    juce::File getRootFolder() const { return rootFolder; }
    void setRootFolder (const juce::File&);
    bool isFloat() const noexcept { return asFloat; }
    void setFloat (bool);
    bool getPairsAsMono() const noexcept { return pairsAsMono; }
    void setPairsAsMono (bool);
    /** Virtual soundcheck: a marker "Scene N" recalls scene N when playback
        passes it, and a jump recalls the scene in effect where it lands. */
    bool getFollowScenes() const noexcept { return followScenes; }
    void setFollowScenes (bool);
    /** The scene marker in effect at @p sample of the loaded recording: its index, or -1. */
    int sceneMarkerAt (juce::int64 sample) const;
    void pollSceneMarkers();   // tests: the follower's step at once
    static juce::File defaultRootFolder();
    void setSettingsFileForTesting (const juce::File&);

    // Audio thread: around the engine's block
    void beginBlock (const float* const* inputs, int numSamples) noexcept;
    void endBlock() noexcept;
    bool isActive() const noexcept { return recorder.isActive(); }

    // MixEngine::OutputTaps
    void main (const float* left, const float* right, int numSamples) noexcept override;
    void bus (int bus, const float* left, const float* right, int numSamples) noexcept override;

    /** The host parameters' presses and changes (any thread). */
    void recordParameterChanged (bool on) noexcept { wantRecord = on ? 1 : 0; }
    void markerPressed() noexcept { ++markerPresses; }

    MixRecorder& getRecorder() noexcept { return recorder; }
    void poll() { timerCallback(); }   // tests: the timer's step at once

private:
    RKMixProcessor& mixer;
    MixRecorder recorder;
    juce::File settingsFile, rootFolder;
    bool asFloat = false, pairsAsMono = false, followScenes = true;
    juce::TimedCallback sceneTimer { [this] { pollSceneMarkers(); } };
    juce::String followKey;                              // the recording and scene marker last followed
    bool followPlaying = false;
    juce::String status;
    int markerCount = 0;
    std::array<bool, 16> takeLinks {};                   // the links when Record was pressed
    std::atomic<int> wantRecord { -1 }, markerPresses { 0 };
    bool active = false;                                 // isActive() for this block (audio thread)

    void loadSettings();
    void saveSettings() const;
    void setRecordParameter (bool on);
    void timerCallback() override;
};
