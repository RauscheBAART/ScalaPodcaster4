// Development-only smoke test: instantiates the processor + editor and
// exercises resizing / tab switching without needing a real audio device.
// Not part of the shipped plugin. Build with -DSCALA_BUILD_EDITOR_SMOKE_TEST=ON.
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "[1] Creating processor..." << std::endl;
    auto processor = std::make_unique<ScalaPodcasterAudioProcessor>();
    std::cout << "    OK" << std::endl;

    std::cout << "[2] Creating editor (this calls setSize -> resized)..." << std::endl;
    auto* rawEditor = processor->createEditor();
    std::unique_ptr<ScalaPodcasterAudioProcessorEditor> editor(
        dynamic_cast<ScalaPodcasterAudioProcessorEditor*>(rawEditor));
    if (editor == nullptr) { std::cerr << "FAILED: editor is null or wrong type" << std::endl; return 1; }
    std::cout << "    OK - size " << editor->getWidth() << "x" << editor->getHeight() << std::endl;

    std::cout << "[3] Resizing through several sizes (min/max/default)..." << std::endl;
    const int sizes[][2] = { {860, 560}, {1500, 950}, {980, 640}, {1200, 700}, {860, 950} };
    for (auto& s : sizes)
    {
        editor->setSize(s[0], s[1]);
        std::cout << "    " << s[0] << "x" << s[1] << " OK" << std::endl;
    }

    std::cout << "[4] Forcing repaint..." << std::endl;
    editor->repaint();
    std::cout << "    OK" << std::endl;

    editor = nullptr;
    processor = nullptr;

    std::cout << "ALL SMOKE TESTS PASSED" << std::endl;
    return 0;
}
