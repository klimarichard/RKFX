#include "PluginEditor.h"
#include "PluginProcessor.h"

RKFreqShiftEditor::RKFreqShiftEditor (RKFreqShiftProcessor& p)
    : rkfx::EditorBase (p, 640, 400)
{
    finishSetup();
}

void RKFreqShiftEditor::layoutContent() {}

void RKFreqShiftEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Shifts");
}
