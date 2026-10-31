#include "MixScenes.h"

#include "MixLayout.h"

namespace
{
    /** The strip a parameter ID belongs to: "ch5_trim", "ch_fader_5" -> channel 4; "bus2_eq_on", "bus_mute_2" -> bus 1. */
    void ownerOf (const juce::String& id, int& channel, int& bus)
    {
        channel = bus = -1;
        if (id.startsWith ("ch_fader_") || id.startsWith ("ch_mute_"))
            channel = id.fromLastOccurrenceOf ("_", false, false).getIntValue() - 1;
        else if (id.startsWith ("bus_fader_") || id.startsWith ("bus_mute_"))
            bus = id.fromLastOccurrenceOf ("_", false, false).getIntValue() - 1;
        else if (id.startsWith ("ch") && id.containsChar ('_'))
            channel = id.substring (2).getIntValue() - 1;
        else if (id.startsWith ("bus") && id.containsChar ('_'))
            bus = id.substring (3).getIntValue() - 1;
    }

    /** A scene value's place in a recall scope: its category, or the bus its send goes to. */
    void classify (const juce::String& id, MixScenes::Scope::Category& category, int& sendBus)
    {
        using Scope = MixScenes::Scope;
        sendBus = -1;
        if (id.startsWith ("ch_fader_") || id.startsWith ("ch_mute_") || id.startsWith ("bus_fader_") || id.startsWith ("bus_mute_")
            || id == "main_fader" || id == "main_mute" || id == "mono_fader" || id == "mono_mute")
        {
            category = Scope::faders;
            return;
        }

        // What follows the strip: "ch5_send3" -> "send3", "bus2_eq_on" -> "eq_on", "main_bal" -> "bal".
        const auto field = id.fromFirstOccurrenceOf ("_", false, false);
        const auto numbered = [&field] (const char* prefix)
        {
            const auto rest = field.fromFirstOccurrenceOf (prefix, false, false);
            return field.startsWith (prefix) && rest.isNotEmpty() && rest.containsOnly ("0123456789");
        };
        if (numbered ("send") || numbered ("span"))
        {
            category = Scope::routing;
            sendBus = field.substring (4).getIntValue() - 1;
            return;
        }
        if (numbered ("grp") || field == "main")
            category = Scope::routing;
        else if (field == "pan" || id == "main_bal")
            category = Scope::pans;
        else if (id.startsWith ("bus") || id.startsWith ("main_") || id.startsWith ("mono_"))
            category = Scope::busProcessing;
        else
            category = Scope::channelProcessing;
    }

    void setNormalised (juce::RangedAudioParameter& p, float value)
    {
        if (! juce::exactlyEqual (p.getValue(), value))
            p.setValueNotifyingHost (value);
    }
}

bool MixScenes::isSceneValue (const juce::String& id)
{
    // The host's faders and mutes.
    if (id.startsWith ("ch_fader_") || id.startsWith ("ch_mute_") || id.startsWith ("bus_fader_") || id.startsWith ("bus_mute_")
        || id == "main_fader" || id == "main_mute" || id == "mono_fader" || id == "mono_mute")
        return true;
    if (id.startsWith ("assign") || id == "scene" || id == "talk" || id == "bypass")
        return false;

    // Structure, setup and solo never change with a scene.
    for (const auto* prefix : { "link", "talk_", "osc_", "solo_", "cue_", "midi_", "scene_" })
        if (id.startsWith (prefix))
            return false;
    for (const auto* suffix : { "_type", "_format", "_out", "_solosafe", "_recallsafe", "_gate_key", "_comp_key", "_panmode", "_tap" })
        if (id.endsWith (suffix))
            return false;
    if (id.startsWith ("ch") && id.contains ("_tap"))
        return false;
    return true;
}

//==============================================================================
juce::String MixScenes::Scope::name (Category c)
{
    switch (c)
    {
        case faders:            return "Faders & mutes";
        case pans:              return "Pans";
        case channelProcessing: return "Channel processing";
        case busProcessing:     return "Bus & main processing";
        case inserts:           return "Inserts";
        case routing:           return "Group & main assigns";
        case numCategories:     break;
    }
    return {};
}

juce::String MixScenes::Scope::key (Category c)
{
    switch (c)
    {
        case faders:            return "faders";
        case pans:              return "pans";
        case channelProcessing: return "channels";
        case busProcessing:     return "buses";
        case inserts:           return "inserts";
        case routing:           return "assigns";
        case numCategories:     break;
    }
    return {};
}

juce::String MixScenes::Scope::describe (const std::function<juce::String (int)>& busName) const
{
    if (isEverything())
        return "Everything";
    if (isNothing())
        return "Nothing";

    juce::StringArray parts;
    if (sendBuses == kAllSends)
        parts.add ("All sends");
    else if (sendBuses != 0)
    {
        juce::StringArray buses;
        for (int bus = 0; bus < mix::kBuses; ++bus)
            if (sendsTo (bus))
                buses.add (busName (bus));
        parts.add ("Sends to " + buses.joinIntoString (", "));
    }
    for (int c = 0; c < numCategories; ++c)
        if (has ((Category) c))
            parts.add (name ((Category) c));
    return parts.joinIntoString (", ");
}

bool MixScenes::isInScope (const Scope& scope, const juce::String& id)
{
    Scope::Category category;
    int sendBus;
    classify (id, category, sendBus);
    return sendBus >= 0 ? juce::isPositiveAndBelow (sendBus, mix::kBuses) && scope.sendsTo (sendBus) : scope.has (category);
}

MixScenes::Scope MixScenes::getScope (int scene) const
{
    return scopes[(size_t) juce::jlimit (0, kNumScenes - 1, scene)];
}

void MixScenes::setScope (int scene, Scope scope)
{
    scope.categories &= Scope::kAllCategories;
    scope.sendBuses &= Scope::kAllSends;
    auto& s = scopes[(size_t) juce::jlimit (0, kNumScenes - 1, scene)];
    if (s != scope)
    {
        s = scope;
        ++version;
    }
}

//==============================================================================
MixScenes::MixScenes (MixSettings& s, juce::AudioProcessor& host, InsertRack& r) : settings (s), rack (r)
{
    const auto add = [this] (juce::AudioProcessorParameter* p)
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || ! isSceneValue (ranged->getParameterID()))
            return;
        Entry e;
        e.param = ranged;
        ownerOf (ranged->getParameterID(), e.channel, e.bus);
        classify (ranged->getParameterID(), e.category, e.sendBus);
        e.glides = ! ranged->isDiscrete() && ! ranged->isBoolean();
        entries.push_back (e);
    };
    for (auto* p : host.getParameters())
        add (p);
    for (auto* p : settings.getParameters())
        add (p);
}

MixScenes::~MixScenes()
{
    stopTimer();
}

//==============================================================================
juce::RangedAudioParameter* MixScenes::knobParam (const Knob& k) const
{
    auto* plugin = rack.get (InsertRack::slotAt (k.slot));
    return plugin != nullptr ? plugin->findParam (k.paramId) : nullptr;
}

bool MixScenes::isRecallSafe (const Entry& e) const
{
    if (e.channel >= 0)
        return settings.flag (mix::channelId (e.channel, "recallsafe"));
    if (e.bus >= 0)
        return settings.flag (mix::busId (e.bus, "recallsafe"));
    return false;
}

MixScenes::Scene MixScenes::capture() const
{
    // A glide under way counts as arrived.
    Scene s;
    s.stored = true;
    s.values.reserve (entries.size());
    for (const auto& e : entries)
    {
        auto value = e.param->getValue();
        for (const auto& g : glides)
            if (g.param == e.param)
                value = g.to;
        s.values.push_back (value);
    }
    for (int slot = 0; slot < InsertRack::kTotalSlots; ++slot)
        s.insertOn[(size_t) slot] = rack.isOn (InsertRack::slotAt (slot));
    for (const auto& k : knobs)
    {
        auto* p = knobParam (k);
        auto value = p != nullptr ? p->getValue() : 0.0f;
        for (const auto& g : glides)
            if (g.param == p)
                value = g.to;
        s.knobs.push_back (value);
    }
    return s;
}

void MixScenes::apply (const Scene& s, const Scope& scope)
{
    finishGlides();
    glideMs = settings.value ("scene_glide");

    // A recall-safe strip keeps everything of its own, its inserts too.
    const auto slotSafe = [this] (int slot)
    {
        const auto where = InsertRack::slotAt (slot);
        if (where.strip == InsertRack::Strip::channel)
            return settings.flag (mix::channelId (where.index, "recallsafe"));
        if (where.strip == InsertRack::Strip::bus)
            return settings.flag (mix::busId (where.index, "recallsafe"));
        return false;
    };

    const auto set = [this] (juce::RangedAudioParameter& p, float to, bool canGlide)
    {
        if (juce::exactlyEqual (p.getValue(), to))
            return;
        if (canGlide && glideMs > 0.0)
            glides.push_back ({ &p, p.getValue(), to });
        else
            p.setValueNotifyingHost (to);
    };

    // Only what the scene's recall scope covers.
    const auto inScope = [&scope] (const Entry& e)
    {
        return e.sendBus >= 0 ? scope.sendsTo (e.sendBus) : scope.has (e.category);
    };
    for (size_t i = 0; i < entries.size() && i < s.values.size(); ++i)
        if (inScope (entries[i]) && ! isRecallSafe (entries[i]))
            set (*entries[i].param, s.values[i], entries[i].glides);

    if (scope.has (Scope::inserts))
    {
        for (int slot = 0; slot < InsertRack::kTotalSlots; ++slot)
            if (rack.get (InsertRack::slotAt (slot)) != nullptr && ! slotSafe (slot)
                && rack.isOn (InsertRack::slotAt (slot)) != s.insertOn[(size_t) slot])
                rack.setOn (InsertRack::slotAt (slot), s.insertOn[(size_t) slot]);

        for (size_t i = 0; i < knobs.size() && i < s.knobs.size(); ++i)
            if (auto* p = knobParam (knobs[i]); p != nullptr && ! slotSafe (knobs[i].slot))
                set (*p, s.knobs[i], ! p->isDiscrete() && ! p->isBoolean());
    }

    if (! glides.empty())
    {
        glideStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }
}

void MixScenes::finishGlides()
{
    stopTimer();
    for (const auto& g : glides)
        setNormalised (*g.param, g.to);
    glides.clear();
}

void MixScenes::timerCallback()
{
    const auto t = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - glideStart) / juce::jmax (1.0, glideMs));
    if (t >= 1.0)
    {
        finishGlides();
        return;
    }
    for (const auto& g : glides)
        setNormalised (*g.param, g.from + (float) t * (g.to - g.from));
}

void MixScenes::recall (int scene)
{
    scene = juce::jlimit (0, kNumScenes - 1, scene);
    if (scene == current)
        return;

    scenes[(size_t) current] = capture();
    current = scene;
    ++version;

    // A scene never stored starts as the mix is now (nothing changes).
    auto& s = scenes[(size_t) scene];
    if (! s.stored)
    {
        s = capture();
        return;
    }
    apply (s, scopes[(size_t) scene]);
}

void MixScenes::copy (int from, int to)
{
    from = juce::jlimit (0, kNumScenes - 1, from);
    to = juce::jlimit (0, kNumScenes - 1, to);
    const auto source = from == current ? capture() : scenes[(size_t) from];
    scenes[(size_t) to] = source.stored ? source : capture();
    scopes[(size_t) to] = scopes[(size_t) from];
    if (to == current)
        apply (scenes[(size_t) to], scopes[(size_t) to]);
    ++version;
}

//==============================================================================
bool MixScenes::isSceneControlled (int slot, const juce::String& paramId) const
{
    for (const auto& k : knobs)
        if (k.slot == slot && k.paramId == paramId)
            return true;
    return false;
}

bool MixScenes::setSceneControlled (int slot, const juce::String& paramId, bool controlled)
{
    for (size_t i = 0; i < knobs.size(); ++i)
        if (knobs[i].slot == slot && knobs[i].paramId == paramId)
        {
            if (! controlled)
            {
                knobs.erase (knobs.begin() + (std::ptrdiff_t) i);
                for (auto& s : scenes)
                    if (i < s.knobs.size())
                        s.knobs.erase (s.knobs.begin() + (std::ptrdiff_t) i);
                ++version;
            }
            return true;
        }

    if (! controlled)
        return true;
    if ((int) knobs.size() >= kMaxKnobs)
        return false;

    // Every scene starts at the knob's current value.
    knobs.push_back ({ slot, paramId });
    auto* p = knobParam (knobs.back());
    const auto value = p != nullptr ? p->getValue() : 0.0f;
    for (auto& s : scenes)
        if (s.stored)
            s.knobs.push_back (value);
    ++version;
    return true;
}

void MixScenes::insertRemoved (int slot)
{
    for (int i = (int) knobs.size() - 1; i >= 0; --i)
        if (knobs[(size_t) i].slot == slot)
            setSceneControlled (slot, knobs[(size_t) i].paramId, false);
    finishGlides();
}

//==============================================================================
juce::var MixScenes::toVar() const
{
    // The current scene as the mix is now (edits made in it are part of it).
    auto all = scenes;
    all[(size_t) current] = capture();

    auto* root = new juce::DynamicObject();
    root->setProperty ("current", current + 1);

    juce::Array<juce::var> knobList;
    for (const auto& k : knobs)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("slot", k.slot);
        item->setProperty ("param", k.paramId);
        knobList.add (juce::var (item));
    }
    root->setProperty ("knobs", knobList);

    juce::Array<juce::var> list;
    for (size_t index = 0; index < all.size(); ++index)
    {
        const auto& s = all[index];
        auto* scene = new juce::DynamicObject();

        // The recall scope, unless it is everything: what a recall changes.
        if (const auto& scope = scopes[index]; ! scope.isEverything())
        {
            juce::Array<juce::var> keys;
            for (int c = 0; c < Scope::numCategories; ++c)
                if (scope.has ((Scope::Category) c))
                    keys.add (Scope::key ((Scope::Category) c));
            for (int bus = 0; bus < mix::kBuses; ++bus)
                if (scope.sendsTo (bus))
                    keys.add ("sends" + mix::num (bus));
            scene->setProperty ("scope", keys);
        }

        if (s.stored)
        {
            // Values that differ from the parameter's default (normalised).
            auto* values = new juce::DynamicObject();
            for (size_t i = 0; i < entries.size() && i < s.values.size(); ++i)
                if (std::abs (s.values[i] - entries[i].param->getDefaultValue()) > 1.0e-6f)
                    values->setProperty (entries[i].param->getParameterID(), s.values[i]);
            scene->setProperty ("values", juce::var (values));

            juce::Array<juce::var> on;
            for (int slot = 0; slot < InsertRack::kTotalSlots; ++slot)
                if (s.insertOn[(size_t) slot])
                    on.add (slot);
            scene->setProperty ("insertsOn", on);

            juce::Array<juce::var> knobValues;
            for (auto v : s.knobs)
                knobValues.add (v);
            scene->setProperty ("knobs", knobValues);
        }
        list.add (juce::var (scene));
    }
    root->setProperty ("scenes", list);
    return juce::var (root);
}

void MixScenes::fromVar (const juce::var& v)
{
    finishGlides();
    knobs.clear();
    for (auto& s : scenes)
        s = {};
    for (auto& s : scopes)
        s = {};

    if (const auto* knobList = v["knobs"].getArray())
        for (const auto& k : *knobList)
            if ((int) knobs.size() < kMaxKnobs)
                knobs.push_back ({ juce::jlimit (0, InsertRack::kTotalSlots - 1, (int) k["slot"]), k["param"].toString() });

    if (const auto* list = v["scenes"].getArray())
        for (int i = 0; i < juce::jmin (kNumScenes, list->size()); ++i)
        {
            const auto& item = list->getReference (i);
            if (const auto* keys = item["scope"].getArray())
            {
                Scope scope;
                scope.categories = scope.sendBuses = 0;
                for (const auto& key : *keys)
                {
                    const auto k = key.toString();
                    for (int c = 0; c < Scope::numCategories; ++c)
                        if (k == Scope::key ((Scope::Category) c))
                            scope.set ((Scope::Category) c, true);
                    if (k.startsWith ("sends") && k.substring (5).containsOnly ("0123456789"))
                        if (const auto bus = k.substring (5).getIntValue() - 1; juce::isPositiveAndBelow (bus, mix::kBuses))
                            scope.setSendsTo (bus, true);
                }
                scopes[(size_t) i] = scope;
            }
            if (! item.hasProperty ("values"))
                continue;
            auto& s = scenes[(size_t) i];
            s.stored = true;
            const auto& values = item["values"];
            for (const auto& e : entries)
            {
                const auto id = e.param->getParameterID();
                s.values.push_back (values.hasProperty (id) ? juce::jlimit (0.0f, 1.0f, (float) values[juce::Identifier (id)])
                                                            : e.param->getDefaultValue());
            }
            if (const auto* on = item["insertsOn"].getArray())
                for (const auto& slot : *on)
                    if (juce::isPositiveAndBelow ((int) slot, InsertRack::kTotalSlots))
                        s.insertOn[(size_t) (int) slot] = true;
            const auto* knobValues = item["knobs"].getArray();
            for (size_t k = 0; k < knobs.size(); ++k)
                s.knobs.push_back (knobValues != nullptr && (int) k < knobValues->size() ? (float) knobValues->getReference ((int) k) : 0.0f);
        }

    // The mix's values are the current scene's already (they were saved as they are).
    current = juce::jlimit (0, kNumScenes - 1, (int) v.getProperty ("current", 1) - 1);
    ++version;
}
