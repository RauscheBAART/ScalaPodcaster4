// Development-only tool: renders the plugin editor off-screen and saves it
// as a PNG, so the GUI can be visually inspected without a real DAW host.
// Not part of the shipped plugin.
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String outPath = (argc > 1) ? juce::String(argv[1]) : juce::String("/tmp/editor_screenshot.png");
    int tabIndex = (argc > 2) ? juce::String(argv[2]).getIntValue() : 0;

    auto processor = std::make_unique<ScalaPodcasterAudioProcessor>();
    auto* rawEditor = processor->createEditor();
    std::unique_ptr<ScalaPodcasterAudioProcessorEditor> editor(
        dynamic_cast<ScalaPodcasterAudioProcessorEditor*>(rawEditor));
    if (editor == nullptr) { std::cerr << "FAILED: editor null" << std::endl; return 1; }

    juce::DocumentWindow window ("ScalaPodcaster Screenshot",
                                  juce::Colours::black,
                                  juce::DocumentWindow::allButtons);
    window.setUsingNativeTitleBar(false);
    window.setContentNonOwned(editor.get(), true);
    window.setVisible(true);
    window.setBounds(0, 0, editor->getWidth(), editor->getHeight());

    juce::Thread::sleep(200);

    // tabIndex 0=EQ 1=DeEsser 2=Compressor 3=Loudness
    editor->debugSelectTabForScreenshot(tabIndex);

    juce::Thread::sleep(200);

    auto image = editor->createComponentSnapshot(editor->getLocalBounds());

    juce::File outFile(outPath);
    outFile.deleteFile();
    juce::FileOutputStream stream(outFile);
    juce::PNGImageFormat png;
    bool ok = png.writeImageToStream(image, stream);

    std::cout << (ok ? "Screenshot saved: " : "FAILED to save: ") << outPath.toStdString() << std::endl;

    window.setContentNonOwned(nullptr, false);
    return ok ? 0 : 1;
}
