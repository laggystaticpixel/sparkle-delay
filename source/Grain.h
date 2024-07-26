#include <juce_audio_basics/juce_audio_basics.h>
#include <chowdsp_data_structures/chowdsp_data_structures.h>
#include <chowdsp_dsp_utils/chowdsp_dsp_utils.h>

namespace lsp {
    class Grain {
        public:
        Grain(
            int sampleOffset, 
            int delayNumSamples, 
            juce::ADSR adsr, 
            int sampleRate, 
            bool reversed = false, 
            bool enablePitchShift = false
        );
        ~Grain();
        void process(juce::AudioBuffer<float>& buffer, float gain = 1.0f, int startSample = 0);
        void applyADSR();

        juce::ADSR adsr;
        int sampleOffset;
        int delayNumSamples;
        int progress = 0;
        int sampleRate;
        bool reversed;
        juce::AudioBuffer<float> internalBuffer;
        bool adsrNoteOn = true;
        bool active = true;
        bool readyToPlay = false;
        bool isPlaying = false;
        bool enablePitchShift = false;
        int lengthInSamples;

        
    };
} // namespace lsp
