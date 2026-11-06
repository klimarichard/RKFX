#include "MixUndo.h"

#include "MixScenes.h"


MixUndo::MixUndo (std::vector<juce::RangedAudioParameter*> tracked, MixScenes& s, std::function<void (int)> setSceneParameter)
    : params (std::move (tracked)), scenes (s), setScene (std::move (setSceneParameter))
{
    history = std::make_unique<rkfx::UndoHistory> (static_cast<rkfx::UndoHistory::Client&> (*this));
}

MixUndo::~MixUndo()
{
    history.reset();
}

void MixUndo::read (std::vector<std::pair<rkfx::UndoHistory::Key, float>>& values)
{
    values.reserve (params.size());
    for (size_t i = 0; i < params.size(); ++i)
        values.emplace_back ((rkfx::UndoHistory::Key) i, params[i]->getValue());
}

void MixUndo::write (rkfx::UndoHistory::Key key, float value)
{
    if (juce::isPositiveAndBelow (key, (rkfx::UndoHistory::Key) params.size()))
        params[(size_t) key]->setValueNotifyingHost (value);
}

juce::String MixUndo::nameOf (rkfx::UndoHistory::Key key) const
{
    return juce::isPositiveAndBelow (key, (rkfx::UndoHistory::Key) params.size()) ? params[(size_t) key]->getName (60) : juce::String();
}

int MixUndo::getScene() const
{
    return scenes.getCurrent();
}

void MixUndo::restoreScene (int scene)
{
    // Quietly: then the Scene parameter can't recall anything.
    scenes.restoreCurrent (scene);
    if (setScene != nullptr)
        setScene (scene);
}

void MixUndo::beginApply()
{
    scenes.cancelGlides();
}
