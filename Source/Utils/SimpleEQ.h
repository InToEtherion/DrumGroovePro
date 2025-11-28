#pragma once

#include <JuceHeader.h>

/**
 * Simple 3-Band EQ for drum channels
 * - Low: Low shelf at 100Hz
 * - Mid: Peak filter at 1kHz
 * - High: High shelf at 8kHz
 * 
 * Each band has gain control only (-12dB to +12dB)
 */
class SimpleEQ
{
public:
    enum Band
    {
        Low = 0,
        Mid,
        High,
        NumBands
    };

    SimpleEQ();
    ~SimpleEQ() = default;

    void prepareToPlay(double sampleRate, int maximumBlockSize);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void reset();

    // Gain control for each band (-12 to +12 dB)
    void setGain(Band band, float gainDb);
    float getGain(Band band) const;

    // Get fixed frequencies (read-only)
    static float getFrequency(Band band);
    static juce::String getBandName(Band band);

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    double currentSampleRate { 44100.0 };
    std::array<float, NumBands> gains { 0.0f, 0.0f, 0.0f };  // dB

    // Stereo filters (ProcessorDuplicator handles both channels)
    std::array<juce::dsp::ProcessorDuplicator<Filter, Coefficients>, NumBands> filters;

    // Fixed frequencies
    static constexpr float lowFreq = 100.0f;
    static constexpr float midFreq = 1000.0f;
    static constexpr float highFreq = 8000.0f;
    static constexpr float midQ = 0.707f;  // Butterworth Q for mid band

    void updateFilterCoefficients(Band band);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEQ)
};
