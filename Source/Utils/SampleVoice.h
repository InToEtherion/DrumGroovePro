#pragma once

#include <JuceHeader.h>
#include <atomic>

/**
 * Represents a single playing sample (one drum hit)
 * Handles playback of one audio buffer with volume control
 *
 * CRITICAL: Supports sample-accurate triggering via pendingStartSample
 * This ensures notes start at the exact sample position they were triggered
 *
 * VELOCITY SCALING: MIDI velocity (0-127) is applied as a gain multiplier
 * Velocity 127 = full volume, Velocity 64 = ~50% volume, Velocity 0 = silent
 *
 * POLYPHONIC: Multiple voices can play the same note simultaneously
 * This allows natural decay layering for fast repeated hits (metal drumming)
 *
 * CROSSFADE SUPPORT: Voices can fade out smoothly when stolen to prevent clicks
 */
class SampleVoice
{
public:
    SampleVoice() = default;
    ~SampleVoice() = default;

    // Start playing a sample with drum part assignment
    // startSampleInBlock: the sample offset within the current block where playback should begin
    void trigger(const juce::AudioBuffer<float>* buffer, float volumeDb, int midiNote,
                 int velocity, int drumPart, int startSampleInBlock, float ampVeltrack = 100.0f);

    // Stop playing immediately (hard stop)
    void stop();

    // Start a fade-out for smooth voice stealing (prevents clicks)
    // fadeTimeSamples: number of samples over which to fade out
    void startFadeOut(int fadeTimeSamples = 256);

    // Process audio for this voice
    // blockSize: total samples in the current audio block
    void processBlock(juce::AudioBuffer<float>& outputBuffer, int blockSize);

    // Check if voice is currently playing
    bool isActive() const { return active.load(std::memory_order_acquire); }

    // Check if voice is fading out (being stolen)
    bool isFadingOut() const { return fadingOut.load(std::memory_order_acquire); }

    // Get the MIDI note this voice is playing
    int getMidiNote() const { return currentNote.load(std::memory_order_acquire); }

    // Get the velocity of this voice
    int getVelocity() const { return currentVelocity.load(std::memory_order_relaxed); }

    int getCurrentNote() const { return currentNote.load(std::memory_order_acquire); }

    // Get the drum part this voice belongs to (for per-part routing)
    int getDrumPart() const { return currentDrumPart.load(std::memory_order_relaxed); }

    // Get current playback position (for voice stealing priority - older voices have higher positions)
    int getPlaybackPosition() const { return playbackPosition.load(std::memory_order_relaxed); }

    // Get the time when this voice was triggered (for age-based stealing)
    juce::int64 getTriggerTime() const { return triggerTime.load(std::memory_order_relaxed); }

private:
    const juce::AudioBuffer<float>* sampleBuffer { nullptr };
    std::atomic<int> playbackPosition { 0 };

    // Combined gain from sample volume (dB) and velocity
    float combinedGainLinear { 1.0f };

    float ampVeltrack = 100.0f;

    std::atomic<bool> active { false };
    std::atomic<int> currentNote { -1 };
    std::atomic<int> currentVelocity { 0 };
    std::atomic<int> currentDrumPart { 0 };  // Which drum part this voice belongs to

    // CRITICAL: Sample-accurate triggering support (atomic for thread safety)
    // When a voice is triggered mid-block, this stores which sample to start at
    std::atomic<int> pendingStartSample { 0 };
    std::atomic<bool> hasPendingStart { false };

    // Fade-out support for smooth voice stealing
    std::atomic<bool> fadingOut { false };
    std::atomic<int> fadeOutSamplesRemaining { 0 };
    std::atomic<int> fadeOutTotalSamples { 256 };

    // Trigger timestamp for age-based voice stealing
    std::atomic<juce::int64> triggerTime { 0 };

    // Static counter for trigger timestamps
    static std::atomic<juce::int64> globalTriggerCounter;

    // Convert dB to linear gain
    static float dbToLinear(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }

    // Convert MIDI velocity (0-127) to linear gain (0.0-1.0)
    // Uses a slight curve for more natural response
    static float velocityToGain(int velocity)
    {
        if (velocity <= 0)
            return 0.0f;
        if (velocity >= 127)
            return 1.0f;

        // IMPROVED VELOCITY CURVE: Gentle power curve (1.2)
        // Not as aggressive as 1.5, but better than linear
        // Makes low velocities more audible without excessive attenuation
        float normalizedVel = static_cast<float>(velocity) / 127.0f;
        return std::pow(normalizedVel, 1.2f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleVoice)
};
