#pragma once

#include <JuceHeader.h>

/**
 * Simple Compressor for drum channels
 * Optimized for drum sounds with typical settings:
 * - Fast attack for snare punch
 * - Medium release for natural decay
 * - Moderate ratios for glue compression
 */
class DrumCompressor
{
public:
    DrumCompressor();
    ~DrumCompressor() = default;

    void prepareToPlay(double sampleRate, int maximumBlockSize);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void reset();

    // Parameters
    void setThreshold(float thresholdDb);   // -60 to 0 dB
    void setRatio(float ratio);             // 1.0 to 20.0
    void setAttack(float attackMs);         // 0.1 to 100 ms
    void setRelease(float releaseMs);       // 10 to 500 ms
    void setMakeupGain(float gainDb);       // 0 to +24 dB
    void setEnabled(bool shouldEnable);

    // Getters
    float getThreshold() const { return thresholdDb; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attackMs; }
    float getRelease() const { return releaseMs; }
    float getMakeupGain() const { return makeupGainDb; }
    bool isEnabled() const { return enabled; }

    // Get current gain reduction for metering (in dB, negative value)
    float getGainReduction() const { return currentGainReductionDb; }

private:
    // Parameters
    float thresholdDb { -20.0f };
    float ratio { 4.0f };
    float attackMs { 10.0f };
    float releaseMs { 100.0f };
    float makeupGainDb { 0.0f };
    bool enabled { false };

    // Internal state
    double sampleRate { 44100.0 };
    float attackCoeff { 0.0f };
    float releaseCoeff { 0.0f };
    float envelope { 0.0f };  // Current envelope follower value
    float currentGainReductionDb { 0.0f };  // For metering

    void updateCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumCompressor)
};
