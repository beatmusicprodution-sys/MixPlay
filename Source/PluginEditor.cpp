#include "PluginEditor.h"
#include <thread>

namespace
{
    juce::String T (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bg     { 0xFF0D1110 };
    const juce::Colour panel  { 0xFF161B19 };
    const juce::Colour green  { 0xFF22E06B };
    const juce::Colour text   { 0xFFE6EDE9 };
    const juce::Colour dim    { 0xFF7F8C86 };
    const char* bandNames[Analysis::numBands] = { "31","63","125","250","500","1k","2k","4k","8k","16k" };
}

MixPlayEditor::MixPlayEditor (MixPlayProcessor& proc) : AudioProcessorEditor (&proc), p (proc)
{
    laf.setColour (juce::TextButton::buttonColourId, green);
    laf.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    laf.setColour (juce::ComboBox::backgroundColourId, panel);
    laf.setColour (juce::ComboBox::textColourId, text);
    laf.setColour (juce::ComboBox::outlineColourId, green);
    laf.setColour (juce::ComboBox::arrowColourId, green);
    laf.setColour (juce::PopupMenu::backgroundColourId, panel);
    laf.setColour (juce::PopupMenu::textColourId, text);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, green);
    laf.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setLookAndFeel (&laf);

    loadBtn.setButtonText (T ("Carregar referência"));
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (exportBtn);
    addAndMakeVisible (instrumentBox);
    addAndMakeVisible (bypassBtn);
    laf.setColour (juce::TextButton::buttonOnColourId, dim);
    laf.setColour (juce::TextButton::textColourOnId, text);
    bypassBtn.setClickingTogglesState (true);
    bypassBtn.setToggleState (p.bypass.load(), juce::dontSendNotification);
    bypassBtn.onClick = [this] { p.bypass = bypassBtn.getToggleState(); };
    instrumentBox.addItemList ({ "Bateria", "Kick", "Snare", "Baixo", "Voz", "Mix geral" }, 1);
    instrumentBox.setText (p.instrument, juce::dontSendNotification);

    loadBtn.onClick = [this] { loadReference(); };
    exportBtn.onClick = [this] { exportPreset(); };
    instrumentBox.onChange = [this] { p.instrument = instrumentBox.getText(); runAnalysis(); };

    setSize (760, 560);
}

void MixPlayEditor::resized()
{
    loadBtn.setBounds (20, 70, 170, 38);
    instrumentBox.setBounds (200, 70, 150, 38);
    bypassBtn.setBounds (360, 70, 100, 38);
    exportBtn.setBounds (getWidth() - 210, 70, 190, 38);
}

void MixPlayEditor::refresh()
{
    if (p.analysis.valid)
        p.setPreset (Analyzer::makePreset (p.analysis, p.instrument));
    repaint();
}

void MixPlayEditor::runAnalysis()
{
    if (busy || ! refFile.existsAsFile()) return;
    busy = true;
    loadBtn.setEnabled (false);
    instrumentBox.setEnabled (false);
    repaint();

    juce::Component::SafePointer<MixPlayEditor> safe (this);
    const auto file = refFile;
    const auto instr = p.instrument;
    std::thread ([safe, file, instr]
    {
        auto result = Analyzer::analyseFile (file, instr);
        juce::MessageManager::callAsync ([safe, result]
        {
            if (safe == nullptr) return;
            safe->p.analysis = result;
            safe->busy = false;
            safe->loadBtn.setEnabled (true);
            safe->instrumentBox.setEnabled (true);
            safe->refresh();
        });
    }).detach();
}

void MixPlayEditor::loadReference()
{
    chooser = std::make_unique<juce::FileChooser> (T ("Escolha o áudio de referência"), juce::File(),
                                                   "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.m4a");
    juce::Component::SafePointer<MixPlayEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (safe == nullptr || ! f.existsAsFile()) return;
            safe->fileName = f.getFileName();
            safe->refFile = f;
            safe->runAnalysis();
        });
}

void MixPlayEditor::exportPreset()
{
    if (! p.analysis.valid) return;
    auto def = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("MixPlay_" + p.instrument + ".mixplay");
    chooser = std::make_unique<juce::FileChooser> ("Salvar preset", def, "*.mixplay");
    juce::Component::SafePointer<MixPlayEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (safe == nullptr || f == juce::File()) return;
            f.replaceWithText (juce::JSON::toString (Analyzer::presetToJson (safe->p.preset)));
        });
}

void MixPlayEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (green);
    g.setFont (juce::FontOptions (28.f, juce::Font::bold));
    g.drawText ("MIX PLAY", 20, 15, 300, 40, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (14.f));
    g.drawText (fileName.isEmpty() ? T ("Nenhuma referência carregada") : T ("Referência: ") + fileName,
                330, 20, getWidth() - 350, 30, juce::Justification::centredRight);

    if (busy)
    {
        g.setColour (green);
        g.setFont (juce::FontOptions (18.f));
        g.drawText ("Analisando e separando: " + p.instrument + "...", getBounds().withTrimmedTop (120),
                    juce::Justification::centred);
        return;
    }

    if (! p.analysis.valid)
    {
        g.setColour (dim);
        g.drawText (T ("Carregue uma música de referência para gerar o preset."),
                    getBounds().withTrimmedTop (120), juce::Justification::centred);
        return;
    }

    auto drawBars = [&] (juce::Rectangle<float> area, const float* v, float range, const juce::String& title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (area, 8.f);
        g.setColour (text);
        g.setFont (juce::FontOptions (14.f, juce::Font::bold));
        g.drawText (title, area.removeFromTop (26).reduced (12, 0), juce::Justification::centredLeft);
        auto plot = area.reduced (12, 4).withTrimmedBottom (20);
        const float mid = plot.getCentreY(), w = plot.getWidth() / Analysis::numBands;
        g.setColour (dim.withAlpha (0.4f));
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
        for (int i = 0; i < Analysis::numBands; ++i)
        {
            const float h = juce::jlimit (-range, range, v[i]) / range * (plot.getHeight() * 0.5f);
            auto bar = juce::Rectangle<float> (plot.getX() + i * w + w * 0.2f, h >= 0 ? mid - h : mid, w * 0.6f, std::abs (h));
            g.setColour (green);
            g.fillRect (bar);
            g.setColour (dim);
            g.setFont (juce::FontOptions (11.f));
            g.drawText (bandNames[i], (int) (plot.getX() + i * w), (int) plot.getBottom() + 2, (int) w, 16, juce::Justification::centred);
        }
    };

    float rel[Analysis::numBands], mean = 0.f;
    for (float x : p.analysis.bandDb) mean += x;
    mean /= Analysis::numBands;
    for (int i = 0; i < Analysis::numBands; ++i) rel[i] = p.analysis.bandDb[i] - mean;

    drawBars ({ 20.f, 125.f, 350.f, 210.f }, rel, 12.f, T ("Curva da referência (dB relativo)"));
    drawBars ({ 390.f, 125.f, 350.f, 210.f }, p.preset.eqGainDb, 6.f, "EQ sugerido - " + p.instrument + " (dB)");

    auto& a = p.analysis; auto& s = p.preset;
    g.setColour (panel);
    g.fillRoundedRectangle (20.f, 350.f, 720.f, 190.f, 8.f);
    g.setColour (text);
    g.setFont (juce::FontOptions (15.f));
    juce::StringArray lines;
    lines.add ("Pico: " + juce::String (a.peakDb, 1) + " dBFS    RMS: " + juce::String (a.rmsDb, 1)
            + " dBFS    Crista: " + juce::String (a.crestDb, 1) + T (" dB    Largura estéreo: ") + juce::String (a.widthPct, 0) + "%");
    lines.add ("Compressor: limiar " + juce::String (s.compThresholdDb, 1) + T (" dB | razão ") + juce::String (s.compRatio, 1)
            + ":1 | ataque " + juce::String (s.compAttackMs, 0) + " ms | release " + juce::String (s.compReleaseMs, 0) + " ms");
    lines.add ("Transientes: ataque " + juce::String (s.transientAttackDb, 1) + " dB | sustain " + juce::String (s.transientSustainDb, 1) + " dB");
    lines.add (T ("Saturação: ") + juce::String (s.saturationPct, 0) + "%    Reverb: " + juce::String (s.reverbMixPct, 0)
            + "%    Teto do limiter: " + juce::String (s.ceilingDb, 1) + " dB");

    for (int i = 0; i < lines.size(); ++i)
        g.drawText (lines[i], 40, 365 + i * 32, 680, 28, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.f));
    g.drawText (T ("Preset aplicado em tempo real. Instrumento separado de forma aproximada (sem IA); valores são estimativas."),
                40, 505, 680, 24, juce::Justification::centredLeft);
}
