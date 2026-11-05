#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MixMonitorEqSession.h"

#include <functional>

class RKMixProcessor;

/** Monitor EQ in the bus page (in place of its processing): the options on
    the left, the 31 bands on the right (the response, the target, the
    correction and the GraphEQ as measured), the steps and Apply below.

    Closing it (selecting another strip, the bus page's button) stops a run
    it started; a run started from the remote carries on. */
class MixMonitorEqView : public juce::Component,
                         private juce::ChangeListener
{
public:
    MixMonitorEqView (RKMixProcessor&, int bus, juce::Colour accent, std::function<juce::String (int channel)> inputName);
    ~MixMonitorEqView() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    MixMonitorEqSession& session;
    const int bus;
    const juce::Colour accent;
    std::function<juce::String (int)> inputName;
    bool startedHere = false;

    juce::TextButton speaker { "Speaker" }, inEar { "In-ear" };
    juce::ComboBox input, mic, target, sides, source;
    juce::Slider level, positions, bass, strength;
    juce::TextButton startButton { "Start" }, nextButton { "Next" }, finishButton { "Finish" }, stopButton { "Stop" },
                     applyButton { "Apply" }, undoButton { "Undo" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> controlsArea, graphArea, progressArea, strengthLabel;
    struct Row { juce::String label; juce::Component* control; };
    std::vector<Row> rows;

    enum MicItem { micFirst = 1, calibrationItem = 100, loadCalibrationItem = 101 };
    enum SourceItem { measureItem = 1, fileItem = 2, loadFileItem = 3 };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void edit (const std::function<void (MixMonitorEqSession::Options&)>&);
    void chooseFile (bool calibration);
    void paintGraph (juce::Graphics&, juce::Rectangle<int>);
};
