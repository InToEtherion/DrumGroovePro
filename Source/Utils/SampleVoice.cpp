#include "SampleVoice.h"

// Initialize static counter
std::atomic<juce::int64> SampleVoice::globalTriggerCounter { 0 };

void SampleVoice::trigger(const juce::AudioBuffer<float>* buffer, float volumeDb,
                          int midiNote, int velocity, int drumPart, int startSampleInBlock)
{
    if (buffer == nullptr || buffer->getNumSamples() == 0)
    {
        return;
    }

    // Reset fade-out state if this voice was being stolen
    fadingOut.store(false, std::memory_order_relaxed);
    fadeOutSamplesRemaining.store(0, std::memory_order_relaxed);

    // Set all parameters before marking active
    sampleBuffer = buffer;
    playbackPosition.store(0, std::memory_order_relaxed);

    // CRITICAL FIX: Combine sample volume (dB) with MIDI velocity
    // volumeDb comes from SFZ/DrumGizmo definition
    // velocity comes from MIDI note-on message (0-127)
    float sampleGain = dbToLinear(volumeDb);
    float velocityGain = velocityToGain(velocity);
    combinedGainLinear = sampleGain * velocityGain;

    currentVelocity.store(velocity, std::memory_order_relaxed);
    currentDrumPart.store(drumPart, std::memory_order_relaxed);
    currentNote.store(midiNote, std::memory_order_relaxed);

    // Record trigger time for age-based voice stealing
    triggerTime.store(globalTriggerCounter.fetch_add(1, std::memory_order_relaxed), std::memory_order_relaxed);

    // CRITICAL: Store the start sample for sample-accurate playback
    // If startSampleInBlock > 0, the voice will skip that many samples
    // at the beginning of the output buffer before starting to write
    pendingStartSample.store(startSampleInBlock, std::memory_order_relaxed);
    hasPendingStart.store(startSampleInBlock > 0, std::memory_order_relaxed);

    // Mark active last (release fence ensures all above writes are visible)
    active.store(true, std::memory_order_release);
}

void SampleVoice::stop()
{
    // Mark inactive first
    active.store(false, std::memory_order_release);
    fadingOut.store(false, std::memory_order_relaxed);
    currentNote.store(-1, std::memory_order_relaxed);
    playbackPosition.store(0, std::memory_order_relaxed);
    sampleBuffer = nullptr;
    currentVelocity.store(0, std::memory_order_relaxed);
    currentDrumPart.store(0, std::memory_order_relaxed);
    pendingStartSample.store(0, std::memory_order_relaxed);
    hasPendingStart.store(false, std::memory_order_relaxed);
    fadeOutSamplesRemaining.store(0, std::memory_order_relaxed);
    combinedGainLinear = 1.0f;
}

void SampleVoice::startFadeOut(int fadeTimeSamples)
{
    if (!active.load(std::memory_order_acquire))
        return;

    // Don't restart fade if already fading
    if (fadingOut.load(std::memory_order_relaxed))
        return;

    fadeOutTotalSamples.store(fadeTimeSamples, std::memory_order_relaxed);
    fadeOutSamplesRemaining.store(fadeTimeSamples, std::memory_order_relaxed);
    fadingOut.store(true, std::memory_order_release);
}

void SampleVoice::processBlock(juce::AudioBuffer<float>& outputBuffer, int blockSize)
{
    if (!active.load(std::memory_order_acquire) || sampleBuffer == nullptr)
        return;

    const int outputChannels = outputBuffer.getNumChannels();
    const int sampleChannels = sampleBuffer->getNumChannels();
    const int totalSampleLength = sampleBuffer->getNumSamples();

    int currentPos = playbackPosition.load(std::memory_order_relaxed);

    // CRITICAL: Handle sample-accurate start position
    // On the first block after triggering, we may need to start writing
    // at a position other than 0
    int startSample = 0;
    if (hasPendingStart.load(std::memory_order_relaxed))
    {
        startSample = pendingStartSample.load(std::memory_order_relaxed);
        hasPendingStart.store(false, std::memory_order_relaxed);
        pendingStartSample.store(0, std::memory_order_relaxed);

        // CRITICAL: Clamp startSample to valid range to prevent buffer overrun
        startSample = juce::jlimit(0, blockSize - 1, startSample);
    }

    // Calculate how many samples we can write in this block
    int samplesAvailable = blockSize - startSample;
    if (samplesAvailable <= 0)
    {
        // This voice was triggered for a future block - shouldn't happen normally
        return;
    }

    // Don't write beyond what's left in the sample
    int samplesRemaining = totalSampleLength - currentPos;
    int samplesToWrite = juce::jmin(samplesAvailable, samplesRemaining);

    if (samplesToWrite <= 0)
    {
        // Finished playing
        stop();
        return;
    }

    // Check if we're fading out
    bool isFading = fadingOut.load(std::memory_order_acquire);
    int fadeRemaining = fadeOutSamplesRemaining.load(std::memory_order_relaxed);
    int fadeTotal = fadeOutTotalSamples.load(std::memory_order_relaxed);

    // Cache the base gain value for this block
    const float baseGain = combinedGainLinear;

    // Mix sample into output buffer at the correct position
    for (int channel = 0; channel < outputChannels; ++channel)
    {
        // Get output pointer at the correct start position
        auto* outputData = outputBuffer.getWritePointer(channel, startSample);

        // Get source channel (mono samples play on all channels)
        int sourceChannel = juce::jmin(channel, sampleChannels - 1);
        const auto* sampleData = sampleBuffer->getReadPointer(sourceChannel, currentPos);

        if (isFading && fadeRemaining > 0)
        {
            // Apply fade-out envelope
            int localFadeRemaining = fadeRemaining;
            for (int i = 0; i < samplesToWrite; ++i)
            {
                float fadeGain = 1.0f;
                if (localFadeRemaining > 0)
                {
                    fadeGain = static_cast<float>(localFadeRemaining) / static_cast<float>(fadeTotal);
                    localFadeRemaining--;
                }
                outputData[i] += sampleData[i] * baseGain * fadeGain;
            }
        }
        else
        {
            // Normal playback - no fade
            // Use SIMD-friendly loop
            for (int i = 0; i < samplesToWrite; ++i)
            {
                outputData[i] += sampleData[i] * baseGain;
            }
        }
    }

    // Update fade remaining (after processing all channels)
    if (isFading)
    {
        fadeRemaining -= samplesToWrite;
        if (fadeRemaining <= 0)
        {
            // Fade complete - stop the voice
            stop();
            return;
        }
        fadeOutSamplesRemaining.store(fadeRemaining, std::memory_order_relaxed);
    }

    currentPos += samplesToWrite;
    playbackPosition.store(currentPos, std::memory_order_relaxed);

    // Check if we've finished playing
    if (currentPos >= totalSampleLength)
    {
        stop();
    }
}
