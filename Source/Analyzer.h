#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

struct Analysis
{
    static constexpr int numBands = 10;   // 31 Hz ... 16 kHz (oitavas)
    float bandDb[numBands] {};
    float rmsDb = -100.f, peakDb = -100.f, crestDb = 0.f, widthPct = 0.f;
    int firstBand = 0, lastBand = numBands - 1;
    bool fullRange = true;
    bool valid = false;
};

struct Preset
{
    juce::String instrument;
    float eqGainDb[Analysis::numBands] {};
    float compThresholdDb = -18.f, compRatio = 3.f, compAttackMs = 10.f, compReleaseMs = 80.f;
    float transientAttackDb = 0.f, transientSustainDb = 0.f;
    float saturationPct = 0.f, reverbMixPct = 0.f, ceilingDb = -0.3f;
};

namespace Analyzer
{
    // Le ate 60 s do meio do arquivo, separa o instrumento (aproximado) e mede.
    // Pesado: chame fora da thread de interface.
    Analysis analyseFile (const juce::File& file, const juce::String& instrument);
    Preset   makePreset  (const Analysis& a, const juce::String& instrument);
    juce::var presetToJson (const Preset& p);
    Preset    presetFromJson (const juce::var& v);
}
