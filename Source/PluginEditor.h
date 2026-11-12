#pragma once
#include "PluginProcessor.h"

class MixPlayEditor : public juce::AudioProcessorEditor
{
public:
    explicit MixPlayEditor (MixPlayProcessor&);
    ~MixPlayEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void loadReference();
    void exportPreset();
    void refresh();
    void runAnalysis();

    MixPlayProcessor& p;
    juce::LookAndFeel_V4 laf;
    juce::TextButton loadBtn { "Carregar referencia" }, exportBtn { "Exportar preset" };
    juce::TextButton bypassBtn { "Bypass" };
    juce::ComboBox instrumentBox;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String fileName;
    juce::File refFile;
    bool busy = false;
};
