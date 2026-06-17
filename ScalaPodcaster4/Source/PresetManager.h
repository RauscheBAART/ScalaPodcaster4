#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

/**
 * PresetManager
 * -------------
 * Handles saving, loading, and managing named presets for the plugin.
 *
 * FACTORY PRESETS: embedded as XML strings in this file; tuned for common
 *   podcast/voice use cases based on real-voice analysis.
 *
 * USER PRESETS: stored as XML files in
 *   <user documents>/ScalaPodcaster/Presets/
 *
 * Design: the manager owns a sorted list of PresetInfo structs (factory +
 * user). The PluginEditor shows them in a ComboBox; selecting one calls
 * loadPreset(). Saving calls saveCurrentAsUserPreset(name).
 */
class PresetManager
{
public:
    struct PresetInfo
    {
        juce::String name;
        bool         isFactory = false;
        juce::String xmlContent;   // for factory presets, stored inline
        juce::File   file;         // for user presets, path on disk
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvtsIn)
        : apvts(apvtsIn)
    {
        buildFactoryPresets();
        scanUserPresets();
    }

    // -----------------------------------------------------------------------
    const std::vector<PresetInfo>& getAllPresets() const { return allPresets; }

    int getCurrentPresetIndex() const { return currentIndex; }

    void setCurrentIndex(int idx) { currentIndex = idx; }

    std::function<void()> onPresetListChanged;

    // -----------------------------------------------------------------------
    // Load a preset by index into the APVTS.
    bool loadPreset(int index)
    {
        if (index < 0 || index >= (int)allPresets.size()) return false;

        juce::String xmlText;
        if (allPresets[(size_t)index].isFactory)
        {
            xmlText = allPresets[(size_t)index].xmlContent;
        }
        else
        {
            xmlText = allPresets[(size_t)index].file.loadFileAsString();
            if (xmlText.isEmpty()) return false;
        }

        auto xml = juce::XmlDocument::parse(xmlText);
        if (xml == nullptr) return false;

        auto vt = juce::ValueTree::fromXml(*xml);
        if (!vt.isValid()) return false;

        apvts.replaceState(vt);
        currentIndex = index;
        return true;
    }

    // -----------------------------------------------------------------------
    // Save the current APVTS state as a user preset.
    bool saveCurrentAsUserPreset(const juce::String& name)
    {
        if (name.isEmpty()) return false;

        juce::File dir = getUserPresetsDir();
        dir.createDirectory();

        juce::File outFile = dir.getChildFile(name + ".xml");

        auto state = apvts.copyState();
        auto xml   = state.createXml();
        if (xml == nullptr) return false;

        juce::String xmlText = xml->toString();
        if (!outFile.replaceWithText(xmlText)) return false;

        // Rebuild list and update current index
        scanUserPresets();
        for (int i = 0; i < (int)allPresets.size(); ++i)
        {
            if (!allPresets[(size_t)i].isFactory && allPresets[(size_t)i].file == outFile)
            {
                currentIndex = i;
                break;
            }
        }
        if (onPresetListChanged) onPresetListChanged();
        return true;
    }

    // -----------------------------------------------------------------------
    // Delete a user preset by index. Returns false if it's a factory preset.
    bool deleteUserPreset(int index)
    {
        if (index < 0 || index >= (int)allPresets.size()) return false;
        if (allPresets[(size_t)index].isFactory) return false;

        allPresets[(size_t)index].file.deleteFile();
        scanUserPresets();
        currentIndex = -1;
        if (onPresetListChanged) onPresetListChanged();
        return true;
    }

    // -----------------------------------------------------------------------
    juce::File getUserPresetsDir() const
    {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("ScalaPodcaster")
                   .getChildFile("Presets");
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<PresetInfo> allPresets;
    int currentIndex = -1;

    // -----------------------------------------------------------------------
    void buildFactoryPresets()
    {
        allPresets.clear();

        // ── Factory preset 1: Default (flat, all processors at defaults) ───
        addFactory("Default (Flat)", R"(
<Parameters>
  <PARAM id="hpfOn" value="1"/>
  <PARAM id="hpfPreset" value="3"/>
  <PARAM id="hpfFreqCustom" value="90"/>
  <PARAM id="band1On" value="0"/> <PARAM id="band2On" value="0"/>
  <PARAM id="band3On" value="0"/> <PARAM id="band4On" value="0"/>
  <PARAM id="band5On" value="0"/> <PARAM id="band6On" value="0"/>
  <PARAM id="deEsserOn" value="1"/> <PARAM id="deEsserFreq" value="8500"/>
  <PARAM id="deEsserThresh" value="-20"/> <PARAM id="deEsserWide" value="0"/>
  <PARAM id="threshold" value="-26"/> <PARAM id="ratio" value="4"/>
  <PARAM id="knee" value="6"/> <PARAM id="attack" value="0.1"/>
  <PARAM id="release" value="1000"/> <PARAM id="makeup" value="0"/>
  <PARAM id="lufsPreset" value="0"/> <PARAM id="lufsCustom" value="-14"/>
  <PARAM id="fadeIn" value="1.5"/> <PARAM id="fadeOut" value="2.0"/>
  <PARAM id="bypass" value="0"/>
</Parameters>)");

        // ── Factory preset 2: Male Voice (tuned for Christian-Test analysis) ─
        addFactory("Male Voice — Podcast", R"(
<Parameters>
  <PARAM id="hpfOn" value="1"/>
  <PARAM id="hpfPreset" value="1"/>
  <PARAM id="hpfFreqCustom" value="80"/>
  <PARAM id="band1On" value="1"/> <PARAM id="band1Type" value="0"/>
  <PARAM id="band1Freq" value="200"/> <PARAM id="band1Gain" value="-2.5"/>
  <PARAM id="band1Q" value="1.2"/>
  <PARAM id="band2On" value="1"/> <PARAM id="band2Type" value="0"/>
  <PARAM id="band2Freq" value="3000"/> <PARAM id="band2Gain" value="2.0"/>
  <PARAM id="band2Q" value="1.5"/>
  <PARAM id="band3On" value="1"/> <PARAM id="band3Type" value="2"/>
  <PARAM id="band3Freq" value="8000"/> <PARAM id="band3Gain" value="-1.5"/>
  <PARAM id="band3Q" value="0.8"/>
  <PARAM id="band4On" value="0"/> <PARAM id="band5On" value="0"/>
  <PARAM id="band6On" value="0"/>
  <PARAM id="deEsserOn" value="1"/> <PARAM id="deEsserFreq" value="8500"/>
  <PARAM id="deEsserThresh" value="-22"/> <PARAM id="deEsserWide" value="0"/>
  <PARAM id="threshold" value="-24"/> <PARAM id="ratio" value="3.5"/>
  <PARAM id="knee" value="6"/> <PARAM id="attack" value="5"/>
  <PARAM id="release" value="150"/> <PARAM id="makeup" value="2"/>
  <PARAM id="lufsPreset" value="0"/> <PARAM id="lufsCustom" value="-14"/>
  <PARAM id="fadeIn" value="0.3"/> <PARAM id="fadeOut" value="2.0"/>
  <PARAM id="bypass" value="0"/>
</Parameters>)");

        // ── Factory preset 3: Female Voice ──────────────────────────────────
        addFactory("Female Voice — Podcast", R"(
<Parameters>
  <PARAM id="hpfOn" value="1"/>
  <PARAM id="hpfPreset" value="2"/>
  <PARAM id="hpfFreqCustom" value="100"/>
  <PARAM id="band1On" value="1"/> <PARAM id="band1Type" value="0"/>
  <PARAM id="band1Freq" value="300"/> <PARAM id="band1Gain" value="-1.5"/>
  <PARAM id="band1Q" value="1.0"/>
  <PARAM id="band2On" value="1"/> <PARAM id="band2Type" value="0"/>
  <PARAM id="band2Freq" value="2500"/> <PARAM id="band2Gain" value="1.5"/>
  <PARAM id="band2Q" value="1.2"/>
  <PARAM id="band3On" value="1"/> <PARAM id="band3Type" value="2"/>
  <PARAM id="band3Freq" value="7000"/> <PARAM id="band3Gain" value="-2.0"/>
  <PARAM id="band3Q" value="0.9"/>
  <PARAM id="band4On" value="0"/> <PARAM id="band5On" value="0"/>
  <PARAM id="band6On" value="0"/>
  <PARAM id="deEsserOn" value="1"/> <PARAM id="deEsserFreq" value="7500"/>
  <PARAM id="deEsserThresh" value="-18"/> <PARAM id="deEsserWide" value="1"/>
  <PARAM id="threshold" value="-22"/> <PARAM id="ratio" value="3"/>
  <PARAM id="knee" value="5"/> <PARAM id="attack" value="8"/>
  <PARAM id="release" value="120"/> <PARAM id="makeup" value="1.5"/>
  <PARAM id="lufsPreset" value="0"/> <PARAM id="lufsCustom" value="-14"/>
  <PARAM id="fadeIn" value="0.3"/> <PARAM id="fadeOut" value="2.0"/>
  <PARAM id="bypass" value="0"/>
</Parameters>)");

        // ── Factory preset 4: Broadcast (EBU R128 -23 LUFS) ─────────────────
        addFactory("Broadcast — EBU R128", R"(
<Parameters>
  <PARAM id="hpfOn" value="1"/>
  <PARAM id="hpfPreset" value="3"/>
  <PARAM id="hpfFreqCustom" value="90"/>
  <PARAM id="band1On" value="0"/> <PARAM id="band2On" value="0"/>
  <PARAM id="band3On" value="0"/> <PARAM id="band4On" value="0"/>
  <PARAM id="band5On" value="0"/> <PARAM id="band6On" value="0"/>
  <PARAM id="deEsserOn" value="1"/> <PARAM id="deEsserFreq" value="8000"/>
  <PARAM id="deEsserThresh" value="-20"/> <PARAM id="deEsserWide" value="0"/>
  <PARAM id="threshold" value="-28"/> <PARAM id="ratio" value="2.5"/>
  <PARAM id="knee" value="8"/> <PARAM id="attack" value="15"/>
  <PARAM id="release" value="200"/> <PARAM id="makeup" value="0"/>
  <PARAM id="lufsPreset" value="2"/> <PARAM id="lufsCustom" value="-23"/>
  <PARAM id="fadeIn" value="1.5"/> <PARAM id="fadeOut" value="2.0"/>
  <PARAM id="bypass" value="0"/>
</Parameters>)");

        // ── Factory preset 5: Warm & Intimate (Solo podcast, intimate style)
        addFactory("Warm & Intimate", R"(
<Parameters>
  <PARAM id="hpfOn" value="1"/>
  <PARAM id="hpfPreset" value="1"/>
  <PARAM id="hpfFreqCustom" value="80"/>
  <PARAM id="band1On" value="1"/> <PARAM id="band1Type" value="1"/>
  <PARAM id="band1Freq" value="120"/> <PARAM id="band1Gain" value="2.0"/>
  <PARAM id="band1Q" value="0.7"/>
  <PARAM id="band2On" value="1"/> <PARAM id="band2Type" value="0"/>
  <PARAM id="band2Freq" value="500"/> <PARAM id="band2Gain" value="-1.5"/>
  <PARAM id="band2Q" value="1.0"/>
  <PARAM id="band3On" value="1"/> <PARAM id="band3Type" value="0"/>
  <PARAM id="band3Freq" value="2800"/> <PARAM id="band3Gain" value="1.0"/>
  <PARAM id="band3Q" value="1.5"/>
  <PARAM id="band4On" value="0"/> <PARAM id="band5On" value="0"/>
  <PARAM id="band6On" value="0"/>
  <PARAM id="deEsserOn" value="1"/> <PARAM id="deEsserFreq" value="8500"/>
  <PARAM id="deEsserThresh" value="-24"/> <PARAM id="deEsserWide" value="0"/>
  <PARAM id="threshold" value="-20"/> <PARAM id="ratio" value="4"/>
  <PARAM id="knee" value="4"/> <PARAM id="attack" value="3"/>
  <PARAM id="release" value="100"/> <PARAM id="makeup" value="3"/>
  <PARAM id="lufsPreset" value="1"/> <PARAM id="lufsCustom" value="-16"/>
  <PARAM id="fadeIn" value="1.5"/> <PARAM id="fadeOut" value="2.0"/>
  <PARAM id="bypass" value="0"/>
</Parameters>)");

        // ── Factory preset 6: Clean & Bright (Interview, multi-speaker) ─────
        addFactory("Clean & Bright — Interview", R"(
<Parameters>
  <PARAM id="hpfOn" value="1"/>
  <PARAM id="hpfPreset" value="3"/>
  <PARAM id="hpfFreqCustom" value="90"/>
  <PARAM id="band1On" value="1"/> <PARAM id="band1Type" value="0"/>
  <PARAM id="band1Freq" value="400"/> <PARAM id="band1Gain" value="-2.0"/>
  <PARAM id="band1Q" value="1.3"/>
  <PARAM id="band2On" value="1"/> <PARAM id="band2Type" value="0"/>
  <PARAM id="band2Freq" value="3500"/> <PARAM id="band2Gain" value="2.5"/>
  <PARAM id="band2Q" value="1.2"/>
  <PARAM id="band3On" value="1"/> <PARAM id="band3Type" value="2"/>
  <PARAM id="band3Freq" value="10000"/> <PARAM id="band3Gain" value="1.5"/>
  <PARAM id="band3Q" value="0.7"/>
  <PARAM id="band4On" value="0"/> <PARAM id="band5On" value="0"/>
  <PARAM id="band6On" value="0"/>
  <PARAM id="deEsserOn" value="1"/> <PARAM id="deEsserFreq" value="8000"/>
  <PARAM id="deEsserThresh" value="-19"/> <PARAM id="deEsserWide" value="1"/>
  <PARAM id="threshold" value="-22"/> <PARAM id="ratio" value="3.5"/>
  <PARAM id="knee" value="5"/> <PARAM id="attack" value="6"/>
  <PARAM id="release" value="130"/> <PARAM id="makeup" value="2.5"/>
  <PARAM id="lufsPreset" value="0"/> <PARAM id="lufsCustom" value="-14"/>
  <PARAM id="fadeIn" value="0.3"/> <PARAM id="fadeOut" value="2.0"/>
  <PARAM id="bypass" value="0"/>
</Parameters>)");
    }

    void addFactory(const juce::String& name, const juce::String& xml)
    {
        PresetInfo p;
        p.name       = name;
        p.isFactory  = true;
        p.xmlContent = xml.trim();
        allPresets.push_back(std::move(p));
    }

    void scanUserPresets()
    {
        // Remove existing user presets from list, keep factory
        allPresets.erase(
            std::remove_if(allPresets.begin(), allPresets.end(),
                           [](const PresetInfo& p) { return !p.isFactory; }),
            allPresets.end());

        juce::File dir = getUserPresetsDir();
        if (!dir.exists()) return;

        for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            PresetInfo p;
            p.name      = f.getFileNameWithoutExtension();
            p.isFactory = false;
            p.file      = f;
            allPresets.push_back(std::move(p));
        }
    }
};
