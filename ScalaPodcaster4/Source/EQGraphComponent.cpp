#include "EQGraphComponent.h"

const std::array<juce::Colour, EQProcessor::numBands> EQGraphComponent::bandColours = {
    juce::Colour(0xFFF2A03D), // amber (matches plugin accent)
    juce::Colour(0xFF4ECDC4), // teal
    juce::Colour(0xFFE0544A), // red
    juce::Colour(0xFF8C7AE6), // violet
    juce::Colour(0xFF6FCF97), // green
    juce::Colour(0xFFF7D154)  // yellow
};

//==============================================================================
EQGraphComponent::EQGraphComponent(EQProcessor& eqProcessorIn, juce::AudioProcessorValueTreeState& apvtsIn)
    : eq(eqProcessorIn), apvts(apvtsIn)
{
    setWantsKeyboardFocus(false);
    startTimerHz(30);
}

EQGraphComponent::~EQGraphComponent()
{
    stopTimer();
}

void EQGraphComponent::timerCallback()
{
    bool changed = false;
    changed |= eq.preAnalyzer.updateIfReady();
    changed |= eq.postAnalyzer.updateIfReady();
    if (changed || draggedBand >= 0)
        repaint();
}

//==============================================================================
// --- Coordinate mapping: log-frequency 20Hz-20kHz horizontally, linear
//     gain -18dB..+18dB vertically (matches the band gain parameter range).
static constexpr float kMinFreq = 20.0f;
static constexpr float kMaxFreq = 20000.0f;
static constexpr float kMinGainDb = -18.0f;
static constexpr float kMaxGainDb = 18.0f;

juce::Rectangle<float> EQGraphComponent::getGraphBounds() const
{
    return getLocalBounds().toFloat().reduced(4.0f);
}

float EQGraphComponent::freqToX(float freqHz) const
{
    auto b = getGraphBounds();
    float logMin = std::log10(kMinFreq);
    float logMax = std::log10(kMaxFreq);
    float logF   = std::log10(juce::jlimit(kMinFreq, kMaxFreq, freqHz));
    return b.getX() + (logF - logMin) / (logMax - logMin) * b.getWidth();
}

float EQGraphComponent::xToFreq(float x) const
{
    auto b = getGraphBounds();
    float logMin = std::log10(kMinFreq);
    float logMax = std::log10(kMaxFreq);
    float t = juce::jlimit(0.0f, 1.0f, (x - b.getX()) / b.getWidth());
    return std::pow(10.0f, logMin + t * (logMax - logMin));
}

float EQGraphComponent::gainToY(float gainDb) const
{
    auto b = getGraphBounds();
    float t = (gainDb - kMinGainDb) / (kMaxGainDb - kMinGainDb);
    return b.getBottom() - t * b.getHeight();
}

float EQGraphComponent::yToGain(float y) const
{
    auto b = getGraphBounds();
    float t = juce::jlimit(0.0f, 1.0f, (b.getBottom() - y) / b.getHeight());
    return kMinGainDb + t * (kMaxGainDb - kMinGainDb);
}

//==============================================================================
void EQGraphComponent::resized()
{
    repaint();
}

void EQGraphComponent::paint(juce::Graphics& g)
{
    auto bounds = getGraphBounds();

    g.setColour(juce::Colour(0xFF09090D));
    g.fillRoundedRectangle(bounds, 6.0f);

    drawGrid(g, bounds);
    drawSpectrum(g, bounds, eq.preAnalyzer,  juce::Colour(0xFF5A5D6A), 0.35f);
    drawSpectrum(g, bounds, eq.postAnalyzer, juce::Colour(0xFFF2A03D), 0.45f);
    drawHpfMarker(g, bounds);
    drawResponseCurve(g, bounds);
    drawHandles(g, bounds);

    g.setColour(juce::Colour(0xFF3A3D4A));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

//==============================================================================
void EQGraphComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());

    juce::Colour gridCol = juce::Colour(0xFF22242F);
    g.setColour(gridCol);

    // Vertical lines at standard frequency reference points
    const float freqLines[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    g.setFont(juce::Font("Courier New", 9.0f, juce::Font::plain));
    for (float f : freqLines)
    {
        float x = freqToX(f);
        g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());

        juce::String label = (f >= 1000.0f) ? juce::String(f / 1000.0f, (f == 1000.0f ? 0 : 1)) + "k"
                                              : juce::String((int)f);
        g.setColour(juce::Colour(0xFF6A6D7A));
        g.drawText(label, juce::Rectangle<float>(x - 14, bounds.getBottom() - 13, 28, 12),
                   juce::Justification::centred);
        g.setColour(gridCol);
    }

    // Horizontal lines at gain reference points
    const float gainLines[] = { -18, -12, -6, 0, 6, 12, 18 };
    for (float gdb : gainLines)
    {
        float y = gainToY(gdb);
        g.setColour(gdb == 0.0f ? juce::Colour(0xFF3A3D4A) : gridCol);
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());

        g.setColour(juce::Colour(0xFF6A6D7A));
        g.setFont(juce::Font("Courier New", 9.0f, juce::Font::plain));
        g.drawText(juce::String((int)gdb), juce::Rectangle<float>(bounds.getX() + 2, y - 12, 26, 12),
                   juce::Justification::left);
    }

    g.restoreState();
}

void EQGraphComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     SpectrumAnalyzer& analyzer, juce::Colour colour, float alpha)
{
    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());

    juce::Path path;
    bool started = false;

    const int numPoints = 200;
    for (int i = 0; i <= numPoints; ++i)
    {
        float t    = (float)i / (float)numPoints;
        float freq = std::pow(10.0f, std::log10(kMinFreq) + t * (std::log10(kMaxFreq) - std::log10(kMinFreq)));
        float magDb = analyzer.getMagnitudeAtFrequency(freq);

        // Spectrum magnitudes are absolute dBFS-ish (not the +-18dB EQ gain
        // scale), so remap a sensible visual range (-90..0 dB) onto the
        // bottom ~80% of the graph height purely for the filled curve.
        float normalised = juce::jlimit(0.0f, 1.0f, (magDb + 90.0f) / 90.0f);
        float y = bounds.getBottom() - normalised * bounds.getHeight() * 0.85f;
        float x = freqToX(freq);

        if (!started) { path.startNewSubPath(x, y); started = true; }
        else           path.lineTo(x, y);
    }

    juce::Path fillPath = path;
    fillPath.lineTo(bounds.getRight(), bounds.getBottom());
    fillPath.lineTo(bounds.getX(), bounds.getBottom());
    fillPath.closeSubPath();

    g.setColour(colour.withAlpha(alpha * 0.5f));
    g.fillPath(fillPath);

    g.setColour(colour.withAlpha(alpha + 0.25f));
    g.strokePath(path, juce::PathStrokeType(1.4f));

    g.restoreState();
}

void EQGraphComponent::drawResponseCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());

    juce::Path path;
    bool started = false;

    const int numPoints = 300;
    for (int i = 0; i <= numPoints; ++i)
    {
        float t    = (float)i / (float)numPoints;
        float freq = std::pow(10.0f, std::log10(kMinFreq) + t * (std::log10(kMaxFreq) - std::log10(kMinFreq)));
        float responseDb = eq.getResponseAtFrequency(freq);
        float x = freqToX(freq);
        float y = gainToY(juce::jlimit(kMinGainDb, kMaxGainDb, responseDb));

        if (!started) { path.startNewSubPath(x, y); started = true; }
        else           path.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xFFEDEDF2));
    g.strokePath(path, juce::PathStrokeType(2.2f));

    g.restoreState();
}

void EQGraphComponent::drawHpfMarker(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!eq.hpfEnabled) return;

    float x = freqToX(eq.hpfFrequency);
    g.setColour(juce::Colour(0xFF8A8D99).withAlpha(0.6f));
    juce::Path dashed;
    dashed.startNewSubPath(x, bounds.getY());
    dashed.lineTo(x, bounds.getBottom());
    float dashLengths[] = { 4.0f, 3.0f };
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, dashed, dashLengths, 2);
    g.fillPath(dashed);

    g.setFont(juce::Font("Courier New", 9.0f, juce::Font::bold));
    g.drawText("HPF", juce::Rectangle<float>(x + 3, bounds.getY() + 2, 30, 12), juce::Justification::left);
}

//==============================================================================
void EQGraphComponent::drawHandles(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ignoreUnused(bounds);

    for (int i = 0; i < EQProcessor::numBands; ++i)
    {
        auto& band = eq.bands[(size_t)i];
        if (!band.enabled) continue;

        float x = freqToX(band.freqHz);
        float y = gainToY(juce::jlimit(kMinGainDb, kMaxGainDb, band.gainDb));

        juce::Colour col = bandColours[(size_t)i];
        bool isHovered  = (hoveredBand == i);
        bool isDragged  = (draggedBand == i);
        bool isSelected = (selectedBand == i);

        float radius = (isDragged || isHovered) ? 8.0f : 6.5f;

        if (isSelected || isDragged)
        {
            g.setColour(col.withAlpha(0.18f));
            g.fillEllipse(x - radius - 5.0f, y - radius - 5.0f, (radius + 5.0f) * 2.0f, (radius + 5.0f) * 2.0f);
        }

        g.setColour(juce::Colour(0xFF09090D));
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(col);
        g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        g.setFont(juce::Font("Courier New", 9.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1), juce::Rectangle<float>(x - radius, y - radius, radius * 2.0f, radius * 2.0f),
                   juce::Justification::centred);
    }
}

//==============================================================================
int EQGraphComponent::findHandleAt(juce::Point<float> pos) const
{
    int   closest = -1;
    float closestDist = 16.0f; // px hit-test radius

    for (int i = 0; i < EQProcessor::numBands; ++i)
    {
        auto& band = eq.bands[(size_t)i];
        if (!band.enabled) continue;

        float x = freqToX(band.freqHz);
        float y = gainToY(juce::jlimit(kMinGainDb, kMaxGainDb, band.gainDb));
        float dist = pos.getDistanceFrom(juce::Point<float>(x, y));
        if (dist < closestDist)
        {
            closestDist = dist;
            closest = i;
        }
    }
    return closest;
}

//==============================================================================
void EQGraphComponent::mouseMove(const juce::MouseEvent& e)
{
    int newHover = findHandleAt(e.position);
    if (newHover != hoveredBand)
    {
        hoveredBand = newHover;
        setMouseCursor(hoveredBand >= 0 ? juce::MouseCursor::PointingHandCursor
                                         : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EQGraphComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoveredBand != -1) { hoveredBand = -1; repaint(); }
}

void EQGraphComponent::mouseDown(const juce::MouseEvent& e)
{
    int hit = findHandleAt(e.position);
    if (hit >= 0)
    {
        draggedBand  = hit;
        selectedBand = hit;
        dragStartPos  = e.position;
        dragStartFreq = eq.bands[(size_t)hit].freqHz;
        dragStartGain = eq.bands[(size_t)hit].gainDb;

        if (onBandSelected) onBandSelected(hit);
        repaint();
        return;
    }

    // Clicked empty space: find the nearest disabled band slot and activate
    // it at the clicked frequency/gain, so the graph itself can "add" a band
    // the way Pro-Q lets you click anywhere to create a new point.
    for (int i = 0; i < EQProcessor::numBands; ++i)
    {
        if (!eq.bands[(size_t)i].enabled)
        {
            float freq = xToFreq(e.position.x);
            float gain = yToGain(e.position.y);

            auto setParam = [&](const juce::String& paramId, float value)
            {
                if (auto* p = apvts.getParameter(paramId))
                    p->setValueNotifyingHost(p->convertTo0to1(value));
            };

            juce::String n = juce::String(i + 1);
            setParam("band" + n + "Freq", freq);
            setParam("band" + n + "Gain", gain);
            if (auto* onParam = apvts.getParameter("band" + n + "On"))
                onParam->setValueNotifyingHost(1.0f);

            draggedBand  = i;
            selectedBand = i;
            dragStartPos  = e.position;
            dragStartFreq = freq;
            dragStartGain = gain;

            if (onBandSelected) onBandSelected(i);
            repaint();
            return;
        }
    }
    // All 6 bands already in use — nothing to do.
}

void EQGraphComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedBand < 0) return;

    float newFreq = xToFreq(e.position.x);
    float newGain = yToGain(e.position.y);

    juce::String n = juce::String(draggedBand + 1);

    if (auto* freqParam = apvts.getParameter("band" + n + "Freq"))
        freqParam->setValueNotifyingHost(freqParam->convertTo0to1(newFreq));
    if (auto* gainParam = apvts.getParameter("band" + n + "Gain"))
        gainParam->setValueNotifyingHost(gainParam->convertTo0to1(newGain));

    repaint();
}

void EQGraphComponent::mouseUp(const juce::MouseEvent&)
{
    draggedBand = -1;
    repaint();
}

void EQGraphComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int hit = findHandleAt(e.position);
    if (hit < 0) hit = selectedBand;
    if (hit < 0) return;

    juce::String n = juce::String(hit + 1);
    if (auto* qParam = apvts.getParameter("band" + n + "Q"))
    {
        float currentQ = eq.bands[(size_t)hit].q;
        float newQ = juce::jlimit(0.1f, 18.0f, currentQ * (1.0f + wheel.deltaY * 0.6f));
        qParam->setValueNotifyingHost(qParam->convertTo0to1(newQ));
        repaint();
    }
}

void EQGraphComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    int hit = findHandleAt(e.position);
    if (hit < 0) return;

    // Double-click toggles the band off (quick disable without leaving the graph).
    juce::String n = juce::String(hit + 1);
    if (auto* onParam = apvts.getParameter("band" + n + "On"))
        onParam->setValueNotifyingHost(0.0f);

    if (selectedBand == hit) selectedBand = -1;
    repaint();
}

void EQGraphComponent::setExternallySelectedBand(int bandIndex)
{
    selectedBand = bandIndex;
    repaint();
}
