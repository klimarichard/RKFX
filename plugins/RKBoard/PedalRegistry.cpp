#include "PedalRegistry.h"

// Every plugin's processor, compiled into the board (RKFX_BOARD leaves out
// their own plugin entry points).
#include "../RKAcoustic/PluginProcessor.h"
#include "../RKChorus/PluginProcessor.h"
#include "../RKCompress/PluginProcessor.h"
#include "../RKDeEss/PluginProcessor.h"
#include "../RKDelay/PluginProcessor.h"
#include "../RKDistort/PluginProcessor.h"
#include "../RKDouble/PluginProcessor.h"
#include "../RKFilter/PluginProcessor.h"
#include "../RKFlange/PluginProcessor.h"
#include "../RKFreqShift/PluginProcessor.h"
#include "../RKGate/PluginProcessor.h"
#include "../RKGraphEQ/PluginProcessor.h"
#include "../RKLimit/PluginProcessor.h"
#include "../RKLoop/PluginProcessor.h"
#include "../RKHold/PluginProcessor.h"
#include "../RKMultiComp/PluginProcessor.h"
#include "../RKRide/PluginProcessor.h"
#include "../RKParaEQ/PluginProcessor.h"
#include "../RKPhase/PluginProcessor.h"
#include "../RKPreamp/PluginProcessor.h"
#include "../RKRingMod/PluginProcessor.h"
#include "../RKRotary/PluginProcessor.h"
#include "../RKSaturate/PluginProcessor.h"
#include "../RKSwell/PluginProcessor.h"
#include "../RKSynth/PluginProcessor.h"
#include "../RKTransient/PluginProcessor.h"
#include "../RKTremolo/PluginProcessor.h"
#include "../RKUtility/PluginProcessor.h"
#include "../RKVerb/PluginProcessor.h"
#include "../RKOctave/PluginProcessor.h"
#include "../RKPolyPitch/PluginProcessor.h"
#include "../RKVibrato/PluginProcessor.h"
#include "../RKVocode/PluginProcessor.h"
#include "../RKVoice/PluginProcessor.h"

// The board-only pedals.
#include "pedals/VolumePedal.h"
#include "pedals/WahPedal.h"

// Every factory preset list in the board (generated, one per plugin).
#define RKBOARD_PRESET_LISTS(X) \
    X (RKBoard) \
    X (RKAcoustic) \
    X (RKChorus) \
    X (RKCompress) \
    X (RKDeEss) \
    X (RKDelay) \
    X (RKDistort) \
    X (RKDouble) \
    X (RKFilter) \
    X (RKFlange) \
    X (RKFreqShift) \
    X (RKGate) \
    X (RKGraphEQ) \
    X (RKLimit) \
    X (RKLoop) \
    X (RKHold) \
    X (RKMultiComp) \
    X (RKRide) \
    X (RKParaEQ) \
    X (RKPhase) \
    X (RKPreamp) \
    X (RKRingMod) \
    X (RKRotary) \
    X (RKSaturate) \
    X (RKSwell) \
    X (RKSynth) \
    X (RKTransient) \
    X (RKTremolo) \
    X (RKUtility) \
    X (RKVerb) \
    X (RKOctave) \
    X (RKPolyPitch) \
    X (RKRemap) \
    X (RKVibrato) \
    X (RKVocode) \
    X (RKVoice) \
    X (RKVolume) \
    X (RKWah)

namespace rkfx
{
   #define RKBOARD_DECLARE_LIST(name) std::vector<FactoryPreset> factoryPresetList_##name();
    RKBOARD_PRESET_LISTS (RKBOARD_DECLARE_LIST)
   #undef RKBOARD_DECLARE_LIST
}

namespace rkboard
{

namespace
{
    std::unique_ptr<rkfx::ProcessorBase> makeRKWah() { return std::make_unique<RKWahProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKVolume() { return std::make_unique<RKVolumeProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKFilter() { return std::make_unique<RKFilterProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKParaEQ() { return std::make_unique<RKParaEQProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKGraphEQ() { return std::make_unique<RKGraphEQProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKVocode() { return std::make_unique<RKVocodeProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKVoice() { return std::make_unique<RKVoiceProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKCompress() { return std::make_unique<RKCompressProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKMultiComp() { return std::make_unique<RKMultiCompProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKRide() { return std::make_unique<RKRideProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKGate() { return std::make_unique<RKGateProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKLimit() { return std::make_unique<RKLimitProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKTransient() { return std::make_unique<RKTransientProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKDeEss() { return std::make_unique<RKDeEssProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKSwell() { return std::make_unique<RKSwellProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKSaturate() { return std::make_unique<RKSaturateProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKDistort() { return std::make_unique<RKDistortProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKChorus() { return std::make_unique<RKChorusProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKDouble() { return std::make_unique<RKDoubleProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKFlange() { return std::make_unique<RKFlangeProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKPhase() { return std::make_unique<RKPhaseProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKTremolo() { return std::make_unique<RKTremoloProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKAcoustic() { return std::make_unique<RKAcousticProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKPreamp() { return std::make_unique<RKPreampProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKSynth() { return std::make_unique<RKSynthProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKRingMod() { return std::make_unique<RKRingModProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKLoop() { return std::make_unique<RKLoopProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKHold() { return std::make_unique<RKHoldProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKOctave() { return std::make_unique<RKOctaveProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKPolyPitch() { return std::make_unique<RKPolyPitchProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKVibrato() { return std::make_unique<RKVibratoProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKRotary() { return std::make_unique<RKRotaryProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKFreqShift() { return std::make_unique<RKFreqShiftProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKDelay() { return std::make_unique<RKDelayProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKVerb() { return std::make_unique<RKVerbProcessor>(); }
    std::unique_ptr<rkfx::ProcessorBase> makeRKUtility() { return std::make_unique<RKUtilityProcessor>(); }
}

const std::vector<PedalType>& pedalTypes()
{
    registerFactoryPresets();   // every pedal is created through a type

    using Family = rkfx::Palette::Family;
    static const std::vector<PedalType> types {
        { "RKAcoustic", "Acoustic", Family::filterEQ, makeRKAcoustic },
        { "RKWah", "Wah", Family::filterEQ, makeRKWah },
        { "RKFilter", "Filter", Family::filterEQ, makeRKFilter },
        { "RKParaEQ", "ParaEQ", Family::filterEQ, makeRKParaEQ },
        { "RKGraphEQ", "GraphEQ", Family::filterEQ, makeRKGraphEQ },
        { "RKVocode", "Vocode", Family::filterEQ, makeRKVocode },
        { "RKCompress", "Compress", Family::dynamics, makeRKCompress },
        { "RKMultiComp", "MultiComp", Family::dynamics, makeRKMultiComp },
        { "RKRide", "Ride", Family::dynamics, makeRKRide },
        { "RKGate", "Gate", Family::dynamics, makeRKGate },
        { "RKLimit", "Limit", Family::dynamics, makeRKLimit },
        { "RKTransient", "Transient", Family::dynamics, makeRKTransient },
        { "RKDeEss", "DeEss", Family::dynamics, makeRKDeEss },
        { "RKSwell", "Swell", Family::dynamics, makeRKSwell },
        { "RKSaturate", "Saturate", Family::drive, makeRKSaturate },
        { "RKDistort", "Distort", Family::drive, makeRKDistort },
        { "RKPreamp", "Preamp", Family::drive, makeRKPreamp },
        { "RKChorus", "Chorus", Family::modulation, makeRKChorus },
        { "RKDouble", "Double", Family::modulation, makeRKDouble },
        { "RKFlange", "Flange", Family::modulation, makeRKFlange },
        { "RKPhase", "Phase", Family::modulation, makeRKPhase },
        { "RKTremolo", "Tremolo", Family::modulation, makeRKTremolo },
        { "RKVibrato", "Vibrato", Family::modulation, makeRKVibrato },
        { "RKRotary", "Rotary", Family::modulation, makeRKRotary },
        { "RKFreqShift", "FreqShift", Family::modulation, makeRKFreqShift },
        { "RKOctave", "Octave", Family::modulation, makeRKOctave },
        { "RKPolyPitch", "PolyPitch", Family::modulation, makeRKPolyPitch },
        { "RKSynth", "Synth", Family::modulation, makeRKSynth },
        { "RKRingMod", "RingMod", Family::modulation, makeRKRingMod },
        { "RKVoice", "Voice", Family::modulation, makeRKVoice },
        { "RKDelay", "Delay", Family::delay, makeRKDelay, 10.0 },
        { "RKLoop", "Loop", Family::delay, makeRKLoop },
        { "RKHold", "Hold", Family::delay, makeRKHold },
        { "RKVerb", "Verb", Family::reverb, makeRKVerb, 10.0 },
        { "RKVolume", "Volume", Family::utility, makeRKVolume },
        { "RKUtility", "Utility", Family::utility, makeRKUtility }
    };
    return types;
}

void registerFactoryPresets()
{
    static const bool registered = []
    {
       #define RKBOARD_REGISTER_LIST(name) rkfx::registerFactoryPresets (#name, &rkfx::factoryPresetList_##name);
        RKBOARD_PRESET_LISTS (RKBOARD_REGISTER_LIST)
       #undef RKBOARD_REGISTER_LIST
        return true;
    }();
    juce::ignoreUnused (registered);
}

const PedalType* findPedalType (const juce::String& name)
{
    for (const auto& type : pedalTypes())
        if (type.name == name)
            return &type;
    return nullptr;
}

juce::String familyName (rkfx::Palette::Family family)
{
    switch (family)
    {
        case rkfx::Palette::Family::filterEQ:   return "Filter and EQ";
        case rkfx::Palette::Family::dynamics:   return "Dynamics";
        case rkfx::Palette::Family::drive:      return "Drive";
        case rkfx::Palette::Family::modulation: return "Modulation";
        case rkfx::Palette::Family::delay:      return "Delay";
        case rkfx::Palette::Family::reverb:     return "Reverb";
        case rkfx::Palette::Family::utility:    return "Utility";
        case rkfx::Palette::Family::midi:       return "MIDI";
    }
    return {};
}

} // namespace rkboard
