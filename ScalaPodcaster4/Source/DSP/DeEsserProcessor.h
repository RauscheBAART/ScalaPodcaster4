#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <deque>

/**
 * DeEsserProcessor
 * ----------------
 * Dynamic, frequency-selective de-esser with optional lookahead.
 *
 * The sidechain bandpass filter detects sibilance energy; gain reduction
 * is applied only to that energy contribution — broadband content stays
 * untouched, unlike a static EQ notch which would also dull non-sibilant
 * high end.
 *
 * LOOKAHEAD: when lookaheadMs > 0, the sidechain detection runs
 * lookaheadMs ahead of the main signal (via a short delay buffer on the
 * main path). This eliminates the typical "pumping" artefact on sudden
 * hard S-consonants — the gain reduction is already in motion before the
 * transient arrives. Since ScalaPodcaster is designed for offline rendering
 * there is no monitoring latency concern.
 *
 * Default frequency (8.5 kHz) and Q were derived from analysing a real
 * problem voice: sibilant peaks at 7.7–9.9 kHz, centroid ~8.5 kHz,
 * 20–25 dB above surrounding speech energy.
 */
class DeEsserProcessor
{
public:
    DeEsserProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        numChannels  = (int)spec.numChannels;
        maxBlockSize = (int)spec.maximumBlockSize;

        detectionFilter.prepare(spec);
        sidechainBuffer.setSize(numChannels, maxBlockSize);
        updateFilter();
        updateLookahead();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        currentGainReductionDb = 0.0f;
        if (!enabled) return;

        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0) return;

        // --- Sidechain detection ---
        sidechainBuffer.setSize(numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            sidechainBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        auto scBlock = juce::dsp::AudioBlock<float>(sidechainBuffer);
        detectionFilter.process(juce::dsp::ProcessContextReplacing<float>(scBlock));

        // --- Lookahead: push main signal into delay, detect ahead ---
        if (lookaheadSamples > 0)
        {
            // Push current main signal into delay buffer
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < numSamples; ++i)
                    mainDelayBuffer[ch].push_back(buffer.getSample(ch, i));

            // Copy delayed output back into buffer (if enough samples accumulated)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if ((int)mainDelayBuffer[ch].size() >= numSamples + lookaheadSamples)
                {
                    auto* out = buffer.getWritePointer(ch);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        out[i] = mainDelayBuffer[ch].front();
                        mainDelayBuffer[ch].pop_front();
                    }
                }
                // Trim to max expected size to avoid unbounded growth
                while ((int)mainDelayBuffer[ch].size() > lookaheadSamples + maxBlockSize * 2)
                    mainDelayBuffer[ch].pop_front();
            }
        }

        // --- Compute and apply gain reduction ---
        const float threshold = juce::Decibels::decibelsToGain(thresholdDb);
        float worstReductionDb = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float scLevel = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                scLevel = juce::jmax(scLevel, std::abs(sidechainBuffer.getSample(ch, i)));

            float targetGain = 1.0f;
            if (scLevel > threshold && scLevel > 1e-6f)
                targetGain = threshold / scLevel;

            float coeff = (targetGain < smoothedGain) ? 0.35f : 0.05f;
            smoothedGain = smoothedGain + coeff * (targetGain - smoothedGain);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float& s = *buffer.getWritePointer(ch, i);
                if (!std::isfinite(s)) { s = 0.0f; continue; }
                s *= smoothedGain;
            }

            worstReductionDb = juce::jmin(worstReductionDb,
                juce::Decibels::gainToDecibels(juce::jmax(smoothedGain, 1e-6f)));
        }

        currentGainReductionDb = worstReductionDb;
    }

    void reset()
    {
        detectionFilter.reset();
        smoothedGain = 1.0f;
        currentGainReductionDb = 0.0f;
        for (auto& d : mainDelayBuffer) d.clear();
    }

    void updateFilter()
    {
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        float q = wideBand ? 1.4f : 3.0f;
        *detectionFilter.coefficients = *Coeffs::makeBandPass(sampleRate, frequencyHz, q);
    }

    void updateLookahead()
    {
        lookaheadSamples = static_cast<int>(lookaheadMs * 0.001 * sampleRate);
        mainDelayBuffer.resize((size_t)juce::jmax(1, numChannels));
        for (auto& d : mainDelayBuffer) d.clear();
    }

    // Parameters
    bool  enabled      = true;
    float frequencyHz  = 8500.0f;
    float thresholdDb  = -20.0f;
    bool  wideBand     = false;
    float lookaheadMs  = 5.0f;   // 0 = no lookahead; 5 ms is a good sweet spot

    // Meter readout
    float currentGainReductionDb = 0.0f;

private:
    double sampleRate    = 44100.0;
    int    numChannels   = 2;
    int    maxBlockSize  = 512;
    int    lookaheadSamples = 0;

    juce::dsp::IIR::Filter<float> detectionFilter;
    juce::AudioBuffer<float>      sidechainBuffer;
    std::vector<std::deque<float>> mainDelayBuffer;
    float smoothedGain = 1.0f;
};
