#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <chowdsp_data_structures/chowdsp_data_structures.h>
#include "Grain.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)

    juce::AudioProcessorValueTreeState::ParameterLayout getParameterLayout();
    void updateParameters(int sampleRate);

    juce::AudioProcessorValueTreeState apvts;
    void updateDelayBufferSizes(int);

    template <typename T>
    void updateParameter(T& paramRef, const char* parameterName);

    float dryMix;
    float wetMix;
    float feedback;
    float delayTime;
    float delayTimeVar;
    int grainPeriod;
    float grainAttack;
    float grainDecay;
    float grainSustain;
    float grainRelease;
    float grainRate;
    
    int currentGrainOffset = 0;
    bool debugFlag;
    std::vector<chowdsp::DoubleBuffer<float>> delayBuffers;    
    std::vector<lsp::Grain> futureGrainQueue = {};
};
