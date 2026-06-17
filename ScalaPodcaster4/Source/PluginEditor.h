#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "EQGraphComponent.h"

//==============================================================================
/**
 * ScalaPodcasterAudioProcessorEditor
 *
 * Nectar-inspired layout:
 *   [Header: logo + title]
 *   [Module tab row: EQ | DE-ESSER | COMPRESSOR | LOUDNESS]
 *   [Main content area: shows controls for the selected module]  [Meter sidebar]
 *
 * Only one module's controls are visible at a time (tab behaviour); the
 * meter sidebar (Gain Reduction / Momentary LUFS / Integrated LUFS) and the
 * global Bypass control are always visible regardless of the selected tab.
 */
class ScalaPodcasterAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                           public juce::Timer
{
public:
    explicit ScalaPodcasterAudioProcessorEditor (ScalaPodcasterAudioProcessor&);
    ~ScalaPodcasterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;   // refresh meters @ 30 Hz

#if SCALA_BUILD_EDITOR_SMOKE_TEST
    // Dev-only hook used by dev/editor_screenshot.cpp to switch tabs without
    // simulating real mouse events. Compiled out of the shipped plugin.
    void debugSelectTabForScreenshot (int tabIndex);
#endif

private:
    ScalaPodcasterAudioProcessor& audioProcessor;

    //--- Palette (Nectar-inspired: dark slate background, single amber accent) ---
    juce::Colour bgDarkest  { 0xFF09090D };  // outer canvas / graph / meter wells (near-black)
    juce::Colour bgPanel    { 0xFF22242F };  // card / panel surfaces (clearly lighter than canvas)
    juce::Colour bgPanelHi  { 0xFF2A2D3A };  // header bar (lighter still)
    juce::Colour strokeCol  { 0xFF3A3D4A };  // hairline borders
    juce::Colour accent     { 0xFFF2A03D };  // amber accent (active state)
    juce::Colour text       { 0xFFEDEDF2 };
    juce::Colour textDim    { 0xFF9A9DA9 };
    juce::Colour warn       { 0xFFE0544A };

    enum class ModuleTab { EQ, DeEsser, Compressor, Loudness, Fade };
    ModuleTab activeTab = ModuleTab::EQ;

    //--- Header: Preset selector (always visible) ---
    juce::ComboBox   presetBox;
    juce::TextButton presetSaveBtn  { "Save" };
    juce::TextButton presetDeleteBtn { "Del" };
    juce::Image logoFullImage;
    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    void refreshPresetBox();

    //--- Module tab row ---
    struct ModuleTabButton : public juce::Component
    {
        juce::String name;
        ModuleTab id;
        bool active = false;
        std::function<void()> onClicked;

        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override { if (onClicked) onClicked(); }
    };

    ModuleTabButton eqTab, deEsserTab, compressorTab, loudnessTab, fadeTab;
    std::array<ModuleTabButton*, 5> allTabs;
    void selectTab (ModuleTab newTab);

    //--- EQ Section ---
    // HPF row: enable toggle + preset dropdown + custom frequency slider
    // (custom slider only enabled when "Custom" is selected in the preset).
    juce::ToggleButton hpfOnButton { "HPF" };
    juce::ComboBox      hpfPresetBox;
    juce::Slider         hpfCustomSlider;
    juce::Label           hpfCustomLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   hpfOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfPresetAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   hpfCustomAtt;
    void updateHpfCustomEnablement();

    // The big interactive Pro-Q-style graph (analyzer + draggable band points).
    std::unique_ptr<EQGraphComponent> eqGraph;
    int selectedEqBand = -1;

    // Compact per-band strip for the currently selected band: enable, type,
    // and numeric Freq/Gain/Q (the graph drag is the primary interaction,
    // these give precise numeric control + a way to see/edit Q, which isn't
    // easily graph-draggable).
    struct BandRowControls
    {
        juce::ToggleButton onButton { "" };
        juce::ComboBox      typeBox;
        juce::Slider         freqSlider, gainSlider, qSlider;
        juce::Label           freqLabel, gainLabel, qLabel, indexLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   freqAtt, gainAtt, qAtt;
    };
    std::array<std::unique_ptr<BandRowControls>, EQProcessor::numBands> bandRows;
    void buildBandRow(int index);
    void layoutBandRow(int index, juce::Rectangle<int> area);

    //--- De-Esser Section ---
    juce::Slider deEsserThreshSlider, deEsserFreqSlider;
    juce::Label  deEsserThreshLabel, deEsserFreqLabel;
    juce::ToggleButton deEsserButton { "De-Esser Active" };
    juce::ToggleButton deEsserWideButton { "Wide Band" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> deEsserThreshAtt, deEsserFreqAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deEsserOnAtt, deEsserWideAtt;

    //--- Compressor Section ---
    juce::Slider threshSlider, ratioSlider, kneeSlider, attackSlider, releaseSlider, makeupSlider;
    juce::Label  threshLabel, ratioLabel, kneeLabel, attackLabel, releaseLabel, makeupLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        threshAtt, ratioAtt, kneeAtt, attackAtt, releaseAtt, makeupAtt;

    //--- Loudness / LUFS Section ---
    juce::ComboBox lufsPresetBox;
    juce::Label    lufsPresetLabel;
    juce::Slider   lufsCustomSlider;
    juce::Label    lufsCustomLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lufsPresetAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   lufsCustomAtt;
    void updateLufsCustomEnablement();

    //--- Fade Section ---
    juce::ToggleButton fadeEnableButton { "Fade Enable" };
    juce::ComboBox      fadeInPresetBox, fadeOutPresetBox;
    juce::Label          fadeInLabel, fadeOutLabel;
    juce::Slider         fadeInSlider, fadeOutSlider;
    juce::Label          fadeInCustomLabel, fadeOutCustomLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   fadeEnableAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fadeInPresetAtt, fadeOutPresetAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   fadeInAtt, fadeOutAtt;
    void updateFadeCustomEnablement();

    //--- De-Esser Lookahead Slider (in De-Esser tab) ---
    juce::Slider deEsserLookaheadSlider;
    juce::Label  deEsserLookaheadLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> deEsserLookaheadAtt;

    //--- Meter sidebar (always visible) ---
    juce::Rectangle<int> grMeterBounds, momentBounds, integrBounds, normGainBounds;
    float displayGR       = 0.0f;
    float displayMoment   = -120.0f;
    float displayIntegr   = -120.0f;
    float displayPeak     = -120.0f;
    float displayNormGain = 0.0f;

    void drawMeter (juce::Graphics& g, juce::Rectangle<int> bounds,
                    float valueLufs, float minLufs, float maxLufs,
                    juce::String label, bool showTarget = false, float targetLufs = -14.0f);

    void drawGRMeter (juce::Graphics& g, juce::Rectangle<int> bounds, float grDb);
    void drawNormGain(juce::Graphics& g, juce::Rectangle<int> bounds, float gainDb);

    float currentLufsTarget() const;
    static constexpr int getMeterSidebarWidth() { return 220; }

    void styleSlider (juce::Slider& s, juce::Label& l, const juce::String& name);
    void setModuleControlsVisible();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScalaPodcasterAudioProcessorEditor)
};
