#pragma once

#include <juce_graphics/juce_graphics.h>

#include <vector>

/** A QR code for a short text (a URL): byte mode, error correction level M,
    versions 1 to 10 (up to 213 bytes). The encoding follows ISO/IEC 18004,
    as in Project Nayuki's reference generator. GUI-free apart from draw(). */
class MixQr
{
public:
    /** Encodes @p text; isValid() is false when it doesn't fit. */
    explicit MixQr (const juce::String& text);

    bool isValid() const noexcept { return size > 0; }
    int getSize() const noexcept { return size; }           // modules per side
    bool isDark (int x, int y) const noexcept { return modules[(size_t) (y * size + x)]; }

    /** Draws it dark on light, with a quiet zone of 4 modules, into @p area (square, centred). */
    void draw (juce::Graphics&, juce::Rectangle<float> area, juce::Colour dark = juce::Colours::black,
               juce::Colour light = juce::Colours::white) const;

private:
    int size = 0;
    std::vector<bool> modules, isFunction;

    void set (int x, int y, bool dark) { modules[(size_t) (y * size + x)] = dark; }
    void setFunction (int x, int y, bool dark) { set (x, y, dark); isFunction[(size_t) (y * size + x)] = true; }
    void drawFunctionPatterns (int version);
    void drawFormatBits (int mask);
    void drawVersion (int version);
    void drawCodewords (const std::vector<std::uint8_t>&);
    void applyMask (int mask);
    long penalty() const;
};
