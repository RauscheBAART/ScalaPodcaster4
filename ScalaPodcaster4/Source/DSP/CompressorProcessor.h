#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>

/**
 * CompressorProcessor
 * -------------------
 * Stage 2 of the PodcastMaster chain.
 *
 * Feed-forward RMS compressor with:
 *   - Threshold, Ratio, Knee, Attack, Release
 *   - Make-up gain
 *   - Gain-reduction meter readout (for the GUI VU)
 *
 * Default preset is tuned for spoken-word / podcast:
 *   Threshold -24 dBFS, Ratio 4:1, Attack 10 ms, Release 80 ms
 */
class CompressorProcessor
{
public:
    CompressorProcessor() = default;

    //==========================================================================
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate     = spec.sampleRate;
        numChannels    = spec.numChannels;
        envelopeBuffer.resize(numChannels, 0.0f);

        updateCoefficients();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = static_cast<int>(numChannels);

        currentGainReductionDb = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // --- 1. Compute RMS level across all channels ---
            float sumSq = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sumSq += buffer.getReadPointer(ch)[i] * buffer.getReadPointer(ch)[i];

            float rmsLevel = std::sqrt(sumSq / static_cast<float>(numCh));
            float rmsDb    = (rmsLevel > 1e-6f)
                             ? juce::Decibels::gainToDecibels(rmsLevel)
                             : -120.0f;

            // --- 2. Gain computer (with soft knee) ---
            float gainDb = computeGain(rmsDb);

            // --- 3. Smooth envelope (ballistics) ---
            for (int ch = 0; ch < numCh; ++ch)
            {
                float coeff = (gainDb < envelopeBuffer[ch]) ? attackCoeff : releaseCoeff;
                envelopeBuffer[ch] = gainDb + coeff * (envelopeBuffer[ch] - gainDb);
            }

            // --- 4. Apply gain + makeup ---
            for (int ch = 0; ch < numCh; ++ch)
            {
                float totalGainDb = envelopeBuffer[ch] + makeupGainDb;
                float linearGain  = juce::Decibels::decibelsToGain(totalGainDb);
                buffer.getWritePointer(ch)[i] *= linearGain;
            }

            // Track worst-case GR for meter
            float gr = envelopeBuffer[0]; // negative dB = reduction
            if (gr < currentGainReductionDb)
                currentGainReductionDb = gr;
        }
    }

    void reset()
    {
        std::fill(envelopeBuffer.begin(), envelopeBuffer.end(), 0.0f);
    }

    // Call after any parameter change
    void updateCoefficients()
    {
        // Time constants: coeff = e^(-1 / (time_s * sampleRate))
        attackCoeff  = std::exp(-1.0f / (static_cast<float>(sampleRate) * attackMs  * 0.001f));
        releaseCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * releaseMs * 0.001f));
    }

    //==========================================================================
    // Parameters
    float thresholdDb  = -24.0f;   // dBFS
    float ratio        =   4.0f;   // n:1
    float kneeDb       =   6.0f;   // soft knee width in dB
    float attackMs     =  10.0f;   // ms
    float releaseMs    =  80.0f;   // ms
    float makeupGainDb =   0.0f;   // dB (auto-makeup can be calculated externally)

    // Read-only output for GUI meter
    float currentGainReductionDb = 0.0f;

private:
    double sampleRate   = 44100.0;
    uint32_t numChannels = 2;

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;

    std::vector<float> envelopeBuffer;

    // Soft-knee gain computer — returns gain *change* in dB (≤ 0)
    float computeGain(float inputDb) const
    {
        float halfKnee = kneeDb * 0.5f;
        float overThresh = inputDb - thresholdDb;

        if (overThresh < -halfKnee)
        {
            return 0.0f; // below knee — no reduction
        }
        else if (overThresh < halfKnee)
        {
            // Inside knee — quadratic interpolation
            float t = (overThresh + halfKnee) / kneeDb;
            return (1.0f / ratio - 1.0f) * t * t * halfKnee;
        }
        else
        {
            // Above knee — linear compression
            return overThresh * (1.0f / ratio - 1.0f);
        }
    }
};
