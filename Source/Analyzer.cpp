#include "Analyzer.h"
#include <cmath>
#include <algorithm>

namespace
{
    const float centres[Analysis::numBands] = { 31.5f, 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f };

    int bandFor (double f)
    {
        if (f < 22.0 || f > 22627.0) return -1;
        for (int i = 0; i < Analysis::numBands; ++i)
            if (f < centres[i] * 1.4142f) return i;
        return -1;
    }

    struct StemSpec { bool percussive; bool separate; int first, last; };

    StemSpec specFor (const juce::String& s)
    {
        if (s == "Kick")      return { true,  true,  0, 2 };
        if (s == "Snare")     return { true,  true,  3, 8 };
        if (s == "Baixo")     return { false, true,  0, 3 };
        if (s == "Voz")       return { false, true,  3, 8 };
        if (s == "Mix geral") return { true,  false, 0, 9 };
        return { true, true, 0, 9 };   // Bateria
    }

    float median (float* v, int n)
    {
        std::nth_element (v, v + n / 2, v + n);
        return v[n / 2];
    }

    // Separacao harmonico/percussivo (HPSS) por filtro de mediana no espectrograma.
    // Nao e IA: e aproximada. Percussivo ~ bateria; harmonico ~ baixo/voz/instrumentos.
    void separate (const std::vector<float>& L, const std::vector<float>& R, double sr,
                   const StemSpec& sp, std::vector<float>& oL, std::vector<float>& oR)
    {
        const int order = 11, N = 1 << order, hop = N / 4, B = N / 2 + 1, K = 8;
        const int len = (int) L.size();
        const int frames = (len - N) / hop + 1;
        oL = L; oR = R;
        if (frames < 2 * K + 2) return;

        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> wf (N, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> w ((size_t) N, 1.f);
        wf.multiplyWithWindowingTable (w.data(), (size_t) N);

        std::vector<float> data ((size_t) 2 * N);
        std::vector<float> mag ((size_t) frames * B), mask ((size_t) frames * B);

        for (int f = 0; f < frames; ++f)
        {
            std::fill (data.begin(), data.end(), 0.f);
            for (int i = 0; i < N; ++i)
                data[(size_t) i] = 0.5f * (L[(size_t) (f * hop + i)] + R[(size_t) (f * hop + i)]) * w[(size_t) i];
            fft.performRealOnlyForwardTransform (data.data(), true);
            for (int k = 0; k < B; ++k)
                mag[(size_t) f * B + k] = std::hypot (data[(size_t) 2 * k], data[(size_t) 2 * k + 1]);
        }

        const double binHz = sr / N;
        const double loHz = centres[sp.first] / 1.4142, hiHz = centres[sp.last] * 1.4142;
        float tmp[2 * K + 1];
        for (int f = 0; f < frames; ++f)
            for (int k = 0; k < B; ++k)
            {
                int n = 0;
                for (int d = -K; d <= K; ++d)
                {
                    const int ff = f + d;
                    if (ff >= 0 && ff < frames) tmp[n++] = mag[(size_t) ff * B + k];
                }
                const float H = median (tmp, n);
                n = 0;
                for (int d = -K; d <= K; ++d)
                {
                    const int kk = k + d;
                    if (kk >= 0 && kk < B) tmp[n++] = mag[(size_t) f * B + kk];
                }
                const float P = median (tmp, n);
                const float h2 = H * H, p2 = P * P;
                float m = (sp.percussive ? p2 : h2) / (h2 + p2 + 1e-12f);
                const double hz = k * binHz;
                if (hz < loHz || hz > hiHz) m = 0.f;
                mask[(size_t) f * B + k] = m;
            }

        auto synth = [&] (const std::vector<float>& x, bool useMask, std::vector<float>& out)
        {
            out.assign ((size_t) len, 0.f);
            for (int f = 0; f < frames; ++f)
            {
                std::fill (data.begin(), data.end(), 0.f);
                for (int i = 0; i < N; ++i)
                    data[(size_t) i] = x[(size_t) (f * hop + i)] * w[(size_t) i];
                fft.performRealOnlyForwardTransform (data.data(), false);
                if (useMask)
                    for (int k = 0; k <= N / 2; ++k)
                    {
                        const float m = mask[(size_t) f * B + k];
                        data[(size_t) 2 * k] *= m; data[(size_t) 2 * k + 1] *= m;
                        if (k > 0 && k < N / 2)
                        {
                            data[(size_t) 2 * (N - k)] *= m; data[(size_t) 2 * (N - k) + 1] *= m;
                        }
                    }
                fft.performRealOnlyInverseTransform (data.data());
                for (int i = 0; i < N; ++i)
                    out[(size_t) (f * hop + i)] += data[(size_t) i] * w[(size_t) i];
            }
        };

        // calibra o ganho da reconstrucao (independe da escala da FFT)
        std::vector<float> full;
        synth (L, false, full);
        double eo = 0, er = 0;
        for (int i = N; i < len - N; ++i) { eo += (double) L[(size_t) i] * L[(size_t) i]; er += (double) full[(size_t) i] * full[(size_t) i]; }
        const float g = er > 1e-12 ? (float) std::sqrt (eo / er) : 1.f;

        synth (L, true, oL);
        synth (R, true, oR);
        for (auto& v : oL) v *= g;
        for (auto& v : oR) v *= g;
    }

    Analysis measure (const std::vector<float>& L, const std::vector<float>& R, double sr)
    {
        Analysis a;
        const int order = 12, N = 1 << order, hop = N / 2;
        const int len = (int) L.size();
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window (N, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> data ((size_t) 2 * N);
        double bandPow[Analysis::numBands] = {};
        double sumSq = 0, sumM = 0, sumS = 0, count = 0;
        float peak = 0.f;
        long frames = 0;

        for (int pos = 0; pos + N <= len; pos += hop)
        {
            std::fill (data.begin(), data.end(), 0.f);
            for (int i = 0; i < N; ++i)
            {
                const float l = L[(size_t) (pos + i)], r = R[(size_t) (pos + i)];
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r);
                peak = std::max ({ peak, std::abs (l), std::abs (r) });
                sumSq += (double) l * l + (double) r * r;
                sumM += (double) mid * mid;
                sumS += (double) side * side;
                data[(size_t) i] = mid;
            }
            count += 2.0 * N;
            window.multiplyWithWindowingTable (data.data(), (size_t) N);
            fft.performFrequencyOnlyForwardTransform (data.data());
            for (int k = 1; k < N / 2; ++k)
            {
                const int b = bandFor (k * sr / N);
                if (b >= 0) bandPow[b] += (double) data[(size_t) k] * data[(size_t) k];
            }
            ++frames;
        }
        if (frames == 0 || sumSq <= 1e-9) return a;

        for (int i = 0; i < Analysis::numBands; ++i)
            a.bandDb[i] = (float) (10.0 * std::log10 (std::max (bandPow[i] / frames, 1e-12)));
        a.rmsDb    = (float) (10.0 * std::log10 (sumSq / count + 1e-12));
        a.peakDb   = 20.f * std::log10 (peak + 1e-9f);
        a.crestDb  = a.peakDb - a.rmsDb;
        a.widthPct = (float) (100.0 * sumS / (sumM + sumS + 1e-12));
        a.valid = true;
        return a;
    }
}

Analysis Analyzer::analyseFile (const juce::File& file, const juce::String& instrument)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) return {};

    const double sr = reader->sampleRate;
    const juce::int64 total = reader->lengthInSamples;
    const juce::int64 len = std::min<juce::int64> (total, (juce::int64) (60.0 * sr));
    const juce::int64 start = (total - len) / 2;
    if (len < 16384) return {};

    const bool stereo = reader->numChannels > 1;
    juce::AudioBuffer<float> buf (2, (int) len);
    buf.clear();
    reader->read (&buf, 0, (int) len, start, true, stereo);
    std::vector<float> L (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    std::vector<float> R = stereo ? std::vector<float> (buf.getReadPointer (1), buf.getReadPointer (1) + len) : L;

    const auto sp = specFor (instrument);
    Analysis a;
    if (sp.separate)
    {
        std::vector<float> dl, dr;
        separate (L, R, sr, sp, dl, dr);
        a = measure (dl, dr, sr);
    }
    else
        a = measure (L, R, sr);

    a.firstBand = sp.first;
    a.lastBand = sp.last;
    a.fullRange = (sp.first == 0 && sp.last == Analysis::numBands - 1);
    return a;
}

Preset Analyzer::makePreset (const Analysis& a, const juce::String& instrument)
{
    Preset p;
    p.instrument = instrument;

    if (a.fullRange)
    {
        // desvio da curva em relacao a uma curva "tipica" (queda de 1,5 dB/oitava)
        float mean = 0.f;
        for (float v : a.bandDb) mean += v;
        mean /= Analysis::numBands;
        for (int i = 0; i < Analysis::numBands; ++i)
        {
            const float typical = -1.5f * ((float) i - 4.5f);
            p.eqGainDb[i] = juce::jlimit (-6.f, 6.f, (a.bandDb[i] - mean) - typical);
        }
    }
    else
    {
        // instrumento limitado a uma faixa: ajusta so dentro dela, relativo a media da faixa
        float mean = 0.f;
        const int n = a.lastBand - a.firstBand + 1;
        for (int i = a.firstBand; i <= a.lastBand; ++i) mean += a.bandDb[i];
        mean /= (float) n;
        for (int i = a.firstBand; i <= a.lastBand; ++i)
            p.eqGainDb[i] = juce::jlimit (-6.f, 6.f, a.bandDb[i] - mean);
    }

    p.compRatio = a.crestDb > 16.f ? 2.f : a.crestDb > 12.f ? 3.f : a.crestDb > 9.f ? 4.f : 6.f;
    p.compThresholdDb = juce::jlimit (-40.f, -6.f, a.rmsDb + 0.4f * a.crestDb);

    struct T { const char* n; float atk, rel; };
    static const T times[] = { {"Bateria",10,80}, {"Kick",15,100}, {"Snare",5,120},
                               {"Baixo",20,150}, {"Voz",8,100}, {"Mix geral",30,200} };
    p.compAttackMs = 10.f; p.compReleaseMs = 80.f;
    for (auto& t : times)
        if (instrument == t.n) { p.compAttackMs = t.atk; p.compReleaseMs = t.rel; }

    p.transientAttackDb  = a.crestDb > 14.f ? 3.f : -1.f;
    p.transientSustainDb = a.crestDb > 14.f ? -1.f : 2.f;
    p.saturationPct = juce::jlimit (0.f, 40.f, 10.f + (12.f - a.crestDb) * 2.f);
    p.reverbMixPct  = juce::jlimit (0.f, 25.f, a.widthPct * 0.3f);
    p.ceilingDb = instrument == "Mix geral" ? -1.f : -0.3f;
    return p;
}

juce::var Analyzer::presetToJson (const Preset& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "Mix Play");
    o->setProperty ("instrument", p.instrument);
    juce::Array<juce::var> eq;
    for (int i = 0; i < Analysis::numBands; ++i)
    {
        auto* b = new juce::DynamicObject();
        b->setProperty ("hz", centres[i]);
        b->setProperty ("gainDb", p.eqGainDb[i]);
        eq.add (juce::var (b));
    }
    o->setProperty ("eq", eq);
    o->setProperty ("compThresholdDb", p.compThresholdDb);
    o->setProperty ("compRatio", p.compRatio);
    o->setProperty ("compAttackMs", p.compAttackMs);
    o->setProperty ("compReleaseMs", p.compReleaseMs);
    o->setProperty ("transientAttackDb", p.transientAttackDb);
    o->setProperty ("transientSustainDb", p.transientSustainDb);
    o->setProperty ("saturationPct", p.saturationPct);
    o->setProperty ("reverbMixPct", p.reverbMixPct);
    o->setProperty ("ceilingDb", p.ceilingDb);
    return juce::var (o);
}

Preset Analyzer::presetFromJson (const juce::var& v)
{
    Preset p;
    if (! v.isObject()) return p;
    auto f = [&v] (const char* k) { return (float) (double) v[k]; };
    p.instrument = v["instrument"].toString();
    if (auto* eq = v["eq"].getArray())
        for (int i = 0; i < juce::jmin (eq->size(), Analysis::numBands); ++i)
            p.eqGainDb[i] = (float) (double) (*eq)[i]["gainDb"];
    p.compThresholdDb = f ("compThresholdDb"); p.compRatio = f ("compRatio");
    p.compAttackMs = f ("compAttackMs");       p.compReleaseMs = f ("compReleaseMs");
    p.transientAttackDb = f ("transientAttackDb"); p.transientSustainDb = f ("transientSustainDb");
    p.saturationPct = f ("saturationPct"); p.reverbMixPct = f ("reverbMixPct"); p.ceilingDb = f ("ceilingDb");
    return p;
}
