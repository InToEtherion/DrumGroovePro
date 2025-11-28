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
                case 48: case 50: case 56: return DrumPartType::Tom2;  // 56 = High Tom
                case 41: case 43: return DrumPartType::FloorTom;

                // Cymbals - Extended for Ugritone
                case 49: case 52: case 55: case 57: return DrumPartType::Crash;
                case 58: case 60: case 61: case 62: return DrumPartType::Crash;  // China/Effects
                case 51: case 53: case 59: return DrumPartType::Ride;

                // Percussion
                case 39: return DrumPartType::Clap;
                case 54: return DrumPartType::Shaker;

                // Extended low notes (some Ugritone packs use these)
                case 32: case 33: case 34: return DrumPartType::Kick;

                default: break;  // Fall through to GM mappings
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

                        case DrumLibrary::MuldjordKit3:
                            switch (midiNote)
                            {
                                // Kicks
                                case 35: case 36: return DrumPartType::Kick;

                                // Snare
                                case 37: return DrumPartType::Snare;  // Cross-stick
                                case 38: return DrumPartType::Snare;

                                // Hi-hats
                                case 42: return DrumPartType::HiHatClosed;
                                case 46: return DrumPartType::HiHatOpen;

                                // Toms (Tom1=highest/48, Tom4=lowest/floor/41)
                                case 41: return DrumPartType::FloorTom;  // Tom4 (Floor)
                                case 45: return DrumPartType::Tom1;      // Tom3
                                case 47: return DrumPartType::Tom1;      // Tom2
                                case 48: return DrumPartType::Tom2;      // Tom1 (highest)

                                // Crashes
                                case 49: case 57: return DrumPartType::Crash;

                                // Rides
                                case 51: case 59: case 53: return DrumPartType::Ride;

                                default: break;
                            }
                            break;

                                default:
                                    break;
    }

    // ==========================================================================
    // Standard General MIDI / salamanderDrumkit mappings
    // Updated to match actual salamanderDrumkit SFZ note assignments
    // ==========================================================================
    switch (midiNote)
    {
        // Extended low range - some libraries use these
        case 32:  // Some libraries: Extra kick or low tom
        case 33:  // Some libraries: Extra kick
        case 34:  // Some libraries: Extra kick
            return DrumPartType::Kick;

            // Kicks (35=Kick1, 36=Kick2)
        case 35:
        case 36:
            return DrumPartType::Kick;

            // Snares (37=Side Stick, 38=Acoustic Snare, 40=Electric Snare)
        case 37:  // Side Stick
        case 38:  // Acoustic Snare
        case 40:  // Electric Snare
            return DrumPartType::Snare;

            // Hand Clap
        case 39:  // Hand Clap
            return DrumPartType::Clap;

            // Hi-hats (42=closed, 44=pedal, 46=open)
        case 42:  // Hi-Hat Closed
        case 44:  // Hi-Hat Pedal / Foot
            return DrumPartType::HiHatClosed;

        case 46:  // Hi-Hat Open
            return DrumPartType::HiHatOpen;

            // Toms (41=Low Floor Tom, 43=High Floor Tom, 45=Low Tom, 47=Low-Mid Tom, 48=Hi-Mid Tom, 50=High Tom)
        case 41:  // Low Floor Tom
        case 43:  // High Floor Tom
            return DrumPartType::FloorTom;

        case 45:  // Low Tom
        case 47:  // Low-Mid Tom
            return DrumPartType::Tom1;

        case 48:  // Hi-Mid Tom
        case 50:  // High Tom
            return DrumPartType::Tom2;

            // Cymbals
        case 49:  // Crash Cymbal 1
            return DrumPartType::Crash;

        case 51:  // Ride Cymbal 1
        case 53:  // Ride Bell
            return DrumPartType::Ride;

        case 52:  // Chinese Cymbal
        case 55:  // Splash Cymbal
        case 57:  // Crash Cymbal 2
            return DrumPartType::Crash;

            // Percussion
        case 54:  // Tambourine
            return DrumPartType::Shaker;
        case 56:  // Cowbell
            return DrumPartType::Cowbell;

            // Extended Crashes, Chinas, Effects (58-64)
        case 58:  // Vibraslap (but often used as China in some libraries)
        case 59:  // Ride Cymbal 2
            return DrumPartType::Ride;

        case 60:  // Hi Bongo
        case 61:  // Low Bongo
        case 62:  // Mute Hi Conga
        case 63:  // Open Hi Conga
        case 64:  // Low Conga
            return DrumPartType::Other;  // Percussion

            // Extended range - Shakers/Percussion (GM standard)
        case 69:  // Cabasa
        case 70:  // Maracas
            return DrumPartType::Shaker;

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

    // Determine which library to use for dissection (categorization)
    // In Bypass mode, we use GM for categorization but NO remapping
    DrumLibrary dissectionLibrary = sourceLibrary;

    if (targetLibrary == DrumLibrary::Bypass)
    {
        // In Bypass mode: use General MIDI for dissection/categorization
        dissectionLibrary = DrumLibrary::GeneralMIDI;
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

    // FIXED: Pass the ACTUAL targetLibrary to analyzeSequence
    // The Bypass check in analyzeSequence will handle no-remapping
    analyzeSequence(combinedTrackSequence, parts, dissectionLibrary, targetLibrary, &libraryManager);

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

                                                // Remap note to target library for playback
                                                uint8_t finalNote = originalNote;
                                                // FIXED: Bypass means NO remapping at all
                                                bool isRemapping = (sourceLibrary != targetLibrary) && (targetLibrary != DrumLibrary::Bypass);
                                                if (libraryManager && isRemapping)
                                                {
                                                    finalNote = libraryManager->mapNoteToLibrary(originalNote, sourceLibrary, targetLibrary);
                                                }

                                                // Categorize based on TARGET library (what the note becomes after remapping)
                                                DrumPartType partType = getPartTypeFromNote(finalNote, targetLibrary);

                                                if (partType != DrumPartType::Other || isValidDrumNote(originalNote))
                                                {
                                                    auto& part = partMap[partType];

                                                    // Store original note
                                                    part.originalNotes.addIfNotAlreadyThere(originalNote);

                                                    // Store remapped note when source != target (even if note number is same)
                                                    // This shows what the note means in the target library context
                                                    if (isRemapping)
                                                    {
                                                        part.remappedNotes.addIfNotAlreadyThere(finalNote);
                                                    }

                                                    // Create note-on with remapped note
                                                    juce::MidiMessage processedMsg = juce::MidiMessage::noteOn(msg.getChannel(),
                                                                                                               finalNote,
                                                                                                               static_cast<juce::uint8>(msg.getVelocity()));
                                                    processedMsg.setTimeStamp(msg.getTimeStamp());

                                                    part.sequence.addEvent(processedMsg);
                                                    part.eventCount++;

                                                    // Track duration
                                                    double timestamp = msg.getTimeStamp();
                                                    if (timestamp > part.duration)
                                                        part.duration = timestamp;
                                                }
                                            }
                                            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
                                            {
                                                uint8_t originalNote = static_cast<uint8_t>(msg.getNoteNumber());

                                                // Remap note-off to target library
                                                uint8_t finalNote = originalNote;
                                                // FIXED: Bypass means NO remapping
                                                if (libraryManager && sourceLibrary != targetLibrary && targetLibrary != DrumLibrary::Bypass)
                                                {
                                                    finalNote = libraryManager->mapNoteToLibrary(originalNote, sourceLibrary, targetLibrary);
                                                }

                                                // Categorize based on TARGET library
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

                                                // Check for custom drum name from target library
                                                // Priority: 1) Custom name from target, 2) Hardcoded name, 3) "Other"
                                                if (libraryManager != nullptr && !part.remappedNotes.isEmpty())
                                                {
                                                    // Use the first remapped note to look up custom name
                                                    uint8_t primaryNote = part.remappedNotes[0];

                                                    if (libraryManager->hasCustomDrumName(targetLibrary, primaryNote))
                                                    {
                                                        juce::String customName = libraryManager->getCustomDrumName(targetLibrary, primaryNote);
                                                        if (customName.isNotEmpty())
                                                        {
                                                            part.displayName = customName;
                                                            DBG("Using custom name for note " + juce::String(primaryNote) + ": " + customName);
                                                        }
                                                    }
                                                }
                                                else if (libraryManager != nullptr && !part.originalNotes.isEmpty())
                                                {
                                                    // No remapping - check original notes against target library
                                                    uint8_t primaryNote = part.originalNotes[0];

                                                    if (libraryManager->hasCustomDrumName(targetLibrary, primaryNote))
                                                    {
                                                        juce::String customName = libraryManager->getCustomDrumName(targetLibrary, primaryNote);
                                                        if (customName.isNotEmpty())
                                                        {
                                                            part.displayName = customName;
                                                            DBG("Using custom name for note " + juce::String(primaryNote) + ": " + customName);
                                                        }
                                                    }
                                                }

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

                                                // Check for custom drum name from target library
                                                uint8_t primaryNote = part.remappedNotes[0];
                                                if (libraryManager.hasCustomDrumName(newTargetLibrary, primaryNote))
                                                {
                                                    juce::String customName = libraryManager.getCustomDrumName(newTargetLibrary, primaryNote);
                                                    if (customName.isNotEmpty())
                                                    {
                                                        part.displayName = customName;
                                                        DBG("Remap: Using custom name for note " + juce::String(primaryNote) + ": " + customName);
                                                    }
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
