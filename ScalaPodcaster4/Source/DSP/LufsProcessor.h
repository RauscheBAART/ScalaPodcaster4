#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <deque>

class LufsProcessor
{
public:
    static constexpr float TRUE_PEAK_CEILING = -1.0f;
    float targetLufs = -14.0f;

    LufsProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = spec.numChannels;

        gateBlockSamples = static_cast<int>(sampleRate * 0.400);
        currentBlockPos  = 0;
        blockMeanSquares.assign(numChannels, 0.0f);

        // K-weighting filters — one pair per channel
        preFilters.clear();
        rlbFilters.clear();
        preFilters.resize(numChannels);
        rlbFilters.resize(numChannels);

        // Pre-filter and RLB work on full blocks, not per-sample
        juce::dsp::ProcessSpec monoSpec { sampleRate, spec.maximumBlockSize, 1 };

        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        for (uint32_t ch = 0; ch < numChannels; ++ch)
        {
            preFilters[ch].prepare(monoSpec);
            rlbFilters[ch].prepare(monoSpec);
            *preFilters[ch].coefficients = *Coeffs::makeHighShelf(
                sampleRate, 1681.0f, 0.707f, juce::Decibels::decibelsToGain(4.0f));
            *rlbFilters[ch].coefficients = *Coeffs::makeHighPass(
                sampleRate, 38.2f, 0.5f);
        }

        limiter.prepare(spec);
        limiter.setThreshold(TRUE_PEAK_CEILING);
        limiter.setRelease(50.0f);

        // Pre-allocate working buffer
        workBuffer.setSize((int)numChannels, (int)spec.maximumBlockSize);

        // Smoothed makeup gain so the very first block already gets a sensible
        // gain instead of waiting for the first full 400ms gating window.
        // ~80ms ramp keeps it fast enough to feel instant but avoids clicks.
        smoothedGain.reset(sampleRate, 0.08);
        smoothedGain.setCurrentAndTargetValue(1.0f);

        // Fast running loudness estimate (simple one-pole envelope on mean-square),
        // used as a stand-in for integrated LUFS before the gated measurement
        // has enough data, and blended out once the real measurement is reliable.
        fastEnvelopeMs = 0.0f;
        fastEnvelopeCoeff = std::exp(-1.0f / (float)(0.2 * sampleRate)); // ~200ms time constant

        reset();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = (int)numChannels;

        if (numSamples == 0 || numCh == 0) return;

        // --- K-weighted copy for loudness measurement ---
        workBuffer.setSize(numCh, numSamples, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            workBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        // Apply K-weighting filters to work buffer (block-wise, no per-sample alloc)
        for (int ch = 0; ch < numCh; ++ch)
        {
            // Wrap single channel in AudioBlock
            float* chPtr = workBuffer.getWritePointer(ch);
            juce::dsp::AudioBlock<float> block (&chPtr, 1, (size_t)numSamples);
            auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
            preFilters[ch].process(ctx);
            rlbFilters[ch].process(ctx);
        }

        // Accumulate mean-square in gating blocks, and update a fast running
        // loudness envelope sample-by-sample so we have a usable loudness
        // estimate from the very first sample (no "dead" startup period).
        for (int i = 0; i < numSamples; ++i)
        {
            float sampleMs = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float v = workBuffer.getSample(ch, i);
                sampleMs += v * v;
                blockMeanSquares[ch] += v * v;
            }

            // One-pole envelope follower on instantaneous mean-square energy.
            fastEnvelopeMs = fastEnvelopeCoeff * fastEnvelopeMs
                           + (1.0f - fastEnvelopeCoeff) * sampleMs;

            ++currentBlockPos;

            if (currentBlockPos >= gateBlockSamples)
            {
                float blockMs = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                    blockMs += blockMeanSquares[ch] / (float)gateBlockSamples;

                if (blockMs > 1.0e-7f)
                    gatedBlocks.push_back(blockMs);

                std::fill(blockMeanSquares.begin(), blockMeanSquares.end(), 0.0f);
                currentBlockPos = 0;
            }
        }

        // Update momentary LUFS
        if (!gatedBlocks.empty())
        {
            float ms = gatedBlocks.back();
            momentaryLufs = (ms > 0.0f) ? (-0.691f + 10.0f * std::log10(ms)) : -120.0f;
        }
        updateIntegratedLufs();

        // --- Resolve the loudness estimate used for the makeup gain ---
        // Below ~3 gated blocks (~1.2s) the true integrated measurement is
        // statistically unreliable, so we lean on the fast envelope instead
        // and cross-fade smoothly into the gated measurement as it matures.
        // This guarantees the whole track is normalised, including the very
        // first fraction of a second, instead of leaving an untreated gap.
        float fastLufs = (fastEnvelopeMs > 1.0e-9f)
            ? (-0.691f + 10.0f * std::log10(fastEnvelopeMs)) : -120.0f;

        float effectiveLufs;
        const int reliableBlockCount = 3;
        if (gatedBlocks.empty())
        {
            effectiveLufs = fastLufs;
        }
        else if ((int)gatedBlocks.size() < reliableBlockCount)
        {
            float blend = (float)gatedBlocks.size() / (float)reliableBlockCount;
            effectiveLufs = fastLufs + blend * (integratedLufs - fastLufs);
        }
        else
        {
            effectiveLufs = integratedLufs;
        }

        // --- Apply makeup gain toward target (sample-accurate smoothing) ---
        float targetGain = 1.0f;
        if (effectiveLufs > -120.0f)
        {
            float makeupDb = juce::jlimit(-40.0f, 30.0f, targetLufs - effectiveLufs);
            targetGain = juce::Decibels::decibelsToGain(makeupDb);
        }
        smoothedGain.setTargetValue(targetGain);

        for (int i = 0; i < numSamples; ++i)
        {
            float g = smoothedGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample(ch, i, buffer.getSample(ch, i) * g);
        }

        // --- True-peak limiter ---
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto ctx   = juce::dsp::ProcessContextReplacing<float>(block);
        limiter.process(ctx);
    }

    void reset()
    {
        for (auto& f : preFilters) f.reset();
        for (auto& f : rlbFilters) f.reset();
        limiter.reset();
        gatedBlocks.clear();
        blockMeanSquares.assign(numChannels, 0.0f);
        currentBlockPos = 0;
        integratedLufs  = -120.0f;
        momentaryLufs   = -120.0f;
        fastEnvelopeMs  = 0.0f;
        smoothedGain.setCurrentAndTargetValue(1.0f);
    }

    float getIntegratedLufs() const { return integratedLufs; }
    float getMomentaryLufs()  const { return momentaryLufs;  }

    // Returns the current normalisation gain in dB — how much the LUFS
    // makeup stage has shifted the signal relative to input level.
    float getCurrentGainDb() const
    {
        float g = smoothedGain.getCurrentValue();
        return (g > 1e-6f) ? juce::Decibels::gainToDecibels(g) : -120.0f;
    }

private:
    double   sampleRate  = 44100.0;
    uint32_t numChannels = 2;

    std::vector<juce::dsp::IIR::Filter<float>> preFilters;
    std::vector<juce::dsp::IIR::Filter<float>> rlbFilters;
    juce::dsp::Limiter<float> limiter;
    juce::AudioBuffer<float>  workBuffer;

    int   gateBlockSamples = 0;
    int   currentBlockPos  = 0;
    std::vector<float> blockMeanSquares;
    std::deque<float>  gatedBlocks;

    float integratedLufs = -120.0f;
    float momentaryLufs  = -120.0f;

    // Fast loudness envelope (covers the startup gap before the gated
    // measurement has enough data) and smoothed output gain.
    float fastEnvelopeMs    = 0.0f;
    float fastEnvelopeCoeff = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedGain;

    void updateIntegratedLufs()
    {
        if (gatedBlocks.empty()) return;

        float ungatedMs = 0.0f;
        for (float ms : gatedBlocks) ungatedMs += ms;
        ungatedMs /= (float)gatedBlocks.size();

        float relGate = ungatedMs * 0.1f;
        float sumMs   = 0.0f;
        int   count   = 0;
        for (float ms : gatedBlocks)
            if (ms >= relGate) { sumMs += ms; ++count; }

        if (count > 0)
        {
            float meanMs = sumMs / (float)count;
            integratedLufs = (meanMs > 0.0f)
                ? (-0.691f + 10.0f * std::log10(meanMs)) : -120.0f;
        }
    }
};
