cmake_minimum_required(VERSION 3.22)
project(ScalaPodcaster VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ScalaPodcaster
    VERSION                     2.0.0
    COMPANY_NAME                "YourStudio"
    COMPANY_WEBSITE             ""
    COMPANY_EMAIL               ""
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    YSTD
    PLUGIN_CODE                 Scpc
    # Multi-format: VST3 + LV2 + Standalone everywhere, AU added automatically on macOS
    FORMATS                     VST3 LV2 AU Standalone
    PRODUCT_NAME                "ScalaPodcaster"
    # VST3 category for DaVinci Resolve compatibility
    VST3_CATEGORIES             "Fx"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
    LV2URI                      "https://yourstudio.com/scalapodcaster"
    # Needed for correct channel layout detection
    NEEDS_CURL                  FALSE
    NEEDS_WEB_BROWSER           FALSE
)

# Embed the Scala logo assets (full lockup + square wordmark-only icon) as
# compiled-in binary resources, so the plugin doesn't depend on any external
# files at runtime — required for a single-file VST3/AU/LV2 bundle.
juce_add_binary_data(ScalaPodcasterAssets
    SOURCES
        Source/Assets/scala_logo_full_transparent.png
        Source/Assets/scala_icon_square.png
)
set_target_properties(ScalaPodcasterAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(ScalaPodcaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
        Source/EQGraphComponent.cpp
)

target_compile_definitions(ScalaPodcaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    SCALA_BUILD_EDITOR_SMOKE_TEST=0
    # Strict bus layout for DAW compatibility
)

target_link_libraries(ScalaPodcaster PRIVATE
    ScalaPodcasterAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_core
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# --- Optional editor-only smoke test (no audio device required) -------------
# Builds a tiny headless-friendly app that instantiates the processor/editor
# and exercises several resize sizes, used during development to verify GUI
# stability without depending on a real sound card being present.
if (SCALA_BUILD_EDITOR_SMOKE_TEST)
    add_executable(EditorSmokeTest
        dev/editor_smoke_test.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EQGraphComponent.cpp
    )
    target_link_libraries(EditorSmokeTest PRIVATE
        ScalaPodcasterAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_core
        juce::juce_recommended_config_flags
    )
    # Reuse the exact JucePlugin_* defines generated for the real plugin
    # target, since PluginProcessor.h references JucePlugin_Name etc.
    target_compile_definitions(EditorSmokeTest PRIVATE
        $<TARGET_PROPERTY:ScalaPodcaster,COMPILE_DEFINITIONS>
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        SCALA_BUILD_EDITOR_SMOKE_TEST=1
    )

    add_executable(EditorScreenshot
        dev/editor_screenshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EQGraphComponent.cpp
    )
    target_link_libraries(EditorScreenshot PRIVATE
        ScalaPodcasterAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_core
        juce::juce_recommended_config_flags
    )
    target_compile_definitions(EditorScreenshot PRIVATE
        $<TARGET_PROPERTY:ScalaPodcaster,COMPILE_DEFINITIONS>
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        SCALA_BUILD_EDITOR_SMOKE_TEST=1
    )
endif()
