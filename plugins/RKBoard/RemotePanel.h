#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BoardProcessor.h"

namespace rkboard
{

/** The remote's page in the window: on or off, the address and its QR code
    (with the PIN in it: scanning it signs a phone in), the PIN, the port,
    this board's name in the remote's list, and the devices connected (each
    can be let go). The settings are the computer's (every RKBoard shares
    them); the name is this board's. */
class RemotePanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit RemotePanel (BoardProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BoardProcessor& board;
    BoardRemoteHub* hub;

    juce::ToggleButton onButton { "Remote on" }, ipButton { "Address by IP (if the name doesn't work)" };
    juce::Label nameLabel, nameField, portLabel, portField, pinLabel, pinField;
    juce::TextButton newPinButton { "New PIN" };
    struct DeviceRow
    {
        int id = 0;
        juce::String text;
        std::unique_ptr<juce::TextButton> disconnect;
    };
    std::vector<DeviceRow> devices;
    int lastVersion = -1;
    juce::Rectangle<int> qrArea, statusArea, devicesArea;

    void refresh();
    void timerCallback() override;
};

} // namespace rkboard
