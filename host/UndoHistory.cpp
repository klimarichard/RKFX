#include "UndoHistory.h"

namespace rkfx
{

UndoHistory::UndoHistory (Client& c) : client (c)
{
    clear();
    startTimerHz (10);
}

UndoHistory::~UndoHistory()
{
    stopTimer();
}

void UndoHistory::resync()
{
    last.clear();
    client.read (last);
    lastScene = client.getScene();
    lastExtraVersion = client.getExtraVersion();
    lastExtra = client.readExtra();
}

void UndoHistory::clear()
{
    resync();
    undoStack.clear();
    redoStack.clear();
    open = {};
    openSlot.clear();
    isOpen = false;
    ++version;
}

void UndoHistory::openStep()
{
    if (isOpen)
        return;
    open = {};
    open.sceneBefore = open.sceneAfter = lastScene;
    openSlot.clear();
    isOpen = true;
}

void UndoHistory::poll()
{
    if (applying)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    bool changed = false;

    current.clear();
    client.read (current);

    // Both sorted by key: a control in both that moved is a change; one in
    // only one of them came or went with the structure (no change).
    for (size_t i = 0, j = 0; i < current.size(); ++i)
    {
        const auto [key, value] = current[i];
        while (j < last.size() && last[j].first < key)
            ++j;
        if (j >= last.size() || last[j].first != key || juce::exactlyEqual (last[j].second, value))
            continue;

        openStep();
        // One entry per control in a step: its value before the step, and its latest.
        if (const auto slot = openSlot.find (key); slot == openSlot.end())
        {
            openSlot[key] = open.changes.size();
            open.changes.push_back ({ key, last[j].second, value });
        }
        else
            open.changes[slot->second].after = value;
        changed = true;
    }
    last.swap (current);

    if (const auto scene = client.getScene(); scene != lastScene)
    {
        openStep();
        open.sceneAfter = scene;
        lastScene = scene;
        changed = true;
    }

    // The extra settings: recorded when their version moves; otherwise only
    // taken again (values that changed may have changed them along).
    if (const auto extraVersion = client.getExtraVersion(); extraVersion != lastExtraVersion)
    {
        lastExtraVersion = extraVersion;
        const auto extra = client.readExtra();
        if (extra != lastExtra)
        {
            openStep();
            if (! open.hasExtra)
            {
                open.hasExtra = true;
                open.extraBefore = lastExtra;
            }
            open.extraAfter = extra;
            lastExtra = extra;
            changed = true;
        }
    }
    else if (changed)
    {
        lastExtra = client.readExtra();
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

void UndoHistory::push (Step&& step)
{
    undoStack.push_back (std::move (step));
    while ((int) undoStack.size() > kMaxSteps)
        undoStack.pop_front();
    ++version;
}

void UndoHistory::closeStep()
{
    if (! isOpen)
        return;
    isOpen = false;
    openSlot.clear();

    // Controls that went back to where they were are no change.
    open.changes.erase (std::remove_if (open.changes.begin(), open.changes.end(),
                                        [] (const Change& c) { return juce::exactlyEqual (c.before, c.after); }),
                        open.changes.end());
    if (open.hasExtra && open.extraBefore == open.extraAfter)
        open.hasExtra = false;
    if (! open.empty())
        push (std::move (open));
    open = {};
}

void UndoHistory::beginStructure()
{
    poll();
    closeStep();
}

void UndoHistory::endStructure (const juce::var& data, const juce::String& description)
{
    if (applying)
        return;
    Step step;
    step.structure = data;
    step.label = description;
    step.sceneBefore = step.sceneAfter = lastScene;
    push (std::move (step));
    if (! redoStack.empty())
        redoStack.clear();
    resync();   // the new controls are where they are: no change
}

bool UndoHistory::canUndo() const
{
    return ! undoStack.empty() || (isOpen && ! open.empty());
}

void UndoHistory::apply (const Step& step, bool backwards)
{
    const juce::ScopedValueSetter<bool> busy (applying, true);
    client.beginApply();

    if (step.isStructure())
    {
        client.applyStructure (step.structure, backwards);
    }
    else
    {
        // The scene first, quietly: then nothing is recalled.
        if (step.sceneBefore != step.sceneAfter)
            client.restoreScene (backwards ? step.sceneBefore : step.sceneAfter);

        current.clear();
        client.read (current);
        const auto valueOf = [this] (Key key, float& value)
        {
            const auto it = std::lower_bound (current.begin(), current.end(), key,
                                              [] (const std::pair<Key, float>& a, Key k) { return a.first < k; });
            if (it == current.end() || it->first != key)
                return false;
            value = it->second;
            return true;
        };

        for (auto it = step.changes.rbegin(); it != step.changes.rend(); ++it)
        {
            const auto value = backwards ? it->before : it->after;
            float now = 0.0f;
            if (valueOf (it->key, now) && ! juce::exactlyEqual (now, value))
                client.write (it->key, value);
        }

        if (step.hasExtra)
            client.writeExtra (backwards ? step.extraBefore : step.extraAfter);
    }

    client.endApply();

    // Taken as they are now, so the poll doesn't record the undo itself.
    resync();
}

void UndoHistory::undo()
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

void UndoHistory::redo()
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

juce::String UndoHistory::describe (const Step& step) const
{
    if (step.isStructure())
        return step.label;
    if (step.sceneBefore != step.sceneAfter)
        return client.sceneName (step.sceneAfter) + " recall";
    if (step.changes.empty() && step.hasExtra)
        return client.describeExtra (step.extraBefore, step.extraAfter);
    if (step.changes.size() == 1 && ! step.hasExtra)
        return client.nameOf (step.changes.front().key);
    return juce::String ((int) step.changes.size() + (step.hasExtra ? 1 : 0)) + " changes";
}

juce::String UndoHistory::undoDescription() const
{
    if (isOpen && ! open.empty())
        return describe (open);
    return undoStack.empty() ? juce::String() : describe (undoStack.back());
}

juce::String UndoHistory::redoDescription() const
{
    return redoStack.empty() ? juce::String() : describe (redoStack.back());
}

} // namespace rkfx
