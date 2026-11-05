#include "MixMonitorEqSession.h"

#include "PluginProcessor.h"
#include "../RKGraphEQ/PluginProcessor.h"
#include "../RKGraphEQ/GraphEQLayout.h"

using namespace monitoreq;

namespace
{
    constexpr int kTimerHz = 10;
    const juce::String kGraphEq ("RKGraphEQ");
    constexpr int kThirtyOneMode = 2;      // RKGraphEQ's "bands" choice: 10, 15, 31

    juce::var bandsToVar (const Bands& b)
    {
        juce::Array<juce::var> list;
        for (auto v : b)
            list.add (v);
        return list;
    }

    bool bandsFromVar (const juce::var& v, Bands& out)
    {
        const auto* a = v.getArray();
        if (a == nullptr || a->size() != kBands)
            return false;
        for (int b = 0; b < kBands; ++b)
            out[(size_t) b] = (float) a->getReference (b);
        return true;
    }

    template <typename E>
    E enumFrom (const juce::var& v, E fallback, int count)
    {
        if (! v.isInt() && ! v.isDouble())
            return fallback;
        const auto i = (int) v;
        return i >= 0 && i < count ? (E) i : fallback;
    }

    void setParam (rkfx::ProcessorBase& p, const juce::String& id, float value)
    {
        if (auto* param = p.findParam (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
}

MixMonitorEqSession::MixMonitorEqSession (RKMixProcessor& p) : processor (p) {}

MixMonitorEqSession::~MixMonitorEqSession()
{
    stopTimer();
    if (isRunning())
        measure().stop();
}

MixMeasure& MixMonitorEqSession::measure() const { return processor.getMeasure(); }

//==============================================================================
monitoreq::Target MixMonitorEqSession::currentTarget() const noexcept
{
    return options.type == Type::speaker ? options.speakerTarget : options.inEarTarget;
}

float MixMonitorEqSession::currentLevelDb() const noexcept
{
    return options.type == Type::speaker ? options.speakerLevelDb : options.inEarLevelDb;
}

int MixMonitorEqSession::currentPositions() const noexcept
{
    return options.type == Type::speaker ? options.speakerPositions : options.inEarPositions;
}

const Bands& MixMonitorEqSession::currentMicCurve() const noexcept
{
    return options.useCalibration && options.hasCalibration ? options.calibration : micCurve (options.mic);
}

void MixMonitorEqSession::setOptions (const Options& o)
{
    auto n = o;
    const auto has = [] (const std::vector<Target>& list, Target t) { return std::find (list.begin(), list.end(), t) != list.end(); };
    if (! has (targetsFor (Type::speaker), n.speakerTarget))
        n.speakerTarget = targetsFor (Type::speaker).front();
    if (! has (targetsFor (Type::inEar), n.inEarTarget))
        n.inEarTarget = targetsFor (Type::inEar).front();
    n.input = juce::jlimit (0, mix::kChannels - 1, n.input);
    n.bassDb = juce::jlimit (0.0f, 8.0f, n.bassDb);
    n.speakerLevelDb = juce::jlimit (MixMeasure::kMinNoiseDb, MixMeasure::kMaxNoiseDb, n.speakerLevelDb);
    n.inEarLevelDb = juce::jlimit (MixMeasure::kMinNoiseDb, kMaxInEarLevelDb, n.inEarLevelDb);
    n.speakerPositions = juce::jlimit (1, kMaxPositions, n.speakerPositions);
    n.inEarPositions = juce::jlimit (1, kMaxPositions, n.inEarPositions);
    n.strength = juce::jlimit (0.0f, 1.0f, n.strength);
    n.useCalibration = n.useCalibration && n.hasCalibration;
    n.useFile = n.useFile && n.hasFile;

    // A run keeps what it started with; the rest (target, strength, level) follows now.
    if (isRunning())
    {
        n.type = options.type;
        n.input = options.input;
        n.sides = options.sides;
        n.useFile = options.useFile;
    }
    options = n;

    if (isRunning() && (step == Step::settling || step == Step::measuring))
        measure().setNoise (true, currentLevelDb());
    if (hasResponse)
        updateSuggestion();
    changed();
}

bool MixMonitorEqSession::readFile (const juce::File& file, Bands& out, juce::String& name)
{
    if (file.getSize() > 4 * 1024 * 1024)
    {
        message = file.getFileName() + " is too big for a response file.";
        changed();
        return false;
    }
    const auto bands = readResponseFile (file.loadFileAsString().toStdString());
    if (! bands)
    {
        message = file.getFileName() + " has no frequency and dB lines.";
        changed();
        return false;
    }
    out = *bands;
    name = file.getFileNameWithoutExtension();
    message.clear();
    return true;
}

bool MixMonitorEqSession::loadCalibration (const juce::File& file)
{
    auto o = options;
    if (! readFile (file, o.calibration, o.calibrationName))
        return false;
    o.hasCalibration = o.useCalibration = true;
    setOptions (o);
    return true;
}

bool MixMonitorEqSession::loadMeasurementFile (const juce::File& file)
{
    auto o = options;
    if (! readFile (file, o.file, o.fileName))
        return false;
    o.hasFile = o.useFile = true;
    setOptions (o);
    return true;
}

//==============================================================================
bool MixMonitorEqSession::start (int busIndex)
{
    if (isRunning())
        return false;
    if (busIndex < 0 || busIndex >= mix::kBuses || processor.getBusType (busIndex) != mix::BusType::aux)
    {
        message = "Monitor EQ measures Aux buses only.";
        changed();
        return false;
    }

    if (processor.getSettings().choice (mix::busId (busIndex, "out")) == mix::kTargetOff
        && ! (options.type == Type::inEar && options.useFile && options.hasFile))
    {
        message = "The bus has no output: set its Output first.";
        changed();
        return false;
    }

    bus = busIndex;
    positions.clear();
    positionSide.clear();
    hasResponse = false;
    earDifference = 0.0f;
    progress = 0.0f;
    message.clear();
    suggestion = {};

    const auto slot = findGraphEq (bus);
    measuredGains = slot >= 0 ? graphEqGains (bus, slot) : Bands {};

    if (options.type == Type::inEar && options.useFile && options.hasFile)
    {
        response = fileResponse (options.file);
        hasResponse = true;
        step = Step::done;
        updateSuggestion();
        changed();
        return true;
    }

    // Right after a run, the bus may still be fading back (50 ms): the start waits for it.
    pendingStart = ! measure().start (bus, options.input, positionSides (0));
    scene = processor.getScenes().getCurrent();
    step = Step::floor;
    marked = false;
    startTimerHz (kTimerHz);
    changed();
    return true;
}

MixMeasure::Sides MixMonitorEqSession::positionSides (int position) const noexcept
{
    switch (currentSides())
    {
        case Sides::both:    return MixMeasure::Sides::both;
        case Sides::left:    return MixMeasure::Sides::left;
        case Sides::right:   return MixMeasure::Sides::right;
        case Sides::eachEar: return position % 2 == 0 ? MixMeasure::Sides::left : MixMeasure::Sides::right;
    }
    return MixMeasure::Sides::both;
}

void MixMonitorEqSession::beginPosition()
{
    measure().setSides (positionSides ((int) positions.size()));
    measure().setNoise (true, currentLevelDb());
    measure().takeInputPeak();
    step = Step::settling;
    marked = false;
    progress = 0.0f;
    message.clear();
}

void MixMonitorEqSession::next()
{
    if (step != Step::waiting)
        return;
    beginPosition();
    changed();
}

void MixMonitorEqSession::finish()
{
    if (! isRunning() || positions.empty())
        return;
    computeResult();
}

void MixMonitorEqSession::stop()
{
    if (! isRunning())
        return;
    endRun ({});
}

void MixMonitorEqSession::clear()
{
    if (isRunning())
        endRun ({});
    step = Step::idle;
    hasResponse = false;
    suggestion = {};
    message.clear();
    changed();
}

void MixMonitorEqSession::endRun (const juce::String& why)
{
    stopTimer();
    if (! pendingStart)
        measure().stop();
    pendingStart = false;
    step = hasResponse ? Step::done : Step::idle;
    progress = 0.0f;
    message = why;
    changed();
}

//==============================================================================
void MixMonitorEqSession::timerCallback()
{
    if (! isRunning())
    {
        stopTimer();
        return;
    }

    if (pendingStart)
    {
        if (! measure().start (bus, options.input, positionSides (0)))
            return;
        pendingStart = false;
    }

    // Anything that ends a run at once.
    if (! measure().isBusy())
        return endRun ("The audio restarted; the measurement stopped.");
    if (processor.getScenes().getCurrent() != scene)
        return endRun ("A scene was recalled; the measurement stopped.");
    if (processor.getBusType (bus) != mix::BusType::aux)
        return endRun ("The bus is no longer an Aux bus; the measurement stopped.");

    const auto rate = measure().getSampleRate();
    const auto sums = measure().read();
    const auto peak = measure().takeInputPeak();
    const auto steadyFor = [&] { return (double) (sums.steadySamples - mark.steadySamples) / rate; };

    if (peak >= kClipLevel && step != Step::waiting)
    {
        measure().setNoise (false, currentLevelDb());
        step = Step::waiting;
        progress = 0.0f;
        message = "The input clipped. Lower the mic's gain (or the level) and measure this position again.";
        if (positions.empty() && floor.seconds <= 0.0)
            return endRun (message);
        changed();
        return;
    }

    switch (step)
    {
        case Step::floor:
            if (! sums.steady)
                break;
            if (! marked)
            {
                mark = sums;
                marked = true;
                floor = {};
            }
            progress = (float) juce::jlimit (0.0, 1.0, steadyFor() / kFloorSeconds);
            if (steadyFor() >= kFloorSeconds)
            {
                floor = windowBetween (mark, sums, rate);
                beginPosition();
            }
            changed();
            break;

        case Step::settling:
            if (! sums.steady || ! sums.noiseOn)
                break;
            if (! marked)
            {
                mark = sums;
                marked = true;
            }
            progress = (float) juce::jlimit (0.0, 1.0, steadyFor() / kSettleSeconds);
            if (steadyFor() >= kSettleSeconds)
            {
                mark = sums;
                step = Step::measuring;
                progress = 0.0f;
            }
            changed();
            break;

        case Step::measuring:
        {
            const auto window = windowBetween (mark, sums, rate);
            progress = (float) juce::jlimit (0.0, 1.0, steadyFor() / kPositionSeconds);
            live (window);
            if (steadyFor() >= kPositionSeconds)
            {
                positions.push_back (window);
                positionSide.push_back ((int) positionSides ((int) positions.size() - 1));
                measure().setNoise (false, currentLevelDb());
                if ((int) positions.size() >= currentPositions())
                    return computeResult();
                step = Step::waiting;
                progress = 0.0f;
            }
            changed();
            break;
        }

        case Step::waiting:
        case Step::idle:
        case Step::done:
            break;
    }
}

void MixMonitorEqSession::live (const Window& window)
{
    response = measuredResponse (floor, { window }, currentMicCurve(), measure().getSampleRate());
    hasResponse = false;
    updateSuggestion();
}

void MixMonitorEqSession::computeResult()
{
    const auto rate = measure().getSampleRate();
    const auto& mic = currentMicCurve();
    response = measuredResponse (floor, positions, mic, rate);
    hasResponse = true;

    // Each ear: left against right.
    earDifference = 0.0f;
    if (currentSides() == Sides::eachEar)
    {
        std::vector<Window> left, right;
        for (size_t i = 0; i < positions.size(); ++i)
            (positionSide[i] == (int) MixMeasure::Sides::left ? left : right).push_back (positions[i]);
        if (! left.empty() && ! right.empty())
            earDifference = maxDifferenceDb (measuredResponse (floor, left, mic, rate), measuredResponse (floor, right, mic, rate));
    }

    stopTimer();
    measure().stop();
    step = Step::done;
    progress = 0.0f;
    updateSuggestion();
    message = earDifference > kEarDifferenceWarningDb
                ? "The ears differ by up to " + juce::String (earDifference, 1)
                      + " dB. Check the fit in the coupler; the correction uses their average."
                : juce::String();
    changed();
}

void MixMonitorEqSession::updateSuggestion()
{
    monitoreq::Options o;
    o.type = options.type;
    o.target = currentTarget();
    o.bassDb = options.bassDb;
    o.strength = options.strength;
    suggestion = suggest (response, o, measuredGains);
}

juce::String MixMonitorEqSession::describeStep() const
{
    const auto total = currentPositions();
    const auto n = (int) positions.size();
    const auto where = [this] (int position)
    {
        if (currentSides() == Sides::eachEar)
            return juce::String (positionSides (position) == MixMeasure::Sides::left ? " (left earpiece)" : " (right earpiece)");
        return juce::String();
    };
    switch (step)
    {
        case Step::idle:
            return options.type == Type::speaker
                     ? "Hold the mic at ear height where the performer stands, 50 cm to 1 m from the wedge, pointed at it."
                     : "Put the in-ear in the coupler. Never wear the in-ears while measuring.";
        case Step::floor:     return "Measuring the room's noise. Keep quiet.";
        case Step::settling:
        case Step::measuring: return "Position " + juce::String (n + 1) + " of " + juce::String (total) + where (n) + ": measuring.";
        case Step::waiting:
            return options.type == Type::speaker
                     ? "Move the mic a little (another spot around the performer's head), then Next."
                     : "Seat the " + juce::String (currentSides() == Sides::eachEar ? "next earpiece" : "in-ear again")
                           + where (n) + " in the coupler, then Next.";
        case Step::done:
            return hasResponse ? "Done. Check the suggestion, then Apply." : juce::String();
    }
    return {};
}

//==============================================================================
int MixMonitorEqSession::findGraphEq (int busIndex) const
{
    auto& rack = processor.getRack();
    for (int s = 0; s < InsertRack::numSlots (InsertRack::Strip::bus); ++s)
        if (const auto* type = rack.typeOf ({ InsertRack::Strip::bus, busIndex, s }); type != nullptr && type->name == kGraphEq)
            return s;
    return -1;
}

int MixMonitorEqSession::findFreeSlot (int busIndex) const
{
    auto& rack = processor.getRack();
    for (int s = 0; s < InsertRack::numSlots (InsertRack::Strip::bus); ++s)
        if (rack.get ({ InsertRack::Strip::bus, busIndex, s }) == nullptr)
            return s;
    return -1;
}

Bands MixMonitorEqSession::graphEqGains (int busIndex, int slot) const
{
    Bands out {};
    auto& rack = processor.getRack();
    const InsertRack::Slot s { InsertRack::Strip::bus, busIndex, slot };
    auto* eq = dynamic_cast<RKGraphEQProcessor*> (rack.get (s));
    if (eq == nullptr || ! rack.isOn (s))
        return out;
    const auto settings = eq->currentSettings();
    if (settings.mode == GraphEQEngine::Mode::thirtyOne)
    {
        for (int b = 0; b < kBands; ++b)
            out[(size_t) b] = settings.gainsDb[(size_t) kThirtyOneMode][(size_t) b];
        return out;
    }
    const auto rate = juce::jmax (44100.0, measure().getSampleRate());
    for (int b = 0; b < kBands; ++b)
        out[(size_t) b] = (float) GraphEQEngine::magnitudeDb (settings, rate, MixMeasure::centres()[(size_t) b]);
    return out;
}

bool MixMonitorEqSession::canApply() const
{
    return applyProblem().isEmpty();
}

juce::String MixMonitorEqSession::applyProblem() const
{
    if (! hasResult())
        return "Nothing measured yet.";
    if (processor.getBusType (bus) != mix::BusType::aux)
        return "The bus is no longer an Aux bus.";
    if (findGraphEq (bus) < 0 && findFreeSlot (bus) < 0)
        return "All 4 insert slots on this bus are taken. Free one for an RKGraphEQ.";
    return {};
}

void MixMonitorEqSession::apply()
{
    if (! canApply())
        return;
    auto& rack = processor.getRack();

    UndoState u;
    u.bus = bus;
    u.slot = findGraphEq (bus);
    if (u.slot < 0)
    {
        u.slot = findFreeSlot (bus);
        u.inserted = true;
        if (rack.insert ({ InsertRack::Strip::bus, bus, u.slot }, kGraphEq, processor.channelsOf (InsertRack::Strip::bus, bus)) == nullptr)
            return;
    }
    const InsertRack::Slot slot { InsertRack::Strip::bus, bus, u.slot };
    auto* eq = dynamic_cast<RKGraphEQProcessor*> (rack.get (slot));
    if (eq == nullptr)
        return;

    if (! u.inserted)
    {
        u.wasOn = rack.isOn (slot);
        u.mode = rkfx::ProcessorBase::valueOf (eq->findParam ("bands"));
        for (int b = 0; b < kBands; ++b)
            u.gains[(size_t) b] = rkfx::ProcessorBase::valueOf (eq->findParam (GraphEQParams::bandId (kThirtyOneMode, b)));
    }

    setParam (*eq, "bands", (float) kThirtyOneMode);
    for (int b = 0; b < kBands; ++b)
        setParam (*eq, GraphEQParams::bandId (kThirtyOneMode, b), suggestion.gains[(size_t) b]);
    rack.setOn (slot, true);

    undoState = u;
    message = "Applied to " + juce::String (u.inserted ? "a new RKGraphEQ" : "the RKGraphEQ") + " in insert "
              + juce::String (u.slot + 1) + ". Measure again to check.";
    changed();
}

void MixMonitorEqSession::undo()
{
    if (! undoState)
        return;
    const auto u = *undoState;
    undoState.reset();
    auto& rack = processor.getRack();
    const InsertRack::Slot slot { InsertRack::Strip::bus, u.bus, u.slot };
    auto* eq = dynamic_cast<RKGraphEQProcessor*> (rack.get (slot));
    if (eq == nullptr)
    {
        message = "The RKGraphEQ is gone; nothing to undo.";
    }
    else if (u.inserted)
    {
        rack.remove (slot);
        message = "Undone: the RKGraphEQ removed.";
    }
    else
    {
        setParam (*eq, "bands", u.mode);
        for (int b = 0; b < kBands; ++b)
            setParam (*eq, GraphEQParams::bandId (kThirtyOneMode, b), u.gains[(size_t) b]);
        rack.setOn (slot, u.wasOn);
        message = "Undone: the RKGraphEQ as it was.";
    }
    changed();
}

//==============================================================================
juce::var MixMonitorEqSession::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", (int) options.type);
    o->setProperty ("input", options.input);
    o->setProperty ("mic", (int) options.mic);
    o->setProperty ("useCalibration", options.useCalibration);
    if (options.hasCalibration)
    {
        o->setProperty ("calibrationName", options.calibrationName);
        o->setProperty ("calibration", bandsToVar (options.calibration));
    }
    o->setProperty ("speakerTarget", (int) options.speakerTarget);
    o->setProperty ("inEarTarget", (int) options.inEarTarget);
    o->setProperty ("bass", options.bassDb);
    o->setProperty ("speakerLevel", options.speakerLevelDb);
    o->setProperty ("inEarLevel", options.inEarLevelDb);
    o->setProperty ("sides", (int) options.sides);
    o->setProperty ("speakerPositions", options.speakerPositions);
    o->setProperty ("inEarPositions", options.inEarPositions);
    o->setProperty ("strength", options.strength);
    o->setProperty ("useFile", options.useFile);
    if (options.hasFile)
    {
        o->setProperty ("fileName", options.fileName);
        o->setProperty ("file", bandsToVar (options.file));
    }
    return juce::var (o);
}

void MixMonitorEqSession::fromVar (const juce::var& v)
{
    clear();
    Options o;
    if (v.isObject())
    {
        o.type = enumFrom (v["type"], Type::speaker, 2);
        o.input = (int) v.getProperty ("input", 0);
        o.mic = enumFrom (v["mic"], Mic::flat, 4);
        o.hasCalibration = bandsFromVar (v["calibration"], o.calibration);
        o.calibrationName = v["calibrationName"].toString();
        o.useCalibration = (bool) v.getProperty ("useCalibration", false);
        o.speakerTarget = enumFrom (v["speakerTarget"], Target::gentleTilt, 5);
        o.inEarTarget = enumFrom (v["inEarTarget"], Target::speakersInRoom, 5);
        o.bassDb = (float) v.getProperty ("bass", 4.0);
        o.speakerLevelDb = (float) v.getProperty ("speakerLevel", -20.0);
        o.inEarLevelDb = (float) v.getProperty ("inEarLevel", -40.0);
        o.sides = enumFrom (v["sides"], Sides::both, 4);
        o.speakerPositions = (int) v.getProperty ("speakerPositions", 4);
        o.inEarPositions = (int) v.getProperty ("inEarPositions", 2);
        o.strength = (float) v.getProperty ("strength", 0.75);
        o.hasFile = bandsFromVar (v["file"], o.file);
        o.fileName = v["fileName"].toString();
        o.useFile = (bool) v.getProperty ("useFile", false);
    }
    undoState.reset();
    setOptions (o);
}
