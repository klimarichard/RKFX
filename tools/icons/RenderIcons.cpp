// Renders the RKMusic plugin icons: "RK" over a name strip in the plugin's
// accent colour, set in Barlow Condensed (fonts/, SIL Open Font License).
//
//   cmake --build build --target RKIconRender
//   build/tools/RKIconRender_artefacts/Release/RKIconRender tools/icons/fonts <out dir>
//
// Writes <out>/<Plugin>/icon_big.png (1024 px) and icon_small.png (256 px,
// the small-size design: a taller band and a heavier, wider name, so it
// still reads at 32 px). The tile follows Apple's grid (824 px on 1024).
// Copy each pair into the plugin's resources folder (ICON_BIG / ICON_SMALL
// in its juce_add_plugin): RKModular/resources, RKSampler/resources,
// RKTracks/resources, RKFX/plugins/RKBoard/resources, RKFX/plugins/RKMix/resources.
#include <juce_graphics/juce_graphics.h>
#include <juce_events/juce_events.h>

namespace
{
    struct Plugin { const char* folder; const char* name; juce::uint32 accent; };
    const Plugin plugins[] = {
        { "RKModular", "MODULAR", 0xffd0a35c },
        { "RKSampler", "SAMPLER", 0xff4fa3e0 },
        { "RKTracks",  "TRACKS",  0xff8a8a8a },
        { "RKBoard",   "BOARD",   0xffe6e2d8 },
        { "RKMix",     "MIX",     0xff6b8ca6 },
    };

    // Text whose ink is centred on (cx, cy). `size` = CSS font-size (em),
    // `tracking` = letter-spacing in em.
    void drawCentred (juce::Graphics& g, const juce::Typeface::Ptr& face, const juce::String& text,
                      float size, float tracking, float cx, float cy, juce::Colour colour)
    {
        const juce::Font font (juce::FontOptions (face).withPointHeight (size).withKerningFactor (tracking));
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.0f, 0.0f);
        // Ink of the glyphs (the trailing letter-spacing after the last one doesn't count).
        const auto ink = ga.getBoundingBox (0, -1, true);
        g.setColour (colour);
        ga.draw (g, juce::AffineTransform::translation (cx - ink.getCentreX(), cy - ink.getCentreY()));
    }

    // Design units: the tile is 200 x 200, as on the design canvas.
    juce::Image render (const Plugin& p, int canvas, bool small, const juce::Typeface::Ptr& heavy,
                        const juce::Typeface::Ptr& semi)
    {
        juce::Image img (juce::Image::ARGB, canvas, canvas, true);
        juce::Graphics g (img);

        // Apple's grid: an 824 px tile centred on a 1024 px canvas.
        const float tile = (float) canvas * 824.0f / 1024.0f;
        const float u = tile / 200.0f;
        g.addTransform (juce::AffineTransform::scale (u).translated (((float) canvas - tile) * 0.5f,
                                                                     ((float) canvas - tile) * 0.5f));
        juce::Path shape;
        shape.addRoundedRectangle (0.0f, 0.0f, 200.0f, 200.0f, 44.0f);
        g.reduceClipRegion (shape);

        const float band = small ? 92.0f : 60.0f;
        g.setColour (juce::Colour (0xff1c1d20));
        g.fillRect (0.0f, 0.0f, 200.0f, 200.0f);
        g.setColour (juce::Colour (p.accent));
        g.fillRect (0.0f, 200.0f - band, 200.0f, band);

        // "RK": centred in the top part (which starts a few units down, as in the design).
        const float pad = small ? 6.0f : 8.0f;
        const float topCentre = pad + (200.0f - band - pad) * 0.5f;
        drawCentred (g, heavy, "RK", small ? 96.0f : 112.0f, -0.02f, 100.0f, topCentre, juce::Colour (0xffe8e8ea));

        // The name, centred in the band.
        if (small)
            drawCentred (g, heavy, p.name, 50.0f, -0.01f, 100.0f, 200.0f - band * 0.5f, juce::Colour (0xff15161a));
        else
            drawCentred (g, semi, p.name, 29.0f, 0.2f, 100.0f, 200.0f - band * 0.5f, juce::Colour (0xff15161a));
        return img;
    }
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: RKIconRender <fonts dir> <out dir>\n");
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File fonts (juce::String::fromUTF8 (argv[1])), out (juce::String::fromUTF8 (argv[2]));

    auto load = [&] (const char* file) {
        juce::MemoryBlock data;
        fonts.getChildFile (file).loadFileAsData (data);
        return juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());
    };
    const auto heavy = load ("BarlowCondensed-ExtraBold.ttf");
    const auto semi = load ("BarlowCondensed-SemiBold.ttf");
    if (heavy == nullptr || semi == nullptr)
    {
        std::printf ("could not load the fonts\n");
        return 1;
    }

    juce::PNGImageFormat png;
    for (const auto& p : plugins)
    {
        const auto dir = out.getChildFile (p.folder);
        dir.createDirectory();
        for (bool small : { false, true })
        {
            const auto file = dir.getChildFile (small ? "icon_small.png" : "icon_big.png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            if (! stream.openedOk() || ! png.writeImageToStream (render (p, small ? 256 : 1024, small, heavy, semi), stream))
            {
                std::printf ("could not write %s\n", file.getFullPathName().toRawUTF8());
                return 1;
            }
        }
        std::printf ("%s: icon_big.png, icon_small.png\n", p.folder);
    }
    return 0;
}
