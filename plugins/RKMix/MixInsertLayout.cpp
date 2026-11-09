#include "MixInsertLayout.h"

#include "plugin/EditorBase.h"
#include "plugin/ProcessorBase.h"

namespace
{
    // Shown: visible, and every parent up to the editor too.
    bool isShown (const juce::Component* c, const juce::Component* editor)
    {
        for (; c != nullptr && c != editor; c = c->getParentComponent())
            if (! c->isVisible())
                return false;
        return true;
    }

    // A control the iPad can't use: hidden in the picture, so no dead buttons show.
    bool isInteractive (const juce::Component* c)
    {
        return dynamic_cast<const juce::Button*> (c) != nullptr || dynamic_cast<const juce::ComboBox*> (c) != nullptr
            || dynamic_cast<const juce::Slider*> (c) != nullptr || dynamic_cast<const juce::TextEditor*> (c) != nullptr
            || dynamic_cast<const juce::ResizableCornerComponent*> (c) != nullptr;
    }
}

MixInsertLayout::MixInsertLayout (rkfx::ProcessorBase& p, int slotNumber) : plugin (p), slot (slotNumber)
{
    // A hidden editor of its own (the plugin's active editor may be open in RKMix's window).
    editor.reset (plugin.createEditor());
    if (dynamic_cast<rkfx::EditorBase*> (editor.get()) == nullptr)
    {
        editor.reset();
        return;
    }
    // Its base size, where the canvas isn't scaled: the controls' bounds are base-size coordinates.
    area = editor->getLocalBounds().withTrimmedTop (rkfx::EditorBase::kHeaderHeight);
    scan();
    signature = signatureOf();
    drawImage();
}

MixInsertLayout::~MixInsertLayout()
{
    editor.reset();
}

//==============================================================================
void MixInsertLayout::scan()
{
    controls.clear();
    tagged.clear();
    std::function<void (juce::Component&)> visit = [&] (juce::Component& parent)
    {
        for (auto* c : parent.getChildren())
        {
            // A tap area that sets a parameter to one value (RKLoop's loop tiles pick the loop).
            if (const auto* pick = c->getProperties().getVarPointer ("rkfx.pick"))
            {
                tagged.addIfNotAlreadyThere (pick->toString());
                if (auto* param = plugin.findParam (pick->toString()); param != nullptr && isShown (c, editor.get()))
                {
                    Control control;
                    control.component = c;
                    control.id = pick->toString();
                    control.widget = "pick";
                    control.bounds = editor->getLocalArea (c, c->getLocalBounds()).translated (0, -area.getY());
                    control.pickValue = param->convertTo0to1 ((float) c->getProperties()["rkfx.pickValue"]);
                    controls.push_back (control);
                }
                continue;
            }
            const auto* id = c->getProperties().getVarPointer ("rkfx.parameter");
            if (id == nullptr)
            {
                visit (*c);
                continue;
            }
            tagged.addIfNotAlreadyThere (id->toString());
            // A control that stands for others too (RKLoop's role switch: every loop's role).
            tagged.addTokens (c->getProperties()["rkfx.reaches"].toString(), " ", {});
            tagged.removeDuplicates (false);
            if (id->toString() == "bypass" || ! isShown (c, editor.get()))
                continue;   // the iPad's On is the bypass

            Control control;
            control.component = c;
            control.id = id->toString();
            control.bounds = editor->getLocalArea (c, c->getLocalBounds()).translated (0, -area.getY());
            const auto& props = c->getProperties();
            control.widget = props["rkfx.widget"].toString();
            control.label = props["rkfx.label"].toString();
            control.dimmed = (bool) props.getWithDefault ("rkfx.dimmed", false);
            control.compact = (bool) props.getWithDefault ("rkfx.compact", false);
            if (control.widget == "segments")
            {
                control.title = control.label;
                control.label.clear();
                control.labels = juce::StringArray::fromLines (props["rkfx.labels"].toString());
            }
            if (auto* param = plugin.findParam (control.id))
            {
                // A plugin's own control (tagged without saying what it is): by its parameter.
                if (control.widget.isEmpty())
                    control.widget = dynamic_cast<juce::AudioParameterBool*> (param) != nullptr ? "toggle"
                                   : dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr ? "choice" : "knob";
                if (control.label.isEmpty() && control.widget != "segments")
                    control.label = param->getName (32);
                const auto& range = param->getNormalisableRange();
                if (range.start < 0.0f && range.end > 0.0f)
                    control.zero = range.convertTo0to1 (0.0f);
            }
            controls.push_back (control);
        }
    };
    visit (*editor);
}

juce::String MixInsertLayout::signatureOf() const
{
    juce::String s;
    for (const auto& c : controls)
        s << c.id << ':' << c.bounds.toString() << (c.dimmed ? "d" : "") << ';';
    return s;
}

bool MixInsertLayout::drawImage()
{
    // The window without the controls the iPad draws itself (and without the ones it can't use).
    std::vector<juce::Component*> hidden;
    for (const auto& c : controls)
        if (c.widget != "pick" && c.component->isVisible())
            hidden.push_back (c.component);
    std::function<void (juce::Component&)> visit = [&] (juce::Component& parent)
    {
        for (auto* c : parent.getChildren())
        {
            if (c->getProperties().contains ("rkfx.parameter"))
            {
                if (c->isVisible() && std::find (hidden.begin(), hidden.end(), c) == hidden.end())
                    hidden.push_back (c);   // the bypass
                continue;
            }
            if (isInteractive (c) && c->isVisible())
                hidden.push_back (c);
            else
                visit (*c);
        }
    };
    visit (*editor);

    for (auto* c : hidden)
        c->setVisible (false);
    const auto picture = editor->createComponentSnapshot (area, true, kScale);
    for (auto* c : hidden)
        c->setVisible (true);

    // Unchanged (most of the time): nothing to send.
    juce::uint64 hash = 14695981039346656037ull;
    {
        const juce::Image::BitmapData pixels (picture, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < pixels.height; ++y)
        {
            const auto* row = pixels.getLinePointer (y);
            for (int x = 0; x < pixels.width * pixels.pixelStride; ++x)
                hash = (hash ^ row[x]) * 1099511628211ull;
        }
    }
    if (hash == imageHash && image.isNotEmpty())
        return false;
    imageHash = hash;

    juce::MemoryOutputStream png;
    juce::PNGImageFormat().writeImageToStream (picture, png);
    image = "data:image/png;base64," + juce::Base64::toBase64 (png.getData(), png.getDataSize());
    return true;
}

//==============================================================================
juce::String MixInsertLayout::layoutJson()
{
    if (editor == nullptr)
        return {};
    const auto prefix = "ins:" + juce::String (slot) + ":";

    juce::Array<juce::var> list;
    for (const auto& c : controls)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", prefix + c.id);
        o->setProperty ("k", c.widget);
        o->setProperty ("x", c.bounds.getX());
        o->setProperty ("y", c.bounds.getY());
        o->setProperty ("w", c.bounds.getWidth());
        o->setProperty ("h", c.bounds.getHeight());
        if (c.label.isNotEmpty())
            o->setProperty ("label", c.label);
        if (c.title.isNotEmpty())
            o->setProperty ("title", c.title);
        if (! c.labels.isEmpty())
        {
            juce::Array<juce::var> labels;
            for (const auto& l : c.labels)
                labels.add (l);
            o->setProperty ("labels", labels);
        }
        if (c.dimmed)
            o->setProperty ("dim", true);
        if (c.compact)
            o->setProperty ("compact", true);
        if (c.zero > 0.0f)
            o->setProperty ("zero", c.zero);
        if (c.widget == "pick")
            o->setProperty ("value", c.pickValue);
        list.add (juce::var (o));
    }

    // Controls the window doesn't show at all (none in the suite today): the iPad lists them.
    juce::Array<juce::var> more;
    for (auto* param : plugin.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param); p != nullptr && ! tagged.contains (p->getParameterID()))
            more.add (prefix + p->getParameterID());

    auto* layout = new juce::DynamicObject();
    layout->setProperty ("w", area.getWidth());
    layout->setProperty ("h", area.getHeight());
    layout->setProperty ("img", image);
    layout->setProperty ("accent", (juce::int64) plugin.getConfig().accent.getARGB());
    layout->setProperty ("ctl", list);
    layout->setProperty ("more", more);
    return juce::JSON::toString (juce::var (layout), true);
}

juce::String MixInsertLayout::update()
{
    if (editor == nullptr)
        return {};
    scan();
    if (const auto s = signatureOf(); s != signature)
    {
        signature = s;
        drawImage();
        return "layout";
    }
    return drawImage() ? "image" : juce::String();
}
