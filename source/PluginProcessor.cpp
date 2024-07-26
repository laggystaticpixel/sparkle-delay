#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SharedResources.h"

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
    updateParameters(sampleRate);
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
//
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::getParameterLayout() 
{
    return 
    {
        std::make_unique<juce::AudioParameterFloat>("grainRate", "grainRate", 0.1f, 200.0f, 0.5f),
        std::make_unique<juce::AudioParameterFloat>("grainAttack", "grainAttack", 1.0f, 50.0f, 3.0f),
        std::make_unique<juce::AudioParameterFloat>("grainDecay", "grainDecay", 1.0f, 50.0f, 10.0f),
        std::make_unique<juce::AudioParameterFloat>("grainSustain", "grainSustain", 0.0f, 1.0f, 1.0f),
        std::make_unique<juce::AudioParameterFloat>("grainRelease", "grainRelease", 1.0f, 50.0f, 20.0f),
        std::make_unique<juce::AudioParameterFloat>("delayTime", "Delay Time", 0.01f, 4.0f, 1.0f),
        std::make_unique<juce::AudioParameterFloat>("delayTimeVar", "delayTimeVar", 0.0f, 0.5f, 0.1f),
        std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 0.99f, 0.2f),
        std::make_unique<juce::AudioParameterFloat>("dryMix", "Dry Mix", 0.0f, 1.0f, 0.8f),
        std::make_unique<juce::AudioParameterFloat>("wetMix", "Wet Mix", 0.0f, 1.0f, 0.6f),
        std::make_unique<juce::AudioParameterBool>("DEBUG", "DEBUG", false),
    };
}

template <typename T>
void PluginProcessor::updateParameter(T& paramRef, const char* parameterName) {
    paramRef = apvts.getRawParameterValue(parameterName)->load();
}


void PluginProcessor::updateParameters(int sampleRate) {
    updateParameter(grainAttack, "grainAttack");
    grainAttack /= 1000.0f;
    updateParameter(grainDecay, "grainDecay");
    grainDecay /= 1000.0f;
    updateParameter(grainSustain, "grainSustain");
    updateParameter(grainRelease, "grainRelease");
    grainRelease /= 1000.0f;
    updateParameter(grainRate, "grainRate");
    // grainRate = static_cast<juce::AudioParameterFloat*>(apvts.getParameter("grainRate"))->get();
    grainPeriod = (int)ceil(sampleRate / grainRate);

    updateParameter(delayTimeVar, "delayTimeVar");
    delayTime = apvts.getParameterAsValue("delayTime").getValue();;
    feedback = apvts.getParameterAsValue("feedback").getValue();
    dryMix = apvts.getParameterAsValue("dryMix").getValue();
    wetMix = apvts.getParameterAsValue("wetMix").getValue();
    // wow! what an intuitive way to get my boolean parameter!
    auto debugFlagParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("DEBUG"));
    if (debugFlagParam != nullptr) {
        debugFlag = debugFlagParam->get();
    }
    
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

    // save dryMix and wetMix before updating them for smoothing later
    auto oldDryMix = dryMix;
    auto oldWetMix = wetMix;

    auto sampleRate = getSampleRate();
    updateParameters(sampleRate);

    auto wetBuffer = juce::AudioBuffer<float>(
        totalNumInputChannels, 
        numSamples
    );
    wetBuffer.clear();

    auto bufferCopy = juce::AudioBuffer<float>();
    bufferCopy.makeCopyOf(buffer);
    auto delayNumSamples = (int)ceil(delayTime * sampleRate);

    while (currentGrainOffset <= numSamples) {
        // Add fresh grains to the queue
        auto adsr = juce::ADSR();
        adsr.setParameters({grainAttack, grainDecay, grainSustain, grainRelease});
        adsr.setSampleRate(sampleRate);
        auto normalizedAddedOffset = lsp::SharedResources::random.nextFloat() - 0.5f;
        auto addedOffsetSamples = (int)(delayTimeVar * sampleRate * normalizedAddedOffset);
        auto rev = lsp::SharedResources::random.nextBool();
        auto shiftPitch = lsp::SharedResources::random.nextBool();
        futureGrainQueue.push_back(lsp::Grain(currentGrainOffset, delayNumSamples, adsr, sampleRate, rev, shiftPitch));
        currentGrainOffset += grainPeriod + (addedOffsetSamples > 0 ? addedOffsetSamples : 0);
    }

    for (auto& g: futureGrainQueue) {
        if (g.lengthInSamples + g.sampleOffset < 0 && !g.readyToPlay) {
            // put data from dBuffer into the not yet playing grains that have their content just recorded
            g.internalBuffer.setSize(totalNumInputChannels, g.lengthInSamples);
            
            for (int channel = 0; channel < totalNumInputChannels; channel++){
                auto delayDataPointer = delayBuffers[channel].getWritePointer() + delayBuffers[channel].size() - delayNumSamples;    
                g.internalBuffer.copyFrom(
                    channel, 0, 
                    delayBuffers[channel].data(delayDataPointer + g.sampleOffset), 
                    g.lengthInSamples
                );
            }
            g.readyToPlay = true;
            g.applyADSR();
        } 
    }

    for (auto& g: futureGrainQueue) {
        // skip grain if the contents arent saved yet
        if (!g.readyToPlay)
            continue;

        if (g.progress == 0 && g.delayNumSamples + g.sampleOffset < 0 && !g.isPlaying) {
            g.progress = g.lengthInSamples;
            continue;
        }

        if (g.progress == 0 && 
            g.delayNumSamples + g.sampleOffset < numSamples && 
            !g.isPlaying
        ) {
            // start the grain
            g.process(wetBuffer, 1.0f, g.delayNumSamples + g.sampleOffset);
            g.isPlaying = true;
        } else if (g.progress != 0 && g.delayNumSamples + g.lengthInSamples + g.sampleOffset > 0) {
            // play the grain
            g.process(wetBuffer, 1.0f);
        }
        
    }

    for (int channel = 0; channel < totalNumInputChannels; channel++)
    {
        // TODO: cover the case that delayNumSamples < numSamples at low delayTime (maybe process sample-by-sample?)
        auto delayDataPointer = delayBuffers[channel].getWritePointer() + delayBuffers[channel].size() - delayNumSamples;
        bufferCopy.addFrom(channel, 0, wetBuffer, channel, 0, numSamples, feedback);
        delayBuffers[channel].push(bufferCopy.getReadPointer(channel), numSamples);
    }
    buffer.applyGainRamp(0, numSamples, oldDryMix, dryMix);
    wetBuffer.applyGainRamp(0, numSamples, oldWetMix, wetMix);
    for (int channel = 0; channel < totalNumInputChannels; channel++)
    {
        buffer.addFrom(channel, 0, wetBuffer, channel, 0, numSamples);
    }

    // Remove the grains that have finished playing
    auto isGrainActive = [](lsp::Grain g){ return g.progress == g.lengthInSamples;};
    futureGrainQueue.erase(
        std::remove_if(begin(futureGrainQueue), end(futureGrainQueue), isGrainActive),
        end(futureGrainQueue)
    );
    
    // subtract numSamples from sampleOffset of all grains (make grains "older")
    for (auto& g: futureGrainQueue) {
        g.sampleOffset -= numSamples;
    }

    currentGrainOffset -= numSamples;

    if (debugFlag) {
        // TODO: remove this, this is bad :)
        // (use DBG to print something)
        apvts.getParameter("DEBUG")->setValue(false);
        debugFlag = false;
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
