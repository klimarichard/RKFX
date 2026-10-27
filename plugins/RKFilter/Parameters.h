#pragma once

#include "dsp/TempoSync.h"
#include "plugin/ParamBuilder.h"

/** RKFilter parameter IDs and ranges. IDs are permanent: never rename, reuse
    or reorder; only append. */
namespace FilterParams
{
    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        p.addChoice    ("mode", "Mode", { "Low-pass", "High-pass", "Band-pass" }, { "lowpass", "highpass", "bandpass" }, 0);
        p.addChoice    ("slope", "Slope", { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, { "12", "24", "36", "48" }, 0);
        p.addFrequency ("cutoff", "Cutoff", 20.0f, 20000.0f, 1000.0f);
        p.addPercent   ("resonance", "Resonance", 0.0f, 100.0f, 20.0f);

        p.addPercent   ("env_amount", "Envelope Amount", -100.0f, 100.0f, 0.0f);
        p.addDecibels  ("env_sens", "Envelope Sensitivity", -24.0f, 24.0f, 0.0f);
        p.addTime      ("env_attack", "Envelope Attack", 1.0f, 100.0f, 5.0f);
        p.addTime      ("env_release", "Envelope Release", 20.0f, 1000.0f, 150.0f);

        juce::StringArray divisionNames, divisionKeys;
        for (int i = 0; i < rkfx::SyncDivision::kCount; ++i)
        {
            divisionNames.add (rkfx::SyncDivision::names[i]);
            divisionKeys.add (rkfx::SyncDivision::keys[i]);
        }

        p.addPercent   ("lfo_depth", "LFO Depth", -100.0f, 100.0f, 0.0f);
        p.addChoice    ("lfo_shape", "LFO Shape",
                        { "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Sample & Hold" },
                        { "sine", "triangle", "square", "sawup", "sawdown", "samplehold" }, 0);
        p.addFrequency ("lfo_rate", "LFO Rate", 0.05f, 20.0f, 1.0f);
        p.addBool      ("lfo_sync", "LFO Sync", false);
        p.addChoice    ("lfo_division", "LFO Division", divisionNames, divisionKeys, rkfx::SyncDivision::index (4));
        p.addDegrees   ("lfo_phase", "LFO Stereo Phase", 0.0f, 180.0f, 0.0f);

        return p;
    }
}
