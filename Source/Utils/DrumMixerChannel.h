#pragma once

#include <JuceHeader.h>
#include "SimpleEQ.h"
#include "DrumCompressor.h"

/**
 * One mixer channel for a drum part (Kick, Snare, etc.)
 * Includes: Volume, Reverb Send, 3-band EQ, Compressor, Solo/Mute
 * 
 * Signal flow: Input → EQ → Compressor → Volume/Pan → Output + Reverb Send
 */
class DrumMixerChannel
{
public:
    DrumMixerChannel();
    ~DrumMixerChannel() = default;

    void prepareToPlay(double sampleRate, int maximumBlockSize);
    void processBlock(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& reverbSendBuffer);
    void reset();

    // Volume & Pan
    void setVolume(float linearGain);      // 0.0 to 1.0
    void setPan(float pan);                // -1.0 (left) to +1.0 (right)
    float getVolume() const { return volume; }
    float getPan() const { return pan; }

    // Reverb send
    void setReverbSend(float send);        // 0.0 to 1.0
    float getReverbSend() const { return reverbSend; }

    // Solo/Mute
    void setSolo(bool shouldSolo);
    void setMute(bool shouldMute);
    bool isSoloed() const { return solo; }
    bool isMuted() const { return mute; }

    // 3-band EQ access
    SimpleEQ& getEQ() { return eq; }
    const SimpleEQ& getEQ() const { return eq; }

    // Compressor access
    DrumCompressor& getCompressor() { return compressor; }
    const DrumCompressor& getCompressor() const { return compressor; }

private:
    SimpleEQ eq;
    DrumCompressor compressor;

    float volume { 0.8f };
    float pan { 0.0f };
    float reverbSend { 0.0f };
    bool solo { false };
    bool mute { false };

    // Panning gains
    float leftGain { 0.707f };
    float rightGain { 0.707f };

    void updatePanGains();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumMixerChannel)
};
