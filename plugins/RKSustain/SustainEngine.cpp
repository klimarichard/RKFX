#include "SustainEngine.h"

void SustainEngine::prepare()
{
    out.ensureSize (4096);
    reset();
}

void SustainEngine::reset()
{
    for (auto* a : { &keyDown, &sounding, &caught })
        for (auto& channel : *a)
            channel.fill (false);
    ccPedal.fill (false);
    pedal = parameterWas = false;
    keysDown = 0;
}

void SustainEngine::endHeld (int position, bool exceptKeysDown) noexcept
{
    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            if (sounding[(size_t) ch][(size_t) n] && ! (exceptKeysDown && keyDown[(size_t) ch][(size_t) n]))
            {
                out.addEvent (juce::MidiMessage::noteOff (ch + 1, n), position);
                sounding[(size_t) ch][(size_t) n] = false;
                caught[(size_t) ch][(size_t) n] = false;
            }
}

void SustainEngine::setPedal (bool down, int position) noexcept
{
    if (down == pedal)
        return;
    pedal = down;

    switch (settings.mode)
    {
        case Mode::sustain:
            // Up: what isn't held by a key ends.
            if (! down)
                endHeld (position, true);
            break;

        case Mode::sostenuto:
            // Down: catch the notes held now; up: release them (unless still held).
            for (int ch = 0; ch < 16; ++ch)
                for (int n = 0; n < 128; ++n)
                {
                    if (down)
                        caught[(size_t) ch][(size_t) n] = keyDown[(size_t) ch][(size_t) n];
                    else if (caught[(size_t) ch][(size_t) n])
                    {
                        caught[(size_t) ch][(size_t) n] = false;
                        if (! keyDown[(size_t) ch][(size_t) n] && sounding[(size_t) ch][(size_t) n])
                        {
                            out.addEvent (juce::MidiMessage::noteOff (ch + 1, n), position);
                            sounding[(size_t) ch][(size_t) n] = false;
                        }
                    }
                }
            break;

        case Mode::hold:
        case Mode::latch:
            // A press ends everything held.
            if (down)
                endHeld (position, true);
            break;
    }
}

void SustainEngine::noteOn (int channel, int note, juce::uint8 velocity, int position) noexcept
{
    auto& isSounding = sounding[(size_t) channel][(size_t) note];

    if (settings.mode == Mode::latch)
    {
        // Each press switches the note.
        if (isSounding)
            out.addEvent (juce::MidiMessage::noteOff (channel + 1, note), position);
        else
            out.addEvent (juce::MidiMessage::noteOn (channel + 1, note, velocity), position);
        isSounding = ! isSounding;
        return;
    }

    // Hold: a new chord (after all keys were up) replaces the held one.
    if (settings.mode == Mode::hold && keysDown == 0)
        endHeld (position, false);

    if (! keyDown[(size_t) channel][(size_t) note])
        ++keysDown;
    keyDown[(size_t) channel][(size_t) note] = true;

    // Played again while sustained: restart it.
    if (isSounding)
        out.addEvent (juce::MidiMessage::noteOff (channel + 1, note), position);
    out.addEvent (juce::MidiMessage::noteOn (channel + 1, note, velocity), position);
    isSounding = true;
}

void SustainEngine::noteOff (int channel, int note, int position) noexcept
{
    if (settings.mode == Mode::latch)
        return;    // latched notes end on the next press

    if (keyDown[(size_t) channel][(size_t) note])
    {
        keyDown[(size_t) channel][(size_t) note] = false;
        keysDown = juce::jmax (0, keysDown - 1);
    }

    auto& isSounding = sounding[(size_t) channel][(size_t) note];
    const auto keep = (settings.mode == Mode::sustain && pedal)
                   || (settings.mode == Mode::sostenuto && caught[(size_t) channel][(size_t) note])
                   || settings.mode == Mode::hold;
    if (keep && isSounding)
        return;

    // Ends now (also a note this engine never saw: passed on).
    out.addEvent (juce::MidiMessage::noteOff (channel + 1, note), position);
    isSounding = false;
}

void SustainEngine::process (juce::MidiBuffer& midi) noexcept
{
    out.clear();

    // The Pedal parameter (a host footswitch) acts at the block's start.
    if (settings.pedalParameter != parameterWas)
    {
        parameterWas = settings.pedalParameter;
        bool anyCC = false;
        for (const auto c : ccPedal)
            anyCC = anyCC || c;
        setPedal (parameterWas || anyCC, 0);
    }

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto channel = juce::jlimit (0, 15, message.getChannel() - 1);
        const auto position = metadata.samplePosition;

        if (message.isNoteOn())
            noteOn (channel, message.getNoteNumber(), message.getVelocity(), position);
        else if (message.isNoteOff())
            noteOff (channel, message.getNoteNumber(), position);
        else if (message.isController() && message.getControllerNumber() == settings.controller)
        {
            // The pedal's controller is swallowed (the instrument mustn't sustain twice).
            ccPedal[(size_t) channel] = message.getControllerValue() >= 64;
            bool anyCC = false;
            for (const auto c : ccPedal)
                anyCC = anyCC || c;
            setPedal (anyCC || parameterWas, position);
        }
        else
        {
            if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                sounding[(size_t) channel].fill (false);
                keyDown[(size_t) channel].fill (false);
                caught[(size_t) channel].fill (false);
            }
            out.addEvent (message, position);
        }
    }

    midi.swapWith (out);
}

void SustainEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            if (sounding[(size_t) ch][(size_t) n])
                midi.addEvent (juce::MidiMessage::noteOff (ch + 1, n), 0);
    reset();
}
