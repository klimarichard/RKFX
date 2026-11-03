#pragma once

#include <juce_core/juce_core.h>

/** The remote's web pages and the console app's files (remote/, embedded at build time). */
namespace MixRemotePage
{
    /** The file called @p name ("monitor.html", "console.js"), or nullptr. */
    const char* find (const juce::String& name, int& size);
}
