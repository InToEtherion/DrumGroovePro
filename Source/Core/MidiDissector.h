#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include "DrumLibraryManager.h"

enum class DrumPartType
{
    Kick = 0,
    Snare,
    HiHatClosed,
    HiHatOpen,
    Crash,
    Ride,
    Tom1,
    Tom2,
    Tom3,
    FloorTom,
    Cowbell,
    Clap,
    Shaker,
    Other,
    COUNT
};

struct DrumPart
{
    DrumPartType type;
    juce::String name;
    juce::String displayName;
    juce::MidiMessageSequence sequence;
    juce::Array<uint8_t> originalNotes;
    juce::Array<uint8_t> remappedNotes;
    int eventCount = 0;
    double duration = 0.0;
    juce::Colour colour;
    DrumLibrary sourceLibrary = DrumLibrary::Unknown;  // NEW: Store source library
    
    // FIXED: Drag description includes source library
    juce::String getDragDescription(const juce::File& originalFile) const
    {
        return displayName + "|PART|" + 
               originalFile.getFullPathName() + "|" + 
               juce::String(static_cast<int>(type)) + "|" +
               juce::String(static_cast<int>(sourceLibrary));
    }
    
    bool hasEvents() const { return eventCount > 0; }
};

class MidiDissector
{
public:
    MidiDissector();
    ~MidiDissector();
    
    // Enhanced dissection with library manager (PREFERRED METHOD)
    juce::Array<DrumPart> dissectMidiFileWithLibraryManager(const juce::File& midiFile,
                                                            DrumLibrary sourceLibrary,
                                                            DrumLibrary targetLibrary,
                                                            const DrumLibraryManager& libraryManager);
    
    // Remap existing parts to different target library
    juce::Array<DrumPart> remapDrumPartsToTarget(const juce::Array<DrumPart>& originalParts,
                                                  DrumLibrary sourceLibrary,
                                                  DrumLibrary newTargetLibrary,
                                                  const DrumLibraryManager& libraryManager);
    
    // Get part type from MIDI note considering source library
    static DrumPartType getPartTypeFromNote(uint8_t midiNote, DrumLibrary sourceLibrary = DrumLibrary::GeneralMIDI);
    
    // Display info for part types
    static juce::String getPartDisplayName(DrumPartType type);
    static juce::String getPartShortName(DrumPartType type);
    static juce::Colour getPartColour(DrumPartType type);
    
private:
    void analyzeSequence(const juce::MidiMessageSequence& sequence, 
                        juce::Array<DrumPart>& parts, 
                        DrumLibrary sourceLibrary,
                        DrumLibrary targetLibrary,
                        const DrumLibraryManager* libraryManager) const;
    
    void initializeNoteMappings();
    
    static bool isValidDrumNote(uint8_t midiNote);
    static int getPartPriority(DrumPartType type);
    void sortPartsByPriority(juce::Array<DrumPart>& parts) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiDissector)
};