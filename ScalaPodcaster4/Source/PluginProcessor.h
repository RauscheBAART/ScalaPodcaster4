#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/EQProcessor.h"
#include "DSP/DeEsserProcessor.h"
#include "DSP/CompressorProcessor.h"
#include "DSP/LufsProcessor.h"
#include "DSP/FadeProcessor.h"
#include "PresetManager.h"

//==============================================================================
/**
 * ScalaPodcasterAudioProcessor
 *
 * Signal chain:
 *   Input → EQ / De-Esser → Compressor → LUFS Makeup + True-Peak Limiter → Output
 */
class ScalaPodcasterAudioProcessor  : public juce::AudioProcessor
{
public:
    ScalaPodcasterAudioProcessor();
    ~ScalaPodcasterAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    //==========================================================================
    // Parameter tree (APVTS)
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Resolves the active LUFS target from the preset choice + custom slider.
    // Index: 0=-14  1=-16  2=-23  3=-24  4=Custom
    static constexpr float lufsPresetValues[4] = { -14.0f, -16.0f, -23.0f, -24.0f };
    static float resolveLufsTarget(int presetIndex, float customValue)
    {
        if (presetIndex >= 0 && presetIndex < 4)
            return lufsPresetValues[presetIndex];
        return customValue; // index 4 == Custom
    }

    // Resolves the active HPF frequency from the preset choice + custom slider.
    // Index: 0=Gentle(40) 1=VoiceMale(80) 2=VoiceFemale(100) 3=Podcast(90) 4=Aggressive(150) 5=Custom
    static constexpr float hpfPresetValues[5] = { 40.0f, 80.0f, 100.0f, 90.0f, 150.0f };
    static float resolveHpfFrequency(int presetIndex, float customValue)
    {
        if (presetIndex >= 0 && presetIndex < 5)
            return hpfPresetValues[presetIndex];
        return customValue; // index 5 == Custom
    }

    // Live metering (read by the editor)
    std::atomic<float> gainReductionDb  { 0.0f };
    std::atomic<float> momentaryLufs    { -120.0f };
    std::atomic<float> integratedLufs   { -120.0f };
    std::atomic<float> outputPeakDb     { -120.0f };
    std::atomic<float> normalisationGainDb { 0.0f };  // total LUFS makeup gain applied

    // Accessor for the editor's EQ graph (reads filter response + spectrum
    // analyzers; the graph also writes band parameters directly via apvts,
    // not through this reference).
    EQProcessor& getEqProcessor() { return eqProcessor; }

    // Preset manager — created after apvts so it can reference it.
    std::unique_ptr<PresetManager> presetManager;

private:
    EQProcessor         eqProcessor;
    DeEsserProcessor    deEsser;
    CompressorProcessor compressor;
    LufsProcessor       lufsProcessor;
    FadeProcessor       fadeProcessor;

    // Cached parameter pointers (fast access in processBlock)
    std::atomic<float>* pHpfOn         = nullptr;
    std::atomic<float>* pHpfPreset     = nullptr;
    std::atomic<float>* pHpfFreqCustom = nullptr;

    // Per-band parameter pointers: [band][0]=on [1]=type [2]=freq [3]=gain [4]=q
    struct BandParams
    {
        std::atomic<float>* on    = nullptr;
        std::atomic<float>* type  = nullptr;
        std::atomic<float>* freq  = nullptr;
        std::atomic<float>* gain  = nullptr;
        std::atomic<float>* q     = nullptr;
    };
    std::array<BandParams, EQProcessor::numBands> pBands;

    std::atomic<float>* pDeEsserFreq   = nullptr;
    std::atomic<float>* pDeEsserThresh = nullptr;
    std::atomic<float>* pDeEsserWide   = nullptr;
    std::atomic<float>* pDeEsserOn     = nullptr;

    std::atomic<float>* pThreshold     = nullptr;
    std::atomic<float>* pRatio         = nullptr;
    std::atomic<float>* pKnee          = nullptr;
    std::atomic<float>* pAttack        = nullptr;
    std::atomic<float>* pRelease       = nullptr;
    std::atomic<float>* pMakeup        = nullptr;

    std::atomic<float>* pBypass        = nullptr;
    std::atomic<float>* pLufsPreset    = nullptr;
    std::atomic<float>* pLufsCustom    = nullptr;

    std::atomic<float>* pFadeEnable     = nullptr;
    std::atomic<float>* pFadeInPreset   = nullptr;
    std::atomic<float>* pFadeIn         = nullptr;
    std::atomic<float>* pFadeOutPreset  = nullptr;
    std::atomic<float>* pFadeOut        = nullptr;
    std::atomic<float>* pDeEsserLookahead = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScalaPodcasterAudioProcessor)
};
