#include "PluginProcessor.h"
#include "PluginEditor.h"

MixPlayProcessor::MixPlayProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

void MixPlayProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    chain.prepare ({ sampleRate, (juce::uint32) blockSize, 2 });
}

void MixPlayProcessor::setPreset (const Preset& p)
{
    preset = p;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pending = p;
    }
    hasPreset = true;
    hasPending = true;
}

void MixPlayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (hasPending.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked()) chain.setPreset (pending);
        else hasPending = true;
    }

    if (! hasPreset || bypass || buffer.getNumChannels() < 2) return;
    chain.process (buffer);
}

void MixPlayProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (! hasPreset) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("preset", Analyzer::presetToJson (preset));
    o->setProperty ("bypass", bypass.load());
    const auto s = juce::JSON::toString (juce::var (o));
    dest.append (s.toRawUTF8(), s.getNumBytesAsUTF8());
}

void MixPlayProcessor::setStateInformation (const void* data, int size)
{
    auto v = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, size));
    if (! v.isObject()) return;
    bypass = (bool) v["bypass"];
    auto p = Analyzer::presetFromJson (v["preset"]);
    if (p.instrument.isNotEmpty())
    {
        instrument = p.instrument;
        setPreset (p);
    }
}

juce::AudioProcessorEditor* MixPlayProcessor::createEditor() { return new MixPlayEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixPlayProcessor(); }
