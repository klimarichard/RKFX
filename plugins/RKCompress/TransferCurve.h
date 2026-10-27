#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** Static compressor curve (input level vs output level, -60 to 0 dB),
    redrawn only when a parameter changes, with a small live dot showing the
    current input level. The dot repaints only its own area. */
class TransferCurve : public juce::Component
{
public:
    TransferCurve (std::function<float (float inputDb)> transferDb, juce::Colour accent);

    void refresh();
    void setInputLevel (float inputDb);
    void setListening (bool isListening);

    void paint (juce::Graphics&) override;
    void resized() override { refresh(); }

    static constexpr float kMinDb = -60.0f;

private:
    std::function<float (float)> transfer;
    juce::Colour accent;
    juce::Path curve;
    float inputDb = kMinDb;
    bool listening = false;

    juce::Point<float> pointFor (float inDb, float outDb) const;
    juce::Rectangle<int> dotArea() const;
};
