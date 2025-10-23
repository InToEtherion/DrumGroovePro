#pragma once
#include <JuceHeader.h>
#include "Track.h"

class MultiTrackContainer;

/**
 * Manages timeline save/load/export operations
 */
class TimelineManager
{
public:
    TimelineManager(MultiTrackContainer* container);
    ~TimelineManager();

    // File operations
    void saveTimelineState();
    void loadTimelineState();
    void exportTimelineAsMidi();
    void exportTimelineAsSeparateMidis();

    // Drag and drop
    void beginDragOfSelectedClips(const juce::MouseEvent& e);
    bool isDragInProgress() const { return dragInProgress; }

private:
    MultiTrackContainer* container;
    bool dragInProgress = false;
    juce::File lastTempDragFile; 
	
    // Helper methods
    juce::File getTimelineFolder() const;
    juce::File chooseSaveLocation() const;
    juce::File chooseLoadLocation() const;
    juce::File chooseExportLocation(bool isSingleFile) const;
    
    // FIXED: Updated signature - now modifies state and accepts target folder
    void copyTempMidiFiles(const juce::File& targetFolder, juce::ValueTree& state) const;
    void createTimelineMetadata(juce::ValueTree& state, const juce::File& folder) const;
    void restoreTimelineMetadata(const juce::ValueTree& state, const juce::File& folder);
    
    // MIDI export helpers
    juce::MidiFile createMidiFileForTrack(int trackIndex, bool includeSilence) const;
    juce::MidiFile createCombinedMidiFile() const;
    void addSilenceToMidiFile(juce::MidiFile& midiFile, double silenceDuration, int trackIndex) const;
    
    // Drag and drop helpers
    juce::var createDragDataForSelectedClips() const;
    void performExternalDrag(const juce::MouseEvent& e, const juce::var& dragData);
	
	// Overlap detection helper
    struct ClipBoundary {
        double startTime;
        double endTime;
        double bpm;
        int trackIndex;
        const MidiClip* clip;
    };
    bool checkForOverlapsWithDifferentBPM(const std::vector<ClipBoundary>& boundaries, juce::String& errorMessage) const;
    
	// Folder safety checks
    bool isFolderEmpty(const juce::File& folder) const;
    bool clearFolderContents(const juce::File& folder) const;
    bool confirmOverwriteFolder(const juce::File& folder) const;
	
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineManager)
};