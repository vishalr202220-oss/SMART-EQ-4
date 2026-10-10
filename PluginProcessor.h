#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace smart
{
    constexpr int numBands = 8;

    enum FilterType { Bell = 0, LowShelf, HighShelf, LowCut12, LowCut24, HighCut12, HighCut24, Notch, numTypes };

    inline juce::StringArray typeNames()
    {
        return { "Bell", "Low Shelf", "High Shelf", "Low Cut 12 dB/oct", "Low Cut 24 dB/oct",
                 "High Cut 12 dB/oct", "High Cut 24 dB/oct", "Notch" };
    }
    inline bool hasGain (int t)     { return t == Bell || t == LowShelf || t == HighShelf; }
    inline bool isTwoStage (int t)  { return t == LowCut24 || t == HighCut24; }
}

class SmartEQAudioProcessor : public juce::AudioProcessor
{
public:
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    SmartEQAudioProcessor();
    ~SmartEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SMART EQ PRODUCERTOY"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Band ke current parameters se coefficients (null agar band off hai)
    Coeffs::Ptr makeBandCoeffs (int band, double sampleRate) const;
    void getAnalyzerSnapshot (float* dest, int numSamples) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::dsp::IIR::Filter<float> filters[2][smart::numBands][2];

    bool  cacheOn[smart::numBands] {};
    int   cacheType[smart::numBands] {};
    float cacheF[smart::numBands] {}, cacheG[smart::numBands] {}, cacheQ[smart::numBands] {};

    std::array<float, 4096> ring {};
    std::atomic<int> ringPos { 0 };
    double currentSR = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartEQAudioProcessor)
};
