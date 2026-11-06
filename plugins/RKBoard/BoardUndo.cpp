#include "BoardUndo.h"

#include "BoardProcessor.h"

namespace rkboard
{

namespace
{
    using Key = rkfx::UndoHistory::Key;

    // A control's key: its owner (a pedal ID, routingOwner (split), or 0 for
    // the board's In and Out) and its parameter index (kBypassIndex: the
    // pedal's bypass on the board). Sorted by owner, then index.
    constexpr int kOwnerOffset = 16;
    constexpr int kBypassIndex = 0xfffff;
    constexpr Key makeKey (int owner, int index) noexcept { return ((Key) (owner + kOwnerOffset) << 20) | (Key) index; }
    constexpr int ownerOf (Key key) noexcept { return (int) (key >> 20) - kOwnerOffset; }
    constexpr int indexOf (Key key) noexcept { return (int) (key & 0xfffff); }

    juce::var idList (const std::vector<int>& ids)
    {
        juce::Array<juce::var> list;
        for (const auto id : ids)
            list.add (id);
        return list;
    }

    std::vector<int> idsOf (const juce::var& list)
    {
        std::vector<int> ids;
        if (const auto* a = list.getArray())
            for (const auto& v : *a)
                ids.push_back ((int) v);
        return ids;
    }

    juce::String shortName (const rkfx::ProcessorBase& pedal)
    {
        if (const auto* type = findPedalType (pedal.getConfig().name))
            return type->shortName;
        return pedal.getConfig().name;
    }
}

//==============================================================================
BoardUndo::BoardUndo (BoardProcessor& b) : board (b)
{
    history = std::make_unique<rkfx::UndoHistory> (static_cast<rkfx::UndoHistory::Client&> (*this));
}

BoardUndo::~BoardUndo()
{
    history.reset();
}

//==============================================================================
void BoardUndo::read (std::vector<std::pair<Key, float>>& values)
{
    // Splits (owners -4..-1), the board (0), then the pedals: in key order.
    for (int split = kMaxSplits - 1; split >= 0; --split)
        if (split < board.numSplits)
        {
            const auto& params = board.splits[(size_t) split]->settings.getParameters();
            for (int i = 0; i < params.size(); ++i)
                values.emplace_back (makeKey (BoardProcessor::routingOwner (split), i), params[i]->getValue());
        }

    values.emplace_back (makeKey (0, 0), static_cast<juce::AudioProcessorParameter*> (board.inputParam)->getValue());
    values.emplace_back (makeKey (0, 1), static_cast<juce::AudioProcessorParameter*> (board.outputParam)->getValue());

    for (int id = 1; id <= BoardProcessor::kMaxPedals; ++id)
        if (auto* pedal = board.getPedal (id))
        {
            const auto* bypass = pedal->getBypassParameter();
            const auto& params = pedal->getParameters();
            for (int i = 0; i < params.size(); ++i)
                if (params[i] != bypass)
                    values.emplace_back (makeKey (id, i), params[i]->getValue());
            values.emplace_back (makeKey (id, kBypassIndex), board.isPedalBypassed (id) ? 1.0f : 0.0f);
        }
}

void BoardUndo::write (Key key, float value)
{
    const auto owner = ownerOf (key), index = indexOf (key);
    if (owner == 0)
    {
        (index == 0 ? board.inputParam : board.outputParam)->setValueNotifyingHost (value);
        return;
    }
    if (owner > 0 && index == kBypassIndex)
    {
        board.setPedalBypassed (owner, value >= 0.5f);
        return;
    }
    if (auto* processor = board.ownerProcessor (owner))
        if (index < processor->getParameters().size())
            processor->getParameters()[index]->setValueNotifyingHost (value);
}

juce::String BoardUndo::nameOf (Key key) const
{
    const auto owner = ownerOf (key), index = indexOf (key);
    if (owner == 0)
        return index == 0 ? "Input" : "Output";
    if (owner < 0)
    {
        auto* processor = board.ownerProcessor (owner);
        return "Split " + juce::String (-owner) + ": "
             + (processor != nullptr && index < processor->getParameters().size() ? processor->getParameters()[index]->getName (40) : juce::String());
    }
    auto* pedal = board.getPedal (owner);
    if (pedal == nullptr)
        return {};
    if (index == kBypassIndex)
        return shortName (*pedal) + " On/Off";
    return shortName (*pedal) + ": " + (index < pedal->getParameters().size() ? pedal->getParameters()[index]->getName (40) : juce::String());
}

//==============================================================================
int BoardUndo::getScene() const
{
    return board.currentScene.load();
}

void BoardUndo::restoreScene (int scene)
{
    // Current first: then the Scene parameter recalls nothing (nothing moves,
    // nothing is stored); the values come back as they were after it.
    board.currentScene = scene;
    board.sceneParam->setValueNotifyingHost (board.sceneParam->convertTo0to1 ((float) scene + 1.0f));
    ++board.boardCounter;
}

juce::String BoardUndo::sceneName (int scene) const
{
    return "Scene " + board.describeScene (scene);
}

void BoardUndo::beginApply()
{
    // A glide still under way would move a knob after it was set back.
    for (auto& k : board.sceneKnobs)
        k.gliding = false;
}

void BoardUndo::endApply()
{
    ++board.boardCounter;
    board.markModified();
}

//==============================================================================
// The settings that aren't values, as JSON text: assignments, scene knobs
// (with every scene's value), trails, scene names, each scene's bypasses.

int BoardUndo::getExtraVersion() const
{
    return board.settingsCounter.load();
}

juce::String BoardUndo::readExtra()
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> assigns;
    for (int slot = 0; slot < BoardProcessor::kNumAssigns; ++slot)
    {
        const auto& a = board.assignments[(size_t) slot];
        if (a.pedal == 0)
            continue;
        assigns.add (juce::Array<juce::var> { slot, a.pedal.load(), a.paramIndex.load(), a.min.load(), a.max.load(), a.invert.load() });
    }
    root->setProperty ("assign", assigns);

    juce::Array<juce::var> knobs;
    for (int k = 0; k < BoardProcessor::kMaxSceneKnobs; ++k)
    {
        const auto& knob = board.sceneKnobs[(size_t) k];
        if (knob.pedal == 0)
            continue;
        juce::Array<juce::var> entry { k, knob.pedal.load(), knob.paramIndex.load() };
        for (const auto& v : knob.values)
            entry.add (v.load());
        knobs.add (entry);
    }
    root->setProperty ("knobs", knobs);

    std::vector<int> trails;
    std::array<std::vector<int>, BoardProcessor::kNumScenes> bypassed;
    for (int id = 1; id <= BoardProcessor::kMaxPedals; ++id)
        if (board.getPedal (id) != nullptr)
        {
            if (board.getTrails (id))
                trails.push_back (id);
            for (int scene = 0; scene < BoardProcessor::kNumScenes; ++scene)
                if (board.sceneBypass[(size_t) scene][(size_t) id - 1])
                    bypassed[(size_t) scene].push_back (id);
        }
    root->setProperty ("trails", idList (trails));

    juce::Array<juce::var> names, bypass;
    for (int scene = 0; scene < BoardProcessor::kNumScenes; ++scene)
    {
        names.add (board.getSceneName (scene));
        bypass.add (idList (bypassed[(size_t) scene]));
    }
    root->setProperty ("names", names);
    root->setProperty ("bypass", bypass);

    return juce::JSON::toString (juce::var (root), true);
}

void BoardUndo::writeExtra (const juce::String& text)
{
    const auto root = juce::JSON::parse (text);
    if (! root.isObject())
        return;

    for (auto& a : board.assignments)
    {
        a.pedal = 0;
        a.paramIndex = -1;
    }
    if (const auto* list = root["assign"].getArray())
        for (const auto& e : *list)
        {
            const auto slot = (int) e[0];
            if (! juce::isPositiveAndBelow (slot, BoardProcessor::kNumAssigns))
                continue;
            auto& a = board.assignments[(size_t) slot];
            a.paramIndex = (int) e[2];
            a.min = (float) e[3];
            a.max = (float) e[4];
            a.invert = (bool) e[5];
            a.pedal = (int) e[1];
            board.followTarget (a.pedal, a.paramIndex, board.targetValue (a.pedal, a.paramIndex));
        }

    for (auto& k : board.sceneKnobs)
    {
        k.pedal = 0;
        k.paramIndex = -1;
        k.gliding = false;
    }
    if (const auto* list = root["knobs"].getArray())
        for (const auto& e : *list)
        {
            const auto slot = (int) e[0];
            if (! juce::isPositiveAndBelow (slot, BoardProcessor::kMaxSceneKnobs))
                continue;
            auto& k = board.sceneKnobs[(size_t) slot];
            for (int scene = 0; scene < BoardProcessor::kNumScenes; ++scene)
                k.values[(size_t) scene] = (float) e[3 + scene];
            k.paramIndex = (int) e[2];
            k.pedal = (int) e[1];
        }

    const auto trails = idsOf (root["trails"]);
    for (int id = 1; id <= BoardProcessor::kMaxPedals; ++id)
        if (board.canHaveTrails (id))
            board.setTrails (id, std::find (trails.begin(), trails.end(), id) != trails.end());

    if (const auto* names = root["names"].getArray())
    {
        {
            const juce::SpinLock::ScopedLockType sl (board.sceneNamesLock);
            for (int scene = 0; scene < BoardProcessor::kNumScenes && scene < names->size(); ++scene)
                board.sceneNames[(size_t) scene] = (*names)[scene].toString();
        }
        board.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withParameterInfoChanged (true));
    }

    if (const auto* lists = root["bypass"].getArray())
        for (int scene = 0; scene < BoardProcessor::kNumScenes && scene < lists->size(); ++scene)
        {
            const auto ids = idsOf ((*lists)[scene]);
            for (int id = 1; id <= BoardProcessor::kMaxPedals; ++id)
                if (board.getPedal (id) != nullptr)
                    board.sceneBypass[(size_t) scene][(size_t) id - 1] = std::find (ids.begin(), ids.end(), id) != ids.end();
        }

    ++board.settingsCounter;
}

juce::String BoardUndo::describeExtra (const juce::String& before, const juce::String& after) const
{
    const auto a = juce::JSON::parse (before), b = juce::JSON::parse (after);
    const auto differs = [&] (const char* key) { return juce::JSON::toString (a[key], true) != juce::JSON::toString (b[key], true); };
    if (differs ("names"))  return "Scene name";
    if (differs ("assign")) return "Assignment";
    if (differs ("knobs"))  return "Scene control";
    if (differs ("trails")) return "Trails";
    return "Scene copy";
}

//==============================================================================
juce::var BoardUndo::layoutVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("splits", board.numSplits);

    juce::Array<juce::var> sections, routing;
    for (int s = 0; s < board.getNumSections(); ++s)
        sections.add (idList (board.layout[(size_t) s]));
    for (int k = 0; k < board.numSplits; ++k)
        routing.add (board.splits[(size_t) k]->settings.toVar (false));
    root->setProperty ("sections", sections);
    root->setProperty ("routing", routing);

    auto* types = new juce::DynamicObject();
    for (int id = 1; id <= BoardProcessor::kMaxPedals; ++id)
        if (auto* pedal = board.getPedal (id))
            types->setProperty (juce::String (id), pedal->getConfig().name);
    root->setProperty ("types", juce::var (types));
    return juce::var (root);
}

juce::var BoardUndo::pedalState (int id) const
{
    auto* pedal = board.getPedal (id);
    if (pedal == nullptr)
        return {};
    auto* state = new juce::DynamicObject();
    state->setProperty ("preset", pedal->getPresetVar (true));
    state->setProperty ("bypass", board.isPedalBypassed (id));
    return juce::var (state);
}

void BoardUndo::beginStructure (const std::vector<int>& leaving)
{
    if (history->isApplying() || board.loadingBoard || depth++ > 0)
        return;
    history->beginStructure();
    layoutBefore = layoutVar();
    extraBefore = readExtra();
    states = new juce::DynamicObject();
    for (const auto id : leaving)
        states->setProperty (juce::String (id), pedalState (id));
}

void BoardUndo::endStructure (const juce::String& description)
{
    if (history->isApplying() || board.loadingBoard || depth == 0 || --depth > 0)
        return;

    const auto after = layoutVar();
    if (juce::JSON::toString (after, true) == juce::JSON::toString (layoutBefore, true))
        return;   // nothing moved (a pedal dropped where it was)

    // The pedals that came: their state, for redo.
    if (const auto* typesBefore = layoutBefore["types"].getDynamicObject())
        if (const auto* typesAfter = after["types"].getDynamicObject())
            for (const auto& p : typesAfter->getProperties())
                if (! typesBefore->hasProperty (p.name))
                    states->setProperty (p.name, pedalState (p.name.toString().getIntValue()));

    auto* data = new juce::DynamicObject();
    data->setProperty ("before", layoutBefore);
    data->setProperty ("after", after);
    data->setProperty ("extraBefore", extraBefore);
    data->setProperty ("extraAfter", readExtra());
    data->setProperty ("pedals", juce::var (states.get()));
    history->endStructure (juce::var (data), description);
    states = nullptr;
}

void BoardUndo::applyStructure (const juce::var& data, bool backwards)
{
    const auto target = data[backwards ? "before" : "after"];
    const auto* types = target["types"].getDynamicObject();
    if (types == nullptr)
        return;

    // Re-create what's missing, with its own ID (prepared here, outside the
    // audio thread's view until the chain is swapped).
    for (const auto& p : types->getProperties())
    {
        const auto id = p.name.toString().getIntValue();
        if (id < 1 || id > BoardProcessor::kMaxPedals || board.getPedal (id) != nullptr)
            continue;
        const auto state = data["pedals"][p.name];
        auto pedal = board.createPedal (p.value.toString());
        if (pedal == nullptr)
            continue;
        pedal->loadPresetVar (state["preset"], true);
        pedal->getBypassParameter()->setValue ((bool) state["bypass"] ? 1.0f : 0.0f);
        board.attachPedal (id, std::move (pedal));
    }

    // The splits' settings, quietly (no automatic panning).
    const auto numSplits = juce::jlimit (0, kMaxSplits, (int) target["splits"]);
    {
        const BoardProcessor::LoadingScope loading (board.loadingBoard);
        const auto* routing = target["routing"].getArray();
        for (int k = 0; k < kMaxSplits; ++k)
        {
            auto& split = *board.splits[(size_t) k];
            split.settings.fromVar (routing != nullptr && k < numSplits && k < routing->size() ? (*routing)[k] : juce::var());
            split.lastMode = split.settings.getSplitMode();
        }
    }

    // The layout, then one swap; then the pedals that aren't on it go.
    board.numSplits = numSplits;
    const auto* sections = target["sections"].getArray();
    for (int s = 0; s < kMaxSections; ++s)
    {
        board.layout[(size_t) s].clear();
        if (sections != nullptr && s < sections->size() && s < board.getNumSections())
            for (const auto id : idsOf ((*sections)[s]))
                if (types->hasProperty (juce::String (id)) && board.getPedal (id) != nullptr)
                    board.layout[(size_t) s].push_back (id);
    }
    board.publishChain();

    for (int id = 1; id <= BoardProcessor::kMaxPedals; ++id)
        if (board.getPedal (id) != nullptr && ! types->hasProperty (juce::String (id)))
            board.detachPedal (id, false);

    writeExtra (data[backwards ? "extraBefore" : "extraAfter"].toString());
}

} // namespace rkboard
