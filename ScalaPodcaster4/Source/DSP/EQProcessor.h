#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SpectrumAnalyzer.h"
#include <array>

/**
 * EQProcessor
 * -----------
 * Parametric EQ with:
 *   - A fixed High-Pass Filter (12 dB/oct, two cascaded 1st-order stages)
 *     with selectable voice/podcast-tuned frequency presets.
 *   - Up to 6 fully parametric bands (Bell / Low Shelf / High Shelf / Notch),
 *     each independently enabled/disabled, with free Frequency/Gain/Q.
 *   - Built-in pre- and post-EQ spectrum analyzers for the GUI's live graph.
 */
class EQProcessor
{
public:
    static constexpr int numBands = 6;

    enum class FilterType { Bell, LowShelf, HighShelf, Notch };

    struct Band
    {
        bool        enabled  = false;
        FilterType  type     = FilterType::Bell;
        float       freqHz   = 1000.0f;
        float       gainDb   = 0.0f;
        float       q        = 1.0f;
    };

    EQProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        hpfStage1.prepare(spec);
        hpfStage2.prepare(spec);

        for (auto& f : bandFilters)
            f.prepare(spec);

        preAnalyzer.prepare(sampleRate);
        postAnalyzer.prepare(sampleRate);

        updateFilters();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        preAnalyzer.pushBlock(buffer);

        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto ctx   = juce::dsp::ProcessContextReplacing<float>(block);

        if (hpfEnabled)
        {
            hpfStage1.process(ctx);
            hpfStage2.process(ctx);
        }

        for (int i = 0; i < numBands; ++i)
            if (bands[(size_t)i].enabled)
                bandFilters[(size_t)i].process(ctx);

        postAnalyzer.pushBlock(buffer);
    }

    void reset()
    {
        hpfStage1.reset();
        hpfStage2.reset();
        for (auto& f : bandFilters)
            f.reset();
    }

    void updateFilters()
    {
        using Coeffs = juce::dsp::IIR::Coefficients<float>;

        // HPF: two cascaded Butterworth 1st-order stages = 12 dB/oct overall,
        // each at Q=0.707 so the combined response is maximally flat.
        *hpfStage1.coefficients = *Coeffs::makeHighPass(sampleRate, hpfFrequency, 0.707f);
        *hpfStage2.coefficients = *Coeffs::makeHighPass(sampleRate, hpfFrequency, 0.707f);

        for (int i = 0; i < numBands; ++i)
        {
            auto& b = bands[(size_t)i];
            float freq = juce::jlimit(20.0f, 20000.0f, b.freqHz);
            float q    = juce::jmax(0.05f, b.q);
            float gain = juce::Decibels::decibelsToGain(b.gainDb);

            switch (b.type)
            {
                case FilterType::Bell:
                    *bandFilters[(size_t)i].coefficients = *Coeffs::makePeakFilter(sampleRate, freq, q, gain);
                    break;
                case FilterType::LowShelf:
                    *bandFilters[(size_t)i].coefficients = *Coeffs::makeLowShelf(sampleRate, freq, q, gain);
                    break;
                case FilterType::HighShelf:
                    *bandFilters[(size_t)i].coefficients = *Coeffs::makeHighShelf(sampleRate, freq, q, gain);
                    break;
                case FilterType::Notch:
                    *bandFilters[(size_t)i].coefficients = *Coeffs::makeNotch(sampleRate, freq, q);
                    break;
            }
        }
    }

    // Returns the combined EQ magnitude response (dB) at a given frequency,
    // used by the GUI to draw the static filter curve. Includes HPF + all
    // enabled bands.
    float getResponseAtFrequency(float freqHz) const
    {
        double totalMagnitude = 1.0;

        if (hpfEnabled)
        {
            totalMagnitude *= hpfStage1.coefficients->getMagnitudeForFrequency(freqHz, sampleRate);
            totalMagnitude *= hpfStage2.coefficients->getMagnitudeForFrequency(freqHz, sampleRate);
        }

        for (int i = 0; i < numBands; ++i)
            if (bands[(size_t)i].enabled)
                totalMagnitude *= bandFilters[(size_t)i].coefficients->getMagnitudeForFrequency(freqHz, sampleRate);

        return juce::Decibels::gainToDecibels((float)totalMagnitude, -60.0f);
    }

    // Per-band response, for highlighting the currently-selected/dragged band.
    float getBandResponseAtFrequency(int bandIndex, float freqHz) const
    {
        if (bandIndex < 0 || bandIndex >= numBands || !bands[(size_t)bandIndex].enabled)
            return 0.0f;
        double mag = bandFilters[(size_t)bandIndex].coefficients->getMagnitudeForFrequency(freqHz, sampleRate);
        return juce::Decibels::gainToDecibels((float)mag, -60.0f);
    }

    // Parameters
    bool  hpfEnabled  = true;
    float hpfFrequency = 80.0f;
    std::array<Band, numBands> bands;

    // Live analyzers (pushed from process(), read by the GUI)
    SpectrumAnalyzer preAnalyzer;
    SpectrumAnalyzer postAnalyzer;

private:
    double sampleRate = 44100.0;

    juce::dsp::IIR::Filter<float> hpfStage1;
    juce::dsp::IIR::Filter<float> hpfStage2;
    std::array<juce::dsp::IIR::Filter<float>, numBands> bandFilters;
};
