#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/**
 * SpectrumAnalyzer
 * ----------------
 * Continuously collects audio into a rolling buffer and produces an FFT
 * magnitude spectrum, smoothed over time. Designed to be fed from the audio
 * thread (pushSamples) and read from the GUI thread (getMagnitudes), using
 * a lock-free double-buffer handoff so neither thread ever blocks the other.
 *
 * This is a generic "give me a spectrum" helper — it does not know about
 * EQ bands. PluginProcessor owns two instances (pre/post EQ).
 */
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder      = 11;                    // 2048-point FFT
    static constexpr int fftSize       = 1 << fftOrder;
    static constexpr int numBins       = fftSize / 2;

    SpectrumAnalyzer() : fft(fftOrder)
    {
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(
            (size_t)fftSize, juce::dsp::WindowingFunction<float>::hann);
        fifo.fill(0.0f);
        fftData.fill(0.0f);
        displayMagnitudesDb.fill(-100.0f);
    }

    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        fifoIndex = 0;
        nextFftBlockReady = false;
        fifo.fill(0.0f);
        displayMagnitudesDb.fill(-100.0f);
    }

    // Call from the audio thread for every sample (mono-summed if multi-channel).
    void pushSample(float sample)
    {
        if (fifoIndex == fftSize)
        {
            if (!nextFftBlockReady.load())
            {
                std::copy(fifo.begin(), fifo.end(), fftBlockForFft.begin());
                nextFftBlockReady.store(true);
            }
            fifoIndex = 0;
        }
        fifo[(size_t)fifoIndex++] = sample;
    }

    void pushBlock(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();
        if (numSamples == 0 || numCh == 0) return;

        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sum += buffer.getSample(ch, i);
            pushSample(sum / (float)numCh);
        }
    }

    // Call periodically from the GUI thread (e.g. timer @ 30Hz). Performs the
    // FFT if a new block is ready and updates the smoothed display magnitudes.
    // Returns true if the display data changed (worth repainting).
    bool updateIfReady()
    {
        if (!nextFftBlockReady.load())
            return false;

        std::copy(fftBlockForFft.begin(), fftBlockForFft.end(), fftData.begin());
        nextFftBlockReady.store(false);

        window->multiplyWithWindowingTable(fftData.data(), (size_t)fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        constexpr float minDb = -100.0f;
        for (int i = 0; i < numBins; ++i)
        {
            float magnitude = fftData[(size_t)i] / (float)fftSize;
            float db = juce::Decibels::gainToDecibels(magnitude, minDb);

            // Smooth: fast attack, slower release, so the curve doesn't flicker.
            float& display = displayMagnitudesDb[(size_t)i];
            display = (db > display) ? (display + (db - display) * 0.6f)
                                      : (display + (db - display) * 0.15f);
        }
        return true;
    }

    // Returns the smoothed magnitude (dB) for a given frequency, interpolated
    // between the nearest FFT bins. Safe to call from the GUI thread only
    // (after updateIfReady()).
    float getMagnitudeAtFrequency(float freqHz) const
    {
        if (sampleRate <= 0.0) return -100.0f;
        float binWidth = (float)(sampleRate / (double)fftSize);
        float binPos   = freqHz / binWidth;
        int   bin0     = (int)binPos;
        int   bin1     = bin0 + 1;
        float frac     = binPos - (float)bin0;

        if (bin0 < 0 || bin0 >= numBins) return -100.0f;
        if (bin1 >= numBins) bin1 = numBins - 1;

        return displayMagnitudesDb[(size_t)bin0] * (1.0f - frac)
             + displayMagnitudesDb[(size_t)bin1] * frac;
    }

    double getSampleRate() const { return sampleRate; }

private:
    double sampleRate = 44100.0;

    juce::dsp::FFT fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    std::array<float, (size_t)fftSize> fifo;
    int fifoIndex = 0;

    // Double-buffer handoff: audio thread writes fftBlockForFft, GUI thread
    // copies it into fftData once nextFftBlockReady flips true.
    std::array<float, (size_t)fftSize> fftBlockForFft;
    std::atomic<bool> nextFftBlockReady { false };

    std::array<float, (size_t)fftSize> fftData;
    std::array<float, (size_t)numBins> displayMagnitudesDb;
};
