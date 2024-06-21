#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "CoolAudioProcessorValueTreeType", getParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    updateParameters();
    updateDelayBufferSizes(sampleRate);
    juce::ignoreUnused (samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::getParameterLayout() 
{
    return 
    {
        std::make_unique<juce::AudioParameterFloat>("delayTime", "Delay Time", 0.01f, 4.0f, 0.2f),
        std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 0.99f, 0.3f),
        std::make_unique<juce::AudioParameterFloat>("dryMix", "Dry Mix", 0.0f, 1.0f, 0.8f),
        std::make_unique<juce::AudioParameterFloat>("wetMix", "Wet Mix", 0.0f, 1.0f, 0.6f),
        std::make_unique<juce::AudioParameterBool>("DEBUG", "DEBUG", false),
    };
}

void PluginProcessor::updateParameters() {
    delayTime = apvts.getParameterAsValue("delayTime").getValue();;
    feedback = apvts.getParameterAsValue("feedback").getValue();
    dryMix = apvts.getParameterAsValue("dryMix").getValue();
    wetMix = apvts.getParameterAsValue("wetMix").getValue();
    debugFlag = apvts.getParameterAsValue("DEBUG").getValue();
}

void PluginProcessor::updateDelayBufferSizes(int sampleRate) {
    auto minDelayBufferSize = (int)ceil(2 * sampleRate * apvts.getParameterRange("delayTime").getRange().getEnd());
    auto channelSet = getBusesLayout().getMainOutputChannelSet();
    uint numChannels = 1;
    // if stereo, make sure to make 2 delay buffers
    // also if more channels are supported in the future, the code below should stay valid
    if (channelSet == juce::AudioChannelSet::stereo()) {
        numChannels = 2;
    }
    while (delayBuffers.size() < numChannels) {
        delayBuffers.push_back(chowdsp::DoubleBuffer<float>(512));
    }
    for (int i = 0; i < numChannels; i++) {
        if (delayBuffers[i].size() < minDelayBufferSize)
            delayBuffers[i].resize(minDelayBufferSize);
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    updateParameters();
    
    auto srate = getSampleRate();

    auto wetBuffer = juce::AudioBuffer<float>(
        totalNumInputChannels, 
        numSamples
    );

    auto bufferCopy = juce::AudioBuffer<float>();
    bufferCopy.makeCopyOf(buffer);
    auto delayNumSamples = (int)ceil(delayTime * srate);

    for (int channel = 0; channel < totalNumInputChannels; channel++)
    {
        // TODO: cover the case that delayNumSamples < numSamples at low delayTime (maybe process sample-by-sample?)
        auto delayDataPointer = delayBuffers[channel].getWritePointer() + delayBuffers[channel].size() - delayNumSamples;
        wetBuffer.copyFrom(channel, 0, delayBuffers[channel].data(delayDataPointer), numSamples);
        bufferCopy.addFrom(channel, 0, wetBuffer, channel, 0, numSamples, feedback);
        delayBuffers[channel].push(bufferCopy.getReadPointer(channel), numSamples);
    }
    buffer.applyGain(dryMix);
    for (int channel = 0; channel < totalNumInputChannels; channel++)
    {
        buffer.addFrom(channel, 0, wetBuffer, channel, 0, numSamples, wetMix);
    }

    if (debugFlag) {
        // TODO: remove this, this is bad :)
        // (use DBG to print something)
        apvts.getParameter("DEBUG")->setValue(false);
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml.get() != nullptr)
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
