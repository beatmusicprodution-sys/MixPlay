#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Analyzer.h"
#include "PresetChain.h"

class MixPlayProcessor : public juce::AudioProcessor
{
public:
    MixPlayProcessor();

    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Mix Play"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void setPreset (const Preset& p);

    Analysis analysis;
    Preset preset;
    juce::String instrument { "Bateria" };
    std::atomic<bool> bypass { false };

private:
    PresetChain chain;
    juce::SpinLock lock;
    Preset pending;
    std::atomic<bool> hasPending { false }, hasPreset { false };
};
