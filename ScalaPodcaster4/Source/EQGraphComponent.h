#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/EQProcessor.h"

/**
 * EQGraphComponent
 * -----------------
 * Pro-Q-style interactive EQ display:
 *   - Frequency/dB grid with log-frequency axis
 *   - Live pre/post FFT spectrum (filled curves)
 *   - Combined EQ response curve (HPF + all enabled bands)
 *   - One draggable circular handle per enabled band: drag changes
 *     frequency (x) + gain (y); mouse wheel changes Q.
 *   - A fixed marker/line for the HPF cutoff (not draggable here — the HPF
 *     frequency is controlled via the preset dropdown + custom slider
 *     elsewhere in the EQ tab, since it's a fixed first-stage modulator,
 *     not one of the 6 free bands).
 *
 * Frequency axis: logarithmic, 20 Hz - 20 kHz.
 * Gain axis: linear, -18 dB to +18 dB (matches band gain parameter range).
 */
class EQGraphComponent : public juce::Component, private juce::Timer
{
public:
    EQGraphComponent(EQProcessor& eqProcessorIn, juce::AudioProcessorValueTreeState& apvtsIn);
    ~EQGraphComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    // Called by the parent tab when the user selects a band row elsewhere
    // (e.g. a band list), so the graph can highlight the matching handle.
    void setExternallySelectedBand(int bandIndex);
    std::function<void(int)> onBandSelected;

    // Colour-coded per band so the handle, curve-highlight, and the
    // PluginEditor's band-row controls all visually match.
    static const std::array<juce::Colour, EQProcessor::numBands> bandColours;

private:
    void timerCallback() override;

    // --- Coordinate mapping ---
    float freqToX(float freqHz) const;
    float xToFreq(float x) const;
    float gainToY(float gainDb) const;
    float yToGain(float y) const;

    juce::Rectangle<float> getGraphBounds() const;
    int findHandleAt(juce::Point<float> pos) const;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds, SpectrumAnalyzer& analyzer,
                       juce::Colour colour, float alpha);
    void drawResponseCurve(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawHandles(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawHpfMarker(juce::Graphics& g, juce::Rectangle<float> bounds);

    EQProcessor& eq;
    juce::AudioProcessorValueTreeState& apvts;

    int hoveredBand  = -1;
    int draggedBand  = -1;
    int selectedBand = -1;

    juce::Point<float> dragStartPos;
    float dragStartFreq = 0.0f;
    float dragStartGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQGraphComponent)
};
