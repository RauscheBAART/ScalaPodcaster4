#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::ModuleTabButton::paint (juce::Graphics& g)
{
    auto* editor = findParentComponentOfClass<ScalaPodcasterAudioProcessorEditor>();
    auto bounds  = getLocalBounds().toFloat().reduced(2.0f);

    juce::Colour panelCol  = editor != nullptr ? editor->bgPanel   : juce::Colour(0xFF1A1C24);
    juce::Colour accentCol = editor != nullptr ? editor->accent    : juce::Colour(0xFFF2A03D);
    juce::Colour textCol   = editor != nullptr ? editor->text      : juce::Colours::white;
    juce::Colour dimCol    = editor != nullptr ? editor->textDim   : juce::Colours::grey;
    juce::Colour strokeCol = editor != nullptr ? editor->strokeCol : juce::Colours::darkgrey;

    g.setColour(panelCol);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(active ? accentCol : strokeCol);
    g.drawRoundedRectangle(bounds, 6.0f, active ? 1.6f : 1.0f);

    // Status dot
    auto dotBounds = juce::Rectangle<float>(bounds.getX() + 10.0f, bounds.getY() + 9.0f, 7.0f, 7.0f);
    g.setColour(active ? accentCol : dimCol);
    g.fillEllipse(dotBounds);

    g.setFont(juce::Font("Courier New", 12.0f, juce::Font::bold));
    g.setColour(active ? accentCol : textCol.withAlpha(0.85f));
    g.drawText(name, bounds.reduced(22.0f, 0.0f).withTrimmedTop(2.0f),
               juce::Justification::centredLeft);
}

//==============================================================================
ScalaPodcasterAudioProcessorEditor::ScalaPodcasterAudioProcessorEditor
    (ScalaPodcasterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    logoFullImage = juce::ImageCache::getFromMemory(
        BinaryData::scala_logo_full_transparent_png,
        BinaryData::scala_logo_full_transparent_pngSize);

    //--- Module tabs (5 now: + Fade) ---
    eqTab.name = "EQ";                eqTab.id = ModuleTab::EQ;
    deEsserTab.name = "DE-ESSER";     deEsserTab.id = ModuleTab::DeEsser;
    compressorTab.name = "COMPRESSOR"; compressorTab.id = ModuleTab::Compressor;
    loudnessTab.name = "LOUDNESS";    loudnessTab.id = ModuleTab::Loudness;
    fadeTab.name = "FADE";            fadeTab.id = ModuleTab::Fade;
    allTabs = { &eqTab, &deEsserTab, &compressorTab, &loudnessTab, &fadeTab };

    for (auto* tab : allTabs)
    {
        addAndMakeVisible(tab);
        tab->onClicked = [this, tab] { selectTab(tab->id); };
    }

    //--- Preset Box (header area, always visible) ---
    presetBox.setColour(juce::ComboBox::backgroundColourId, bgDarkest);
    presetBox.setColour(juce::ComboBox::textColourId,       text);
    presetBox.setColour(juce::ComboBox::outlineColourId,    accent.withAlpha(0.4f));
    presetBox.setColour(juce::ComboBox::arrowColourId,      accent);
    addAndMakeVisible(presetBox);
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0) audioProcessor.presetManager->loadPreset(idx);
    };

    auto styleBtn = [&](juce::TextButton& b) {
        b.setColour(juce::TextButton::buttonColourId,   bgPanel);
        b.setColour(juce::TextButton::buttonOnColourId, accent.withAlpha(0.6f));
        b.setColour(juce::TextButton::textColourOffId,  text);
        addAndMakeVisible(b);
    };
    styleBtn(presetSaveBtn);
    styleBtn(presetDeleteBtn);

    presetSaveBtn.onClick = [this]
    {
        auto* aw = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", "My Preset", "Name:");
        aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int result)
        {
            if (result == 1)
            {
                juce::String name = aw->getTextEditorContents("name");
                if (name.isNotEmpty())
                {
                    audioProcessor.presetManager->saveCurrentAsUserPreset(name);
                    refreshPresetBox();
                }
            }
            delete aw;
        }), true);
    };
    presetDeleteBtn.onClick = [this]
    {
        int idx = presetBox.getSelectedItemIndex();
        auto& presets = audioProcessor.presetManager->getAllPresets();
        if (idx >= 0 && idx < (int)presets.size() && !presets[(size_t)idx].isFactory)
        {
            audioProcessor.presetManager->deleteUserPreset(idx);
            refreshPresetBox();
        }
    };
    audioProcessor.presetManager->onPresetListChanged = [this] { refreshPresetBox(); };

    //--- EQ Section ---
    hpfOnButton.setColour(juce::ToggleButton::textColourId, text);
    hpfOnButton.setColour(juce::ToggleButton::tickColourId, accent);
    hpfOnButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(hpfOnButton);
    hpfOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "hpfOn", hpfOnButton);

    hpfPresetBox.addItem("Gentle (40 Hz) - sub-rumble only",      1);
    hpfPresetBox.addItem("Voice Male (80 Hz)",                    2);
    hpfPresetBox.addItem("Voice Female (100 Hz)",                 3);
    hpfPresetBox.addItem("Podcast Standard (90 Hz)",               4);
    hpfPresetBox.addItem("Aggressive (150 Hz) - handling noise",  5);
    hpfPresetBox.addItem("Custom",                                 6);
    hpfPresetBox.setColour(juce::ComboBox::backgroundColourId, bgDarkest);
    hpfPresetBox.setColour(juce::ComboBox::textColourId,       text);
    hpfPresetBox.setColour(juce::ComboBox::outlineColourId,    accent.withAlpha(0.4f));
    hpfPresetBox.setColour(juce::ComboBox::arrowColourId,      accent);
    addAndMakeVisible(hpfPresetBox);
    hpfPresetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "hpfPreset", hpfPresetBox);

    styleSlider(hpfCustomSlider, hpfCustomLabel, "HPF Custom Hz");
    hpfCustomAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "hpfFreqCustom", hpfCustomSlider);
    hpfPresetBox.onChange = [this] { updateHpfCustomEnablement(); };
    updateHpfCustomEnablement();

    eqGraph = std::make_unique<EQGraphComponent>(audioProcessor.getEqProcessor(), audioProcessor.apvts);
    addAndMakeVisible(*eqGraph);
    eqGraph->onBandSelected = [this](int idx)
    {
        selectedEqBand = idx;
        for (int i = 0; i < EQProcessor::numBands; ++i)
            if (bandRows[(size_t)i] != nullptr)
                bandRows[(size_t)i]->indexLabel.setColour(juce::Label::textColourId,
                    (i == idx) ? accent : textDim);
        resized();
    };

    for (int i = 0; i < EQProcessor::numBands; ++i)
        buildBandRow(i);

    //--- De-Esser Section ---
    styleSlider(deEsserThreshSlider, deEsserThreshLabel, "De-Ess Thresh dB");
    deEsserThreshAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "deEsserThresh", deEsserThreshSlider);

    styleSlider(deEsserFreqSlider, deEsserFreqLabel, "De-Ess Freq Hz");
    deEsserFreqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "deEsserFreq", deEsserFreqSlider);

    deEsserButton.setColour(juce::ToggleButton::textColourId, text);
    deEsserButton.setColour(juce::ToggleButton::tickColourId, accent);
    deEsserButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(deEsserButton);
    deEsserOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "deEsserOn", deEsserButton);

    deEsserWideButton.setColour(juce::ToggleButton::textColourId, text);
    deEsserWideButton.setColour(juce::ToggleButton::tickColourId, accent);
    deEsserWideButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(deEsserWideButton);
    deEsserWideAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "deEsserWide", deEsserWideButton);

    styleSlider(deEsserLookaheadSlider, deEsserLookaheadLabel, "Lookahead ms");
    deEsserLookaheadAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "deEsserLookahead", deEsserLookaheadSlider);

    //--- Fade Section ---
    fadeEnableButton.setColour(juce::ToggleButton::textColourId, text);
    fadeEnableButton.setColour(juce::ToggleButton::tickColourId, accent);
    fadeEnableButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(fadeEnableButton);
    fadeEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "fadeEnable", fadeEnableButton);

    auto buildFadePresetBox = [&](juce::ComboBox& box, bool isIn)
    {
        if (isIn) {
            box.addItem("0.0s (off)", 1); box.addItem("0.3s", 2); box.addItem("0.5s", 3);
            box.addItem("1.0s", 4); box.addItem("2.0s", 5); box.addItem("Custom", 6);
        } else {
            box.addItem("0.0s (off)", 1); box.addItem("0.5s", 2); box.addItem("1.0s", 3);
            box.addItem("2.0s", 4); box.addItem("3.0s", 5); box.addItem("Custom", 6);
        }
        box.setColour(juce::ComboBox::backgroundColourId, bgDarkest);
        box.setColour(juce::ComboBox::textColourId,       text);
        box.setColour(juce::ComboBox::outlineColourId,    accent.withAlpha(0.4f));
        box.setColour(juce::ComboBox::arrowColourId,      accent);
        addAndMakeVisible(box);
    };

    buildFadePresetBox(fadeInPresetBox,  true);
    buildFadePresetBox(fadeOutPresetBox, false);

    fadeInPresetAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "fadeInPreset",  fadeInPresetBox);
    fadeOutPresetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "fadeOutPreset", fadeOutPresetBox);

    fadeInLabel.setText("Fade In", juce::dontSendNotification);
    fadeInLabel.setFont(juce::Font("Courier New", 11.0f, juce::Font::plain));
    fadeInLabel.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(fadeInLabel);

    fadeOutLabel.setText("Fade Out", juce::dontSendNotification);
    fadeOutLabel.setFont(juce::Font("Courier New", 11.0f, juce::Font::plain));
    fadeOutLabel.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(fadeOutLabel);

    styleSlider(fadeInSlider,  fadeInCustomLabel,  "Custom In s");
    styleSlider(fadeOutSlider, fadeOutCustomLabel, "Custom Out s");

    fadeInAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "fadeIn",  fadeInSlider);
    fadeOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "fadeOut", fadeOutSlider);

    fadeInPresetBox.onChange  = [this] { updateFadeCustomEnablement(); };
    fadeOutPresetBox.onChange = [this] { updateFadeCustomEnablement(); };
    updateFadeCustomEnablement();

    //--- Compressor Section ---
    styleSlider(threshSlider,  threshLabel,  "Threshold dB");
    styleSlider(ratioSlider,   ratioLabel,   "Ratio");
    styleSlider(kneeSlider,    kneeLabel,    "Knee dB");
    styleSlider(attackSlider,  attackLabel,  "Attack ms");
    styleSlider(releaseSlider, releaseLabel, "Release ms");
    styleSlider(makeupSlider,  makeupLabel,  "Makeup dB");

    threshAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "threshold", threshSlider);
    ratioAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "ratio",     ratioSlider);
    kneeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "knee",      kneeSlider);
    attackAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "attack",    attackSlider);
    releaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "release",   releaseSlider);
    makeupAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "makeup",    makeupSlider);

    //--- Loudness / LUFS Section ---
    lufsPresetBox.addItem("-14 LUFS  (Spotify / YouTube)",       1);
    lufsPresetBox.addItem("-16 LUFS  (Apple Podcasts)",          2);
    lufsPresetBox.addItem("-23 LUFS  (EBU R128 Broadcast)",      3);
    lufsPresetBox.addItem("-24 LUFS  (ATSC A/85 US Broadcast)",  4);
    lufsPresetBox.addItem("Custom",                              5);
    lufsPresetBox.setColour(juce::ComboBox::backgroundColourId, bgDarkest);
    lufsPresetBox.setColour(juce::ComboBox::textColourId,       text);
    lufsPresetBox.setColour(juce::ComboBox::outlineColourId,    accent.withAlpha(0.4f));
    lufsPresetBox.setColour(juce::ComboBox::arrowColourId,      accent);
    addAndMakeVisible(lufsPresetBox);

    lufsPresetLabel.setText("Loudness Target", juce::dontSendNotification);
    lufsPresetLabel.setFont(juce::Font("Courier New", 11.0f, juce::Font::plain));
    lufsPresetLabel.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(lufsPresetLabel);

    lufsPresetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "lufsPreset", lufsPresetBox);

    styleSlider(lufsCustomSlider, lufsCustomLabel, "Custom Target LUFS");
    lufsCustomAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "lufsCustom", lufsCustomSlider);

    lufsPresetBox.onChange = [this] { updateLufsCustomEnablement(); };
    updateLufsCustomEnablement();

    //--- Bypass (always visible, header area) ---
    bypassButton.setColour(juce::ToggleButton::textColourId,         text);
    bypassButton.setColour(juce::ToggleButton::tickColourId,         warn);
    bypassButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(bypassButton);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bypass", bypassButton);

    selectTab(ModuleTab::EQ);

    // Set the size last, once every child component (including allTabs)
    // is fully constructed — setSize() triggers an immediate resized() call,
    // which would dereference null tab pointers if done any earlier.
    setResizable(true, true);
    setResizeLimits(860, 560, 1500, 950);
    setSize (980, 640);

    startTimerHz(30);
}

ScalaPodcasterAudioProcessorEditor::~ScalaPodcasterAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::buildBandRow(int index)
{
    auto row = std::make_unique<BandRowControls>();
    juce::String n = juce::String(index + 1);
    juce::Colour bandCol = EQGraphComponent::bandColours[(size_t)index];

    row->indexLabel.setText(n, juce::dontSendNotification);
    row->indexLabel.setFont(juce::Font("Courier New", 12.0f, juce::Font::bold));
    row->indexLabel.setColour(juce::Label::textColourId, (index == 0) ? accent : textDim);
    row->indexLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(row->indexLabel);

    row->onButton.setColour(juce::ToggleButton::tickColourId, bandCol);
    row->onButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(row->onButton);
    row->onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "band" + n + "On", row->onButton);

    row->typeBox.addItem("Bell",       1);
    row->typeBox.addItem("Low Shelf",  2);
    row->typeBox.addItem("High Shelf", 3);
    row->typeBox.addItem("Notch",      4);
    row->typeBox.setColour(juce::ComboBox::backgroundColourId, bgDarkest);
    row->typeBox.setColour(juce::ComboBox::textColourId,       text);
    row->typeBox.setColour(juce::ComboBox::outlineColourId,    bandCol.withAlpha(0.4f));
    row->typeBox.setColour(juce::ComboBox::arrowColourId,      bandCol);
    addAndMakeVisible(row->typeBox);
    row->typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "band" + n + "Type", row->typeBox);

    auto styleCompact = [&](juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 18);
        s.setColour(juce::Slider::thumbColourId,          bandCol);
        s.setColour(juce::Slider::trackColourId,          bandCol.withAlpha(0.4f));
        s.setColour(juce::Slider::backgroundColourId,     bgDarkest);
        s.setColour(juce::Slider::textBoxTextColourId,    text);
        s.setColour(juce::Slider::textBoxBackgroundColourId, bgDarkest);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);

        l.setText(name, juce::dontSendNotification);
        l.setFont(juce::Font("Courier New", 9.0f, juce::Font::plain));
        l.setColour(juce::Label::textColourId, textDim);
        addAndMakeVisible(l);
    };

    styleCompact(row->freqSlider, row->freqLabel, "Freq");
    styleCompact(row->gainSlider, row->gainLabel, "Gain");
    styleCompact(row->qSlider,    row->qLabel,    "Q");

    row->freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "band" + n + "Freq", row->freqSlider);
    row->gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "band" + n + "Gain", row->gainSlider);
    row->qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "band" + n + "Q", row->qSlider);

    bandRows[(size_t)index] = std::move(row);
}

void ScalaPodcasterAudioProcessorEditor::layoutBandRow(int index, juce::Rectangle<int> area)
{
    auto& row = *bandRows[(size_t)index];

    auto indexArea = area.removeFromLeft(20);
    row.indexLabel.setBounds(indexArea);

    auto onArea = area.removeFromLeft(28);
    row.onButton.setBounds(onArea);

    auto typeArea = area.removeFromLeft(98).reduced(2, 4);
    row.typeBox.setBounds(typeArea);

    int sliderW = (area.getWidth() - 16) / 3;

    auto freqArea = area.removeFromLeft(sliderW).reduced(4, 2);
    row.freqLabel.setBounds(freqArea.removeFromTop(11));
    row.freqSlider.setBounds(freqArea);

    auto gainArea = area.removeFromLeft(sliderW).reduced(4, 2);
    row.gainLabel.setBounds(gainArea.removeFromTop(11));
    row.gainSlider.setBounds(gainArea);

    auto qArea = area.reduced(4, 2);
    row.qLabel.setBounds(qArea.removeFromTop(11));
    row.qSlider.setBounds(qArea);
}

//==============================================================================
#if SCALA_BUILD_EDITOR_SMOKE_TEST
void ScalaPodcasterAudioProcessorEditor::debugSelectTabForScreenshot (int tabIndex)
{
    const ModuleTab tabs[] = { ModuleTab::EQ, ModuleTab::DeEsser, ModuleTab::Compressor,
                                ModuleTab::Loudness, ModuleTab::Fade };
    if (tabIndex >= 0 && tabIndex < 5)
        selectTab(tabs[tabIndex]);
}
#endif

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::selectTab (ModuleTab newTab)
{
    activeTab = newTab;
    for (auto* tab : allTabs)
        tab->active = (tab->id == newTab);
    setModuleControlsVisible();
    resized();
    repaint();
}

void ScalaPodcasterAudioProcessorEditor::setModuleControlsVisible()
{
    bool isEq         = (activeTab == ModuleTab::EQ);
    bool isDeEsser    = (activeTab == ModuleTab::DeEsser);
    bool isCompressor = (activeTab == ModuleTab::Compressor);
    bool isLoudness   = (activeTab == ModuleTab::Loudness);
    bool isFade       = (activeTab == ModuleTab::Fade);

    hpfOnButton.setVisible(isEq);
    hpfPresetBox.setVisible(isEq);
    hpfCustomSlider.setVisible(isEq);
    hpfCustomLabel.setVisible(isEq);
    eqGraph->setVisible(isEq);
    for (auto& row : bandRows)
    {
        if (row == nullptr) continue;
        row->indexLabel.setVisible(isEq);
        row->onButton.setVisible(isEq);
        row->typeBox.setVisible(isEq);
        row->freqSlider.setVisible(isEq);   row->freqLabel.setVisible(isEq);
        row->gainSlider.setVisible(isEq);   row->gainLabel.setVisible(isEq);
        row->qSlider.setVisible(isEq);      row->qLabel.setVisible(isEq);
    }

    deEsserThreshSlider.setVisible(isDeEsser);
    deEsserThreshLabel.setVisible(isDeEsser);
    deEsserFreqSlider.setVisible(isDeEsser);
    deEsserFreqLabel.setVisible(isDeEsser);
    deEsserButton.setVisible(isDeEsser);
    deEsserWideButton.setVisible(isDeEsser);
    deEsserLookaheadSlider.setVisible(isDeEsser);
    deEsserLookaheadLabel.setVisible(isDeEsser);

    fadeEnableButton.setVisible(isFade);
    fadeInLabel.setVisible(isFade);      fadeInPresetBox.setVisible(isFade);
    fadeInCustomLabel.setVisible(isFade); fadeInSlider.setVisible(isFade);
    fadeOutLabel.setVisible(isFade);     fadeOutPresetBox.setVisible(isFade);
    fadeOutCustomLabel.setVisible(isFade); fadeOutSlider.setVisible(isFade);

    threshSlider.setVisible(isCompressor);   threshLabel.setVisible(isCompressor);
    ratioSlider.setVisible(isCompressor);    ratioLabel.setVisible(isCompressor);
    kneeSlider.setVisible(isCompressor);     kneeLabel.setVisible(isCompressor);
    attackSlider.setVisible(isCompressor);   attackLabel.setVisible(isCompressor);
    releaseSlider.setVisible(isCompressor);  releaseLabel.setVisible(isCompressor);
    makeupSlider.setVisible(isCompressor);   makeupLabel.setVisible(isCompressor);

    lufsPresetBox.setVisible(isLoudness);    lufsPresetLabel.setVisible(isLoudness);
    lufsCustomSlider.setVisible(isLoudness); lufsCustomLabel.setVisible(isLoudness);
}

void ScalaPodcasterAudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear(juce::dontSendNotification);
    const auto& presets = audioProcessor.presetManager->getAllPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
    {
        juce::String displayName = presets[(size_t)i].isFactory
            ? presets[(size_t)i].name
            : (juce::String(juce::CharPointer_UTF8("\xe2\x98\x86")) + " " + presets[(size_t)i].name);
        presetBox.addItem(displayName, i + 1);
    }
    int cur = audioProcessor.presetManager->getCurrentPresetIndex();
    if (cur >= 0) presetBox.setSelectedItemIndex(cur, juce::dontSendNotification);
}

void ScalaPodcasterAudioProcessorEditor::updateFadeCustomEnablement()
{
    bool inCustom  = (fadeInPresetBox.getSelectedId()  == 6);
    bool outCustom = (fadeOutPresetBox.getSelectedId() == 6);
    fadeInSlider.setEnabled(inCustom);
    fadeInSlider.setAlpha(inCustom ? 1.0f : 0.4f);
    fadeInCustomLabel.setAlpha(inCustom ? 1.0f : 0.4f);
    fadeOutSlider.setEnabled(outCustom);
    fadeOutSlider.setAlpha(outCustom ? 1.0f : 0.4f);
    fadeOutCustomLabel.setAlpha(outCustom ? 1.0f : 0.4f);
}

void ScalaPodcasterAudioProcessorEditor::updateHpfCustomEnablement()
{
    // Preset index 5 (0-based) == "Custom" — combo IDs are 1-based, so ID 6.
    bool isCustom = (hpfPresetBox.getSelectedId() == 6);
    hpfCustomSlider.setEnabled(isCustom);
    hpfCustomSlider.setAlpha(isCustom ? 1.0f : 0.4f);
    hpfCustomLabel.setAlpha(isCustom ? 1.0f : 0.4f);
}

void ScalaPodcasterAudioProcessorEditor::updateLufsCustomEnablement()
{
    // Preset index 4 (0-based) == "Custom" — only then is the free slider usable.
    bool isCustom = (lufsPresetBox.getSelectedId() == 5);
    lufsCustomSlider.setEnabled(isCustom);
    lufsCustomSlider.setAlpha(isCustom ? 1.0f : 0.4f);
    lufsCustomLabel.setAlpha(isCustom ? 1.0f : 0.4f);
}

float ScalaPodcasterAudioProcessorEditor::currentLufsTarget() const
{
    int presetIndex = lufsPresetBox.getSelectedId() - 1; // combo IDs are 1-based
    return ScalaPodcasterAudioProcessor::resolveLufsTarget(presetIndex, (float)lufsCustomSlider.getValue());
}

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::timerCallback()
{
    auto smooth = [](float current, float target, float fast, float slow) {
        float coeff = (target < current) ? fast : slow;
        return current + coeff * (target - current);
    };

    displayGR        = smooth(displayGR,        audioProcessor.gainReductionDb.load(),     0.6f, 0.1f);
    displayMoment    = smooth(displayMoment,    audioProcessor.momentaryLufs.load(),       0.4f, 0.15f);
    displayIntegr    = smooth(displayIntegr,    audioProcessor.integratedLufs.load(),      0.1f, 0.05f);
    displayPeak      = smooth(displayPeak,      audioProcessor.outputPeakDb.load(),        0.7f, 0.05f);
    displayNormGain  = smooth(displayNormGain,  audioProcessor.normalisationGainDb.load(), 0.1f, 0.05f);

    repaint();
}

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const int  w      = bounds.getWidth();

    g.fillAll(bgDarkest);

    //--- Header bar ---
    auto headerBounds = bounds.withHeight(56);
    g.setColour(bgPanelHi);
    g.fillRect(headerBounds);
    g.setColour(strokeCol);
    g.drawLine(0.0f, 56.0f, (float)w, 56.0f, 1.0f);

    if (logoFullImage.isValid())
    {
        auto logoArea = headerBounds.reduced(0, 8).withTrimmedLeft(16).withWidth(170);
        g.drawImage(logoFullImage, logoArea.toFloat(),
                    juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    }

    //--- Main content panel (below tabs) ---
    auto contentArea = bounds.withTrimmedTop(56 + 48).withTrimmedRight(getMeterSidebarWidth());
    g.setColour(bgPanel);
    g.fillRoundedRectangle(contentArea.reduced(8).toFloat(), 8.0f);
    g.setColour(strokeCol);
    g.drawRoundedRectangle(contentArea.reduced(8).toFloat(), 8.0f, 1.0f);

    //--- Meter sidebar panel ---
    auto sidebar = bounds.withTrimmedTop(56).removeFromRight(getMeterSidebarWidth());
    g.setColour(bgPanel);
    g.fillRoundedRectangle(sidebar.reduced(8).toFloat(), 8.0f);
    g.setColour(strokeCol);
    g.drawRoundedRectangle(sidebar.reduced(8).toFloat(), 8.0f, 1.0f);

    g.setFont(juce::Font("Courier New", 11.0f, juce::Font::bold));
    g.setColour(accent);
    g.drawText("OUTPUT", sidebar.reduced(16, 14).removeFromTop(16), juce::Justification::centredLeft);

    drawGRMeter  (g, grMeterBounds, displayGR);
    drawMeter    (g, momentBounds,  displayMoment, -40.0f, 0.0f, "Momentary LUFS");
    drawMeter    (g, integrBounds,  displayIntegr, -40.0f, 0.0f,
                  "Integrated LUFS", true, currentLufsTarget());
    drawNormGain (g, normGainBounds, displayNormGain);

    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
    g.setColour(textDim);
    auto footerBounds = getLocalBounds();
    g.drawText("ScalaPodcaster v2.0  |  Target: " + juce::String(currentLufsTarget(), 1) + " LUFS",
               footerBounds.removeFromBottom(16).reduced(8, 0),
               juce::Justification::centredRight);
}

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::resized()
{
    const auto bounds = getLocalBounds();
    const int  w      = bounds.getWidth();
    const int  sidebarW = 220;

    //--- Header: preset box left of bypass ---
    bypassButton.setBounds(w - 106, 16, 86, 24);
    presetBox.setBounds(w - 106 - 250 - 8, 14, 200, 28);
    presetSaveBtn.setBounds(w - 106 - 250 - 8 + 204, 14, 40, 28);
    presetDeleteBtn.setBounds(w - 106 - 250 - 8 + 248, 14, 34, 28);

    //--- Module tab row (constrained to the content column width, so it
    //    never overlaps the meter sidebar on the right) ---
    auto tabRow = bounds.withTrimmedTop(56).removeFromTop(48).withTrimmedRight(sidebarW).reduced(8, 6);
    int  tabW   = tabRow.getWidth() / (int)allTabs.size();
    for (auto* tab : allTabs)
        tab->setBounds(tabRow.removeFromLeft(tabW).reduced(4, 0));

    //--- Main content area (selected module's controls) ---
    auto contentArea = bounds.withTrimmedTop(56 + 48).withTrimmedRight(sidebarW).reduced(24, 24);

    auto layoutColumn = [&](std::initializer_list<std::pair<juce::Slider*, juce::Label*>> rows,
                             juce::Rectangle<int> area)
    {
        if (rows.size() == 0) return;
        int rowH = area.getHeight() / (int)rows.size();
        for (auto& [slider, label] : rows)
        {
            auto row = area.removeFromTop(rowH);
            label->setBounds(row.removeFromTop(18));
            slider->setBounds(row.reduced(0, 6));
        }
    };

    switch (activeTab)
    {
        case ModuleTab::EQ:
        {
            auto area = contentArea;

            // HPF row: enable + preset dropdown + custom slider
            auto hpfRow = area.removeFromTop(30);
            hpfOnButton.setBounds(hpfRow.removeFromLeft(60));
            hpfRow.removeFromLeft(8);
            hpfPresetBox.setBounds(hpfRow.removeFromLeft(240));
            hpfRow.removeFromLeft(16);
            hpfCustomLabel.setBounds(hpfRow.removeFromLeft(55));
            hpfCustomSlider.setBounds(hpfRow);

            area.removeFromTop(10);

            // Graph takes most of the remaining space, band rows stacked below
            int bandRowH  = 30;
            int bandsTotalH = bandRowH * EQProcessor::numBands + 6;
            auto graphArea = area.removeFromTop(area.getHeight() - bandsTotalH - 8);
            eqGraph->setBounds(graphArea);

            area.removeFromTop(8);
            for (int i = 0; i < EQProcessor::numBands; ++i)
            {
                auto rowArea = area.removeFromTop(bandRowH);
                layoutBandRow(i, rowArea);
            }
            break;
        }

        case ModuleTab::DeEsser:
        {
            auto area = contentArea;
            deEsserButton.setBounds(area.removeFromTop(28));
            area.removeFromTop(12);
            deEsserFreqLabel.setBounds(area.removeFromTop(18));
            deEsserFreqSlider.setBounds(area.removeFromTop(28));
            area.removeFromTop(16);
            deEsserThreshLabel.setBounds(area.removeFromTop(18));
            deEsserThreshSlider.setBounds(area.removeFromTop(28));
            area.removeFromTop(16);
            deEsserWideButton.setBounds(area.removeFromTop(28));
            area.removeFromTop(16);
            deEsserLookaheadLabel.setBounds(area.removeFromTop(18));
            deEsserLookaheadSlider.setBounds(area.removeFromTop(28));
            break;
        }

        case ModuleTab::Fade:
        {
            auto area = contentArea;
            fadeEnableButton.setBounds(area.removeFromTop(28));
            area.removeFromTop(20);

            auto fadeInRow = area.removeFromTop(28);
            fadeInLabel.setBounds(fadeInRow.removeFromLeft(80));
            fadeInPresetBox.setBounds(fadeInRow.removeFromLeft(200));
            fadeInRow.removeFromLeft(16);
            fadeInCustomLabel.setBounds(fadeInRow.removeFromLeft(55));
            fadeInSlider.setBounds(fadeInRow);

            area.removeFromTop(20);

            auto fadeOutRow = area.removeFromTop(28);
            fadeOutLabel.setBounds(fadeOutRow.removeFromLeft(80));
            fadeOutPresetBox.setBounds(fadeOutRow.removeFromLeft(200));
            fadeOutRow.removeFromLeft(16);
            fadeOutCustomLabel.setBounds(fadeOutRow.removeFromLeft(55));
            fadeOutSlider.setBounds(fadeOutRow);
            break;
        }

        case ModuleTab::Compressor:
        {
            int colW = contentArea.getWidth() / 2;
            auto left  = contentArea.removeFromLeft(colW).reduced(0, 0);
            auto right = contentArea;
            layoutColumn({ {&threshSlider, &threshLabel},
                            {&attackSlider, &attackLabel},
                            {&makeupSlider, &makeupLabel} }, left.reduced(8, 0));
            layoutColumn({ {&ratioSlider, &ratioLabel},
                            {&releaseSlider, &releaseLabel},
                            {&kneeSlider, &kneeLabel} }, right.reduced(8, 0));
            break;
        }

        case ModuleTab::Loudness:
        {
            auto area = contentArea;
            lufsPresetLabel.setBounds(area.removeFromTop(18));
            lufsPresetBox.setBounds(area.removeFromTop(30));
            area.removeFromTop(20);
            lufsCustomLabel.setBounds(area.removeFromTop(18));
            lufsCustomSlider.setBounds(area.removeFromTop(28));
            break;
        }
    }

    //--- Meter sidebar layout ---
    auto sidebar = bounds.withTrimmedTop(56).removeFromRight(sidebarW).reduced(16, 0);
    sidebar.removeFromTop(36); // space for "OUTPUT" title drawn in paint()

    int mh = 50;
    grMeterBounds  = sidebar.removeFromTop(mh); sidebar.removeFromTop(8);
    momentBounds   = sidebar.removeFromTop(mh); sidebar.removeFromTop(8);
    integrBounds   = sidebar.removeFromTop(mh); sidebar.removeFromTop(8);
    normGainBounds = sidebar.removeFromTop(mh);
}

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::drawMeter(
    juce::Graphics& g, juce::Rectangle<int> bounds,
    float valueLufs, float minLufs, float maxLufs,
    juce::String label, bool showTarget, float targetLufs)
{
    g.setColour(bgDarkest);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    float norm = juce::jlimit(0.0f, 1.0f,
        (valueLufs - minLufs) / (maxLufs - minLufs));

    juce::Colour barColor;
    if (norm < 0.6f)       barColor = juce::Colour(0xFF44CC88);
    else if (norm < 0.85f) barColor = accent;
    else                   barColor = warn;

    auto barBounds = bounds.reduced(3, 3);
    int  barWidth  = (int)(barBounds.getWidth() * norm);
    g.setColour(barColor.withAlpha(0.85f));
    g.fillRoundedRectangle(barBounds.removeFromLeft(barWidth).toFloat(), 3.0f);

    if (showTarget)
    {
        float targetNorm = juce::jlimit(0.0f, 1.0f,
            (targetLufs - minLufs) / (maxLufs - minLufs));
        int tx = bounds.getX() + 3 + (int)((bounds.getWidth() - 6) * targetNorm);
        g.setColour(accent);
        g.drawLine((float)tx, (float)bounds.getY() + 1,
                   (float)tx, (float)bounds.getBottom() - 1, 2.0f);
    }

    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
    g.setColour(text.withAlpha(0.7f));
    g.drawText(label, bounds.reduced(5, 2), juce::Justification::topLeft);
    g.setColour(text);
    g.drawText(juce::String(valueLufs, 1) + " LUFS",
               bounds.reduced(5, 2), juce::Justification::topRight);
}

void ScalaPodcasterAudioProcessorEditor::drawGRMeter(
    juce::Graphics& g, juce::Rectangle<int> bounds, float grDb)
{
    g.setColour(bgDarkest);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    float norm = juce::jlimit(0.0f, 1.0f, -grDb / 30.0f);
    auto  barBounds = bounds.reduced(3, 3);
    int   barWidth  = (int)(barBounds.getWidth() * norm);

    g.setColour(accent.withAlpha(0.85f));
    g.fillRoundedRectangle(barBounds.removeFromRight(barWidth).toFloat(), 3.0f);

    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
    g.setColour(text.withAlpha(0.7f));
    g.drawText("GR", bounds.reduced(5, 2), juce::Justification::topLeft);
    g.setColour(text);
    g.drawText(juce::String(grDb, 1) + " dB",
               bounds.reduced(5, 2), juce::Justification::topRight);
}

void ScalaPodcasterAudioProcessorEditor::drawNormGain(
    juce::Graphics& g, juce::Rectangle<int> bounds, float gainDb)
{
    g.setColour(bgDarkest);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Visualise as a centered bar: positive = boost (green), negative = cut (amber)
    float norm = juce::jlimit(-1.0f, 1.0f, gainDb / 24.0f);
    auto barBounds = bounds.reduced(3, 3);
    int midX = barBounds.getX() + barBounds.getWidth() / 2;

    juce::Colour col = (gainDb >= 0.0f) ? juce::Colour(0xFF44CC88) : accent;
    if (norm > 0.0f)
    {
        int barW = (int)(norm * (barBounds.getWidth() / 2));
        g.setColour(col.withAlpha(0.85f));
        g.fillRoundedRectangle(juce::Rectangle<int>(midX, barBounds.getY(), barW, barBounds.getHeight()).toFloat(), 3.0f);
    }
    else if (norm < 0.0f)
    {
        int barW = (int)(-norm * (barBounds.getWidth() / 2));
        g.setColour(col.withAlpha(0.85f));
        g.fillRoundedRectangle(juce::Rectangle<int>(midX - barW, barBounds.getY(), barW, barBounds.getHeight()).toFloat(), 3.0f);
    }

    // Centre line
    g.setColour(strokeCol);
    g.drawVerticalLine(midX, (float)bounds.getY() + 2, (float)bounds.getBottom() - 2);

    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
    g.setColour(text.withAlpha(0.7f));
    g.drawText("Normalisation", bounds.reduced(5, 2), juce::Justification::topLeft);
    g.setColour(text);
    juce::String label = (gainDb >= 0.0f ? "+" : "") + juce::String(gainDb, 1) + " dB";
    g.drawText(label, bounds.reduced(5, 2), juce::Justification::topRight);
}

//==============================================================================
void ScalaPodcasterAudioProcessorEditor::styleSlider(
    juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
    s.setColour(juce::Slider::thumbColourId,          accent);
    s.setColour(juce::Slider::trackColourId,          accent.withAlpha(0.4f));
    s.setColour(juce::Slider::backgroundColourId,     bgDarkest);
    s.setColour(juce::Slider::textBoxTextColourId,    text);
    s.setColour(juce::Slider::textBoxBackgroundColourId, bgDarkest);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setFont(juce::Font("Courier New", 11.0f, juce::Font::plain));
    l.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(l);
}
