#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Analyzer.h"

// Cadeia: EQ 10 bandas -> compressor -> transientes -> saturacao -> reverb -> limiter
class PresetChain
{
public:
    void prepare (const juce::dsp::ProcessSpec& s)
    {
        spec = s;
        for (auto& f : eq) f.prepare (s);
        comp.prepare (s);
        reverb.prepare (s);
        limiter.prepare (s);
        fast = slow = 0.f;
        gain = 1.f;
        setPreset (preset);
    }

    void setPreset (const Preset& p)
    {
        preset = p;
        if (spec.sampleRate <= 0) return;
        static const float freqs[10] = { 31.5f, 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f };
        using C = juce::dsp::IIR::Coefficients<float>;
        for (int i = 0; i < 10; ++i)
        {
            const float g = juce::Decibels::decibelsToGain (p.eqGainDb[i]);
            C::Ptr c = i == 0 ? C::makeLowShelf  (spec.sampleRate, freqs[i], 0.7f, g)
                     : i == 9 ? C::makeHighShelf (spec.sampleRate, freqs[i], 0.7f, g)
                              : C::makePeakFilter (spec.sampleRate, freqs[i], 1.0f, g);
            *eq[(size_t) i].state = *c;
        }
        comp.setThreshold (p.compThresholdDb);
        comp.setRatio (p.compRatio);
        comp.setAttack (p.compAttackMs);
        comp.setRelease (p.compReleaseMs);
        limiter.setThreshold (p.ceilingDb);
        limiter.setRelease (50.f);

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.3f; rp.damping = 0.5f; rp.width = 1.f;
        rp.wetLevel = p.reverbMixPct / 100.f * 0.5f;
        rp.dryLevel = 1.f;
        reverb.setParameters (rp);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        for (auto& f : eq) f.process (ctx);
        comp.process (ctx);
        transient (buffer);
        saturate (buffer);
        reverb.process (ctx);
        limiter.process (ctx);
    }

private:
    void transient (juce::AudioBuffer<float>& b)
    {
        const float rate = (float) spec.sampleRate;
        auto coef = [rate] (float ms) { return std::exp (-1.f / (0.001f * ms * rate)); };
        const float fa = coef (1.f), fr = coef (20.f), sa = coef (30.f), sr = coef (150.f), gc = coef (5.f);
        float* L = b.getWritePointer (0);
        float* R = b.getWritePointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            fast = x > fast ? fa * fast + (1.f - fa) * x : fr * fast + (1.f - fr) * x;
            slow = x > slow ? sa * slow + (1.f - sa) * x : sr * slow + (1.f - sr) * x;
            const float ratio = fast / (slow + 1e-6f);
            const float gdb = ratio > 1.f ? preset.transientAttackDb * std::min (1.f, ratio - 1.f)
                                          : preset.transientSustainDb * std::min (1.f, 1.f - ratio);
            gain = gc * gain + (1.f - gc) * juce::Decibels::decibelsToGain (gdb);
            L[i] *= gain;
            R[i] *= gain;
        }
    }

    void saturate (juce::AudioBuffer<float>& b)
    {
        const float m = juce::jlimit (0.f, 1.f, preset.saturationPct / 100.f);
        if (m <= 0.f) return;
        const float drive = 1.f + m * 4.f, norm = 1.f / std::tanh (drive);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                d[i] = d[i] * (1.f - m) + std::tanh (drive * d[i]) * norm * m;
        }
    }

    juce::dsp::ProcessSpec spec { 0.0, 0, 0 };
    Preset preset;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, 10> eq;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    float fast = 0.f, slow = 0.f, gain = 1.f;
};
