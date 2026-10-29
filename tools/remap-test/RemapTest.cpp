// RKRemap step 0: 64 inputs, 64 outputs, each input passed to the same-numbered
// output, and a light per input. Two layouts: one 64-channel bus each way
// (REMAP_TEST_ONE_BUS=1), or 32 stereo buses each way.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>

namespace
{
    constexpr int kChannels = 64;

}

class RemapTestProcessor : public juce::AudioProcessor
{
public:
    RemapTestProcessor() : juce::AudioProcessor (makeBuses()) {}

    static BusesProperties makeBuses()
    {
        BusesProperties buses;
       #if REMAP_TEST_ONE_BUS
        buses = buses.withInput ("Input", juce::AudioChannelSet::discreteChannels (kChannels), true)
                     .withOutput ("Output", juce::AudioChannelSet::discreteChannels (kChannels), true);
       #else
        for (int pair = 0; pair < kChannels / 2; ++pair)
        {
            const auto name = juce::String (pair * 2 + 1) + "/" + juce::String (pair * 2 + 2);
            buses = buses.withInput ("In " + name, juce::AudioChannelSet::stereo(), true)
                         .withOutput ("Out " + name, juce::AudioChannelSet::stereo(), true);
        }
       #endif
        return buses;
    }


    const juce::String getName() const override { return REMAP_TEST_NAME; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Only the full layout: every bus enabled, as declared.
        if (layouts.inputBuses.size() != layouts.outputBuses.size())
            return false;
        for (int i = 0; i < layouts.inputBuses.size(); ++i)
        {
           #if REMAP_TEST_ONE_BUS
            const auto wanted = juce::AudioChannelSet::discreteChannels (kChannels);
           #else
            const auto wanted = juce::AudioChannelSet::stereo();
           #endif
            if (layouts.inputBuses[i] != wanted || layouts.outputBuses[i] != wanted)
                return false;
        }
        return true;
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        midi.clear();
        // Identity: input n is already in channel n, which is output n. Only measure.
        for (int ch = 0; ch < juce::jmin (kChannels, buffer.getNumChannels()); ++ch)
        {
            const auto peak = buffer.getMagnitude (ch, 0, buffer.getNumSamples());
            auto& p = peaks[(size_t) ch];
            if (peak > p.load())
                p = peak;
        }
        channelsSeen = buffer.getNumChannels();
    }

    float takePeak (int ch) { return peaks[(size_t) ch].exchange (0.0f); }
    int getChannelsSeen() const { return channelsSeen.load(); }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::array<std::atomic<float>, kChannels> peaks {};
    std::atomic<int> channelsSeen { 0 };
};

class RemapTestEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RemapTestEditor (RemapTestProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
    {
        setSize (660, 400);
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1c1d20));
        g.setColour (juce::Colours::white);
        g.setFont (16.0f);
        g.drawText (juce::String (REMAP_TEST_NAME) + ": each input to the same output. Channels in this block: "
                        + juce::String (proc.getChannelsSeen()),
                    12, 8, getWidth() - 24, 24, juce::Justification::centredLeft);

        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto x = 12 + (ch % 16) * 40, y = 44 + (ch / 16) * 86;
            const auto level = held[(size_t) ch];
            g.setColour (level > 0.01f ? juce::Colour (0xffe0a040) : level > 0.001f ? juce::Colour (0xff6a5a30) : juce::Colour (0xff34373c));
            g.fillEllipse ((float) x + 6.0f, (float) y, 22.0f, 22.0f);
            g.setColour (juce::Colour (0xff8d9198));
            g.setFont (12.0f);
            g.drawText ("In " + juce::String (ch + 1), x, y + 26, 36, 16, juce::Justification::centred);
        }
    }

private:
    RemapTestProcessor& proc;
    std::array<float, kChannels> held {};

    void timerCallback() override
    {
        for (int ch = 0; ch < kChannels; ++ch)
            held[(size_t) ch] = juce::jmax (proc.takePeak (ch), held[(size_t) ch] * 0.8f);
        repaint();
    }
};

juce::AudioProcessorEditor* RemapTestProcessor::createEditor() { return new RemapTestEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RemapTestProcessor(); }
