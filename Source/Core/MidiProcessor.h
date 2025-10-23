#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "DrumLibraryManager.h"

struct MidiClipPlayback
{
    juce::String id;
    juce::MidiMessageSequence sequence;
    double startTime = 0.0;
    double duration = 0.0;  // Duration in seconds at originalBPM
    double originalBPM = 120.0;
    double referenceBPM = 120.0;  // Track BPM when clip was added
    double targetBPM = 120.0;     // Current track BPM
    int trackNumber = 0;
    int currentEventIndex = 0;
    double unscaledLocalTime = 0.0;  // Track position in original MIDI time (unaffected by BPM)
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

    // Add a MIDI file to play with precise timing and BPM scaling
    void addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib, double referenceBPM, double targetBPM, int trackNum);
    void clearAllClips();
    void clearClip(const juce::String& clipId);
    
    // Update BPM for all clips on a specific track in real-time
    void updateTrackBPM(int trackNumber, double newBPM);
    
    // NEW: Update clip boundaries in real-time when user resizes/moves clips
    void updateClipBoundaries(const juce::String& clipId, double newStartTime, double newDuration);

    void play();
    void stop();
    void pause();
    bool isPlaying() const { return playing; }

    void setPlayheadPosition(double timeInSeconds);
    double getPlayheadPosition() const { return playheadPosition; }

    void setLoopEnabled(bool enabled) { loopEnabled = enabled; }
    void setLoopRange(double start, double end) { loopStart = start; loopEnd = end; }

    void syncPlayheadPosition(double timeInSeconds);

private:
    DrumLibraryManager& drumLibraryManager;
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    double currentBPM = 120.0;

    bool playing = false;
    bool loopEnabled = false;
    double loopStart = 0.0;
    double loopEnd = 4.0;
    double playheadPosition = 0.0;

    std::vector<std::unique_ptr<MidiClipPlayback>> activeClips;
    juce::CriticalSection clipLock;

    bool loadMidiFileWithPrecision(const juce::File& file, MidiClipPlayback& clip);
    
    void processClipWithSampleAccuracy(MidiClipPlayback& clip, juce::MidiBuffer& buffer,
                                      double blockStartTime, double blockEndTime,
                                      double bpm, DrumLibrary targetLib);
    
    void seekClipToTime(MidiClipPlayback& clip, double globalTime);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiProcessor)
};