#pragma once

#include <JuceHeader.h>
#include "DrumMixerChannel.h"
#include "ReverbProcessor.h"

/**
 * Complete drum mixer managing 7 drum parts + master reverb
 * Each part has its own EQ, volume, pan, and reverb send
 * Maps MIDI notes to appropriate channels
 *
 * Parts: Kick1, Kick2, Snare, Hi-Hat, Toms, Crash, Rides
 */
class DrumMixer
{
public:
    enum DrumPart
    {
        Kick1 = 0,         // Primary kick (note 36)
        Kick2,             // Secondary kick (note 35)
        Snare,
        HiHat,
        Toms,
        CrashChina,        // Crashes, Chinas, Splashes, and effects
        Rides,             // Ride cymbals (bow, bell, edge)
        NumParts           // = 7
    };

    DrumMixer();
    ~DrumMixer() = default;

    void prepareToPlay(double sampleRate, int maximumBlockSize);

    // Process per-part buffers (new architecture - each part in its own buffer)
    void processPerPartBuffers(std::array<juce::AudioBuffer<float>, NumParts>& partBuffers,
                               juce::AudioBuffer<float>& outputBuffer);

    // Legacy single buffer processing (kept for compatibility)
    void processBlock(juce::AudioBuffer<float>& buffer);

    void reset();

    // Channel access
    DrumMixerChannel& getChannel(DrumPart part) { return channels[part]; }
    const DrumMixerChannel& getChannel(DrumPart part) const { return channels[part]; }

    // Also allow access by index
    DrumMixerChannel& getChannel(int partIndex) { return channels[partIndex]; }
    const DrumMixerChannel& getChannel(int partIndex) const { return channels[partIndex]; }

    // Master reverb access
    ReverbProcessor& getReverb() { return masterReverb; }
    const ReverbProcessor& getReverb() const { return masterReverb; }

    // Master volume (allows up to 4x/400% boost)
    void setMasterVolume(float linearGain);
    float getMasterVolume() const { return masterVolume; }

    // Reverb enable/disable
    void setReverbEnabled(bool enabled) { reverbEnabled = enabled; }
    bool isReverbEnabled() const { return reverbEnabled; }

    // Master EQ (8-band) - stored for persistence
    void setMasterEQGain(int band, float gainDb);
    float getMasterEQGain(int band) const;
    void setMasterEQEnabled(bool enabled) { masterEQEnabled = enabled; }
    bool isMasterEQEnabled() const { return masterEQEnabled; }

    // Utility: Get drum part from MIDI note (static for use by SampleEngine)
    // When kickAlternation is false, both 35 and 36 go to Kick1
    // When kickAlternation is true, caller should use getAlternatingKickPart() instead
    static DrumPart getDrumPartForNote(int midiNote);

    // Get number of parts
    static constexpr int getNumParts() { return NumParts; }

    // Get part name for display
    static juce::String getPartName(DrumPart part);
    static juce::String getPartName(int partIndex);

    // Solo management
    bool isAnySoloed() const;

    // State save/restore for persistence
    juce::ValueTree saveState() const;
    void restoreState(const juce::ValueTree& state);

    // Current preset name (for UI persistence)
    void setCurrentPresetName(const juce::String& name) { currentPresetName = name; }
    juce::String getCurrentPresetName() const { return currentPresetName; }

private:
    std::array<DrumMixerChannel, NumParts> channels;
    ReverbProcessor masterReverb;
    float masterVolume { 1.0f };
    bool reverbEnabled { true };
    bool masterEQEnabled { false };
    std::array<float, 8> masterEQGains { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    juce::String currentPresetName;  // Tracks currently active preset for UI

    // 8-band master EQ filters
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    std::array<juce::dsp::ProcessorDuplicator<Filter, Coefficients>, 8> masterEQFilters;
    static constexpr std::array<float, 8> masterEQFreqs = { 60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 15000.0f };
    double eqSampleRate { 44100.0 };

    void updateMasterEQFilter(int band);

    juce::AudioBuffer<float> reverbSendBuffer;
    juce::AudioBuffer<float> tempBuffer;  // For per-part processing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumMixer)
};
