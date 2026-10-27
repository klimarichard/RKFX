#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rkfx
{

/** A frequency response display (20 Hz to 20 kHz, logarithmic) for the EQs.
    Display only. The curve is computed in refresh(), which the editor calls
    when a parameter changes; nothing is redrawn otherwise. */
class ResponseCurve : public juce::Component
{
public:
    /** @param magnitudeDb  response at a frequency in Hz
        @param rangeDb      the vertical scale is +/- this many dB */
    ResponseCurve (std::function<double (double hz)> magnitudeDb, float rangeDb, juce::Colour accent);

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override { refresh(); }

private:
    std::function<double (double)> magnitude;
    float range;
    juce::Colour accent;
    juce::Path curve;

    float xFor (double hz) const;
    float yFor (double db) const;
};

/** Calls a function on the message thread whenever any parameter of a
    processor changes (coalesced; safe when the change comes from the audio
    thread or host automation). */
class ParameterWatcher : private juce::AudioProcessorParameter::Listener,
                         private juce::AsyncUpdater
{
public:
    ParameterWatcher (juce::AudioProcessor&, std::function<void()> onChange);
    ~ParameterWatcher() override;

private:
    juce::AudioProcessor& processor;
    std::function<void()> callback;

    void parameterValueChanged (int, float) override { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override { callback(); }
};

} // namespace rkfx
