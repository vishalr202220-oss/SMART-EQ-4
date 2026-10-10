#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace smart;

SmartEQAudioProcessor::SmartEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SmartEQAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const float defF[numBands] = { 40.f, 120.f, 300.f, 800.f, 2000.f, 5000.f, 10000.f, 18000.f };
    const int   defT[numBands] = { LowCut12, LowShelf, Bell, Bell, Bell, Bell, HighShelf, HighCut12 };

    for (int i = 0; i < numBands; ++i)
    {
        const String id (i), nm ("Band " + String (i + 1));
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "on" + id, 1 }, nm + " On", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "t" + id, 1 },  nm + " Type", typeNames(), defT[i]));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "f" + id, 1 },  nm + " Freq",
                        NormalisableRange<float> (20.f, 20000.f, 0.01f, 0.25f), defF[i]));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "g" + id, 1 },  nm + " Gain",
                        NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "q" + id, 1 },  nm + " Q",
                        NormalisableRange<float> (0.1f, 18.f, 0.01f, 0.4f), 1.0f));
    }
    return layout;
}

SmartEQAudioProcessor::Coeffs::Ptr SmartEQAudioProcessor::makeBandCoeffs (int b, double sr) const
{
    const juce::String id (b);
    if (apvts.getRawParameterValue ("on" + id)->load() < 0.5f)
        return nullptr;

    const int   type = (int) apvts.getRawParameterValue ("t" + id)->load();
    const float f    = juce::jlimit (20.f, (float) (sr * 0.49), apvts.getRawParameterValue ("f" + id)->load());
    const float g    = apvts.getRawParameterValue ("g" + id)->load();
    const float q    = apvts.getRawParameterValue ("q" + id)->load();
    const float gain = juce::Decibels::decibelsToGain (g);

    switch (type)
    {
        case Bell:      return Coeffs::makePeakFilter (sr, f, q, gain);
        case LowShelf:  return Coeffs::makeLowShelf   (sr, f, q, gain);
        case HighShelf: return Coeffs::makeHighShelf  (sr, f, q, gain);
        case LowCut12:  return Coeffs::makeHighPass   (sr, f, q);
        case LowCut24:  return Coeffs::makeHighPass   (sr, f, 0.7071f);   // 2 stage = Linkwitz-Riley 24 dB
        case HighCut12: return Coeffs::makeLowPass    (sr, f, q);
        case HighCut24: return Coeffs::makeLowPass    (sr, f, 0.7071f);
        case Notch:     return Coeffs::makeNotch      (sr, f, q);
        default:        return nullptr;
    }
}

void SmartEQAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSR = sampleRate;
    for (auto& ch : filters) for (auto& band : ch) for (auto& f : band) f.reset();
    for (int b = 0; b < numBands; ++b) cacheOn[b] = false;
}

bool SmartEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void SmartEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();

    for (int b = 0; b < numBands; ++b)
    {
        const juce::String id (b);
        const bool  on   = apvts.getRawParameterValue ("on" + id)->load() > 0.5f;
        const int   type = (int) apvts.getRawParameterValue ("t" + id)->load();
        const float f    = apvts.getRawParameterValue ("f" + id)->load();
        const float g    = apvts.getRawParameterValue ("g" + id)->load();
        const float q    = apvts.getRawParameterValue ("q" + id)->load();

        if (! on) { cacheOn[b] = false; continue; }

        const bool changed = ! cacheOn[b] || type != cacheType[b] || f != cacheF[b] || g != cacheG[b] || q != cacheQ[b];
        if (changed)
        {
            if (! cacheOn[b])
                for (int c = 0; c < 2; ++c) for (int s = 0; s < 2; ++s) filters[c][b][s].reset();

            cacheOn[b] = true; cacheType[b] = type; cacheF[b] = f; cacheG[b] = g; cacheQ[b] = q;

            if (auto co = makeBandCoeffs (b, currentSR))
                for (int c = 0; c < 2; ++c) for (int s = 0; s < 2; ++s)
                    *filters[c][b][s].coefficients = *co;
        }

        const int stages = isTwoStage (type) ? 2 : 1;
        for (int c = 0; c < numCh; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int s = 0; s < stages; ++s)
                for (int i = 0; i < n; ++i)
                    d[i] = filters[c][b][s].processSample (d[i]);
        }
    }

    // Spectrum analyzer ke liye mono copy
    int pos = ringPos.load();
    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        for (int c = 0; c < numCh; ++c) s += buffer.getReadPointer (c)[i];
        ring[(size_t) ((pos + i) & 4095)] = s / (float) juce::jmax (1, numCh);
    }
    ringPos.store ((pos + n) & 4095);
}

void SmartEQAudioProcessor::getAnalyzerSnapshot (float* dest, int numSamples) const
{
    const int p = ringPos.load();
    for (int i = 0; i < numSamples; ++i)
        dest[i] = ring[(size_t) ((p - numSamples + i) & 4095)];
}

juce::AudioProcessorEditor* SmartEQAudioProcessor::createEditor() { return new SmartEQAudioProcessorEditor (*this); }

void SmartEQAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SmartEQAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SmartEQAudioProcessor(); }
