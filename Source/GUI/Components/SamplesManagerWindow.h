#pragma once

#include <JuceHeader.h>
#include "../Utils/SampleDownloader.h"
#include "../Core/DrumLibraryManager.h"
#include "EQPresetManager.h"

// Forward declarations
class DrumMixer;
class DrumMixerChannel;
class SimpleEQ;
class DrumCompressor;
class ReverbProcessor;
class DrumGrooveProcessor;

/**
 * Compact UI component for one mixer channel (one drum part)
 * Shows: Volume, Reverb Send, 3-band EQ (vertical sliders), Compressor (horizontal), Solo/Mute
 */
class MixerChannelComponent : public juce::Component
{
public:
    MixerChannelComponent(const juce::String& name, DrumMixerChannel& channel, bool isKick1 = false);
    ~MixerChannelComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateFromChannel();

    // Kick alternation callback (only used for Kick1 channel)
    std::function<void(bool)> onKickAlternationChanged;

    // Set kick alternation toggle state
    void setKickAlternationEnabled(bool enabled);

private:
    juce::String channelName;
    DrumMixerChannel& mixerChannel;
    bool isKick1Channel { false };

    // Name label
    juce::Label nameLabel;

    // Kick alternation checkbox (only visible for Kick1)
    juce::ToggleButton alternToggle;

    // Volume & Reverb (horizontal sliders)
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    juce::Slider reverbSendSlider;
    juce::Label reverbSendLabel;

    // 3-band EQ - VERTICAL sliders
    juce::GroupComponent eqGroup;
    juce::Slider lowGainSlider;
    juce::Label lowGainLabel;
    juce::Slider midGainSlider;
    juce::Label midGainLabel;
    juce::Slider highGainSlider;
    juce::Label highGainLabel;

    // Compressor - HORIZONTAL layout
    juce::GroupComponent compGroup;
    juce::ToggleButton compEnableToggle;
    juce::Slider thresholdSlider;
    juce::Label thresholdLabel;
    juce::Slider ratioSlider;
    juce::Label ratioLabel;
    juce::Slider attackSlider;
    juce::Label attackLabel;
    juce::Slider releaseSlider;
    juce::Label releaseLabel;
    juce::Slider makeupSlider;
    juce::Label makeupLabel;

    // Solo/Mute buttons
    juce::TextButton soloButton;
    juce::TextButton muteButton;

    void setupHorizontalSlider(juce::Slider& slider, juce::Label& label, const juce::String& text,
                               double min, double max, double defaultValue);
    void setupVerticalSlider(juce::Slider& slider, juce::Label& label, const juce::String& text,
                             double min, double max, double defaultValue);
    void setupCompactSlider(juce::Slider& slider, double min, double max, double defaultValue);
    void setupButton(juce::TextButton& button, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerChannelComponent)
};

/**
 * Main Samples Manager Window
 *
 * Layout (top to bottom):
 * 1. Title "Samples Manager" - centered, cyan
 * 2. Audio Playback + Drum Library + Humanization sections (3 columns)
 * 3. 7 Drum Part mixer channels (Kick1 with Altern, Kick2, Snare, Hi-Hat, Toms, Crash, Rides)
 * 4. Compact row: Mixer Presets + 8-band EQ + Master Volume
 */
class SamplesManagerWindow : public juce::Component
{
public:
    explicit SamplesManagerWindow(DrumMixer& mixer, DrumLibraryManager& libraryManager, DrumGrooveProcessor& proc);
    ~SamplesManagerWindow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void showDownloadSection(bool show);
    bool isDownloadSectionVisible() const { return downloadSectionVisible; }

    void updateFromMixer();

    std::function<void()> onClose;

private:
    DrumMixer& drumMixer;
    DrumLibraryManager& libManager;
    DrumGrooveProcessor& processor;

    // Background image
    juce::Image backgroundImage;

    // Header
    juce::Label titleLabel;
    juce::TextButton closeButton;

    // Audio mode section
    juce::GroupComponent audioModeGroup;
    juce::Label audioModeLabel;
    juce::ToggleButton audioModeToggle;
    juce::Label samplesStatusLabel;
    juce::TextButton loadSamplesButton;
    juce::TextButton midiModeButton;  // Toggle between MIDI and Audio output

    // Library section
    juce::GroupComponent libraryGroup;
    juce::Label libraryLabel;
    juce::ComboBox libraryCombo;
    juce::Label downloadLibLabel;
    juce::ComboBox downloadLibCombo;  // Selector for which library to download
    juce::TextButton downloadButton;
    juce::TextButton deleteButton;

    // Download section (collapsible)
    juce::Component downloadSection;
    juce::Label downloadStatusLabel;
    double currentProgress { 0.0 };
    juce::ProgressBar downloadProgress;
    juce::TextButton cancelDownloadButton;
    std::unique_ptr<SampleDownloader> downloader;
    bool downloadSectionVisible { false };

    // =========================================================================
    // HUMANIZATION SECTION
    // =========================================================================
    juce::GroupComponent humanizationGroup;

    // Velocity Humanization slider (0-100%)
    juce::Slider velocityHumanSlider;
    juce::Label velocityHumanLabel;

    // Timing Humanization slider (0-100%)
    juce::Slider timingHumanSlider;
    juce::Label timingHumanLabel;

    // Round Robin slider (0-100%)
    juce::Slider roundRobinSlider;
    juce::Label roundRobinLabel;

    // Mixer channels (7 drum parts)
    std::array<std::unique_ptr<MixerChannelComponent>, 7> mixerChannels;

    // Master section
    juce::GroupComponent masterGroup;
    juce::Slider masterVolumeSlider;
    juce::Label masterVolumeLabel;

    // Master 8-band EQ (compact)
    juce::GroupComponent masterEQGroup;
    juce::ToggleButton masterEQEnableToggle;

    struct EQBandControls {
        juce::Slider gainSlider;
        juce::Label freqLabel;
    };
    std::array<EQBandControls, 8> eqBands;

    // Preset system
    juce::GroupComponent presetGroup;
    juce::ComboBox presetCombo;
    juce::TextButton savePresetButton;
    juce::TextButton deletePresetButton;
    std::unique_ptr<EQPresetManager> presetManager;

    void setupMasterEQSection();
    void setupPresetSection();
    void refreshPresetList();
    void saveCurrentAsPreset();
    void loadSelectedPreset();
    void deleteSelectedPreset();
    EQPresetManager::EQPreset captureCurrentSettings();
    void applyPresetSettings(const EQPresetManager::EQPreset& preset);
    void resetToDefaultSettings();

    void setupAudioModeSection();
    void setupLibrarySection();
    void setupHumanizationSection();  // NEW
    void setupMixerSection();
    void setupDownloadSection();
    void refreshLibraryList();
    void handleLibraryChange();
    void handleDownloadClick();
    void handleDeleteClick();
    void handleCancelDownload();
    void onDownloadProgress(double progress, juce::String status);
    void onDownloadComplete(bool success, juce::String message);
    void updateSamplesStatus();

    // Kick alternation
    void handleKickAlternationChanged(bool enabled);

    // Audio/MIDI mode toggle
    void handleAudioModeToggle();
    void updateAudioModeButton();

    // Humanization handlers
    void handleVelocityHumanizationChanged();
    void handleTimingHumanizationChanged();
    void handleRoundRobinChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplesManagerWindow)
};
