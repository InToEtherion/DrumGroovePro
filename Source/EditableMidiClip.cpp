#include "EditableMidiClip.h"

EditableMidiClip::EditableMidiClip()
{
}

EditableMidiClip::~EditableMidiClip()
{
}

bool EditableMidiClip::loadFromMidiFile(const juce::File& file, DrumLibrary sourceLib)
{
    if (!file.existsAsFile())
        return false;

    try
    {
        juce::FileInputStream stream(file);
        if (!stream.openedOk())
            return false;

        juce::MidiFile midiFile;
        if (!midiFile.readFrom(stream))
            return false;

        // Clear existing notes
        notes.clear();

        // Set source library
        sourceLibrary = sourceLib;

        // Get PPQN (ticks per quarter note)
        int ppqn = midiFile.getTimeFormat();
        DBG("MIDI PPQN: " + juce::String(ppqn));

        // Get number of tracks
        int numTracks = midiFile.getNumTracks();
        if (numTracks == 0)
            return false;

        DBG("MIDI has " + juce::String(numTracks) + " tracks");

        // Scan for tempo and time signature in ALL tracks
        for (int trackNum = 0; trackNum < numTracks; ++trackNum)
        {
            const juce::MidiMessageSequence* track = midiFile.getTrack(trackNum);
            if (!track)
                continue;

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& event = track->getEventPointer(i);
                const auto& message = event->message;

                if (message.isTempoMetaEvent())
                {
                    bpm = 60.0 / message.getTempoSecondsPerQuarterNote();
                    DBG("Found tempo: " + juce::String(bpm, 2) + " BPM");
                }
                else if (message.isTimeSignatureMetaEvent())
                {
                    message.getTimeSignatureInfo(timeSignatureNumerator, timeSignatureDenominator);
                    DBG("Found time signature: " + juce::String(timeSignatureNumerator) + "/" + juce::String(timeSignatureDenominator));
                }
            }
        }

        // Find the first track with drum notes
        int drumTrackIndex = -1;
        int maxNotes = 0;

        for (int trackNum = 0; trackNum < numTracks; ++trackNum)
        {
            const juce::MidiMessageSequence* track = midiFile.getTrack(trackNum);
            if (!track)
                continue;

            int noteCount = 0;
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& event = track->getEventPointer(i);
                if (event->message.isNoteOn() && event->message.getVelocity() > 0)
                    noteCount++;
            }

            DBG("Track " + juce::String(trackNum) + " has " + juce::String(noteCount) + " notes");

            // Use the track with the most notes (typically the drum track)
            if (noteCount > maxNotes)
            {
                maxNotes = noteCount;
                drumTrackIndex = trackNum;
            }
        }

        if (drumTrackIndex == -1)
        {
            DBG("No drum notes found in any track!");
            return false;
        }

        DBG("Using track " + juce::String(drumTrackIndex) + " (has " + juce::String(maxNotes) + " notes)");

        // Load notes from the selected track only
        const juce::MidiMessageSequence* track = midiFile.getTrack(drumTrackIndex);

        // Use updateMatchedPairs() to properly pair note on/off
        juce::MidiMessageSequence trackCopy = *track;
        trackCopy.updateMatchedPairs();

        // Extract note on/off pairs
        for (int i = 0; i < trackCopy.getNumEvents(); ++i)
        {
            const auto* event = trackCopy.getEventPointer(i);
            const auto& message = event->message;

            // Only process NoteOn with velocity > 0
            if (message.isNoteOn() && message.getVelocity() > 0)
            {
                int noteNumber = message.getNoteNumber();
                int velocity = message.getVelocity();
                double startTicks = message.getTimeStamp();

                // Get the paired note-off time
                double endTicks = startTicks + (ppqn * 0.25); // Default: 16th note

                if (event->noteOffObject != nullptr)
                {
                    endTicks = event->noteOffObject->message.getTimeStamp();
                }

                // Convert ticks to quarter notes
                double startQuarterNotes = startTicks / ppqn;
                double durationQuarterNotes = (endTicks - startTicks) / ppqn;

                // Sanity check duration
                if (durationQuarterNotes < 0.01)
                    durationQuarterNotes = 0.25; // Minimum 16th note

                    // Categorize drum part
                    DrumPartType partType = MidiDissector::getPartTypeFromNote(noteNumber, sourceLibrary);

                // Create note
                MidiNote note(noteNumber, velocity, startQuarterNotes, durationQuarterNotes, partType);
                notes.push_back(note);
            }
        }

        DBG("TOTAL NOTES LOADED: " + juce::String(notes.size()));

        sortNotes();

        // Estimate length in bars
        if (!notes.empty())
        {
            double maxTime = 0.0;
            for (const auto& note : notes)
            {
                double noteEnd = note.startTime + note.duration;
                if (noteEnd > maxTime)
                    maxTime = noteEnd;
            }
            double beatsPerBar = timeSignatureNumerator;
            lengthInBars = static_cast<int>(std::ceil(maxTime / beatsPerBar));
            if (lengthInBars < 1)
                lengthInBars = 1;

            DBG("MIDI length: " + juce::String(lengthInBars) + " bars, max time: " + juce::String(maxTime, 2) + " QN");
        }

        name = file.getFileNameWithoutExtension();

        DBG("========================================");
        DBG("LOADED MIDI: " + name);
        DBG("Notes: " + juce::String(notes.size()));
        DBG("BPM: " + juce::String(bpm, 2));
        DBG("Length: " + juce::String(lengthInBars) + " bars");
        DBG("========================================");

        // Log note distribution by drum part
        std::map<DrumPartType, int> partCounts;
        for (const auto& note : notes)
        {
            partCounts[note.drumPart]++;
        }

        DBG("Note distribution:");
        for (const auto& pair : partCounts)
        {
            DBG("  Part " + juce::String(static_cast<int>(pair.first)) + ": " + juce::String(pair.second) + " notes");
        }

        return true;
    }
    catch (...)
    {
        DBG("EXCEPTION loading MIDI file!");
        return false;
    }
}

bool EditableMidiClip::saveToMidiFile(const juce::File& file)
{
    try
    {
        juce::MidiFile midiFile;
        int ppqn = 480; // Standard PPQN
        midiFile.setTicksPerQuarterNote(ppqn);

        juce::MidiMessageSequence track;

        // Add tempo event at start (at tick 0)
        track.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(60000000.0 / bpm)), 0.0);

        // Add time signature event (at tick 0)
        track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(timeSignatureNumerator, timeSignatureDenominator), 0.0);

        // Add all notes with TICK-based timestamps
        for (const auto& note : notes)
        {
            // CRITICAL FIX: Convert quarter notes to TICKS, not seconds
            double startTicks = note.startTime * ppqn;  // Quarter notes * PPQN = ticks
            double endTicks = (note.startTime + note.duration) * ppqn;

            // Note on
            track.addEvent(juce::MidiMessage::noteOn(1, note.noteNumber, static_cast<juce::uint8>(note.velocity)), startTicks);

            // Note off
            track.addEvent(juce::MidiMessage::noteOff(1, note.noteNumber, static_cast<juce::uint8>(0)), endTicks);
        }

        track.updateMatchedPairs();
        midiFile.addTrack(track);

        // CRITICAL FIX: Use scoped block to ensure stream is closed and flushed
        {
            juce::FileOutputStream stream(file);
            if (!stream.openedOk())
            {
                DBG("ERROR: Could not open file for writing: " + file.getFullPathName());
                return false;
            }

            if (!midiFile.writeTo(stream))
            {
                DBG("ERROR: Failed to write MIDI data to stream");
                return false;
            }

            // Explicitly flush to ensure data is written to disk
            stream.flush();
        } // Stream is destroyed here, ensuring file is closed

        // CRITICAL: Verify the file was actually written
        if (!file.existsAsFile() || file.getSize() == 0)
        {
            DBG("ERROR: File not created or empty after write");
            return false;
        }

        DBG("MIDI file saved successfully: " + file.getFullPathName() + " (" + juce::String(file.getSize()) + " bytes)");
        return true;
    }
    catch (const std::exception& e)
    {
        DBG("EXCEPTION in saveToMidiFile: " + juce::String(e.what()));
        return false;
    }
    catch (...)
    {
        DBG("UNKNOWN EXCEPTION in saveToMidiFile");
        return false;
    }
}

void EditableMidiClip::addNote(const MidiNote& note)
{
    notes.push_back(note);
    sortNotes();
}

void EditableMidiClip::removeNote(const juce::String& noteId)
{
    notes.erase(std::remove_if(notes.begin(), notes.end(),
        [&noteId](const MidiNote& n) { return n.id == noteId; }), 
        notes.end());
}

void EditableMidiClip::removeNotes(const juce::Array<juce::String>& noteIds)
{
    for (const auto& id : noteIds)
        removeNote(id);
}

void EditableMidiClip::moveNote(const juce::String& noteId, double newStartTime, int newNoteNumber)
{
    auto* note = findNote(noteId);
    if (note)
    {
        note->startTime = newStartTime;
        note->noteNumber = newNoteNumber;
        note->drumPart = MidiDissector::getPartTypeFromNote(newNoteNumber, sourceLibrary);
        sortNotes();
    }
}

void EditableMidiClip::setNoteVelocity(const juce::String& noteId, int newVelocity)
{
    auto* note = findNote(noteId);
    if (note)
    {
        note->velocity = juce::jlimit(1, 127, newVelocity);
    }
}

MidiNote* EditableMidiClip::findNote(const juce::String& noteId)
{
    for (auto& note : notes)
    {
        if (note.id == noteId)
            return &note;
    }
    return nullptr;
}

void EditableMidiClip::selectNote(const juce::String& noteId, bool addToSelection)
{
    if (!addToSelection)
        clearSelection();
    
    auto* note = findNote(noteId);
    if (note)
        note->isSelected = true;
}

void EditableMidiClip::selectNotesInRegion(double startTime, double endTime, int minNote, int maxNote)
{
    for (auto& note : notes)
    {
        if (note.startTime >= startTime && note.startTime <= endTime &&
            note.noteNumber >= minNote && note.noteNumber <= maxNote)
        {
            note.isSelected = true;
        }
    }
}

void EditableMidiClip::clearSelection()
{
    for (auto& note : notes)
        note.isSelected = false;
}

juce::Array<MidiNote*> EditableMidiClip::getSelectedNotes()
{
    juce::Array<MidiNote*> selected;
    for (auto& note : notes)
    {
        if (note.isSelected)
            selected.add(&note);
    }
    return selected;
}

juce::Array<MidiNote*> EditableMidiClip::getNotesInTimeRange(double startTime, double endTime)
{
    juce::Array<MidiNote*> result;
    for (auto& note : notes)
    {
        if (note.startTime >= startTime && note.startTime <= endTime)
            result.add(&note);
    }
    return result;
}

juce::Array<MidiNote*> EditableMidiClip::getNotesForDrumPart(DrumPartType part)
{
    juce::Array<MidiNote*> result;
    for (auto& note : notes)
    {
        if (note.drumPart == part)
            result.add(&note);
    }
    return result;
}

double EditableMidiClip::getDurationInQuarterNotes() const
{
    return lengthInBars * timeSignatureNumerator;
}

double EditableMidiClip::getDurationInSeconds() const
{
    return getDurationInQuarterNotes() * (60.0 / bpm);
}

void EditableMidiClip::sortNotes()
{
    std::sort(notes.begin(), notes.end());
}

void EditableMidiClip::clear()
{
    notes.clear();
    name.clear();
    sourceLibrary = DrumLibrary::Unknown;
    bpm = 120.0;
    timeSignatureNumerator = 4;
    timeSignatureDenominator = 4;
    lengthInBars = 4;
}

double EditableMidiClip::ticksToQuarterNotes(int ticks, int ppqn) const
{
    return static_cast<double>(ticks) / static_cast<double>(ppqn);
}

int EditableMidiClip::quarterNotesToTicks(double quarterNotes, int ppqn) const
{
    return static_cast<int>(quarterNotes * ppqn);
}
