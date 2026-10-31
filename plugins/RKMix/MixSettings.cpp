#include "MixSettings.h"

#include "MixParams.h"

MixSettings::MixSettings()
{
    auto builder = MixParams::createSettings();
    for (auto& owned : builder.params)
    {
        auto* p = owned.release();
        infos.push_back (rkfx::describe (*p));
        addParameter (p);
        params.push_back (p);
        owners.push_back (ownerBits (p->getParameterID()));
        byId[p->getParameterID().toStdString()] = p;
        p->addListener (this);
    }
}

MixSettings::~MixSettings()
{
    for (auto* p : params)
        p->removeListener (this);
}

juce::RangedAudioParameter* MixSettings::findParam (const juce::String& id) const
{
    const auto it = byId.find (id.toStdString());
    return it != byId.end() ? it->second : nullptr;
}

juce::RangedAudioParameter& MixSettings::param (const juce::String& id) const
{
    auto* p = findParam (id);
    jassert (p != nullptr);
    return p != nullptr ? *p : *params.front();
}

float MixSettings::value (const juce::String& id) const
{
    const auto& p = param (id);
    return p.convertFrom0to1 (p.getValue());
}

std::uint64_t MixSettings::ownerBits (const juce::String& id)
{
    // "ch5_trim" -> channel 5; "link3" -> channels 5 and 6 (and the main,
    // which knows the links); "bus2_type" -> bus 2; anything else -> the main.
    if (id.startsWith ("ch") && id.containsChar ('_'))
    {
        const auto ch = id.substring (2).getIntValue() - 1;
        if (ch >= 0 && ch < mix::kChannels)
            return std::uint64_t (1) << ch;
    }
    if (id.startsWith ("link"))
    {
        const auto pair = id.substring (4).getIntValue() - 1;
        if (pair >= 0 && pair < mix::kPairs)
            return (std::uint64_t (3) << (pair * 2)) | kMainBit;
    }
    if (id.startsWith ("bus") && id.containsChar ('_'))
    {
        const auto bus = id.substring (3).getIntValue() - 1;
        if (bus >= 0 && bus < mix::kBuses)
            return busBit (bus);
    }
    return kMainBit;
}

void MixSettings::parameterValueChanged (int parameterIndex, float)
{
    if (onChange != nullptr && juce::isPositiveAndBelow (parameterIndex, (int) owners.size()))
        onChange (owners[(size_t) parameterIndex]);
}

juce::var MixSettings::toVar() const
{
    rkfx::ParamValues values;
    for (auto* p : params)
        values[p->getParameterID()] = p->convertFrom0to1 (p->getValue());

    return rkfx::PresetFormat::toVar ("RKMix", 1, {}, infos, values, true)["params"];
}

juce::StringArray MixSettings::fromVar (const juce::var& paramsVar)
{
    // Wrapped as a preset so the shared lenient reader validates it.
    auto* wrapper = new juce::DynamicObject();
    wrapper->setProperty ("plugin", "RKMix");
    wrapper->setProperty ("version", 1);
    wrapper->setProperty ("params", paramsVar.isObject() ? paramsVar : juce::var (new juce::DynamicObject()));

    // An Aux pan mode saved as "centre" (before the send pans) is Own: every send pan starts at centre.
    if (auto* object = paramsVar.getDynamicObject())
    {
        juce::Array<juce::Identifier> centred;
        for (const auto& property : object->getProperties())
            if (property.name.toString().endsWith ("_panmode") && property.value.toString() == "centre")
                centred.add (property.name);
        for (const auto& name : centred)
            object->setProperty (name, "own");
    }

    const auto r = rkfx::PresetFormat::read (rkfx::PresetFormat::toJson (juce::var (wrapper)), "RKMix", 1, infos);

    for (const auto& [id, v] : r.values)
        if (auto* p = findParam (id))
        {
            const auto normalised = p->convertTo0to1 ((float) v);
            if (! juce::exactlyEqual (normalised, p->getValue()))
                p->setValueNotifyingHost (normalised);
        }
    return r.notes;
}
