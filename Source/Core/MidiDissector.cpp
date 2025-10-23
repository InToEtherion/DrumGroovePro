#include "MidiDissector.h"

MidiDissector::MidiDissector()
{
    initializeNoteMappings();
}

MidiDissector::~MidiDissector() = default;

DrumPartType MidiDissector::getPartTypeFromNote(uint8_t midiNote, DrumLibrary sourceLibrary)
{
    // Library-specific mappings FIRST
    switch (sourceLibrary)
    {
        case DrumLibrary::Ugritone:
            switch (midiNote)
            {
                // Kicks
                case 35: case 36: return DrumPartType::Kick;
                
                // Snares
                case 37: case 38: case 40: return DrumPartType::Snare;
                
                // Hi-hats - Ugritone custom notes!
                case 22: case 42: case 44: return DrumPartType::HiHatClosed;
                case 26: case 46: return DrumPartType::HiHatOpen;
                
                // Toms
                case 45: case 47: return DrumPartType::Tom1;
                case 48: case 50: return DrumPartType::Tom2;
                case 41: case 43: return DrumPartType::FloorTom;
                
                // Cymbals
                case 49: case 52: case 55: case 57: return DrumPartType::Crash;
                case 51: case 53: case 59: return DrumPartType::Ride;
                
                // Percussion
                case 39: return DrumPartType::Clap;
                case 56: return DrumPartType::Cowbell;
                case 54: case 58: return DrumPartType::Shaker;
                
                default: break;
            }
            break;
            
        case DrumLibrary::EZdrummer:
            switch (midiNote)
            {
                case 24: case 36: return DrumPartType::Kick;
                case 26: case 38: return DrumPartType::Snare;
                default: break;
            }
            break;
            
        default:
            break;
    }
    
    // Standard General MIDI mappings
    switch (midiNote)
    {
        // Kicks
        case 35: case 36: return DrumPartType::Kick;
        
        // Snares  
        case 38: case 40: return DrumPartType::Snare;
        
        // Hi-hats
        case 42: case 44: return DrumPartType::HiHatClosed;
        case 46: return DrumPartType::HiHatOpen;
        
        // Crashes
        case 49: case 57: return DrumPartType::Crash;
        
        // Ride
        case 51: case 59: return DrumPartType::Ride;
        
        // Toms
        case 45: case 47: return DrumPartType::Tom1;
        case 48: case 50: return DrumPartType::Tom2;
        case 41: case 43: return DrumPartType::FloorTom;
        
        // Percussion
        case 39: return DrumPartType::Clap;
        case 56: return DrumPartType::Cowbell;
        case 69: case 70: return DrumPartType::Shaker;
        
        default:
            return DrumPartType::Other;
    }
}

juce::Array<DrumPart> MidiDissector::dissectMidiFileWithLibraryManager(const juce::File& midiFile,
                                                                       DrumLibrary sourceLibrary,
                                                                       DrumLibrary targetLibrary,
                                                                       const DrumLibraryManager& libraryManager)
{
    juce::Array<DrumPart> parts;
    
    if (!midiFile.existsAsFile())
        return parts;
    
    juce::FileInputStream inputStream(midiFile);
    if (!inputStream.openedOk())
        return parts;
    
    juce::MidiFile midiFileData;
    if (!midiFileData.readFrom(inputStream))
        return parts;
    
    // Handle Bypass mode: use General MIDI for dissection, but don't remap notes
    DrumLibrary dissectionLibrary = sourceLibrary;
	DrumLibrary remapTargetLibrary = targetLibrary;

	if (targetLibrary == DrumLibrary::Bypass)
	{
		// In Bypass mode: use General MIDI for dissection/categorization (as requested)
		dissectionLibrary = DrumLibrary::GeneralMIDI;
    
		// Don't remap notes - keep them as-is by using source library
		if (sourceLibrary != DrumLibrary::Unknown)
		{
			remapTargetLibrary = sourceLibrary; // Use source to prevent remapping
		}
		else
		{
			// If source is unknown, assume General MIDI to prevent errors
			remapTargetLibrary = DrumLibrary::GeneralMIDI;
		}
	}    
    // Merge all tracks into one combined sequence
    juce::MidiMessageSequence combinedTrackSequence;
    
    for (int trackIndex = 0; trackIndex < midiFileData.getNumTracks(); ++trackIndex)
    {
        const juce::MidiMessageSequence* track = midiFileData.getTrack(trackIndex);
        if (track)
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                combinedTrackSequence.addEvent(track->getEventPointer(i)->message);
            }
        }
    }
    
    combinedTrackSequence.sort();
    
    // Analyze with library manager
    // dissectionLibrary is used to identify what type of drum part each note represents
    // remapTargetLibrary is used to remap the notes to the target library
    analyzeSequence(combinedTrackSequence, parts, dissectionLibrary, remapTargetLibrary, &libraryManager);
    
    sortPartsByPriority(parts);
    
    DBG("MIDI Dissection complete: " + juce::String(parts.size()) + " parts found in " + 
        midiFile.getFileName() + " (Source: " + juce::String(static_cast<int>(sourceLibrary)) + 
        ", Target: " + juce::String(static_cast<int>(targetLibrary)) + 
        (targetLibrary == DrumLibrary::Bypass ? " [Bypass Mode - No Remapping]" : "") + ")");
    
    return parts;
}

void MidiDissector::analyzeSequence(const juce::MidiMessageSequence& sequence, 
                                   juce::Array<DrumPart>& parts, 
                                   DrumLibrary sourceLibrary,
                                   DrumLibrary targetLibrary,
                                   const DrumLibraryManager* libraryManager) const
{
    std::map<DrumPartType, DrumPart> partMap;
    
    // Initialize all possible parts
    for (int i = 0; i < static_cast<int>(DrumPartType::COUNT); ++i)
    {
        DrumPartType partType = static_cast<DrumPartType>(i);
        DrumPart part;
        part.type = partType;
        part.name = getPartShortName(partType);
        part.displayName = getPartDisplayName(partType);
        part.colour = getPartColour(partType);
        part.eventCount = 0;
        part.duration = 0.0;
        partMap[partType] = part;
    }
    
    // Process each MIDI event
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const juce::MidiMessage& msg = sequence.getEventPointer(i)->message;
        
        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            uint8_t originalNote = static_cast<uint8_t>(msg.getNoteNumber());
            
            // CRITICAL FIX: Remap note FIRST, then identify based on TARGET library
            uint8_t finalNote = originalNote;
            if (libraryManager && sourceLibrary != targetLibrary)
            {
                finalNote = libraryManager->mapNoteToLibrary(originalNote, sourceLibrary, targetLibrary);
            }
            
            // Determine part type using TARGET library (so we see what it means in target context)
            DrumPartType partType = getPartTypeFromNote(finalNote, targetLibrary);
            
            if (partType != DrumPartType::Other || isValidDrumNote(originalNote))
            {
                auto& part = partMap[partType];
                
                // Store original note
                part.originalNotes.addIfNotAlreadyThere(originalNote);
                
                // Store remapped note
                part.remappedNotes.addIfNotAlreadyThere(finalNote);
                
                // Create processed message with remapped note
                juce::MidiMessage processedMsg = juce::MidiMessage::noteOn(
                    msg.getChannel(), 
                    finalNote, 
                    static_cast<juce::uint8>(msg.getVelocity()));
                processedMsg.setTimeStamp(msg.getTimeStamp());
                
                // Add to sequence
                part.sequence.addEvent(processedMsg);
                part.eventCount++;
                
                // Update duration
                if (msg.getTimeStamp() > part.duration)
                    part.duration = msg.getTimeStamp();
            }
        }
        else if (msg.isNoteOff())
        {
            uint8_t originalNote = static_cast<uint8_t>(msg.getNoteNumber());
            
            // Remap note first
            uint8_t finalNote = originalNote;
            if (libraryManager && sourceLibrary != targetLibrary)
            {
                finalNote = libraryManager->mapNoteToLibrary(originalNote, sourceLibrary, targetLibrary);
            }
            
            // Identify using target library
            DrumPartType partType = getPartTypeFromNote(finalNote, targetLibrary);
            
            if (partType != DrumPartType::Other || isValidDrumNote(originalNote))
            {
                auto& part = partMap[partType];
                
                // Create note-off with remapped note
                juce::MidiMessage processedMsg = juce::MidiMessage::noteOff(msg.getChannel(), finalNote);
                processedMsg.setTimeStamp(msg.getTimeStamp());
                
                part.sequence.addEvent(processedMsg);
            }
        }
    }
    
    // Add non-empty parts to result
    for (auto& [type, part] : partMap)
    {
        if (part.eventCount > 0)
        {
            part.sequence.sort();
            part.sequence.updateMatchedPairs();
            parts.add(part);
        }
    }
}

juce::Array<DrumPart> MidiDissector::remapDrumPartsToTarget(const juce::Array<DrumPart>& originalParts,
                                                            DrumLibrary sourceLibrary,
                                                            DrumLibrary newTargetLibrary,
                                                            const DrumLibraryManager& libraryManager)
{
    juce::Array<DrumPart> remappedParts;
    
    for (const auto& originalPart : originalParts)
    {
        DrumPart part = originalPart;
        
        // Clear remapped notes
        part.remappedNotes.clear();
        
        // Create new sequence with remapped notes
        juce::MidiMessageSequence newSequence;
        
        for (int i = 0; i < originalPart.sequence.getNumEvents(); ++i)
        {
            juce::MidiMessage message = originalPart.sequence.getEventPointer(i)->message;
            
            if (message.isNoteOnOrOff())
            {
                uint8_t originalNote = static_cast<uint8_t>(message.getNoteNumber());
                uint8_t remappedNote = libraryManager.mapNoteToLibrary(originalNote, sourceLibrary, newTargetLibrary);
                
                if (message.isNoteOn())
                {
                    message = juce::MidiMessage::noteOn(message.getChannel(), 
                                                       remappedNote, 
                                                       static_cast<juce::uint8>(message.getVelocity()));
                    part.remappedNotes.addIfNotAlreadyThere(remappedNote);
                }
                else
                {
                    message = juce::MidiMessage::noteOff(message.getChannel(), remappedNote);
                }
                
                message.setTimeStamp(originalPart.sequence.getEventPointer(i)->message.getTimeStamp());
            }
            
            newSequence.addEvent(message);
        }
        
        part.sequence = newSequence;
        part.sequence.sort();
        part.sequence.updateMatchedPairs();
        
        // Recalculate part type based on remapped notes in target library
        if (!part.remappedNotes.isEmpty())
        {
            DrumPartType newPartType = getPartTypeFromNote(part.remappedNotes[0], newTargetLibrary);
            
            if (newPartType != part.type)
            {
                part.type = newPartType;
                part.name = getPartShortName(newPartType);
                part.displayName = getPartDisplayName(newPartType);
                part.colour = getPartColour(newPartType);
            }
        }
        
        remappedParts.add(part);
    }
    
    sortPartsByPriority(remappedParts);
    
    return remappedParts;
}

// Helper methods...

juce::String MidiDissector::getPartDisplayName(DrumPartType type)
{
    switch (type)
    {
        case DrumPartType::Kick: return "Kick Drum";
        case DrumPartType::Snare: return "Snare Drum";
        case DrumPartType::HiHatClosed: return "Hi-Hat Closed";
        case DrumPartType::HiHatOpen: return "Hi-Hat Open";
        case DrumPartType::Crash: return "Crash Cymbal";
        case DrumPartType::Ride: return "Ride Cymbal";
        case DrumPartType::Tom1: return "Tom 1";
        case DrumPartType::Tom2: return "Tom 2";
        case DrumPartType::Tom3: return "Tom 3";
        case DrumPartType::FloorTom: return "Floor Tom";
        case DrumPartType::Cowbell: return "Cowbell";
        case DrumPartType::Clap: return "Hand Clap";
        case DrumPartType::Shaker: return "Shaker";
        case DrumPartType::Other: return "Other";
        default: return "Unknown";
    }
}

juce::String MidiDissector::getPartShortName(DrumPartType type)
{
    switch (type)
    {
        case DrumPartType::Kick: return "Kick";
        case DrumPartType::Snare: return "Snare";
        case DrumPartType::HiHatClosed: return "HH-C";
        case DrumPartType::HiHatOpen: return "HH-O";
        case DrumPartType::Crash: return "Crash";
        case DrumPartType::Ride: return "Ride";
        case DrumPartType::Tom1: return "Tom1";
        case DrumPartType::Tom2: return "Tom2";
        case DrumPartType::Tom3: return "Tom3";
        case DrumPartType::FloorTom: return "FTom";
        case DrumPartType::Cowbell: return "Cowbell";
        case DrumPartType::Clap: return "Clap";
        case DrumPartType::Shaker: return "Shaker";
        case DrumPartType::Other: return "Other";
        default: return "Unknown";
    }
}

juce::Colour MidiDissector::getPartColour(DrumPartType type)
{
    switch (type)
    {
        case DrumPartType::Kick: return juce::Colour(0xffff4444);
        case DrumPartType::Snare: return juce::Colour(0xff44ff44);
        case DrumPartType::HiHatClosed: return juce::Colour(0xff4444ff);
        case DrumPartType::HiHatOpen: return juce::Colour(0xff8888ff);
        case DrumPartType::Crash: return juce::Colour(0xffffaa44);
        case DrumPartType::Ride: return juce::Colour(0xffff8844);
        case DrumPartType::Tom1: return juce::Colour(0xffaa44ff);
        case DrumPartType::Tom2: return juce::Colour(0xffdd44ff);
        case DrumPartType::Tom3: return juce::Colour(0xffff44dd);
        case DrumPartType::FloorTom: return juce::Colour(0xffff4488);
        case DrumPartType::Cowbell: return juce::Colour(0xff44ffaa);
        case DrumPartType::Clap: return juce::Colour(0xffffdd44);
        case DrumPartType::Shaker: return juce::Colour(0xffaaff44);
        case DrumPartType::Other: return juce::Colour(0xff888888);
        default: return juce::Colour(0xff666666);
    }
}

bool MidiDissector::isValidDrumNote(uint8_t midiNote)
{
    return midiNote >= 22 && midiNote <= 81;  // Extended range for Ugritone
}

int MidiDissector::getPartPriority(DrumPartType type)
{
    switch (type)
    {
        case DrumPartType::Kick: return 1;
        case DrumPartType::Snare: return 2;
        case DrumPartType::HiHatClosed: return 3;
        case DrumPartType::HiHatOpen: return 4;
        case DrumPartType::Tom1: return 5;
        case DrumPartType::Tom2: return 6;
        case DrumPartType::FloorTom: return 7;
        case DrumPartType::Crash: return 8;
        case DrumPartType::Ride: return 9;
        case DrumPartType::Clap: return 10;
        case DrumPartType::Cowbell: return 11;
        case DrumPartType::Shaker: return 12;
        case DrumPartType::Other: return 99;
        default: return 100;
    }
}

void MidiDissector::sortPartsByPriority(juce::Array<DrumPart>& parts) const
{
    struct PartComparator
    {
        int compareElements(const DrumPart& first, const DrumPart& second) const
        {
            int firstPriority = getPartPriority(first.type);
            int secondPriority = getPartPriority(second.type);
            
            if (firstPriority < secondPriority) return -1;
            if (firstPriority > secondPriority) return 1;
            return 0;
        }
    };
    
    PartComparator comparator;
    parts.sort(comparator);
}

void MidiDissector::initializeNoteMappings()
{
    // Future expansion for additional library-specific mappings
}