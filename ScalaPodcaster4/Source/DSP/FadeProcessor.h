#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

/**
 * FadeProcessor
 * -------------
 * Applies a fade-in at the start of the signal and a fade-out at the end.
 *
 * ARCHITECTURE NOTE — why this works offline but not live:
 * This processor tracks the total number of samples processed since
 * prepareToPlay(). Fade-in is straightforward: apply a ramp from sample 0.
 * Fade-out requires knowing when the track ends — a problem in real-time use.
 *
 * Solution: we use two complementary strategies:
 *
 *   1.  isNonRealtime() hint: when the DAW is offline-rendering (Reaper
 *       "Render", Resolve "Export"), the host sets this flag. We don't
 *       actually get the total length from this, but we use it to enable
 *       the end-of-track silence detector (strategy 2) safely, since there
 *       is no live monitoring concern.
 *
 *   2.  Silence detection: when the input RMS drops below -70 dBFS for
 *       more than ~80 ms (silenceGraceMs), we treat that as the track end
 *       and start the fade-out from the last non-silent position. This works
 *       reliably on bounced/rendered tracks where the DAW inserts trailing
 *       silence after the last audio event.
 *
 * Both strategies are gated on isNonRealtime() so the fade-out does NOT
 * trigger during live monitoring, which would cause sudden volume drops.
 *
 * Fade shapes: linear ramp (simple, predictable, correct for podcast audio
 * where the fade is usually just a few hundred ms).
 */
class FadeProcessor
{
public:
    FadeProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
    }

    void reset()
    {
        samplePosition       = 0;
        silentSampleCount    = 0;
        fadeOutStartSample   = -1;
        fadeOutTriggered     = false;
    }

    void process(juce::AudioBuffer<float>& buffer, bool hostIsNonRealtime)
    {
        if (!enabled) return;

        const int   numSamples  = buffer.getNumSamples();
        const int   numCh       = buffer.getNumChannels();
        if (numSamples == 0 || numCh == 0) return;

        const int64_t fadeInSamples  = static_cast<int64_t>(fadeInSeconds  * sampleRate);
        const int64_t fadeOutSamples = static_cast<int64_t>(fadeOutSeconds * sampleRate);
        // 3 seconds of uninterrupted near-digital-silence required before
        // triggering fade-out. This prevents false triggers on natural speech
        // pauses, breath gaps, or room tone — only true end-of-clip silence
        // (as inserted by the DAW after the last audio region) qualifies.
        const int64_t silenceGrace   = static_cast<int64_t>(3.0 * sampleRate);
        // -90 dBFS threshold: only triggers on near-digital silence, not
        // quiet room tone or low-level hum that could appear during pauses.
        const float silenceThreshold = juce::Decibels::decibelsToGain(-90.0f);

        // --- End-of-track detection (offline only) ---
        if (hostIsNonRealtime && !fadeOutTriggered)
        {
            float rmsSum = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    rmsSum += data[i] * data[i];
            }
            float rms = std::sqrt(rmsSum / (float)(numSamples * numCh));
            bool  isSilent = (rms < silenceThreshold);

            if (isSilent)
            {
                silentSampleCount += numSamples;
                if (silentSampleCount >= silenceGrace && fadeOutStartSample < 0)
                {
                    // Start fade-out from the moment silence began.
                    fadeOutStartSample = samplePosition - silentSampleCount;
                    fadeOutTriggered   = true;
                }
            }
            else
            {
                // Any non-silent block resets the counter — a pause in speech
                // won't accumulate toward the 3-second threshold.
                silentSampleCount = 0;
            }
        }

        // --- Apply gains sample by sample ---
        for (int i = 0; i < numSamples; ++i)
        {
            float gain = 1.0f;
            int64_t pos = samplePosition + i;

            // Fade in
            if (fadeInSamples > 0 && pos < fadeInSamples)
                gain *= (float)pos / (float)fadeInSamples;

            // Fade out
            if (fadeOutTriggered && fadeOutSamples > 0 && fadeOutStartSample >= 0)
            {
                int64_t fadePos = pos - fadeOutStartSample;
                if (fadePos >= 0)
                {
                    float t = 1.0f - juce::jlimit(0.0f, 1.0f,
                                                    (float)fadePos / (float)fadeOutSamples);
                    gain *= t;
                }
            }

            gain = juce::jlimit(0.0f, 1.0f, gain);
            for (int ch = 0; ch < numCh; ++ch)
                *buffer.getWritePointer(ch, i) *= gain;
        }

        samplePosition += numSamples;
    }

    // Parameters
    bool  enabled        = true;
    float fadeInSeconds  = 0.5f;   // 0 = disabled
    float fadeOutSeconds = 1.0f;   // 0 = disabled

private:
    double  sampleRate           = 44100.0;
    int64_t samplePosition       = 0;
    int64_t silentSampleCount    = 0;
    int64_t fadeOutStartSample   = -1;
    bool    fadeOutTriggered     = false;
};
