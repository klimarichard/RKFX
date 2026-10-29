#include "FollowEngine.h"

#include <cmath>

using rkfx::Dynamics::kSilenceDb;
using rkfx::Dynamics::toDb;

namespace
{
    constexpr float kSlowLevelMs = 80.0f;   // the "before" level a retrigger jump is measured against

    float midiOf (float hz) noexcept { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
}

int FollowEngine::velocityFor (const Settings& s, float levelDb)
{
    const auto lo = juce::jlimit (1, 127, juce::jmin (s.velocityMin, s.velocityMax));
    const auto hi = juce::jlimit (1, 127, juce::jmax (s.velocityMin, s.velocityMax));
    const auto t = juce::jlimit (0.0f, 1.0f, (levelDb - s.thresholdDb) / kVelocitySpanDb);
    return juce::jlimit (1, 127, juce::roundToInt ((float) lo + t * (float) (hi - lo)));
}

int FollowEngine::expressionFor (const Settings& s, float levelDb)
{
    const auto span = juce::jmax (1.0f, s.ceilingDb - s.floorDb);
    const auto t = juce::jlimit (0.0f, 1.0f, (levelDb - s.floorDb) / span);

    // Curve: +100% rises fast at first (exponent 1/4), -100% slowly (exponent 4).
    const auto shaped = std::pow (t, std::pow (4.0f, -juce::jlimit (-1.0f, 1.0f, s.curve)));
    return juce::jlimit (0, 127, juce::roundToInt ((float) s.expressionMin + shaped * (float) (s.expressionMax - s.expressionMin)));
}

//==============================================================================
void FollowEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    tracker.prepare (sampleRate);
    gateLevel.prepare (sampleRate);
    slowLevel.prepare (sampleRate);
    slowLevel.setTimes (kSlowLevelMs, kSlowLevelMs);
    rms.prepare (sampleRate);
    rms.setMode (rkfx::Dynamics::LevelDetector::Mode::rms);
    configured = false;
    setSettings (settings);
    reset();
}

void FollowEngine::reset()
{
    tracker.setRange (settings.range);
    tracker.reset();
    gateLevel.reset();
    slowLevel.reset();
    rms.reset();

    now = 0;
    lastCycle = -1;
    pendingSince = 0;
    cycleCount = 0;
    lastSpanHz = period = 0.0f;
    candidateNote = -1;
    candidateCount = sinceCycle = 0;
    gateOpen = pitchValid = false;
    heldNote = pendingNote = -1;
    onsetPeakDb = kSilenceDb;
    retriggerArmed = true;

    soundingNote = -1;
    nextMpeChannel = kFirstMpeChannel;
    bend = 0.0f;
    lastBendValue = 8192;
    sinceBend = 0;
    expressionLevel = 0.0f;
    lastExpression = -1;
    sinceExpression = 0;
    mpeAnnounced = false;
    meter = {};
}

void FollowEngine::setSettings (const Settings& s)
{
    // Changes that would leave the sounding note wrong end it first (in process()).
    if (configured && (s.range != settings.range || s.transpose != settings.transpose || s.channel != settings.channel
                       || s.bend != settings.bend || s.bendRange != settings.bendRange))
    {
        releasePending = true;
        if (s.bend != settings.bend || s.bendRange != settings.bendRange)
            mpeAnnounced = false;
    }
    configured = true;

    settings = s;
    settings.channel = juce::jlimit (1, 16, s.channel);
    settings.bendRange = juce::jlimit (1, 96, s.bendRange);
    gateLevel.setTimes (1.0f, juce::jmax (10.0f, 2500.0f / rkfx::PitchTracker::limitsFor (s.range).lowHz));
}

void FollowEngine::announceMpe (juce::MidiBuffer& midi, int sample) noexcept
{
    // MPE configuration (RPN 6 on the master channel): a lower zone of 14
    // member channels, then each member's pitch bend range (RPN 0).
    auto rpn = [&midi, sample] (int channel, int number, int value)
    {
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 101, 0), sample);
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 100, number), sample);
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 6, value), sample);
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 101, 127), sample);
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 100, 127), sample);
    };

    rpn (1, 6, kLastMpeChannel - kFirstMpeChannel + 1);
    for (int ch = kFirstMpeChannel; ch <= kLastMpeChannel; ++ch)
        rpn (ch, 0, settings.bendRange);
    mpeAnnounced = true;
}

void FollowEngine::sendBend (juce::MidiBuffer& midi, int sample, bool force) noexcept
{
    if (settings.bend == Bend::off || soundingNote < 0)
        return;

    const auto value = juce::jlimit (0, 16383, 8192 + juce::roundToInt (bend / (float) settings.bendRange * 8191.0f));

    // At most every kBendIntervalMs, and only when it moves a cent or more.
    const auto oneCent = 8191.0f / (100.0f * (float) settings.bendRange);
    if (! force && (sinceBend < (int) (kBendIntervalMs * 0.001 * sampleRate) || std::abs ((float) (value - lastBendValue)) < oneCent))
        return;

    midi.addEvent (juce::MidiMessage::pitchWheel (soundingChannel, value), sample);
    lastBendValue = value;
    sinceBend = 0;
}

void FollowEngine::noteOn (int trackedNote, float levelDb, juce::MidiBuffer& midi, int sample) noexcept
{
    const auto note = trackedNote + settings.transpose;
    if (note < 0 || note > 127)
        return;

    const auto previousNote = soundingNote, previousChannel = soundingChannel;

    if (settings.bend == Bend::mpe)
    {
        if (! mpeAnnounced)
            announceMpe (midi, sample);
        soundingChannel = nextMpeChannel;
        nextMpeChannel = nextMpeChannel >= kLastMpeChannel ? kFirstMpeChannel : nextMpeChannel + 1;
    }
    else
    {
        soundingChannel = settings.channel;
    }

    const auto velocity = velocityFor (settings, levelDb);

    // The bend for the new note first, then the note, then (legato) the old note's end.
    // The same note again (a retrigger): end it first, or the new note would be cut.
    const auto sameNote = previousNote == note && previousChannel == soundingChannel;
    if (sameNote)
        midi.addEvent (juce::MidiMessage::noteOff (previousChannel, previousNote), sample);

    soundingNote = note;
    bend = settings.bend == Bend::off ? 0.0f : juce::jlimit (-(float) settings.bendRange, (float) settings.bendRange, pitchMidi - (float) trackedNote);
    sendBend (midi, sample, true);
    midi.addEvent (juce::MidiMessage::noteOn (soundingChannel, note, (juce::uint8) velocity), sample);
    if (previousNote >= 0 && ! sameNote)
        midi.addEvent (juce::MidiMessage::noteOff (previousChannel, previousNote), sample);

    meter.note = note;
    meter.velocity = velocity;
}

void FollowEngine::noteOff (juce::MidiBuffer& midi, int sample) noexcept
{
    if (soundingNote < 0)
        return;

    midi.addEvent (juce::MidiMessage::noteOff (soundingChannel, soundingNote), sample);
    if (settings.bend != Bend::off && lastBendValue != 8192)
        midi.addEvent (juce::MidiMessage::pitchWheel (soundingChannel, 8192), sample);

    soundingNote = -1;
    lastBendValue = 8192;
    bend = 0.0f;
    meter.note = -1;
    meter.bendCents = 0.0f;
}

void FollowEngine::release (juce::MidiBuffer& midi, int sample) noexcept
{
    noteOff (midi, sample);
    heldNote = pendingNote = -1;
}

void FollowEngine::trackSample (float x, juce::MidiBuffer& midi, int sample) noexcept
{
    ++now;
    ++sinceCycle;
    ++sinceBend;
    ++sinceExpression;

    // The gate: the threshold, with kGateHysteresisDb below it to close.
    const auto level = gateLevel.process (x);
    const auto levelDb = toDb (level);
    const auto slowDb = toDb (slowLevel.process (x));
    meter.levelDb = juce::jmax (meter.levelDb, levelDb);

    if (! gateOpen && levelDb > settings.thresholdDb)
    {
        gateOpen = true;
        onsetPeakDb = levelDb;
        retriggerArmed = false;
    }
    else if (gateOpen && levelDb < settings.thresholdDb - kGateHysteresisDb)
    {
        gateOpen = false;
        tracker.reset();
        pitchValid = false;
        lastCycle = -1;
        cycleCount = 0;
        lastSpanHz = 0.0f;
        candidateNote = -1;
        candidateCount = 0;
        heldNote = pendingNote = -1;
        noteOff (midi, sample);
    }

    if (gateOpen)
        onsetPeakDb = juce::jmax (onsetPeakDb, levelDb);

    // Retrigger: a sudden jump on the same note (a new bow stroke) starts it again.
    if (settings.retriggerDb > 0.0f && soundingNote >= 0)
    {
        const auto jump = levelDb - slowDb;
        if (retriggerArmed && jump > settings.retriggerDb)
        {
            retriggerArmed = false;
            onsetPeakDb = levelDb;
            noteOn (heldNote, levelDb, midi, sample);
        }
        else if (! retriggerArmed && jump < 0.5f * settings.retriggerDb)
        {
            retriggerArmed = true;
        }
    }

    // No cycle for 2.5 periods: the pitch is lost and the note ends.
    if (pitchValid && (float) sinceCycle > 2.5f * period)
    {
        pitchValid = false;
        heldNote = pendingNote = -1;
        noteOff (midi, sample);
    }

    if (tracker.process (gateOpen ? x : 0.0f) && gateOpen)
    {
        const auto interval = lastCycle >= 0 ? (float) (now - lastCycle) : 0.0f;
        lastCycle = now;
        const auto limits = rkfx::PitchTracker::limitsFor (settings.range);
        const auto inRange = interval > 0.0f && interval >= (float) sampleRate / (limits.highHz * 1.5f)
                                             && interval <= (float) sampleRate / (limits.lowHz * 0.5f);
        if (! inRange)
        {
            cycleCount = 0;   // a missed or doubled crossing: start the span again
            lastSpanHz = 0.0f;
            cycleTimes[cycleCount++] = now;
            return;
        }

        // The cycle times of the last kSpanMs (at least two cycles).
        if (cycleCount > kMaxSpanCycles)
        {
            for (int i = 1; i <= kMaxSpanCycles; ++i)
                cycleTimes[i - 1] = cycleTimes[i];
            cycleCount = kMaxSpanCycles;
        }
        cycleTimes[cycleCount++] = now;

        const auto spanSamples = (std::int64_t) (kSpanMs * 0.001 * sampleRate);
        auto first = cycleCount - 1;
        while (first > 0 && (now - cycleTimes[first - 1] <= spanSamples || cycleCount - first < 3))
            --first;
        const auto cycles = cycleCount - 1 - first;
        if (cycles < 2)
            return;

        const auto spanHz = (float) (sampleRate * cycles / (double) (now - cycleTimes[first]));
        const auto agreement = lastSpanHz > 0.0f ? std::abs (spanHz - lastSpanHz) / spanHz : 1.0f;
        lastSpanHz = spanHz;
        if (agreement >= kStablePeriod)
            return;

        period = interval;
        sinceCycle = 0;
        pitchValid = true;
        pitchMidi = midiOf (spanHz);
        const auto nearest = (int) std::lround (pitchMidi);

        // A first note or a jump: confirmed by kConfirmCycles close agreements.
        candidateCount = (nearest == candidateNote && agreement < kConfirmPeriod) ? candidateCount + 1 : 1;
        candidateNote = nearest;

        auto newNote = -1;
        if (heldNote < 0 || std::abs (pitchMidi - (float) heldNote) > kHoldRangeSemitones)
        {
            pendingNote = -1;
            // ...and once the tracker's filter has settled on it (its own pitch agrees):
            // until then the readings run flat or sharp.
            const auto settled = tracker.getHz() > 0.0f && std::abs (tracker.getHz() - spanHz) < kConfirmPeriod * spanHz;
            if (candidateCount >= kConfirmCycles && settled)
                newNote = nearest;
        }
        else if (std::abs (pitchMidi - (float) heldNote) * 100.0f > kHysteresisCents)
        {
            if (nearest != pendingNote)
            {
                pendingNote = nearest;
                pendingSince = now;
            }
            else if ((double) (now - pendingSince) >= kHoldMs * 0.001 * sampleRate)
            {
                newNote = nearest;
                pendingNote = -1;
            }
        }
        else
        {
            pendingNote = -1;
        }

        if (newNote >= 0 && newNote != heldNote)
        {
            heldNote = juce::jlimit (0, 127, newNote);
            if (settings.notes)
                noteOn (heldNote, onsetPeakDb, midi, sample);
        }
    }

    // Pitch bend: the tracked pitch around the held note, glided.
    if (soundingNote >= 0 && settings.bend != Bend::off && pitchValid)
    {
        const auto target = juce::jlimit (-(float) settings.bendRange, (float) settings.bendRange, pitchMidi - (float) heldNote);
        const auto glide = settings.glideMs <= 0.0f ? 0.0f : rkfx::Dynamics::timeCoefficient (settings.glideMs, sampleRate);
        bend = target + glide * (bend - target);
        meter.bendCents = 100.0f * bend;
        sendBend (midi, sample, false);
    }
}

void FollowEngine::process (const float* const* input, int numChannels, int numSamples, juce::MidiBuffer& midi) noexcept
{
    numChannels = juce::jmax (1, numChannels);
    const auto responseCoef = rkfx::Dynamics::timeCoefficient (juce::jmax (1.0f, settings.responseMs), sampleRate);
    const auto expressionInterval = (int) (kExpressionIntervalMs * 0.001 * sampleRate);

    if (releasePending || (! settings.notes && soundingNote >= 0))
    {
        release (midi, 0);
        releasePending = false;
        tracker.setRange (settings.range);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        auto x = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            x += input[ch][i];
        x /= (float) numChannels;

        trackSample (x, midi, i);

        // Expression: the RMS level, smoothed by Response, sent on change.
        const auto r = rms.process (x);
        expressionLevel = r + responseCoef * (expressionLevel - r);

        if (settings.expression && sinceExpression >= expressionInterval)
        {
            const auto value = expressionFor (settings, toDb (expressionLevel));
            if (value != lastExpression)
            {
                const auto channel = expressionChannel();
                midi.addEvent (settings.controller >= kPressure ? juce::MidiMessage::channelPressureChange (channel, value)
                                                                : juce::MidiMessage::controllerEvent (channel, settings.controller, value),
                               i);
                lastExpression = value;
                meter.expression = value;
                sinceExpression = 0;
            }
        }
    }
}
