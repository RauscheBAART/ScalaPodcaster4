#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ScalaPodcasterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- EQ: High-Pass Filter ---
    // Preset dropdown with voice/podcast-tuned frequencies, plus a Custom
    // option that unlocks the free "hpfFreqCustom" slider.
    // Frequencies are chosen from real male/female speech low-end analysis:
    //  - "Off" effectively disables the HPF (handled via hpfOn instead)
    //  - "Gentle" only removes sub-rumble below the lowest male fundamentals
    //  - "Voice" / "Podcast" track typical fundamental ranges
    //  - "Aggressive" for very boomy rooms / handling noise
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "hpfOn", "HPF Enable", true));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "hpfPreset", "HPF Preset",
        juce::StringArray {
            "Gentle (40 Hz) - sub-rumble only",
            "Voice Male (80 Hz)",
            "Voice Female (100 Hz)",
            "Podcast Standard (90 Hz)",
            "Aggressive (150 Hz) - handling noise",
            "Custom"
        }, 3)); // default: Podcast Standard

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hpfFreqCustom", "HPF Custom Frequency",
        juce::NormalisableRange<float>(20.0f, 400.0f, 1.0f, 0.4f), 90.0f, "Hz"));

    // --- EQ: 6 fully parametric bands ---
    // Each band: enabled, filter type (Bell/LowShelf/HighShelf/Notch),
    // frequency, gain, Q. All free and independently switchable.
    for (int i = 0; i < 6; ++i)
    {
        juce::String n = juce::String(i + 1);

        layout.add(std::make_unique<juce::AudioParameterBool>(
            "band" + n + "On", "Band " + n + " Enable", false));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            "band" + n + "Type", "Band " + n + " Type",
            juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Notch" }, 0));

        // Spread default frequencies across the spectrum so freshly-enabled
        // bands don't all stack on top of each other.
        static constexpr float defaultFreqs[6] = { 120.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 9000.0f };
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "band" + n + "Freq", "Band " + n + " Frequency",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), defaultFreqs[i], "Hz"));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "band" + n + "Gain", "Band " + n + " Gain",
            juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB"));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "band" + n + "Q", "Band " + n + " Q",
            juce::NormalisableRange<float>(0.1f, 18.0f, 0.01f, 0.3f), 1.0f, ""));
    }

    // --- De-Esser (dynamic, separate from the parametric EQ) ---
    // Default frequency/threshold tuned from analysis of a representative
    // problem voice: sibilant peaks centred near 8.5 kHz, 20-25 dB above
    // surrounding speech energy.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "deEsserFreq", "De-Esser Frequency",
        juce::NormalisableRange<float>(2000.0f, 14000.0f, 10.0f, 0.4f), 8500.0f, "Hz"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "deEsserThresh", "De-Esser Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.5f), -20.0f, "dB"));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "deEsserWide", "De-Esser Wide Band", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "deEsserOn", "De-Esser Enable", true));

    // --- Compressor ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "threshold", "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.5f), -26.0f, "dB"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "ratio", "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.4f), 4.0f, ":1"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "knee", "Knee",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 6.0f, "dB"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.4f), 0.1f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "release", "Release",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.4f), 1000.0f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "makeup", "Makeup Gain",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f, "dB"));

    // --- LUFS Target ---
    // Preset dropdown: standardized loudness targets, plus a Custom option
    // that unlocks the free-form "lufsCustom" slider below.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "lufsPreset", "LUFS Preset",
        juce::StringArray {
            "-14 LUFS (Spotify/YouTube)",
            "-16 LUFS (Apple Podcasts)",
            "-23 LUFS (EBU R128 Broadcast)",
            "-24 LUFS (ATSC A/85 US Broadcast)",
            "Custom"
        }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "lufsCustom", "LUFS Custom Target",
        juce::NormalisableRange<float>(-36.0f, -6.0f, 0.1f), -14.0f, "LUFS"));

    // --- Fade In / Out ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "fadeEnable", "Fade Enable", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "fadeInPreset", "Fade In Preset",
        juce::StringArray { "0.0s (off)", "0.3s", "0.5s", "1.0s", "2.0s", "Custom" }, 5)); // Custom
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "fadeIn", "Fade In Time",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f, 0.5f), 1.5f, "s"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "fadeOutPreset", "Fade Out Preset",
        juce::StringArray { "0.0s (off)", "0.5s", "1.0s", "2.0s", "3.0s", "Custom" }, 5)); // Custom
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "fadeOut", "Fade Out Time",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f, 0.5f), 2.0f, "s"));

    // --- De-Esser Lookahead ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "deEsserLookahead", "De-Esser Lookahead",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.5f), 5.0f, "ms"));

    // --- Global ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    return layout;
}

//==============================================================================
ScalaPodcasterAudioProcessor::ScalaPodcasterAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Cached parameter pointers
    pHpfOn         = apvts.getRawParameterValue("hpfOn");
    pHpfPreset     = apvts.getRawParameterValue("hpfPreset");
    pHpfFreqCustom = apvts.getRawParameterValue("hpfFreqCustom");

    for (int i = 0; i < EQProcessor::numBands; ++i)
    {
        juce::String n = juce::String(i + 1);
        pBands[(size_t)i].on   = apvts.getRawParameterValue("band" + n + "On");
        pBands[(size_t)i].type = apvts.getRawParameterValue("band" + n + "Type");
        pBands[(size_t)i].freq = apvts.getRawParameterValue("band" + n + "Freq");
        pBands[(size_t)i].gain = apvts.getRawParameterValue("band" + n + "Gain");
        pBands[(size_t)i].q    = apvts.getRawParameterValue("band" + n + "Q");
    }

    pDeEsserFreq   = apvts.getRawParameterValue("deEsserFreq");
    pDeEsserThresh = apvts.getRawParameterValue("deEsserThresh");
    pDeEsserWide   = apvts.getRawParameterValue("deEsserWide");
    pDeEsserOn     = apvts.getRawParameterValue("deEsserOn");

    pThreshold     = apvts.getRawParameterValue("threshold");
    pRatio         = apvts.getRawParameterValue("ratio");
    pKnee          = apvts.getRawParameterValue("knee");
    pAttack        = apvts.getRawParameterValue("attack");
    pRelease       = apvts.getRawParameterValue("release");
    pMakeup        = apvts.getRawParameterValue("makeup");

    pBypass        = apvts.getRawParameterValue("bypass");
    pLufsPreset    = apvts.getRawParameterValue("lufsPreset");
    pLufsCustom    = apvts.getRawParameterValue("lufsCustom");

    pFadeEnable    = apvts.getRawParameterValue("fadeEnable");
    pFadeInPreset  = apvts.getRawParameterValue("fadeInPreset");
    pFadeIn        = apvts.getRawParameterValue("fadeIn");
    pFadeOutPreset = apvts.getRawParameterValue("fadeOutPreset");
    pFadeOut       = apvts.getRawParameterValue("fadeOut");
    pDeEsserLookahead = apvts.getRawParameterValue("deEsserLookahead");

    presetManager = std::make_unique<PresetManager>(apvts);
}

ScalaPodcasterAudioProcessor::~ScalaPodcasterAudioProcessor() {}

//==============================================================================
void ScalaPodcasterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels      = static_cast<uint32_t>(getTotalNumOutputChannels());

    eqProcessor.prepare(spec);
    deEsser.prepare(spec);
    compressor.prepare(spec);
    lufsProcessor.prepare(spec);
    fadeProcessor.prepare(spec);
}

void ScalaPodcasterAudioProcessor::releaseResources()
{
    eqProcessor.reset();
    deEsser.reset();
    compressor.reset();
    lufsProcessor.reset();
    fadeProcessor.reset();
}

bool ScalaPodcasterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept mono and stereo — covers all major DAWs including DaVinci Resolve and Audacity
    const auto& outSet = layouts.getMainOutputChannelSet();
    const auto& inSet  = layouts.getMainInputChannelSet();

    if (outSet != juce::AudioChannelSet::mono()
     && outSet != juce::AudioChannelSet::stereo())
        return false;

    // Input must match output, OR input can be empty (for generator-style hosts)
    if (!inSet.isDisabled() && inSet != outSet)
        return false;

    return true;
}

//==============================================================================
void ScalaPodcasterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // --- Safety checks ---
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    // --- Bypass ---
    if (pBypass->load() > 0.5f)
        return;

    // --- Sync parameters to DSP objects ---

    // HPF: resolve preset → actual frequency, detect changes to avoid
    // rebuilding filter coefficients every block when nothing changed.
    bool hpfOn = pHpfOn->load() > 0.5f;
    int  hpfPresetIdx = static_cast<int>(pHpfPreset->load());
    float hpfFreq = resolveHpfFrequency(hpfPresetIdx, pHpfFreqCustom->load());

    bool eqChanged = (eqProcessor.hpfEnabled != hpfOn) ||
                      (eqProcessor.hpfFrequency != hpfFreq);

    eqProcessor.hpfEnabled   = hpfOn;
    eqProcessor.hpfFrequency = hpfFreq;

    for (int i = 0; i < EQProcessor::numBands; ++i)
    {
        auto& band = eqProcessor.bands[(size_t)i];
        auto& p    = pBands[(size_t)i];

        bool  bOn   = p.on->load() > 0.5f;
        auto  bType = static_cast<EQProcessor::FilterType>(static_cast<int>(p.type->load()));
        float bFreq = p.freq->load();
        float bGain = p.gain->load();
        float bQ    = p.q->load();

        if (band.enabled != bOn || band.type != bType || band.freqHz != bFreq
            || band.gainDb != bGain || band.q != bQ)
            eqChanged = true;

        band.enabled = bOn;
        band.type    = bType;
        band.freqHz  = bFreq;
        band.gainDb  = bGain;
        band.q       = bQ;
    }

    if (eqChanged)
        eqProcessor.updateFilters();

    // De-Esser: separate dynamic module, runs after the static EQ.
    bool deEsserWide = pDeEsserWide->load() > 0.5f;
    float deEsserFreq = pDeEsserFreq->load();
    float newLookahead = pDeEsserLookahead->load();
    bool deEsserFilterChanged = (deEsser.wideBand != deEsserWide) ||
                                 (deEsser.frequencyHz != deEsserFreq);
    bool lookaheadChanged = (deEsser.lookaheadMs != newLookahead);

    deEsser.enabled     = pDeEsserOn->load() > 0.5f;
    deEsser.frequencyHz = deEsserFreq;
    deEsser.thresholdDb = pDeEsserThresh->load();
    deEsser.wideBand    = deEsserWide;
    deEsser.lookaheadMs = newLookahead;
    if (deEsserFilterChanged) deEsser.updateFilter();
    if (lookaheadChanged)     deEsser.updateLookahead();

    bool compParamsChanged =
        (compressor.thresholdDb != pThreshold->load()) ||
        (compressor.ratio       != pRatio->load())     ||
        (compressor.attackMs    != pAttack->load())    ||
        (compressor.releaseMs   != pRelease->load());

    compressor.thresholdDb  = pThreshold->load();
    compressor.ratio        = pRatio->load();
    compressor.kneeDb       = pKnee->load();
    compressor.attackMs     = pAttack->load();
    compressor.releaseMs    = pRelease->load();
    compressor.makeupGainDb = pMakeup->load();
    if (compParamsChanged) compressor.updateCoefficients();

    // Fade: resolve preset → actual seconds value.
    static constexpr float fadeInPresetValues[]  = { 0.0f, 0.3f, 0.5f, 1.0f, 2.0f };
    static constexpr float fadeOutPresetValues[] = { 0.0f, 0.5f, 1.0f, 2.0f, 3.0f };
    int fadeInIdx  = static_cast<int>(pFadeInPreset->load());
    int fadeOutIdx = static_cast<int>(pFadeOutPreset->load());
    fadeProcessor.enabled       = pFadeEnable->load() > 0.5f;
    fadeProcessor.fadeInSeconds  = (fadeInIdx  < 5) ? fadeInPresetValues[fadeInIdx]   : pFadeIn->load();
    fadeProcessor.fadeOutSeconds = (fadeOutIdx < 5) ? fadeOutPresetValues[fadeOutIdx] : pFadeOut->load();

    // --- DSP Chain: Fade-In → EQ → De-Esser → Compressor → LUFS → Fade-Out ---
    // Note: fade-in applied before processing so the processors see a clean
    // ramp; fade-out applied after so level meters reflect the faded signal.
    //
    // We apply the fade-in here and the fade-out in a second pass by
    // splitting the FadeProcessor call. Since FadeProcessor internally
    // handles both in a single pass (keyed on samplePosition), we just
    // call it once after all other processing and it will apply whichever
    // fades are active based on the current position.
    eqProcessor.process(buffer);
    deEsser.process(buffer);
    compressor.process(buffer);

    lufsProcessor.targetLufs = resolveLufsTarget(static_cast<int>(pLufsPreset->load()),
                                                  pLufsCustom->load());
    lufsProcessor.process(buffer);

    // Fade (in/out) applied last so it acts on the fully-mastered signal.
    fadeProcessor.process(buffer, isNonRealtime());

    // --- Update meters for GUI ---
    gainReductionDb.store(compressor.currentGainReductionDb);
    momentaryLufs.store(lufsProcessor.getMomentaryLufs());
    integratedLufs.store(lufsProcessor.getIntegratedLufs());

    // Clip-gain: how much the LUFS normaliser has shifted the level overall.
    normalisationGainDb.store(lufsProcessor.getCurrentGainDb());

    // Peak output level
    float peak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    outputPeakDb.store(peak > 1e-6f ? juce::Decibels::gainToDecibels(peak) : -120.0f);
}

//==============================================================================
void ScalaPodcasterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ScalaPodcasterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScalaPodcasterAudioProcessor();
}

juce::AudioProcessorEditor* ScalaPodcasterAudioProcessor::createEditor()
{
    return new ScalaPodcasterAudioProcessorEditor (*this);
}

// Mono-safe processBlock wrapper already handles numChannels dynamically via DSP modules.
// No additional changes needed — CompressorProcessor and LufsProcessor use getNumChannels().
