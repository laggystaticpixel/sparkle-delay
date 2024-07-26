#include "Grain.h"

namespace lsp {
    Grain::Grain(
        int sampleOffset, 
        int delayNumSamples, 
        juce::ADSR adsr,
        int sampleRate,
        bool reversed,
        bool enablePitchShift
    ): 
        sampleOffset(sampleOffset),
        delayNumSamples(delayNumSamples),
        adsr(adsr),
        reversed(reversed),
        sampleRate(sampleRate),
        enablePitchShift(enablePitchShift)
    {
        auto params = adsr.getParameters();
        lengthInSamples = (int)ceil((params.attack + params.decay + params.decay) * sampleRate);
    }

    Grain::~Grain() 
    {
    }

    void Grain::applyADSR() {
        if (reversed) {
            internalBuffer.reverse(0, lengthInSamples);
        }
        adsr.noteOn();
        auto attackPlusDecay = adsr.getParameters().attack + adsr.getParameters().decay;
        auto apdNumSamples = (int)ceil(attackPlusDecay * sampleRate);
        adsr.applyEnvelopeToBuffer(internalBuffer, 0, apdNumSamples); 
        adsr.noteOff();
        adsr.applyEnvelopeToBuffer(internalBuffer, apdNumSamples, lengthInSamples - apdNumSamples);
        if (enablePitchShift){
            chowdsp::PitchShifter<float> pitchShifter = chowdsp::PitchShifter<float>(lengthInSamples * 2, 10);
            pitchShifter.prepare({(double)sampleRate, (uint32_t)lengthInSamples * 2, 2});
            pitchShifter.setShiftFactor(1.5f);
            pitchShifter.processBlock(internalBuffer);
        }
    }

    void Grain::process(
        juce::AudioBuffer<float>& buffer, 
        float gain,
        int startSample)
    {
        if (progress > lengthInSamples) { // ADSR is over, so we stop
            return;
        }

        auto numSamples = buffer.getNumSamples() - startSample;

        // cap numSamples at lengthInSamples - progress so we dont try accessing stuff beyond internalBuffer
        if (numSamples > lengthInSamples - progress) {
            numSamples = lengthInSamples - progress;
        }
        
        for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
            buffer.addFrom(
                channel, startSample, 
                internalBuffer, channel, progress, 
                numSamples, gain
            );
        }
        
        progress += numSamples;
    }

} // namespace lsp