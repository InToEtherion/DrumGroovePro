#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "DrumLibraryManager.h"
#include <atomic>

struct MidiClipPlayback
{
    juce::String id;
    juce::MidiMessageSequence sequence;
    double startTime = 0.0;
    double duration = 0.0;
    double originalBPM = 120.0;
    double referenceBPM = 120.0;
    double targetBPM = 120.0;
    int trackNumber = 0;
    int currentEventIndex = 0;
    double unscaledLocalTime = 0.0;
    bool isActive = false;
    DrumLibrary sourceLibrary = DrumLibrary::Unknown;
};

class MidiProcessor
{
public:
    MidiProcessor(DrumLibraryManager& drumLibManager);
    ~MidiProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock(juce::MidiBuffer& midiMessages, double currentBPM, DrumLibrary targetLibrary);

    void addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib, double referenceBPM, double targetBPM, int trackNum, const juce::String& clipId);
    void addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib, double referenceBPM, double targetBPM, int trackNum, double explicitDuration, const juce::String& clipId);
    void clearAllClips();
    void clearClip(const juce::String& clipId);

    void updateTrackBPM(int trackNumber, double newBPM);
    void updateClipBoundaries(const juce::String& clipId, double newStartTime, double newDuration);

    void play();
    void stop();
    void pause();
    bool isPlaying() const { return playing.load(); }

    void setPlayheadPosition(double timeInSeconds);
    double getPlayheadPosition() const { return playheadPosition.load(); }

    // Get visual playhead position (with configurable latency offset for better sync perception)
    double getVisualPlayheadPosition() const
    {
        return juce::jmax(0.0, playheadPosition.load() + visualLatencyOffsetSeconds.load());
    }

    void setLoopEnabled(bool enabled) { loopEnabled = enabled; }
    void setLoopRange(double start, double end) { loopStart = start; loopEnd = end; }

    void setPlaybackSpeed(double speed) { playbackSpeed = juce::jlimit(0.25, 2.0, speed); }
    double getPlaybackSpeed() const { return playbackSpeed; }

    // Latency offset control - configurable visual delay to compensate for hardware/system latency
    void setVisualLatencyOffset(double milliseconds)
    {
        visualLatencyOffsetSeconds.store(milliseconds / 1000.0);
    }

    double getVisualLatencyOffsetMs() const { return visualLatencyOffsetSeconds.load() * 1000.0; }

private:
    DrumLibraryManager& drumLibraryManager;
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    double currentBPM = 120.0;

    // THREAD-SAFE: Atomic variables for cross-thread communication (audio thread writes, GUI thread reads)
    std::atomic<bool> playing { false };
    std::atomic<double> playheadPosition { 0.0 };
    std::atomic<double> visualLatencyOffsetSeconds { -0.020 }; // Default -20ms visual delay
    
    // Non-atomic: Only accessed from message/GUI thread
    bool loopEnabled = false;
    double loopStart = 0.0;
    double loopEnd = 4.0;
    double playbackSpeed = 1.0;

    std::vector<std::unique_ptr<MidiClipPlayback>> activeClips;
    juce::CriticalSection clipLock;

    bool loadMidiFileWithPrecision(const juce::File& file, MidiClipPlayback& clip);

    void processClipWithSampleAccuracy(MidiClipPlayback& clip, juce::MidiBuffer& buffer,
                                       double blockStartTime, double blockEndTime,
                                       double bpm, DrumLibrary targetLib);

    void seekClipToTime(MidiClipPlayback& clip, double globalTime);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiProcessor)
};