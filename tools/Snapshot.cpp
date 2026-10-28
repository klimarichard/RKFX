// Renders a plugin's editor offscreen and saves it as a PNG (no screen
// recording permission needed). Built per plugin by tools/CMakeLists.txt.
//
//   RKFXSnapshot_<Plugin> <output.png> [scale] [state.json] [x,y]
//
// state.json, if given, is loaded as the host state first (any preset or
// RKBoard board JSON with the plugin's full state). x,y clicks the editor
// there first (editor coordinates), e.g. to open something.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");
    const auto scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->setPlayConfigDetails (2, 2, 48000.0, 512);
    processor->prepareToPlay (48000.0, 512);

    if (argc > 3)
    {
        const auto json = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]).loadFileAsString();
        processor->setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorIfNeeded());

    if (argc > 4)
    {
        const auto xy = juce::StringArray::fromTokens (argv[4], ",", {});
        const juce::Point<int> pos (xy[0].getIntValue(), xy[1].getIntValue());
        editor->setVisible (true);   // offscreen, but hit-testing skips invisible components
        auto* target = editor->getComponentAt (pos);
        if (target == nullptr)
            std::printf ("nothing to click at %d,%d\n", pos.x, pos.y);
        else
        {
            const auto local = target->getLocalPoint (editor.get(), pos).toFloat();
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), local,
                                      juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      target, target, now, local, now, 1, false);
            target->mouseDown (e);
            target->mouseUp (e.withNewPosition (local));
        }
    }
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const auto ok = stream.openedOk() && png.writeImageToStream (image, stream);

    processor->editorBeingDeleted (editor.get());
    editor.reset();
    processor->releaseResources();
    std::printf ("%s %s\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8());
    return ok ? 0 : 1;
}
