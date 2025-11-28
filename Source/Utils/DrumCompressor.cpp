#include "DrumCompressor.h"

DrumCompressor::DrumCompressor()
{
    // Default settings suitable for drums
    thresholdDb = -20.0f;
    ratio = 4.0f;
    attackMs = 10.0f;
    releaseMs = 100.0f;
    makeupGainDb = 0.0f;
    enabled = false;
}

void DrumCompressor::prepareToPlay(double newSampleRate, int /*maximumBlockSize*/)
{
    sampleRate = newSampleRate;
    envelope = 0.0f;
    currentGainReductionDb = 0.0f;
    updateCoefficients();
}

void DrumCompressor::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
    {
        currentGainReductionDb = 0.0f;
        return;
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    const float thresholdLinear = juce::Decibels::decibelsToGain(thresholdDb);
    const float makeupLinear = juce::Decibels::decibelsToGain(makeupGainDb);

    float maxGainReduction = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get peak level across all channels for this sample
        float inputPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sampleValue = std::abs(buffer.getSample(ch, sample));
            inputPeak = std::max(inputPeak, sampleValue);
        }

        // Envelope follower (peak detection with attack/release)
        if (inputPeak > envelope)
        {
            // Attack phase
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * inputPeak;
        }
        else
        {
            // Release phase
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * inputPeak;
        }

        // Compute gain reduction
        float gainReduction = 1.0f;

        if (envelope > thresholdLinear && thresholdLinear > 0.0f)
        {
            // How much over threshold (in dB)
            float overDb = juce::Decibels::gainToDecibels(envelope / thresholdLinear);

            // Apply ratio: for ratio R, reduce by (1 - 1/R) of the overshoot
            float reductionDb = overDb * (1.0f - 1.0f / ratio);

            gainReduction = juce::Decibels::decibelsToGain(-reductionDb);

            // Track max gain reduction for metering
            maxGainReduction = std::max(maxGainReduction, reductionDb);
        }

        // Apply gain reduction and makeup gain to all channels
        float totalGain = gainReduction * makeupLinear;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.getWritePointer(ch)[sample] *= totalGain;
        }
    }

    // Store gain reduction for metering (negative dB value)
    currentGainReductionDb = -maxGainReduction;
}

void DrumCompressor::reset()
{
    envelope = 0.0f;
    currentGainReductionDb = 0.0f;
}

void DrumCompressor::setThreshold(float newThreshold)
{
    thresholdDb = juce::jlimit(-60.0f, 0.0f, newThreshold);
}

void DrumCompressor::setRatio(float newRatio)
{
    ratio = juce::jlimit(1.0f, 20.0f, newRatio);
}

void DrumCompressor::setAttack(float newAttackMs)
{
    attackMs = juce::jlimit(0.1f, 100.0f, newAttackMs);
    updateCoefficients();
}

void DrumCompressor::setRelease(float newReleaseMs)
{
    releaseMs = juce::jlimit(10.0f, 500.0f, newReleaseMs);
    updateCoefficients();
}

void DrumCompressor::setMakeupGain(float gainDb)
{
    makeupGainDb = juce::jlimit(0.0f, 24.0f, gainDb);
}

void DrumCompressor::setEnabled(bool shouldEnable)
{
    enabled = shouldEnable;
    if (!enabled)
    {
        currentGainReductionDb = 0.0f;
    }
}

void DrumCompressor::updateCoefficients()
{
    if (sampleRate <= 0.0)
        return;

    // Convert ms to samples and compute coefficients
    // Using exponential smoothing: coeff = exp(-1 / (time_constant * sample_rate))
    // where time_constant = time_ms / 1000

    float attackSamples = static_cast<float>((attackMs / 1000.0) * sampleRate);
    float releaseSamples = static_cast<float>((releaseMs / 1000.0) * sampleRate);

    // Prevent division by zero
    attackSamples = std::max(1.0f, attackSamples);
    releaseSamples = std::max(1.0f, releaseSamples);

    attackCoeff = std::exp(-1.0f / attackSamples);
    releaseCoeff = std::exp(-1.0f / releaseSamples);
}
