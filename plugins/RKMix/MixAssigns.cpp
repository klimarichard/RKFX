#include "MixAssigns.h"

#include "MixLayout.h"

MixAssigns::MixAssigns (MixSettings& s, juce::AudioProcessor& h, InsertRack& r) : settings (s), host (h), rack (r)
{
    for (auto* p : host.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            for (int a = 0; a < kNumAssigns; ++a)
                if (ranged->getParameterID() == mix::assignId (a))
                {
                    assignParams[(size_t) a] = ranged;
                    ranged->addListener (this);
                }
    for (int a = 0; a < kNumAssigns; ++a)
        followers[(size_t) a] = std::make_unique<Follower> (*this, a);
}

MixAssigns::~MixAssigns()
{
    for (int a = 0; a < kNumAssigns; ++a)
    {
        detach (a);
        if (auto* p = assignParams[(size_t) a])
            p->removeListener (this);
    }
}

juce::RangedAudioParameter* MixAssigns::resolve (const Target& t) const
{
    if (t.kind == Target::Kind::mix)
    {
        if (auto* p = settings.findParam (t.paramId))
            return p;
        for (auto* p : host.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == t.paramId)
                return ranged;
    }
    else if (t.kind == Target::Kind::insert)
    {
        if (auto* plugin = rack.get (InsertRack::slotAt (t.slot)))
            return plugin->findParam (t.paramId);
    }
    return nullptr;
}

void MixAssigns::attach (int a)
{
    auto* p = resolve (targets[(size_t) a]);
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        targetParams[(size_t) a] = p;
    }
    if (p != nullptr)
        p->addListener (followers[(size_t) a].get());
}

void MixAssigns::detach (int a)
{
    juce::RangedAudioParameter* p = nullptr;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        std::swap (p, targetParams[(size_t) a]);
    }
    if (p != nullptr)
        p->removeListener (followers[(size_t) a].get());
}

MixAssigns::Target MixAssigns::get (int a) const
{
    return targets[(size_t) juce::jlimit (0, kNumAssigns - 1, a)];
}

int MixAssigns::find (Target::Kind kind, const juce::String& paramId, int slot) const
{
    for (int a = 0; a < kNumAssigns; ++a)
    {
        const auto& t = targets[(size_t) a];
        if (t.kind == kind && t.paramId == paramId && (kind != Target::Kind::insert || t.slot == slot))
            return a;
    }
    return -1;
}

void MixAssigns::assign (int a, Target::Kind kind, const juce::String& paramId, int slot)
{
    if (! juce::isPositiveAndBelow (a, kNumAssigns))
        return;
    if (const auto existing = find (kind, paramId, slot); existing >= 0)
        clear (existing);
    clear (a);

    targets[(size_t) a] = { kind, paramId, slot, 0.0f, 1.0f, false };
    attach (a);

    // The control keeps its value: the Assign parameter is set to match.
    if (auto* p = resolve (targets[(size_t) a]))
        targetMoved (a, p->getValue());
    ++version;
}

void MixAssigns::clear (int a)
{
    if (! juce::isPositiveAndBelow (a, kNumAssigns))
        return;
    detach (a);
    targets[(size_t) a] = {};
    ++version;
}

void MixAssigns::setRange (int a, float min, float max, bool invert)
{
    if (! juce::isPositiveAndBelow (a, kNumAssigns))
        return;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        auto& t = targets[(size_t) a];
        t.min = juce::jlimit (0.0f, 1.0f, min);
        t.max = juce::jlimit (0.0f, 1.0f, max);
        t.invert = invert;
    }
    if (auto* p = resolve (targets[(size_t) a]))
        targetMoved (a, p->getValue());
    ++version;
}

void MixAssigns::insertRemoved (int slot)
{
    for (int a = 0; a < kNumAssigns; ++a)
        if (targets[(size_t) a].kind == Target::Kind::insert && targets[(size_t) a].slot == slot)
            clear (a);
}

juce::String MixAssigns::describe (int a) const
{
    const auto t = get (a);
    if (t.kind == Target::Kind::none)
        return {};
    return describeTarget != nullptr ? describeTarget (t) : t.paramId;
}

//==============================================================================
void MixAssigns::parameterValueChanged (int parameterIndex, float value)
{
    // An Assign parameter moved: its control follows, within its range.
    if (following)
        return;

    int a = -1;
    for (int i = 0; i < kNumAssigns; ++i)
        if (assignParams[(size_t) i] != nullptr && assignParams[(size_t) i]->getParameterIndex() == parameterIndex)
            a = i;
    if (a < 0)
        return;

    juce::RangedAudioParameter* target = nullptr;
    float normalised = 0.0f;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        target = targetParams[(size_t) a];
        const auto& t = targets[(size_t) a];
        const auto v = t.invert ? 1.0f - value : value;
        normalised = t.min + v * (t.max - t.min);
    }
    if (target == nullptr)
        return;

    following = true;
    target->setValueNotifyingHost (target->isBoolean() ? (normalised >= 0.5f ? 1.0f : 0.0f) : normalised);
    following = false;
}

void MixAssigns::targetMoved (int a, float normalised)
{
    // A control moved: its Assign parameter follows (the inverse of its range).
    if (following)
        return;

    auto* assignParam = assignParams[(size_t) a];
    float value = 0.0f;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        const auto& t = targets[(size_t) a];
        const auto span = t.max - t.min;
        value = std::abs (span) < 1.0e-6f ? 0.0f : juce::jlimit (0.0f, 1.0f, (normalised - t.min) / span);
        if (t.invert)
            value = 1.0f - value;
    }
    if (assignParam == nullptr)
        return;

    following = true;
    assignParam->setValueNotifyingHost (value);
    following = false;
}

//==============================================================================
juce::var MixAssigns::toVar() const
{
    juce::Array<juce::var> list;
    for (int a = 0; a < kNumAssigns; ++a)
    {
        const auto& t = targets[(size_t) a];
        if (t.kind == Target::Kind::none)
            continue;
        auto* item = new juce::DynamicObject();
        item->setProperty ("assign", a + 1);
        item->setProperty ("kind", t.kind == Target::Kind::insert ? "insert" : "mix");
        item->setProperty ("param", t.paramId);
        if (t.kind == Target::Kind::insert)
            item->setProperty ("slot", t.slot);
        item->setProperty ("min", t.min);
        item->setProperty ("max", t.max);
        item->setProperty ("invert", t.invert);
        list.add (juce::var (item));
    }
    return list;
}

void MixAssigns::fromVar (const juce::var& v)
{
    for (int a = 0; a < kNumAssigns; ++a)
        clear (a);

    if (const auto* list = v.getArray())
        for (const auto& item : *list)
        {
            const auto a = (int) item["assign"] - 1;
            if (! juce::isPositiveAndBelow (a, kNumAssigns))
                continue;
            const auto kind = item["kind"].toString() == "insert" ? Target::Kind::insert : Target::Kind::mix;
            targets[(size_t) a] = { kind, item["param"].toString(), (int) item.getProperty ("slot", -1),
                                    juce::jlimit (0.0f, 1.0f, (float) item.getProperty ("min", 0.0f)),
                                    juce::jlimit (0.0f, 1.0f, (float) item.getProperty ("max", 1.0f)), (bool) item["invert"] };
            attach (a);
        }
    ++version;
}
