#include "MixUndo.h"

#include "MixScenes.h"


MixUndo::MixUndo (std::vector<juce::RangedAudioParameter*> tracked, MixScenes& s, std::function<void (int)> setSceneParameter)
    : params (std::move (tracked)), scenes (s), setScene (std::move (setSceneParameter))
{
    clear();
    startTimerHz (10);
}

MixUndo::~MixUndo()
{
    stopTimer();
}

void MixUndo::clear()
{
    last.resize (params.size());
    openSlot.assign (params.size(), -1);
    for (size_t i = 0; i < params.size(); ++i)
        last[i] = params[i]->getValue();
    lastScene = scenes.getCurrent();
    undoStack.clear();
    redoStack.clear();
    open = {};
    isOpen = false;
    ++version;
}

void MixUndo::poll()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    bool changed = false;

    for (size_t i = 0; i < params.size(); ++i)
    {
        const auto v = params[i]->getValue();
        if (juce::exactlyEqual (v, last[i]))
            continue;
        if (! isOpen)
        {
            open = {};
            open.sceneBefore = open.sceneAfter = lastScene;
            isOpen = true;
        }
        // One entry per control in a step: its value before the step, and its latest.
        if (auto& slot = openSlot[i]; slot < 0)
        {
            slot = (int) open.changes.size();
            open.changes.push_back ({ (int) i, last[i], v });
        }
        else
            open.changes[(size_t) slot].after = v;
        last[i] = v;
        changed = true;
    }

    if (const auto scene = scenes.getCurrent(); scene != lastScene)
    {
        if (! isOpen)
        {
            open = {};
            open.sceneBefore = lastScene;
            isOpen = true;
        }
        open.sceneAfter = scene;
        lastScene = scene;
        changed = true;
    }

    if (changed)
    {
        lastChangeMs = now;
        if (! redoStack.empty())
        {
            redoStack.clear();   // a new change: the steps undone are gone
            ++version;
        }
    }
    else if (isOpen && now - lastChangeMs >= kStepGapMs)
        closeStep();
}

void MixUndo::closeStep()
{
    if (! isOpen)
        return;
    isOpen = false;
    for (const auto& c : open.changes)
        openSlot[(size_t) c.index] = -1;

    // Controls that went back to where they were are no change.
    open.changes.erase (std::remove_if (open.changes.begin(), open.changes.end(),
                                        [] (const Change& c) { return juce::exactlyEqual (c.before, c.after); }),
                        open.changes.end());
    if (open.empty())
        return;
    undoStack.push_back (std::move (open));
    while ((int) undoStack.size() > kMaxSteps)
        undoStack.pop_front();
    open = {};
    ++version;
}

bool MixUndo::canUndo() const
{
    return ! undoStack.empty() || (isOpen && ! open.empty());
}

void MixUndo::apply (const Step& step, bool backwards)
{
    scenes.cancelGlides();

    // The scene first, quietly: then the Scene parameter can't recall anything.
    const auto scene = backwards ? step.sceneBefore : step.sceneAfter;
    if (step.sceneBefore != step.sceneAfter)
    {
        scenes.restoreCurrent (scene);
        if (setScene != nullptr)
            setScene (scene);
    }

    for (auto it = step.changes.rbegin(); it != step.changes.rend(); ++it)
    {
        const auto value = backwards ? it->before : it->after;
        auto* p = params[(size_t) it->index];
        if (! juce::exactlyEqual (p->getValue(), value))
            p->setValueNotifyingHost (value);
    }

    // Taken as they are now, so the poll doesn't record the undo itself.
    for (size_t i = 0; i < params.size(); ++i)
        last[i] = params[i]->getValue();
    lastScene = scenes.getCurrent();
}

void MixUndo::undo()
{
    poll();
    closeStep();
    if (undoStack.empty())
        return;
    auto step = std::move (undoStack.back());
    undoStack.pop_back();
    apply (step, true);
    redoStack.push_back (std::move (step));
    ++version;
}

void MixUndo::redo()
{
    poll();
    closeStep();
    if (redoStack.empty())
        return;
    auto step = std::move (redoStack.back());
    redoStack.pop_back();
    apply (step, false);
    undoStack.push_back (std::move (step));
    ++version;
}

juce::String MixUndo::describe (const Step& step) const
{
    if (step.sceneBefore != step.sceneAfter)
        return "Scene " + juce::String (step.sceneAfter + 1) + " recall";
    if (step.changes.size() == 1)
        return params[(size_t) step.changes.front().index]->getName (60);
    return juce::String ((int) step.changes.size()) + " changes";
}

juce::String MixUndo::undoDescription() const
{
    if (isOpen && ! open.empty())
        return describe (open);
    return undoStack.empty() ? juce::String() : describe (undoStack.back());
}

juce::String MixUndo::redoDescription() const
{
    return redoStack.empty() ? juce::String() : describe (redoStack.back());
}
