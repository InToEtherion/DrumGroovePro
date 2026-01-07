#pragma once

#include <JuceHeader.h>

/**
 * Manages mixer preset storage and retrieval
 * Stores all mixer settings including EQ, compressor, and channel settings
 * Preset format version 3: 7 channels with SimpleEQ (3-band) and Compressor
 */
class EQPresetManager
{
public:
    // Per-channel settings (7 channels: Kick1, Kick2, Snare, Hi-Hat, Toms, Crash, Rides)
    struct ChannelPreset
    {
        // Mixer
        float volume { 0.8f };
        float pan { 0.0f };
        float reverbSend { 0.0f };

        // SimpleEQ (3-band)
        float eqLowGain { 0.0f };
        float eqMidGain { 0.0f };
        float eqHighGain { 0.0f };

        // Compressor
        bool compEnabled { false };
        float compThreshold { -20.0f };
        float compRatio { 4.0f };
        float compAttack { 10.0f };
        float compRelease { 100.0f };
        float compMakeup { 0.0f };
    };

    struct EQPreset
    {
        juce::String name;
        int version { 3 };  // Preset format version

        // Master settings
        float masterVolume { 1.0f };
        bool masterEQEnabled { false };

        // 8-band master EQ gains
        std::array<float, 8> masterEQGains { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

        // Reverb settings
        float reverbRoomSize { 0.5f };
        float reverbDamping { 0.5f };
        float reverbWetLevel { 0.33f };

        // 7 channel presets
        std::array<ChannelPreset, 7> channels;
    };

    EQPresetManager();
    ~EQPresetManager() = default;

    // Preset management
    void savePreset(const EQPreset& preset);
    EQPreset loadPreset(const juce::String& name);
    void deletePreset(const juce::String& name);
    juce::StringArray getPresetList() const;
    bool presetExists(const juce::String& name) const;

    // Preset directory
    juce::File getPresetsDirectory() const;

private:
    juce::File presetsDir;

    juce::File getPresetFile(const juce::String& name) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPresetManager)
};
