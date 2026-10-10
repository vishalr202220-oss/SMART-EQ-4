#pragma once
#include "PluginProcessor.h"

class SmartEQAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit SmartEQAudioProcessorEditor (SmartEQAudioProcessor&);
    ~SmartEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;

    float getParam (const juce::String& id) const;
    void  setParam (const juce::String& id, float v);
    juce::Point<float> nodePos (int band) const;
    int   nodeAt (juce::Point<float> p) const;
    double sr() const { return processor.getSampleRate() > 0 ? processor.getSampleRate() : 44100.0; }

    SmartEQAudioProcessor& proc;
    juce::Rectangle<float> plot;
    int selected = -1;

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    float fftData[fftSize * 2] {};
    float spec[fftSize / 2 + 1] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartEQAudioProcessorEditor)
};
