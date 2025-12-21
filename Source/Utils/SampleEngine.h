#pragma once

#include <JuceHeader.h>
#include "SFZParser.h"
#include "DrumGizmoParser.h"
#include "SampleVoice.h"
#include "DrumMixer.h"
#include <vector>
#include <map>
#include <array>
#include <random>

class SampleEngine
{
public:
    // Format types
    enum class LibraryFormat
    {
        Unknown,
        SFZ,
        DrumGizmo
    };

    // Number of drum parts (must match DrumMixer::NumParts)
    static constexpr int NUM_DRUM_PARTS = 7;  // Kick1, Kick2, Snare, Hi-Hat, Toms, Crash, Rides

    SampleEngine();
    ~SampleEngine();

    // Loading
    bool loadSamplesFromDirectory(const juce::File& samplesDirectory);
    bool isLoaded() const { return samplesLoaded; }
    void unloadSamples();

    // Load a specific library by name (auto-detects format: SFZ or DrumGizmo)
    bool loadLibraryByName(const juce::String& libraryName);

    // Get currently loaded library name
    juce::String getCurrentLibraryName() const { return currentLibraryName; }

    // Get current library format
    LibraryFormat getCurrentFormat() const { return currentFormat; }

    // Playback control
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);

    // Process MIDI and render audio to per-part buffers
    // This is the new main processing method for proper per-part mixing
    // CRITICAL: Uses sample-accurate triggering based on MIDI event timestamps
    void processBlockToPartBuffers(std::array<juce::AudioBuffer<float>, NUM_DRUM_PARTS>& partBuffers,
                                   juce::MidiBuffer& midiMessages);

    // Legacy: Process to single buffer (all parts mixed together)
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    void releaseResources();

    // Status
    int getActiveVoiceCount() const;
    int getLoadedSampleCount() const { return static_cast<int>(loadedSamples.size()); }
    juce::String getStatusText() const;

    // Master volume control - allows up to 4x gain (400%)
    void setMasterGain(float gain) { masterGain = juce::jlimit(0.0f, 4.0f, gain); }
    float getMasterGain() const { return masterGain; }

    // Get per-part buffers (for external access if needed)
    std::array<juce::AudioBuffer<float>, NUM_DRUM_PARTS>& getPartBuffers() { return partBuffers; }

    // Kick Alternation feature
    // When enabled, rapid kick hits alternate between note 35 and 36 to avoid machine-gun effect
    void setKickAlternationEnabled(bool enabled) { kickAlternationEnabled = enabled; }
    bool isKickAlternationEnabled() const { return kickAlternationEnabled; }

    // =========================================================================
    // HUMANIZATION CONTROLS
    // =========================================================================

    /**
     * Set velocity humanization amount (0-100%)
     * 0% = no variation, 100% = Â±15 velocity variation
     */
    void setVelocityHumanization(float percent)
    {
        velocityHumanization = juce::jlimit(0.0f, 100.0f, percent);
    }
    float getVelocityHumanization() const { return velocityHumanization; }

    /**
     * Set timing humanization amount (0-100%)
     * 0% = no variation, 100% = Â±20ms timing variation
     */
    void setTimingHumanization(float percent)
    {
        timingHumanization = juce::jlimit(0.0f, 100.0f, percent);
    }
    float getTimingHumanization() const { return timingHumanization; }

    /**
     * Set round robin amount (0-100%)
     * 0% = always use first sample, 100% = full random selection from available samples
     * Values in between blend deterministic and random selection
     */
    void setRoundRobinAmount(float percent)
    {
        roundRobinAmount = juce::jlimit(0.0f, 100.0f, percent);
    }
    float getRoundRobinAmount() const { return roundRobinAmount; }

private:
    // Voice management
    static constexpr int MAX_VOICES = 64;

    // Crossfade time in samples for voice stealing (prevents clicks)
    // At 44100 Hz, 256 samples = ~5.8ms fade
    static constexpr int VOICE_STEAL_FADE_SAMPLES = 256;

    // Maximum humanization values
    // NOTE: ±25 velocity gives roughly ±2dB volume change which is more audible
    static constexpr int MAX_VELOCITY_VARIATION = 25;       // ±25 velocity at 100%
    static constexpr double MAX_TIMING_VARIATION_MS = 20.0; // ±20ms at 100%

    std::array<SampleVoice, MAX_VOICES> voices;

    // Sample storage
    struct LoadedSample
    {
        juce::AudioBuffer<float> buffer;
        juce::String filePath;
        int referenceCount { 0 };
    };

    std::map<juce::String, LoadedSample> loadedSamples;
    SFZParser sfzParser;
    DrumGizmoParser drumGizmoParser;
    juce::AudioFormatManager formatManager;

    double currentSampleRate { 44100.0 };
    int currentBlockSize { 512 };
    bool samplesLoaded { false };
    juce::File samplesBaseDirectory;
    float masterGain { 1.0f };
    juce::String currentLibraryName;
    LibraryFormat currentFormat { LibraryFormat::Unknown };

    // Per-part output buffers
    std::array<juce::AudioBuffer<float>, NUM_DRUM_PARTS> partBuffers;

    // Kick alternation state
    bool kickAlternationEnabled { false };
    bool lastKickWas35 { false };  // Tracks which kick was played last for alternation

    // Humanization settings (0-100%)
    float velocityHumanization { 0.0f };
    float timingHumanization { 0.0f };
    float roundRobinAmount { 100.0f };  // Default to full round robin

    // CRITICAL FIX: Thread-safe random number generator using C++11 <random>
    // juce::Random is NOT thread-safe, using it on audio thread causes undefined behavior
    std::mt19937 audioThreadRng;  // Mersenne Twister random generator (thread-local in practice)

    // Round robin state - tracks last sample index per note for true round robin cycling
    std::map<int, size_t> lastRoundRobinIndex;

    // CRITICAL FIX: Pre-allocated queue for pending notes to avoid vector::push_back on audio thread
    static constexpr int MAX_PENDING_NOTES = 128;
    struct PendingNote
    {
        int midiNote;
        int velocity;
        int samplesRemaining;  // Samples until this note should trigger
        bool active;           // Is this slot in use?
    };
    std::array<PendingNote, MAX_PENDING_NOTES> pendingNotesArray;
    std::atomic<int> pendingNotesWriteIndex{0};

    // MIDI handling
    // CRITICAL: handleNoteOn now takes sampleOffset for sample-accurate triggering
    void handleNoteOn(int midiNote, int velocity, int sampleOffset);
    void handleNoteOnDirect(int midiNote, int velocity, int sampleOffset);  // Direct trigger (no humanization)
    void handleNoteOff(int midiNote);
    void stopAllVoices();

    // Sample loading helpers
    bool loadSample(const juce::String& relativePath);
    const juce::AudioBuffer<float>* getSampleBuffer(const juce::String& relativePath);

    // Voice allocation - returns a free voice or steals the oldest one with crossfade
    SampleVoice* findFreeVoice();

    // Find the oldest active voice (for stealing)
    SampleVoice* findOldestVoice();

    // Format detection
    LibraryFormat detectLibraryFormat(const juce::File& libraryDir) const;

    // DrumGizmo specific loading
    bool loadDrumGizmoLibrary(const juce::File& libraryDir);

    // SFZ specific loading
    bool loadSFZLibrary(const juce::File& libraryDir);

    // Get the actual note to play (handles kick alternation)
    int getActualNoteToPlay(int requestedNote);

    // Get drum part for a note (handles kick alternation routing)
    int getDrumPartForNote(int midiNote);

    // =========================================================================
    // HUMANIZATION HELPERS
    // =========================================================================

    // Apply velocity humanization and return the modified velocity
    int applyVelocityHumanization(int velocity);

    // Get region with round robin support for SFZ format
    const SFZParser::Region* getRegionWithRoundRobin(int midiNote, int velocity);

    // Get sample with round robin support for DrumGizmo format
    const DrumGizmoParser::Sample* getSampleWithRoundRobin(int midiNote, int velocity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEngine)
};
