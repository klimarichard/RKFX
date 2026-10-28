#include "TunerPanel.h"

#include "gui/LookAndFeel.h"

namespace rkboard
{

using namespace rkfx;

TunerPanel::TunerPanel (BoardProcessor& b) : board (b)
{
    detector.prepare (board.getSampleRate() > 0.0 ? board.getSampleRate() : 48000.0, 4096);
    window.resize ((size_t) detector.getWindowSize());

    referenceLabel.setText ("Reference A4", juce::dontSendNotification);
    referenceLabel.setColour (juce::Label::textColourId, Palette::textDim);
    referenceLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (referenceLabel);

    referenceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    referenceSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    referenceSlider.setRange (430.0, 450.0, 1.0);
    referenceSlider.setTextValueSuffix (" Hz");
    referenceSlider.setDoubleClickReturnValue (true, 440.0);
    referenceSlider.setValue (board.getTunerReference(), juce::dontSendNotification);
    referenceSlider.onValueChange = [this] { board.setTunerOptions ((float) referenceSlider.getValue(), muteButton.getToggleState()); };
    addAndMakeVisible (referenceSlider);

    muteButton.setToggleState (board.getTunerMutes(), juce::dontSendNotification);
    muteButton.onClick = [this] { board.setTunerOptions ((float) referenceSlider.getValue(), muteButton.getToggleState()); };
    addAndMakeVisible (muteButton);

    closeButton.onClick = [this] { board.setTunerActive (false); };   // the editor closes this view
    addAndMakeVisible (closeButton);

    startTimerHz (20);
}

void TunerPanel::timerCallback()
{
    board.copyTunerInput (window.data(), (int) window.size());
    const auto detected = detector.detect (window.data());

    // Hold the last note briefly through short gaps.
    if (detected > 0.0f)
    {
        const auto note = PitchDetector::noteFor (detected, board.getTunerReference());
        shownCents = hz > 0.0f && std::abs (note.cents - shownCents) < 30.0f ? shownCents + 0.5f * (note.cents - shownCents) : note.cents;
        hz = detected;
        quietTicks = 0;
    }
    else if (++quietTicks > 10)
    {
        hz = 0.0f;
    }

    repaint();
}

void TunerPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), Palette::panel, "Tuner");

    auto area = getLocalBounds().reduced (24).withTrimmedTop (16).withTrimmedBottom (60);
    const auto inTune = hz > 0.0f && std::abs (shownCents) < 3.0f;
    const auto accent = inTune ? juce::Colour (0xff3dae5b) : Palette::boardAccent;

    // Note and octave
    auto noteArea = area.removeFromTop (150);
    if (hz > 0.0f)
    {
        const auto note = PitchDetector::noteFor (hz, board.getTunerReference());
        g.setColour (accent);
        g.setFont (Fonts::bold (110.0f));
        g.drawText (note.name, noteArea, juce::Justification::centred);
        g.setFont (Fonts::bold (28.0f));
        g.setColour (Palette::textDim);
        g.drawText (juce::String (note.octave), noteArea.withTrimmedLeft (noteArea.getWidth() / 2 + 70).withTrimmedTop (70),
                    juce::Justification::topLeft);
    }
    else
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (20.0f));
        g.drawText ("Play a single note", noteArea, juce::Justification::centred);
    }

    // Cents meter: -50 .. +50, centre mark, the needle
    area.removeFromTop (10);
    auto meter = area.removeFromTop (40).withSizeKeepingCentre (juce::jmin (area.getWidth(), 600), 40);
    g.setColour (Palette::knobBody);
    g.fillRoundedRectangle (meter.toFloat(), Style::kCornerRadius);
    for (int c = -50; c <= 50; c += 10)
    {
        const auto x = meter.getX() + (float) meter.getWidth() * (c + 50) / 100.0f;
        g.setColour (c == 0 ? Palette::text : Palette::textDim.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (x - (c == 0 ? 1.0f : 0.5f), (float) meter.getY() + (c == 0 ? 2.0f : 12.0f),
                                            c == 0 ? 2.0f : 1.0f, (float) meter.getHeight() - (c == 0 ? 4.0f : 24.0f)));
    }
    if (hz > 0.0f)
    {
        const auto x = meter.getX() + (float) meter.getWidth() * (juce::jlimit (-50.0f, 50.0f, shownCents) + 50.0f) / 100.0f;
        g.setColour (accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.0f, (float) meter.getY() - 4.0f, 6.0f, (float) meter.getHeight() + 8.0f), 3.0f);
    }

    // Numbers
    area.removeFromTop (8);
    g.setColour (Palette::textDim);
    g.setFont (Fonts::regular (15.0f));
    g.drawText (hz > 0.0f ? juce::String (shownCents >= 0.0f ? "+" : "") + juce::String (shownCents, 1) + " cents    "
                                + juce::String (hz, 1) + " Hz"
                          : juce::String(),
                area.removeFromTop (24), juce::Justification::centred);
}

void TunerPanel::resized()
{
    auto bottom = getLocalBounds().reduced (24, 16).removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (80));
    bottom.removeFromRight (24);
    muteButton.setBounds (bottom.removeFromRight (220));
    bottom.removeFromRight (24);
    referenceSlider.setBounds (bottom.removeFromRight (240));
    referenceLabel.setBounds (bottom.removeFromRight (110));
}

} // namespace rkboard
