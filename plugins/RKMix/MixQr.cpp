#include "MixQr.h"

#include <array>

namespace
{
    // Level M, versions 1 to 10: error correction codewords per block, and blocks.
    constexpr std::array<int, 11> kEccPerBlock { -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 };
    constexpr std::array<int, 11> kBlocks      { -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5 };
    constexpr int kMaxVersion = 10;
    constexpr int kFormatBitsM = 0;

    /** Data and error correction modules of a version (all but the function patterns). */
    int rawDataModules (int version)
    {
        auto result = (16 * version + 128) * version + 64;
        if (version >= 2)
        {
            const auto numAlign = version / 7 + 2;
            result -= (25 * numAlign - 10) * numAlign - 55;
            if (version >= 7)
                result -= 36;
        }
        return result;
    }

    int dataCodewords (int version)
    {
        return rawDataModules (version) / 8 - kEccPerBlock[(size_t) version] * kBlocks[(size_t) version];
    }

    std::vector<int> alignmentPositions (int version)
    {
        if (version == 1)
            return {};
        const auto numAlign = version / 7 + 2;
        const auto size = version * 4 + 17;
        const auto step = (version * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2) * 2;
        std::vector<int> result (1, 6);
        for (int i = 0, pos = size - 7; i < numAlign - 1; ++i, pos -= step)
            result.insert (result.begin() + 1, pos);
        return result;
    }

    // Reed-Solomon over GF(2^8), polynomial 0x11D
    std::uint8_t multiply (std::uint8_t x, std::uint8_t y)
    {
        int z = 0;
        for (int i = 7; i >= 0; --i)
        {
            z = (z << 1) ^ ((z >> 7) * 0x11D);
            z ^= ((y >> i) & 1) * x;
        }
        return (std::uint8_t) z;
    }

    std::vector<std::uint8_t> divisor (int degree)
    {
        std::vector<std::uint8_t> result ((size_t) degree, 0);
        result.back() = 1;
        std::uint8_t root = 1;
        for (int i = 0; i < degree; ++i)
        {
            for (size_t j = 0; j < result.size(); ++j)
            {
                result[j] = multiply (result[j], root);
                if (j + 1 < result.size())
                    result[j] ^= result[j + 1];
            }
            root = multiply (root, 0x02);
        }
        return result;
    }

    std::vector<std::uint8_t> remainder (const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& div)
    {
        std::vector<std::uint8_t> result (div.size(), 0);
        for (const auto b : data)
        {
            const auto factor = (std::uint8_t) (b ^ result.front());
            result.erase (result.begin());
            result.push_back (0);
            for (size_t i = 0; i < result.size(); ++i)
                result[i] ^= multiply (div[i], factor);
        }
        return result;
    }

    bool bit (long x, int i) { return ((x >> i) & 1) != 0; }

    bool maskBit (int mask, int x, int y)
    {
        switch (mask)
        {
            case 0: return (x + y) % 2 == 0;
            case 1: return y % 2 == 0;
            case 2: return x % 3 == 0;
            case 3: return (x + y) % 3 == 0;
            case 4: return (x / 3 + y / 2) % 2 == 0;
            case 5: return x * y % 2 + x * y % 3 == 0;
            case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
            case 7: return ((x + y) % 2 + x * y % 3) % 2 == 0;
            default: return false;
        }
    }
}

//==============================================================================
MixQr::MixQr (const juce::String& text)
{
    const auto* utf8 = text.toRawUTF8();
    const auto length = (int) text.getNumBytesAsUTF8();

    // The smallest version that holds it: byte mode, 8-bit count (16 from version 10).
    int version = 1;
    for (; version <= kMaxVersion; ++version)
    {
        const auto countBits = version < 10 ? 8 : 16;
        if (4 + countBits + 8 * length <= dataCodewords (version) * 8)
            break;
    }
    if (version > kMaxVersion)
        return;

    // The bit stream: mode, count, the bytes, a terminator, padding.
    std::vector<bool> bits;
    const auto append = [&bits] (long value, int count)
    {
        for (int i = count - 1; i >= 0; --i)
            bits.push_back (bit (value, i));
    };
    append (0x4, 4);
    append (length, version < 10 ? 8 : 16);
    for (int i = 0; i < length; ++i)
        append ((std::uint8_t) utf8[i], 8);
    const auto capacity = dataCodewords (version) * 8;
    append (0, std::min (4, capacity - (int) bits.size()));
    append (0, (8 - (int) bits.size() % 8) % 8);
    for (std::uint8_t pad = 0xEC; (int) bits.size() < capacity; pad ^= 0xEC ^ 0x11)
        append (pad, 8);

    std::vector<std::uint8_t> data (bits.size() / 8, 0);
    for (size_t i = 0; i < bits.size(); ++i)
        if (bits[i])
            data[i >> 3] |= (std::uint8_t) (1 << (7 - (int) (i & 7)));

    // Split into blocks, add each block's error correction, interleave.
    const auto numBlocks = kBlocks[(size_t) version];
    const auto eccLength = kEccPerBlock[(size_t) version];
    const auto rawCodewords = rawDataModules (version) / 8;
    const auto numShortBlocks = numBlocks - rawCodewords % numBlocks;
    const auto shortBlockLength = rawCodewords / numBlocks;
    const auto div = divisor (eccLength);
    std::vector<std::vector<std::uint8_t>> blocks;
    for (int i = 0, k = 0; i < numBlocks; ++i)
    {
        const auto n = shortBlockLength - eccLength + (i < numShortBlocks ? 0 : 1);
        std::vector<std::uint8_t> block (data.begin() + k, data.begin() + k + n);
        k += n;
        const auto ecc = remainder (block, div);
        if (i < numShortBlocks)
            block.push_back (0);
        block.insert (block.end(), ecc.begin(), ecc.end());
        blocks.push_back (std::move (block));
    }
    std::vector<std::uint8_t> codewords;
    for (size_t i = 0; i < blocks.front().size(); ++i)
        for (size_t j = 0; j < blocks.size(); ++j)
            if ((int) i != shortBlockLength - eccLength || (int) j >= numShortBlocks)
                codewords.push_back (blocks[j][i]);

    // Draw, then keep the mask with the lowest penalty.
    size = version * 4 + 17;
    modules.assign ((size_t) (size * size), false);
    isFunction.assign ((size_t) (size * size), false);
    drawFunctionPatterns (version);
    drawCodewords (codewords);

    int best = 0;
    long bestPenalty = -1;
    for (int mask = 0; mask < 8; ++mask)
    {
        applyMask (mask);
        drawFormatBits (mask);
        const auto p = penalty();
        if (bestPenalty < 0 || p < bestPenalty)
        {
            best = mask;
            bestPenalty = p;
        }
        applyMask (mask);   // XOR: undoes it
    }
    applyMask (best);
    drawFormatBits (best);
}

void MixQr::drawFunctionPatterns (int version)
{
    for (int i = 0; i < size; ++i)
    {
        setFunction (6, i, i % 2 == 0);
        setFunction (i, 6, i % 2 == 0);
    }

    const auto finder = [this] (int cx, int cy)
    {
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx)
            {
                const auto x = cx + dx, y = cy + dy;
                const auto dist = std::max (std::abs (dx), std::abs (dy));
                if (x >= 0 && x < size && y >= 0 && y < size)
                    setFunction (x, y, dist != 2 && dist != 4);
            }
    };
    finder (3, 3);
    finder (size - 4, 3);
    finder (3, size - 4);

    const auto positions = alignmentPositions (version);
    const auto n = (int) positions.size();
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))
                continue;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    setFunction (positions[(size_t) i] + dx, positions[(size_t) j] + dy, std::max (std::abs (dx), std::abs (dy)) != 1);
        }

    drawFormatBits (0);   // reserved now, written for real with the mask
    drawVersion (version);
}

void MixQr::drawFormatBits (int mask)
{
    const auto data = kFormatBitsM << 3 | mask;
    long rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const auto bits = (data << 10 | rem) ^ 0x5412;

    for (int i = 0; i <= 5; ++i)
        setFunction (8, i, bit (bits, i));
    setFunction (8, 7, bit (bits, 6));
    setFunction (8, 8, bit (bits, 7));
    setFunction (7, 8, bit (bits, 8));
    for (int i = 9; i < 15; ++i)
        setFunction (14 - i, 8, bit (bits, i));

    for (int i = 0; i < 8; ++i)
        setFunction (size - 1 - i, 8, bit (bits, i));
    for (int i = 8; i < 15; ++i)
        setFunction (8, size - 15 + i, bit (bits, i));
    setFunction (8, size - 8, true);   // always dark
}

void MixQr::drawVersion (int version)
{
    if (version < 7)
        return;
    long rem = version;
    for (int i = 0; i < 12; ++i)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    const auto bits = (long) version << 12 | rem;
    for (int i = 0; i < 18; ++i)
    {
        const auto a = size - 11 + i % 3, b = i / 3;
        setFunction (a, b, bit (bits, i));
        setFunction (b, a, bit (bits, i));
    }
}

void MixQr::drawCodewords (const std::vector<std::uint8_t>& data)
{
    size_t i = 0;
    for (int right = size - 1; right >= 1; right -= 2)
    {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < size; ++vert)
            for (int j = 0; j < 2; ++j)
            {
                const auto x = right - j;
                const auto upward = ((right + 1) & 2) == 0;
                const auto y = upward ? size - 1 - vert : vert;
                if (! isFunction[(size_t) (y * size + x)] && i < data.size() * 8)
                {
                    set (x, y, bit (data[i >> 3], 7 - (int) (i & 7)));
                    ++i;
                }
            }
    }
}

void MixQr::applyMask (int mask)
{
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (! isFunction[(size_t) (y * size + x)] && maskBit (mask, x, y))
                set (x, y, ! isDark (x, y));
}

long MixQr::penalty() const
{
    long result = 0;

    // Runs of five or more of one colour, in rows and columns.
    for (int pass = 0; pass < 2; ++pass)
        for (int a = 0; a < size; ++a)
        {
            int run = 0;
            bool colour = false;
            for (int b = 0; b < size; ++b)
            {
                const auto dark = pass == 0 ? isDark (b, a) : isDark (a, b);
                if (b > 0 && dark == colour)
                {
                    if (++run == 5)
                        result += 3;
                    else if (run > 5)
                        ++result;
                }
                else
                {
                    colour = dark;
                    run = 1;
                }
            }
        }

    // 2 x 2 blocks of one colour.
    for (int y = 0; y < size - 1; ++y)
        for (int x = 0; x < size - 1; ++x)
        {
            const auto c = isDark (x, y);
            if (c == isDark (x + 1, y) && c == isDark (x, y + 1) && c == isDark (x + 1, y + 1))
                result += 3;
        }

    // Finder-like patterns (1:1:3:1:1 with four light modules on a side).
    const auto finderLike = [this] (int x, int y, int dx, int dy)
    {
        static constexpr bool pattern[] { true, false, true, true, true, false, true, false, false, false, false };
        for (int flip = 0; flip < 2; ++flip)
        {
            bool match = true;
            for (int i = 0; i < 11 && match; ++i)
                match = isDark (x + i * dx, y + i * dy) == pattern[flip == 0 ? i : 10 - i];
            if (match)
                return true;
        }
        return false;
    };
    for (int y = 0; y < size; ++y)
        for (int x = 0; x + 10 < size; ++x)
        {
            if (finderLike (x, y, 1, 0)) result += 40;
            if (finderLike (y, x, 0, 1)) result += 40;
        }

    // Balance of dark and light.
    int dark = 0;
    for (const auto m : modules)
        dark += m ? 1 : 0;
    const auto total = size * size;
    const auto k = (std::abs (dark * 20 - total * 10) + total - 1) / total - 1;   // 5 % steps away from half
    result += std::max (0, k) * 10;
    return result;
}

void MixQr::draw (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour dark, juce::Colour light) const
{
    if (! isValid())
        return;
    const auto side = std::min (area.getWidth(), area.getHeight());
    const auto square = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
    g.setColour (light);
    g.fillRect (square);

    const auto module = side / (float) (size + 8);
    g.setColour (dark);
    juce::RectangleList<float> cells;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (isDark (x, y))
                cells.addWithoutMerging ({ square.getX() + (float) (x + 4) * module, square.getY() + (float) (y + 4) * module,
                                           module + 0.02f, module + 0.02f });
    g.fillRectList (cells);
}
