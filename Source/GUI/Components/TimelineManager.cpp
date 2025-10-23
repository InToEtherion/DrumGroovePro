// TimelineManager.cpp - COMPLETE FIX including temp file handling
// This version correctly saves and restores temporary MIDI files

#include "TimelineManager.h"
#include "MultiTrackContainer.h"
#include <fstream>

//==============================================================================
TimelineManager::TimelineManager(MultiTrackContainer* container)
    : container(container)
{
}

//==============================================================================
TimelineManager::~TimelineManager()
{
    // Clean up temp drag file if it exists
    if (lastTempDragFile.existsAsFile())
    {
        lastTempDragFile.deleteFile();
        DBG("TimelineManager: Cleaned up temp drag file on destruction");
    }
}


//==============================================================================
void TimelineManager::saveTimelineState()
{
    auto targetFolder = chooseSaveLocation();
    if (targetFolder == juce::File{}) return;

    // âœ… CRITICAL FIX: Check if folder is not empty and confirm deletion
    if (!confirmOverwriteFolder(targetFolder))
    {
        DBG("User cancelled save due to non-empty folder");
        return;
    }

    // âœ… CRITICAL FIX: Clear folder contents if not empty
    if (!isFolderEmpty(targetFolder))
    {
        if (!clearFolderContents(targetFolder))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Save Error", 
                "Could not clear folder contents. Please choose an empty folder or delete the contents manually.",
                "OK");
            return;
        }
    }

    if (!targetFolder.createDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Save Error", "Could not create timeline folder", "OK");
        return;
    }

    // Create subfolder for MIDI files
    auto midiFolder = targetFolder.getChildFile("midi_files");
    midiFolder.createDirectory();

    // Save GUI state BEFORE copying files (will be modified)
    auto state = container->saveGuiState();
    
    // Copy temp MIDI files to the save folder and update paths
    copyTempMidiFiles(midiFolder, state);
    
    // Update metadata with folder location
    createTimelineMetadata(state, targetFolder);
    
    // Save state to file
    auto stateFile = targetFolder.getChildFile("timeline.state");
    juce::FileOutputStream stream(stateFile);
    if (stream.openedOk())
    {
        state.writeToStream(stream);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Timeline Saved", "Timeline state saved successfully", "OK");
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Save Error", "Could not save timeline state", "OK");
    }
}

//==============================================================================
void TimelineManager::loadTimelineState()
{
    auto stateFile = chooseLoadLocation();
    if (stateFile == juce::File{} || !stateFile.existsAsFile()) return;

    juce::FileInputStream stream(stateFile);
    if (!stream.openedOk())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Load Error", "Could not read timeline state", "OK");
        return;
    }

    auto state = juce::ValueTree::readFromStream(stream);
    if (!state.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Load Error", "Invalid timeline state file", "OK");
        return;
    }

    // Get the folder where state was loaded from
    auto folder = stateFile.getParentDirectory();
    
    // Restore MIDI file paths (convert from saved folder to current location)
    restoreTimelineMetadata(state, folder);
    
    // Restore timeline
    container->restoreGuiState(state);
    
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "Timeline Loaded", "Timeline state loaded successfully", "OK");
}

//==============================================================================
void TimelineManager::exportTimelineAsMidi()
{
    // âœ… CRITICAL FIX: Force GUI refresh before export
    container->repaint();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    
    auto saveFile = chooseExportLocation(true);
    if (saveFile == juce::File{}) return;

    // Check for overlaps BEFORE creating combined MIDI
    std::vector<ClipBoundary> allClipBoundaries;
    
    // âœ… Log what's being exported
    DBG("=== Starting Combined MIDI Export ===");
    
    for (int trackIdx = 0; trackIdx < container->getNumTracks(); ++trackIdx)
    {
        double trackBPM = container->getTrackBPM(trackIdx);
        auto clips = container->getTrackClips(trackIdx);
        
        DBG("Track " + juce::String(trackIdx + 1) + " (BPM=" + juce::String(trackBPM, 2) + 
            ") has " + juce::String(clips.size()) + " clips");
        
        for (const auto* clip : clips)
        {
            if (!clip || !clip->file.existsAsFile())
                continue;
            
            DBG("  - " + clip->name + " at " + juce::String(clip->startTime, 3) + "s");
            
            ClipBoundary boundary;
            boundary.startTime = clip->startTime;
            
            double visualScaleFactor = clip->referenceBPM / trackBPM;
            double visualDuration = clip->duration * visualScaleFactor;
            boundary.endTime = clip->startTime + visualDuration;
            
            boundary.bpm = trackBPM;
            boundary.trackIndex = trackIdx;
            boundary.clip = clip;
            
            allClipBoundaries.push_back(boundary);
        }
    }
    
    // Check for overlaps with different BPMs
    juce::String errorMessage;
    if (checkForOverlapsWithDifferentBPM(allClipBoundaries, errorMessage))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Error",
            errorMessage,
            "OK"
        );
        return;  // Return WITHOUT creating the file
    }
    
    // Only create the MIDI file if there are no errors
    auto midiFile = createCombinedMidiFile();
    
    // Ensure MIDI file format 1 for multi-track with tempo
    if (midiFile.getNumTracks() > 0)
    {
        DBG("Writing MIDI file with " + juce::String(midiFile.getNumTracks()) + " tracks");
        DBG("MIDI file format: " + juce::String(midiFile.getTimeFormat()) + " ticks per quarter note");
    }
    
    juce::FileOutputStream stream(saveFile);
    if (stream.openedOk())
    {
        midiFile.writeTo(stream);
        stream.flush();  // Ensure all data is written
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Export Complete", "Timeline exported as single MIDI file", "OK");
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Export Error", "Could not export MIDI file", "OK");
    }
}

//==============================================================================
void TimelineManager::exportTimelineAsSeparateMidis()
{
    // Force GUI refresh before export to ensure we have current clips
    container->repaint();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    
    auto targetFolder = chooseExportLocation(false);
    if (targetFolder == juce::File{}) return;


    // Check if folder is not empty and confirm deletion
    if (!confirmOverwriteFolder(targetFolder))
    {
        DBG("User cancelled export due to non-empty folder");
        return;
    }

    // Clear folder contents if not empty
    if (!isFolderEmpty(targetFolder))
    {
        if (!clearFolderContents(targetFolder))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Export Error", 
                "Could not clear folder contents. Please choose an empty folder or delete the contents manually.",
                "OK");
            return;
        }
    }

    auto result = targetFolder.createDirectory();
    if (!result.wasOk())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Export Error", 
            "Could not create export folder:\n" + result.getErrorMessage(), 
            "OK");
        return;
    }

    bool trimSilence = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon,
        "Export Options",
        "Would you like to trim silence from the beginning of each track?\n\n"
        "Select 'No' to preserve the exact timeline positions.",
        "No, keep silence", "Yes, trim");

    // âœ… CRITICAL FIX: Log which clips are being exported for debugging
    DBG("=== Starting MIDI Export ===");
    for (int i = 0; i < container->getNumTracks(); ++i)
    {
        auto clips = container->getTrackClips(i);
        DBG("Track " + juce::String(i + 1) + " has " + juce::String(clips.size()) + " clips:");
        for (const auto* clip : clips)
        {
            if (clip)
            {
                DBG("  - " + clip->name + " at " + juce::String(clip->startTime, 3) + "s");
            }
        }
    }

    int successCount = 0;
    for (int i = 0; i < container->getNumTracks(); ++i)
    {
        // Skip tracks with no clips
        auto clips = container->getTrackClips(i);
        if (clips.empty())
        {
            DBG("Skipping track " + juce::String(i + 1) + " - no clips");
            continue;
        }
        
        auto midiFile = createMidiFileForTrack(i, !trimSilence);
        
        // Use actual track name instead of generic "Track_N"
        juce::String trackName = container->getTrackName(i);
        if (trackName.isEmpty() || trackName == "Track " + juce::String(i + 1))
        {
            trackName = "Track_" + juce::String(i + 1);
        }
        
        // Sanitize filename
        trackName = trackName.replaceCharacters("/\\:*?\"<>|", "_");
        
        auto midiFilePath = targetFolder.getChildFile(trackName + ".mid");
        
        juce::FileOutputStream stream(midiFilePath);
        if (stream.openedOk())
        {
            midiFile.writeTo(stream);
            successCount++;
            DBG("Exported: " + trackName + ".mid");
        }
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "Export Complete", 
        juce::String(successCount) + " MIDI file" + (successCount != 1 ? "s" : "") + " exported successfully", 
        "OK");
}

//==============================================================================
juce::File TimelineManager::chooseSaveLocation() const
{
    juce::FileChooser chooser("Save Timeline State",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*",
        true);

    if (chooser.browseForDirectory())
        return chooser.getResult();
    
    return juce::File{};
}

//==============================================================================
juce::File TimelineManager::chooseLoadLocation() const
{
    juce::FileChooser chooser("Load Timeline State",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.state",
        true);

    if (chooser.browseForFileToOpen())
        return chooser.getResult();
    
    return juce::File{};
}

//==============================================================================
juce::File TimelineManager::chooseExportLocation(bool isSingleFile) const
{
    if (isSingleFile)
    {
        juce::FileChooser chooser("Export MIDI File",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("timeline.mid"),
            "*.mid",
            true);
        
        if (chooser.browseForFileToSave(true))
            return chooser.getResult();
    }
    else
    {
        juce::FileChooser chooser("Choose Export Folder",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("timeline_export"),
            "*",
            true);
        
        if (chooser.browseForDirectory())
            return chooser.getResult();
    }
    
    return juce::File{};
}

//==============================================================================
// FIXED: Complete implementation for copying temp MIDI files
void TimelineManager::copyTempMidiFiles(const juce::File& targetFolder, juce::ValueTree& state) const
{
    // Get system temp directory
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    
    // Track which files we've copied to avoid duplicates
    juce::StringArray copiedFiles;
    int fileCounter = 1;
    
    // Process all tracks
    auto tracksTree = state.getChildWithName("Tracks");
    if (!tracksTree.isValid()) return;
    
    for (auto trackNode : tracksTree)
    {
        auto clipsTree = trackNode.getChildWithName("Clips");
        if (!clipsTree.isValid()) continue;
        
        // Process all clips in this track
        for (auto clipNode : clipsTree)
        {
            juce::String filePath = clipNode.getProperty("file", "").toString();
            if (filePath.isEmpty()) continue;
            
            juce::File clipFile(filePath);
            
            // Check if this file is in the temp directory
            bool isInTempDir = clipFile.getFullPathName().startsWith(tempDir.getFullPathName());
            
            // Also check for common temp file patterns
            bool isTempFile = clipFile.getFileName().startsWith("DrumGroovePro_temp") ||
                             clipFile.getFileName().startsWith("DrumGroovePro_part") ||
                             clipFile.getFileName().startsWith("drum_part_");
            
            if ((isInTempDir || isTempFile) && clipFile.existsAsFile())
            {
                // Generate a new filename that's more permanent
                juce::String newFileName = "clip_" + juce::String(fileCounter).paddedLeft('0', 4) + ".mid";
                fileCounter++;
                
                juce::File targetFile = targetFolder.getChildFile(newFileName);
                
                // Copy the file
                if (clipFile.copyFileTo(targetFile))
                {
                    // Update the path in the state tree to point to the new location
                    // Store as relative path from the state file
                    juce::String relativePath = "midi_files/" + newFileName;
                    clipNode.setProperty("file", relativePath, nullptr);
                    
                    DBG("Copied temp file: " + clipFile.getFileName() + " -> " + newFileName);
                }
                else
                {
                    DBG("Failed to copy temp file: " + clipFile.getFullPathName());
                }
            }
            else if (!clipFile.existsAsFile())
            {
                DBG("WARNING: Clip references non-existent file: " + filePath);
            }
            // Regular files (not in temp) are left with their original paths
        }
    }
    
    DBG("Copied " + juce::String(fileCounter - 1) + " temporary MIDI files");
}

//==============================================================================
void TimelineManager::createTimelineMetadata(juce::ValueTree& state, const juce::File& folder) const
{
    state.setProperty("timelineFolder", folder.getFullPathName(), nullptr);
    state.setProperty("exportDate", juce::Time::getCurrentTime().toString(true, true), nullptr);
    state.setProperty("pluginVersion", "0.9.9", nullptr);
}

//==============================================================================
void TimelineManager::restoreTimelineMetadata(const juce::ValueTree& state, const juce::File& folder)
{
    // Get the saved timeline folder
    juce::String savedFolder = state.getProperty("timelineFolder", "").toString();
    
    if (savedFolder.isEmpty()) return;
    
    // Process all tracks and update file paths
    auto tracksTree = state.getChildWithName("Tracks");
    if (!tracksTree.isValid()) return;
    
    for (auto trackNode : tracksTree)
    {
        auto clipsTree = trackNode.getChildWithName("Clips");
        if (!clipsTree.isValid()) continue;
        
        for (auto clipNode : clipsTree)
        {
            juce::String filePath = clipNode.getProperty("file", "").toString();
            if (filePath.isEmpty()) continue;
            
            // Check if this is a relative path (our saved temp files)
            if (!filePath.contains(":") && !filePath.startsWith("/"))
            {
                // This is a relative path - make it absolute relative to the load folder
                juce::File absoluteFile = folder.getChildFile(filePath);
                
                if (absoluteFile.existsAsFile())
                {
                    clipNode.setProperty("file", absoluteFile.getFullPathName(), nullptr);
                    DBG("Resolved relative path: " + filePath + " -> " + absoluteFile.getFullPathName());
                }
                else
                {
                    DBG("WARNING: Cannot find saved MIDI file: " + absoluteFile.getFullPathName());
                }
            }
            // Absolute paths are left as-is (regular MIDI files from browser)
        }
    }
}

//==============================================================================
// FIXED: Complete implementation for per-track MIDI export
juce::MidiFile TimelineManager::createMidiFileForTrack(int trackIndex, bool includeSilence) const
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);
    
    auto clips = container->getTrackClips(trackIndex);
    
    if (clips.empty())
    {
        juce::MidiMessageSequence emptySequence;
        midiFile.addTrack(emptySequence);
        return midiFile;
    }
    
    // Sort clips by start time
    std::vector<const MidiClip*> sortedClips(clips.begin(), clips.end());
    std::sort(sortedClips.begin(), sortedClips.end(),
        [](const MidiClip* a, const MidiClip* b) {
            return a->startTime < b->startTime;
        });
    
    double startOffset = includeSilence ? 0.0 : sortedClips[0]->startTime;
    double trackBPM = container->getTrackBPM(trackIndex);
    
    DBG("=== Exporting Track " + juce::String(trackIndex + 1) + " ===");
    DBG("Track BPM: " + juce::String(trackBPM, 2));
    DBG("Start offset: " + juce::String(startOffset, 6));
    DBG("Include silence: " + juce::String(includeSilence ? "YES" : "NO"));
    if (!sortedClips.empty())
    {
        DBG("First clip starts at: " + juce::String(sortedClips[0]->startTime, 6) + "s");
        DBG("Last clip ends at: " + juce::String(sortedClips.back()->startTime + sortedClips.back()->duration, 6) + "s");
    }
    
    // Create the MIDI sequence
    juce::MidiMessageSequence trackSequence;
    
    // CRITICAL FIX: Set tempo at tick 0 with proper format
    // Tempo must be the FIRST event in the sequence for DAWs to recognize it
    int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / trackBPM);
    auto tempoMsg = juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote);
    trackSequence.addEvent(tempoMsg, 0.0);
    
    // Add time signature (4/4) at tick 0
    auto timeSigMsg = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
    trackSequence.addEvent(timeSigMsg, 0.0);
    
    DBG("Added tempo: " + juce::String(trackBPM, 2) + " BPM (microseconds: " + 
        juce::String(microsecondsPerQuarterNote) + ") at tick 0");
    
    // Process each clip
    for (const auto* clip : sortedClips)
    {
        if (!clip->file.existsAsFile())
            continue;
        
        juce::MidiFile clipMidiFile;
        juce::FileInputStream stream(clip->file);
        if (!stream.openedOk() || !clipMidiFile.readFrom(stream))
            continue;
        
        // Get clip's original BPM from tempo events
        double clipOriginalBPM = 120.0;
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (event && event->message.isTempoMetaEvent())
                {
                    clipOriginalBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                    DBG("Found clip original BPM: " + juce::String(clipOriginalBPM, 2));
                    break;
                }
            }
            if (clipOriginalBPM != 120.0) break;
        }
        
        // Calculate where this clip starts in the export
        double clipStartInTimeline = clip->startTime;
        double exportClipStartTime = clipStartInTimeline - startOffset;
        
        DBG("Clip: " + clip->name);
        DBG("  Original BPM: " + juce::String(clipOriginalBPM, 2));
        DBG("  Timeline position: " + juce::String(clipStartInTimeline, 6) + "s");
        DBG("  Export position: " + juce::String(exportClipStartTime, 6) + "s");
        DBG("  Duration: " + juce::String(clip->duration, 6) + "s");
        
        int eventCount = 0;
        
        // Process all MIDI events
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (!event) continue;
                
                auto message = event->message;
                
                // Skip tempo and time signature events (we set our own)
                if (message.isTempoMetaEvent() || message.isTimeSignatureMetaEvent())
                    continue;
                
                // Skip other meta events that might interfere
                if (message.isMetaEvent() && !message.isEndOfTrackMetaEvent())
                    continue;
                
                // Get the PPQ from the source file
                double ticksPerQuarterNote = clipMidiFile.getTimeFormat();
                if (ticksPerQuarterNote <= 0) ticksPerQuarterNote = 960.0;
                
                // Convert event time from ticks to seconds using clip's original BPM
                double eventTimeInSeconds = (message.getTimeStamp() / ticksPerQuarterNote) * (60.0 / clipOriginalBPM);
                
                // Only include events within the clip duration
                if (eventTimeInSeconds > clip->duration)
                    continue;
                
                // Calculate absolute time in the exported file
                double absoluteTimeInSeconds = exportClipStartTime + eventTimeInSeconds;
                
                if (absoluteTimeInSeconds < 0.0)
                    continue;
                
                // Convert to ticks using the TRACK's BPM (this is the export BPM)
                double beatsAtTrackBPM = absoluteTimeInSeconds * (trackBPM / 60.0);
                double ticksAtTrackBPM = beatsAtTrackBPM * 960.0;
                
                // Create the event with proper timing
                juce::MidiMessage exportMessage = message;
                trackSequence.addEvent(exportMessage, ticksAtTrackBPM);
                
                eventCount++;
                
                if (eventCount <= 5)  // Log first few events for debugging
                {
                    DBG("    Event " + juce::String(eventCount) + 
                        ": Type=" + (message.isNoteOn() ? "NoteOn" : (message.isNoteOff() ? "NoteOff" : "Other")) +
                        " Note=" + juce::String(message.getNoteNumber()) +
                        " at " + juce::String(absoluteTimeInSeconds, 6) + "s" +
                        " = " + juce::String(ticksAtTrackBPM, 2) + " ticks");
                }
            }
        }
        
        DBG("  Added " + juce::String(eventCount) + " events from this clip");
    }
    
    // Add end-of-track event
    double totalDurationSeconds = 0.0;
    for (const auto* clip : sortedClips)
    {
        double clipEnd = (clip->startTime - startOffset) + clip->duration;
        totalDurationSeconds = juce::jmax(totalDurationSeconds, clipEnd);
    }
    
    if (totalDurationSeconds > 0.0)
    {
        double endTicks = (totalDurationSeconds * (trackBPM / 60.0)) * 960.0;
        auto endOfTrack = juce::MidiMessage::endOfTrack();
        trackSequence.addEvent(endOfTrack, endTicks);
        DBG("Added end-of-track at " + juce::String(totalDurationSeconds, 6) + 
            "s = " + juce::String(endTicks, 2) + " ticks");
    }
    
    // CRITICAL: Sort the sequence AFTER adding all events
    trackSequence.sort();
    trackSequence.updateMatchedPairs();
    
    // Add the track to the MIDI file
    midiFile.addTrack(trackSequence);
    
    // Log final statistics
    int totalEvents = trackSequence.getNumEvents();
    DBG("Total events in track: " + juce::String(totalEvents));
    
    if (totalEvents > 0)
    {
        auto* firstEvent = trackSequence.getEventPointer(0);
        auto* secondEvent = totalEvents > 1 ? trackSequence.getEventPointer(1) : nullptr;
        
        if (firstEvent)
        {
            DBG("First event: " + (firstEvent->message.isTempoMetaEvent() ? "Tempo" : 
                                  (firstEvent->message.isTimeSignatureMetaEvent() ? "TimeSig" : "Other")) +
                " at tick " + juce::String(firstEvent->message.getTimeStamp()));
        }
        
        if (secondEvent)
        {
            DBG("Second event: " + (secondEvent->message.isTempoMetaEvent() ? "Tempo" : 
                                   (secondEvent->message.isTimeSignatureMetaEvent() ? "TimeSig" : "Other")) +
                " at tick " + juce::String(secondEvent->message.getTimeStamp()));
        }
    }
    
    DBG("=== Track Export Complete ===");
    
    return midiFile;
}

//==============================================================================
// FIXED: Correct BPM time conversion
juce::MidiFile TimelineManager::createCombinedMidiFile() const
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);
    
    DBG("=== Creating Combined MIDI File ===");
    
    // Collect all clips with their visual boundaries
    std::vector<ClipBoundary> allClipBoundaries;
    
    for (int trackIdx = 0; trackIdx < container->getNumTracks(); ++trackIdx)
    {
        double trackBPM = container->getTrackBPM(trackIdx);
        auto clips = container->getTrackClips(trackIdx);
        
        for (const auto* clip : clips)
        {
            if (!clip || !clip->file.existsAsFile())
                continue;
            
            ClipBoundary boundary;
            boundary.startTime = clip->startTime;
            
            // CRITICAL: Calculate visual end time considering BPM scaling
            double visualScaleFactor = clip->referenceBPM / trackBPM;
            double visualDuration = clip->duration * visualScaleFactor;
            boundary.endTime = clip->startTime + visualDuration;
            
            boundary.bpm = trackBPM;
            boundary.trackIndex = trackIdx;
            boundary.clip = clip;
            
            allClipBoundaries.push_back(boundary);
            
            DBG("Track " + juce::String(trackIdx) + " - Clip: " + clip->name + 
                " | Start: " + juce::String(boundary.startTime, 3) + 
                " | End: " + juce::String(boundary.endTime, 3) + 
                " | BPM: " + juce::String(boundary.bpm, 2));
        }
    }
    
    if (allClipBoundaries.empty())
    {
        juce::MidiMessageSequence emptySequence;
        midiFile.addTrack(emptySequence);
        return midiFile;
    }
    
    // Check for overlaps with different BPMs
    juce::String errorMessage;
    if (checkForOverlapsWithDifferentBPM(allClipBoundaries, errorMessage))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Error",
            errorMessage,
            "OK"
        );
        
        juce::MidiMessageSequence emptySequence;
        midiFile.addTrack(emptySequence);
        return midiFile;
    }
    
    // Sort clips by start time
    std::sort(allClipBoundaries.begin(), allClipBoundaries.end(),
        [](const ClipBoundary& a, const ClipBoundary& b) {
            return a.startTime < b.startTime;
        });
    
    // Build tempo map and collect all events
    struct TempoChange {
        double timeInSeconds;
        double bpm;
    };
    std::vector<TempoChange> tempoMap;
    
    struct TimedEvent {
        double timeInSeconds;
        juce::MidiMessage message;
    };
    std::vector<TimedEvent> allEvents;
    
    // Process each clip
    for (const auto& boundary : allClipBoundaries)
    {
        const auto* clip = boundary.clip;
        
        // Load the MIDI file
        juce::MidiFile clipMidiFile;
        juce::FileInputStream stream(clip->file);
        if (!stream.openedOk() || !clipMidiFile.readFrom(stream))
            continue;
        
        // Get clip's original BPM
        double clipOriginalBPM = 120.0;
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (event && event->message.isTempoMetaEvent())
                {
                    clipOriginalBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
            if (clipOriginalBPM != 120.0) break;
        }
        
        // Add tempo change at clip start (if BPM is different from previous)
        if (tempoMap.empty() || std::abs(tempoMap.back().bpm - boundary.bpm) > 0.01)
        {
            TempoChange tc;
            tc.timeInSeconds = clip->startTime;
            tc.bpm = boundary.bpm;
            tempoMap.push_back(tc);
            
            DBG("Tempo change: " + juce::String(tc.bpm, 2) + " BPM at " + 
                juce::String(tc.timeInSeconds, 6) + "s");
        }
        
        // Process all note events
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (!event) continue;
                
                auto message = event->message;
                
                // Skip tempo and time signature events
                if (message.isTempoMetaEvent() || message.isTimeSignatureMetaEvent())
                    continue;
                
                // Convert event time from MIDI ticks to seconds using clip's original BPM
                double ticksPerQuarterNote = clipMidiFile.getTimeFormat();
                if (ticksPerQuarterNote <= 0) ticksPerQuarterNote = 960.0;  // Match our file's PPQ
                
                double eventTimeInSeconds = (message.getTimeStamp() / ticksPerQuarterNote) * (60.0 / clipOriginalBPM);
                
                // Calculate absolute time in export
                double absoluteTime = clip->startTime + eventTimeInSeconds;
                
                // Only add if within clip's visual boundaries
                if (absoluteTime >= clip->startTime && absoluteTime <= boundary.endTime)
                {
                    TimedEvent te;
                    te.timeInSeconds = absoluteTime;
                    te.message = message;
                    allEvents.push_back(te);
                }
            }
        }
    }
    
    // Convert everything to MIDI ticks considering tempo changes
    juce::MidiMessageSequence combinedSequence;
    
    // Function to convert seconds to ticks with variable tempo
    auto secondsToTicks = [&tempoMap](double seconds) -> double {
        if (tempoMap.empty()) return seconds * (120.0 / 60.0) * 960.0;
        
        double ticks = 0.0;
        double prevTime = 0.0;
        double prevBPM = tempoMap[0].bpm;
        
        for (const auto& tc : tempoMap)
        {
            if (tc.timeInSeconds > seconds)
                break;
            
            double deltaSeconds = tc.timeInSeconds - prevTime;
            double deltaBeats = deltaSeconds * (prevBPM / 60.0);
            ticks += deltaBeats * 960.0;
            
            prevTime = tc.timeInSeconds;
            prevBPM = tc.bpm;
        }
        
        double deltaSeconds = seconds - prevTime;
        double deltaBeats = deltaSeconds * (prevBPM / 60.0);
        ticks += deltaBeats * 960.0;
        
        return ticks;
    };
    
    // Add tempo changes to MIDI
    for (const auto& tc : tempoMap)
    {
        double ticks = secondsToTicks(tc.timeInSeconds);
        int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / tc.bpm);
        combinedSequence.addEvent(juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote), ticks);
    }
    
    // Add time signature at tick 0 for better DAW compatibility
    juce::MidiMessage timeSigMsg = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
    timeSigMsg.setTimeStamp(0);
    combinedSequence.addEvent(timeSigMsg, 0.0);
    
    // Add all note events
    for (const auto& te : allEvents)
    {
        double eventTicks = secondsToTicks(te.timeInSeconds);
        auto message = te.message;
        message.setTimeStamp(static_cast<int>(eventTicks + 0.5));  // Round to nearest tick
        combinedSequence.addEvent(message);
    }
    
    combinedSequence.updateMatchedPairs();
    combinedSequence.sort();
    midiFile.addTrack(combinedSequence);
    
    DBG("=== Export Complete ===");
    DBG("Total events: " + juce::String(allEvents.size()));
    DBG("Tempo changes: " + juce::String(tempoMap.size()));
    
    return midiFile;
}

bool TimelineManager::checkForOverlapsWithDifferentBPM(const std::vector<ClipBoundary>& boundaries, juce::String& errorMessage) const
{
    for (size_t i = 0; i < boundaries.size(); ++i)
    {
        for (size_t j = i + 1; j < boundaries.size(); ++j)
        {
            const auto& clip1 = boundaries[i];
            const auto& clip2 = boundaries[j];
            
            // Check if clips overlap
            bool overlaps = !(clip1.endTime <= clip2.startTime || clip2.endTime <= clip1.startTime);
            
            if (overlaps)
            {
                // Check if BPMs are different
                if (std::abs(clip1.bpm - clip2.bpm) > 0.01)
                {
                    errorMessage = "Overlapping MIDIs with different BPMs is not allowed.\n\n";
                    errorMessage += "Overlap detected between:\n";
                    errorMessage += "â€¢ Track " + juce::String(clip1.trackIndex + 1) + ": \"" + clip1.clip->name + "\" (" + juce::String(clip1.bpm, 1) + " BPM)\n";
                    errorMessage += "â€¢ Track " + juce::String(clip2.trackIndex + 1) + ": \"" + clip2.clip->name + "\" (" + juce::String(clip2.bpm, 1) + " BPM)\n\n";
                    errorMessage += "Time range: " + juce::String(juce::jmax(clip1.startTime, clip2.startTime), 2) + "s to " + 
                                   juce::String(juce::jmin(clip1.endTime, clip2.endTime), 2) + "s\n\n";
                    errorMessage += "To fix: Either set both clips to the same BPM, or adjust their positions so they don't overlap.";
                    return true;
                }
                
                DBG("Overlap OK: Same BPM (" + juce::String(clip1.bpm, 2) + ")");
            }
        }
    }
    
    return false;
}

//==============================================================================
void TimelineManager::addSilenceToMidiFile(juce::MidiFile& /*midiFile*/, double /*silenceDuration*/, int /*trackIndex*/) const
{
    // Not needed - silence is preserved by respecting clip start times
}

//==============================================================================
void TimelineManager::beginDragOfSelectedClips(const juce::MouseEvent& e)
{
    if (dragInProgress) return;
    
    auto dragData = createDragDataForSelectedClips();
    if (dragData.isVoid()) return;
    
    dragInProgress = true;
    performExternalDrag(e, dragData);
    dragInProgress = false;
}

//==============================================================================
juce::var TimelineManager::createDragDataForSelectedClips() const
{
    // Collect all selected clips from all tracks
    std::vector<const MidiClip*> selectedClips;
    int totalTracks = container->getNumTracks();
    
    for (int trackIdx = 0; trackIdx < totalTracks; ++trackIdx)
    {
        auto trackClips = container->getTrack(trackIdx)->getSelectedClips();
        for (auto* clip : trackClips)
        {
            if (clip)
                selectedClips.push_back(clip);
        }
    }
    
    if (selectedClips.empty())
    {
        DBG("No clips selected for drag");
        return juce::var();
    }
    
    DBG("Creating drag data for " + juce::String(selectedClips.size()) + " selected clip(s)");
    
    // Create a variant array with clip info
    juce::var dragData = juce::var(juce::Array<juce::var>());
    auto* clipArray = dragData.getArray();
    
    for (auto* clip : selectedClips)
    {
        juce::var clipInfo = juce::var(new juce::DynamicObject());
        auto* obj = clipInfo.getDynamicObject();
        
        obj->setProperty("name", clip->name);
        obj->setProperty("file", clip->file.getFullPathName());
        obj->setProperty("startTime", clip->startTime);
        obj->setProperty("duration", clip->duration);
        obj->setProperty("originalBPM", clip->originalBPM);
        obj->setProperty("id", clip->id);
        
        // Find which track this clip belongs to
        for (int trackIdx = 0; trackIdx < totalTracks; ++trackIdx)
        {
            auto trackClips = container->getTrack(trackIdx)->getSelectedClips();
            for (auto* trackClip : trackClips)
            {
                if (trackClip && trackClip->id == clip->id)
                {
                    obj->setProperty("trackIndex", trackIdx);
                    obj->setProperty("trackBPM", container->getTrackBPM(trackIdx));
                    break;
                }
            }
        }
        
        clipArray->add(clipInfo);
    }
    
    return dragData;
}

//==============================================================================
void TimelineManager::performExternalDrag(const juce::MouseEvent& e, const juce::var& dragData)
{
    DBG("=== STARTING TIMELINE CLIP EXTERNAL DRAG ===");
    
    if (!dragData.isArray())
    {
        DBG("ERROR: Invalid drag data");
        return;
    }
    
    auto* clipArray = dragData.getArray();
    if (!clipArray || clipArray->isEmpty())
    {
        DBG("ERROR: No clips in drag data");
        return;
    }
    
    // Find the DragAndDropContainer (should be the PluginEditor)
    auto* editor = container->findParentComponentOfClass<juce::AudioProcessorEditor>();
    if (!editor)
    {
        DBG("ERROR: Could not find PluginEditor parent");
        return;
    }
    
    auto* dragContainer = dynamic_cast<juce::DragAndDropContainer*>(editor);
    if (!dragContainer)
    {
        DBG("ERROR: PluginEditor is not a DragAndDropContainer");
        return;
    }
    
    DBG("Found DragAndDropContainer");
    
    // Create a temporary combined MIDI file for all selected clips
    juce::String tempFileName = "DrumGroovePro_timeline_drag_" + 
        juce::String(juce::Random::getSystemRandom().nextInt64()) + ".mid";
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(tempFileName);
    
    DBG("Creating temp file: " + tempFile.getFullPathName());
    
    // Create the combined MIDI file
    juce::MidiFile combinedMidi;
    combinedMidi.setTicksPerQuarterNote(480);
    
    // Find the earliest start time to make clips relative
    double earliestStartTime = std::numeric_limits<double>::max();
    for (int i = 0; i < clipArray->size(); ++i)
    {
        auto clipInfo = (*clipArray)[i];
        if (auto* obj = clipInfo.getDynamicObject())
        {
            double startTime = obj->getProperty("startTime");
            earliestStartTime = juce::jmin(earliestStartTime, startTime);
        }
    }
    
    // Process each clip - build sequences in a mutable array
    juce::Array<juce::MidiMessageSequence> trackSequences;
    
    for (int i = 0; i < clipArray->size(); ++i)
    {
        auto clipInfo = (*clipArray)[i];
        if (auto* obj = clipInfo.getDynamicObject())
        {
            juce::String filePath = obj->getProperty("file");
            double startTime = obj->getProperty("startTime");
            double relativeStartTime = startTime - earliestStartTime;
            
            juce::File clipFile(filePath);
            if (clipFile.existsAsFile())
            {
                juce::FileInputStream fileStream(clipFile);
                if (fileStream.openedOk())
                {
                    juce::MidiFile clipMidi;
                    if (clipMidi.readFrom(fileStream))
                    {
                        int timeOffset = static_cast<int>(relativeStartTime * 480.0 * 2.0);
                        
                        for (int trackNum = 0; trackNum < clipMidi.getNumTracks(); ++trackNum)
                        {
                            const juce::MidiMessageSequence* sourceTrack = clipMidi.getTrack(trackNum);
                            if (!sourceTrack) continue;
                            
                            // Ensure we have enough tracks in our array
                            while (trackNum >= trackSequences.size())
                                trackSequences.add(juce::MidiMessageSequence());
                            
                            // Add events to our mutable sequence
                            for (int j = 0; j < sourceTrack->getNumEvents(); ++j)
                            {
                                auto* event = sourceTrack->getEventPointer(j);
                                if (event)
                                {
                                    juce::MidiMessage msg = event->message;
                                    msg.setTimeStamp(event->message.getTimeStamp() + timeOffset);
                                    trackSequences.getReference(trackNum).addEvent(msg);
                                }
                            }
                        }
                        
                        DBG("Added clip: " + clipFile.getFileName() + " at offset " + juce::String(timeOffset));
                    }
                }
            }
        }
    }
    
    // Now add all sequences to the combined MIDI file and update matched pairs
    for (int i = 0; i < trackSequences.size(); ++i)
    {
        trackSequences.getReference(i).updateMatchedPairs();
        combinedMidi.addTrack(trackSequences[i]);
    }
    
    // Write the file
    {
        juce::FileOutputStream outputStream(tempFile);
        if (!outputStream.openedOk())
        {
            DBG("ERROR: Could not open temp file for writing");
            return;
        }
        
        if (!combinedMidi.writeTo(outputStream))
        {
            DBG("ERROR: Failed to write MIDI to temp file");
            return;
        }
        
        outputStream.flush();
    }
    
    // Wait for Windows to flush the file to disk
    juce::Thread::sleep(50);
    
    // Verify the file was created and has content
    if (!tempFile.existsAsFile())
    {
        DBG("ERROR: Temp file doesn't exist after writing!");
        return;
    }
    
    juce::int64 fileSize = tempFile.getSize();
    if (fileSize == 0)
    {
        DBG("ERROR: Temp file is empty (0 bytes)!");
        return;
    }
    
    DBG("Temp file created successfully:");
    DBG("  Path: " + tempFile.getFullPathName());
    DBG("  Size: " + juce::String(fileSize) + " bytes");
    
    // Clean up previous temp file if it exists
    if (lastTempDragFile.existsAsFile())
    {
        lastTempDragFile.deleteFile();
        DBG("Cleaned up previous temp drag file");
    }
    
    // Store this temp file reference
    lastTempDragFile = tempFile;
    
    // Perform external drag WITH CALLBACK
    DBG("=== CALLING performExternalDragDropOfFiles ===");
    
    juce::StringArray files;
    files.add(tempFile.getFullPathName());
    
    // FIXED: Pass editor as source component (not 'this'), and capture tempFile for cleanup
    dragContainer->performExternalDragDropOfFiles(files, true, editor, [tempFile]()
    {
        DBG("=== DRAG COMPLETED ===");
        
        // Cleanup temp file after delay
        juce::Timer::callAfterDelay(3000, [tempFile]()
        {
            if (tempFile.existsAsFile())
            {
                tempFile.deleteFile();
                DBG("Temp file cleaned up after drag completion");
            }
        });
    });
}

//==============================================================================
// âœ… NEW: Check if folder is empty
bool TimelineManager::isFolderEmpty(const juce::File& folder) const
{
    if (!folder.exists() || !folder.isDirectory())
        return true;
    
    // Check for any files or subdirectories
    juce::Array<juce::File> contents;
    folder.findChildFiles(contents, juce::File::findFilesAndDirectories, false);
    
    return contents.isEmpty();
}

//==============================================================================
// âœ… NEW: Clear all contents of a folder
bool TimelineManager::clearFolderContents(const juce::File& folder) const
{
    if (!folder.exists() || !folder.isDirectory())
        return true;
    
    // Get all files and subdirectories
    juce::Array<juce::File> contents;
    folder.findChildFiles(contents, juce::File::findFilesAndDirectories, false);
    
    // Delete each item
    for (const auto& item : contents)
    {
        if (item.isDirectory())
        {
            if (!item.deleteRecursively())
            {
                DBG("Failed to delete directory: " + item.getFullPathName());
                return false;
            }
        }
        else
        {
            if (!item.deleteFile())
            {
                DBG("Failed to delete file: " + item.getFullPathName());
                return false;
            }
        }
    }
    
    DBG("Successfully cleared folder: " + folder.getFullPathName());
    return true;
}

//==============================================================================
// âœ… NEW: Show confirmation dialog for non-empty folder
bool TimelineManager::confirmOverwriteFolder(const juce::File& folder) const
{
    if (isFolderEmpty(folder))
        return true;  // Folder is empty, no need to confirm
    
    // Count items in folder
    juce::Array<juce::File> contents;
    folder.findChildFiles(contents, juce::File::findFilesAndDirectories, false);
    
    int fileCount = 0;
    int folderCount = 0;
    for (const auto& item : contents)
    {
        if (item.isDirectory())
            folderCount++;
        else
            fileCount++;
    }
    
    // Build warning message
    juce::String message = "The selected folder is not empty:\n\n";
    message += folder.getFullPathName() + "\n\n";
    message += "It contains:\n";
    if (fileCount > 0)
        message += "  â€¢ " + juce::String(fileCount) + " file" + (fileCount > 1 ? "s" : "") + "\n";
    if (folderCount > 0)
        message += "  â€¢ " + juce::String(folderCount) + " folder" + (folderCount > 1 ? "s" : "") + "\n";
    message += "\nAll contents will be DELETED before saving.\n\n";
    message += "Do you want to continue?";
    
    // Show confirmation dialog
    bool confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Folder Not Empty - Contents Will Be Deleted",
        message,
        "Yes, Delete and Continue",
        "No, Cancel"
    );
    
    return confirmed;
}