#pragma once

#include "host/Tempo.h"

namespace rkboard
{

// The standalone app's tempo comes from the pedal host (host/), shared with RKMix.
using rkfx::TapTempo;
using rkfx::MidiClockFollower;
using BoardPlayHead = rkfx::StandalonePlayHead;

} // namespace rkboard
