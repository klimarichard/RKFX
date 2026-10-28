#include "Switches.h"

#include "LookAndFeel.h"

namespace rkfx
{

ToggleSwitch::ToggleSwitch (juce::RangedAudioParameter& p, juce::Colour accentColour, const juce::String& labelText)
    : accent (accentColour),
      text (labelText.isNotEmpty() ? labelText : p.getName (32)),
      attachment (p, [this] (float v)
                  {
                      on = v >= 0.5f;
                      repaint();
                  })
{
    tagParameter (*this, p.getParameterID());
    attachment.sendInitialUpdate();
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (on ? accent : Palette::knobBody);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? Palette::onAccent (accent) : Palette::text);
    g.setFont (Fonts::regular (13.0f));
    g.drawFittedText (text, getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void ToggleSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;   // a right-click belongs to the host (e.g. RKBoard's assign menu)
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

//==============================================================================
SegmentedSwitch::SegmentedSwitch (juce::RangedAudioParameter& p, juce::Colour accentColour,
                                  juce::StringArray segmentLabels, const juce::String& titleText)
    : accent (accentColour),
      labels (std::move (segmentLabels)),
      title (titleText),
      attachment (p, [this] (float v)
                  {
                      selected = juce::roundToInt (v);
                      repaint();
                  })
{
    if (labels.isEmpty())
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
            labels = choice->choices;

    tagParameter (*this, p.getParameterID());
    attachment.sendInitialUpdate();
}

juce::Rectangle<int> SegmentedSwitch::segmentArea() const
{
    auto bounds = getLocalBounds();
    if (title.isNotEmpty())
        bounds.removeFromTop (18);
    return bounds;
}

void SegmentedSwitch::paint (juce::Graphics& g)
{
    if (title.isNotEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (12.5f));
        g.drawFittedText (title, getLocalBounds().removeFromTop (18), juce::Justification::centred, 1);
    }

    const auto area = segmentArea().toFloat().reduced (0.5f);
    g.setColour (Palette::knobBody);
    g.fillRoundedRectangle (area, 4.0f);

    const auto count = juce::jmax (1, labels.size());
    const auto width = area.getWidth() / (float) count;

    g.setFont (Fonts::regular (13.0f));

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto segment = juce::Rectangle<float> (area.getX() + width * (float) i, area.getY(), width, area.getHeight());

        if (i == selected)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (segment.reduced (1.0f), 3.5f);
        }
        else if (i > 0 && i - 1 != selected)
        {
            g.setColour (Palette::outline);
            g.drawVerticalLine ((int) segment.getX(), segment.getY() + 5.0f, segment.getBottom() - 5.0f);
        }

        g.setColour (i == selected ? Palette::onAccent (accent) : Palette::text);
        g.drawFittedText (labels[i], segment.toNearestInt().reduced (3, 0), juce::Justification::centred, 1);
    }
}

void SegmentedSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;   // a right-click belongs to the host (e.g. RKBoard's assign menu)

    const auto area = segmentArea();
    if (labels.isEmpty() || ! area.contains (e.getPosition()))
        return;

    const auto index = juce::jlimit (0, labels.size() - 1,
                                     (e.x - area.getX()) * labels.size() / juce::jmax (1, area.getWidth()));

    if (index != selected)
        attachment.setValueAsCompleteGesture ((float) index);
}

} // namespace rkfx
