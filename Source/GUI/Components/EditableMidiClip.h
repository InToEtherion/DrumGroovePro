#pragma once

#include <JuceHeader.h>
#include "DrumLibraryManager.h"
#include "MidiDissector.h"

/**
 * Represents a single MIDI note in the editor
 */
struct MidiNote
{
    int noteNumber = 60;           // MIDI note number (0-127)
    int velocity = 100;            // Note velocity (1-127)
    double startTime = 0.0;        // Start time in quarter notes
    double duration = 0.25;        // Duration in quarter notes (default: 16th note)
    DrumPartType drumPart = DrumPartType::Other;  // Categorized drum part
    bool isSelected = false;       // Selection state for editing
    juce::String id = juce::Uuid().toString();  // Unique identifier
    
    MidiNote() = default;
    
    MidiNote(int note, int vel, double start, double dur, DrumPartType part)
        : noteNumber(note), velocity(vel), startTime(start), duration(dur), drumPart(part)
    {
    }
    
    // Comparison for sorting
    bool operator<(const MidiNote& other) const
    {
        return startTime < other.startTime;
    }
};

/**
 * EditableMidiClip - Main data structure for MIDI editing
 * Stores notes in a format suitable for drum grid editing
 */
class EditableMidiClip
{
public:
    EditableMidiClip();
    ~EditableMidiClip();
    
    // Load from MIDI file
    bool loadFromMidiFile(const juce::File& file, DrumLibrary sourceLib);
    
    // Save to MIDI file
    bool saveToMidiFile(const juce::File& file);
    
    // Note management
    void addNote(const MidiNote& note);
    void removeNote(const juce::String& noteId);
    void removeNotes(const juce::Array<juce::String>& noteIds);
    void moveNote(const juce::String& noteId, double newStartTime, int newNoteNumber);
    void setNoteVelocity(const juce::String& noteId, int newVelocity);
    MidiNote* findNote(const juce::String& noteId);
    
    // Selection
    void selectNote(const juce::String& noteId, bool addToSelection);
    void selectNotesInRegion(double startTime, double endTime, int minNote, int maxNote);
    void clearSelection();
    juce::Array<MidiNote*> getSelectedNotes();
    
    // Query notes
    const std::vector<MidiNote>& getNotes() const { return notes; }
    juce::Array<MidiNote*> getNotesInTimeRange(double startTime, double endTime);
    juce::Array<MidiNote*> getNotesForDrumPart(DrumPartType part);
    
    // Clip properties
    void setName(const juce::String& newName) { name = newName; }
    juce::String getName() const { return name; }
    
    void setSourceLibrary(DrumLibrary lib) { sourceLibrary = lib; }
    DrumLibrary getSourceLibrary() const { return sourceLibrary; }
    
    void setBPM(double newBPM) { bpm = newBPM; }
    double getBPM() const { return bpm; }
    
    void setTimeSignature(int num, int denom) { timeSignatureNumerator = num; timeSignatureDenominator = denom; }
    int getTimeSignatureNumerator() const { return timeSignatureNumerator; }
    int getTimeSignatureDenominator() const { return timeSignatureDenominator; }
    
    void setLengthInBars(int bars) { lengthInBars = bars; }
    int getLengthInBars() const { return lengthInBars; }
    
    double getDurationInQuarterNotes() const;
    double getDurationInSeconds() const;
    
    // Utility
    void sortNotes();
    int getNoteCount() const { return static_cast<int>(notes.size()); }
    void clear();
    
private:
    std::vector<MidiNote> notes;
    juce::String name;
    DrumLibrary sourceLibrary = DrumLibrary::Unknown;
    double bpm = 120.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    int lengthInBars = 4;
    
    // Convert MIDI ticks to quarter notes
    double ticksToQuarterNotes(int ticks, int ppqn) const;
    int quarterNotesToTicks(double quarterNotes, int ppqn) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditableMidiClip)
};
