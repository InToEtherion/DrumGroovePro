#include "TimelineManager.h"
#include "MultiTrackContainer.h"
#include <fstream>

//==============================================================================
TimelineManager::TimelineManager(MultiTrackContainer* container, DrumGrooveProcessor& proc)
    : container(container), processor(proc)
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
double TimelineManager::getHeaderBPM() const
{
    // Get the header BPM (same logic as GrooveBrowser)
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    return syncToHost ? processor.getHostBPM() 
                      : processor.parameters.getRawParameterValue("manualBPM")->load();
}

//==============================================================================
DrumLibrary TimelineManager::getTargetLibrary() const
{
    // Get the current target drum library from the plugin's parameters/manager
    return processor.drumLibraryManager.getLastSelectedTargetLibrary();
}

//==============================================================================
void TimelineManager::saveTimelineState()
{
    auto targetFolder = chooseSaveLocation();
    if (targetFolder == juce::File{}) return;

    // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITICAL FIX: Check if folder is not empty and confirm deletion
    if (!confirmOverwriteFolder(targetFolder))
    {
        DBG("User cancelled save due to non-empty folder");
        return;
    }

    // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITICAL FIX: Clear folder contents if not empty
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
    // Force GUI refresh before export
    container->repaint();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    
    auto saveFile = chooseExportLocation(true);
    if (saveFile == juce::File{}) return;


    double headerBPM = getHeaderBPM();
    
    // Log what's being exported
    for (int trackIdx = 0; trackIdx < container->getNumTracks(); ++trackIdx)
    {
        double trackBPM = container->getTrackBPM(trackIdx);
        auto clips = container->getTrackClips(trackIdx);
        
        DBG("Track " + juce::String(trackIdx) + " (BPM: " + juce::String(trackBPM, 2) + 
            ") has " + juce::String(clips.size()) + " clips");
        
        for (const auto* clip : clips)
        {
            if (clip && clip->file.existsAsFile())
            {
                DBG("  Clip: " + clip->file.getFileName());
            }
        }
    }
    
    // Create the combined MIDI file (clips can now overlap freely)
    auto midiFile = createCombinedMidiFile();
    
    if (midiFile.getNumTracks() > 0)
    {
        DBG("Combined MIDI file created with " + juce::String(midiFile.getNumTracks()) + " tracks");
    }
    
    // CRITICAL FIX: Delete existing file first to ensure overwrite works
    if (saveFile.existsAsFile())
    {
        if (!saveFile.deleteFile())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Export Error", 
                "Could not overwrite existing file. Please delete it manually and try again.", 
                "OK");
            return;
        }
    }
    
    juce::FileOutputStream stream(saveFile);
    if (stream.openedOk())
    {
        midiFile.writeTo(stream);
        stream.flush();
        
        DBG("MIDI file exported successfully to: " + saveFile.getFullPathName());
        
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Export Complete", 
            "MIDI file exported successfully", 
            "OK");
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
        "Yes, trim", "No, keep silence");

    // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITICAL FIX: Log which clips are being exported for debugging
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
    
    // Also copy audio track files
    auto audioTracksTree = state.getChildWithName("AudioTracks");
    if (audioTracksTree.isValid())
    {
        // Create audio_files subfolder
        juce::File audioFolder = targetFolder.getParentDirectory().getChildFile("audio_files");
        audioFolder.createDirectory();
        
        int audioCounter = 1;
        for (auto audioTrackNode : audioTracksTree)
        {
            juce::String filePath = audioTrackNode.getProperty("file", "").toString();
            if (filePath.isEmpty()) continue;
            
            juce::File audioFile(filePath);
            if (audioFile.existsAsFile())
            {
                // Generate new filename preserving extension
                juce::String extension = audioFile.getFileExtension();
                juce::String newFileName = "audio_" + juce::String(audioCounter).paddedLeft('0', 4) + extension;
                audioCounter++;
                
                juce::File targetFile = audioFolder.getChildFile(newFileName);
                
                if (audioFile.copyFileTo(targetFile))
                {
                    // Update path to relative
                    juce::String relativePath = "audio_files/" + newFileName;
                    audioTrackNode.setProperty("file", relativePath, nullptr);
                    DBG("Copied audio file: " + audioFile.getFileName() + " -> " + newFileName);
                }
            }
        }
        DBG("Copied " + juce::String(audioCounter - 1) + " audio files");
    }
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
    
    // Process audio tracks and update file paths
    auto audioTracksTree = state.getChildWithName("AudioTracks");
    if (audioTracksTree.isValid())
    {
        for (auto audioTrackNode : audioTracksTree)
        {
            juce::String filePath = audioTrackNode.getProperty("file", "").toString();
            if (filePath.isEmpty()) continue;
            
            // Check if this is a relative path
            if (!filePath.contains(":") && !filePath.startsWith("/"))
            {
                juce::File absoluteFile = folder.getChildFile(filePath);
                
                if (absoluteFile.existsAsFile())
                {
                    audioTrackNode.setProperty("file", absoluteFile.getFullPathName(), nullptr);
                    DBG("Resolved audio relative path: " + filePath + " -> " + absoluteFile.getFullPathName());
                }
                else
                {
                    DBG("WARNING: Cannot find saved audio file: " + absoluteFile.getFullPathName());
                }
            }
        }
    }
}

//==============================================================================

juce::MidiFile TimelineManager::createMidiFileForTrack(int trackIndex, bool includeSilence) const
{
    juce::MidiFile midiFile;
    
    auto clips = container->getTrackClips(trackIndex);
    if (clips.empty())
    {
        midiFile.setTicksPerQuarterNote(960);
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
    
    // Get TPQN from the first clip to preserve original timing resolution
    int outputTPQN = 960;  // Default
    juce::MidiFile firstClipFile;
    juce::FileInputStream firstStream(sortedClips[0]->file);
    if (firstStream.openedOk() && firstClipFile.readFrom(firstStream))
    {
        outputTPQN = firstClipFile.getTimeFormat();
        if (outputTPQN <= 0) outputTPQN = 960;
    }
    
    midiFile.setTicksPerQuarterNote(outputTPQN);
    
    // Calculate offset for trimming silence if needed
    double startOffset = includeSilence ? 0.0 : sortedClips[0]->startTime;
    double trackBPM = container->getTrackBPM(trackIndex);
    double headerBPM = getHeaderBPM();
    DrumLibrary targetLibrary = getTargetLibrary();  // NEW: Get target library for remapping
    
    DBG("=== Exporting Track " + juce::String(trackIndex + 1) + " (Same Logic as Combined Export) ===");
    DBG("Track BPM: " + juce::String(trackBPM, 2));
    DBG("Header BPM: " + juce::String(headerBPM, 2));
    DBG("Target Library: " + DrumLibraryManager::getLibraryName(targetLibrary));  // NEW: Log target library
    DBG("Output TPQN: " + juce::String(outputTPQN));
    DBG("Start offset: " + juce::String(startOffset, 6));
    DBG("Include silence: " + juce::String(includeSilence ? "YES" : "NO"));
    if (!sortedClips.empty())
    {
        DBG("First clip starts at: " + juce::String(sortedClips[0]->startTime, 6) + "s");
        if (sortedClips.size() > 1)
        {
            DBG("Last clip starts at: " + juce::String(sortedClips.back()->startTime, 6) + "s");
        }
    }
    
    // Create the MIDI sequence
    juce::MidiMessageSequence trackSequence;
    
    // Set tempo at tick 0 using HEADER BPM (reference BPM for the MIDI file)
    int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / headerBPM);
    auto tempoMsg = juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote);
    trackSequence.addEvent(tempoMsg, 0.0);
    
    // Add time signature (4/4) at tick 0
    auto timeSigMsg = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
    trackSequence.addEvent(timeSigMsg, 0.0);
    
    DBG("Added tempo: " + juce::String(headerBPM, 2) + " BPM (header BPM as reference)");
    
    // Process each clip using EXACT same logic as createCombinedMidiFile
    for (const auto* clip : sortedClips)
    {
        if (!clip->file.existsAsFile())
            continue;
        
        juce::MidiFile clipMidiFile;
        juce::FileInputStream stream(clip->file);
        if (!stream.openedOk() || !clipMidiFile.readFrom(stream))
        {
            DBG("ERROR: Could not read clip: " + clip->name);
            continue;
        }
        
        // Use the stored originalBPM from the clip
        double clipOriginalBPM = clip->originalBPM;
        
        // Get input TPQN
        double ticksPerQuarterNote = clipMidiFile.getTimeFormat();
        if (ticksPerQuarterNote <= 0) ticksPerQuarterNote = 960.0;
        
        // Calculate actual MIDI file duration to properly filter events
        double maxEventTimeInTicks = 0.0;
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (!event) continue;
                
                // Skip meta events when finding max timestamp
                if (!event->message.isMetaEvent() || event->message.isEndOfTrackMetaEvent())
                {
                    maxEventTimeInTicks = juce::jmax(maxEventTimeInTicks, event->message.getTimeStamp());
                }
            }
        }
        
        double actualMidiDurationSeconds = (maxEventTimeInTicks / ticksPerQuarterNote) * (60.0 / clipOriginalBPM);
        
        DBG("Processing clip: " + clip->name);
        DBG("  === CLIP PROPERTIES ===");
        DBG("  Stored clip->originalBPM: " + juce::String(clipOriginalBPM, 6));
        DBG("  Track BPM: " + juce::String(trackBPM, 2));
        DBG("  Header BPM (export): " + juce::String(headerBPM, 2));
        DBG("  === TIMING INFO ===");
        DBG("  Timeline position (clip->startTime): " + juce::String(clip->startTime, 6) + "s");
        DBG("  Stored clip duration: " + juce::String(clip->duration, 6) + "s");
        DBG("  === MIDI FILE ANALYSIS ===");
        DBG("  MIDI ticks per quarter note: " + juce::String(ticksPerQuarterNote, 2));
        DBG("  Max event timestamp in file: " + juce::String(maxEventTimeInTicks, 2) + " ticks");
        DBG("  Calculated MIDI duration (at originalBPM): " + juce::String(actualMidiDurationSeconds, 6) + "s");
        
        // CRITICAL: Use same tempo scaling as createCombinedMidiFile
        double tempoScale = clipOriginalBPM / trackBPM;
        
        // Calculate clip start position - apply offset for trimming
        double clipStartInExport = clip->startTime - startOffset;
        double clipStartTicks = clipStartInExport * (headerBPM / 60.0) * outputTPQN;
        
        DBG("  === TEMPO SCALING INFO ===");
        DBG("    Tempo scale factor: " + juce::String(tempoScale, 6) + " (originalBPM / trackBPM)");
        DBG("    Clip start in export: " + juce::String(clipStartInExport, 6) + "s");
        DBG("    Clip start in ticks: " + juce::String(clipStartTicks, 2));
        
        // TPQN conversion factor
        double tpqnConversionFactor = static_cast<double>(outputTPQN) / ticksPerQuarterNote;
        DBG("    TPQN conversion factor: " + juce::String(tpqnConversionFactor, 6));
        
        int eventCount = 0;
        bool firstEventLogged = false;
        double lastEventTicks = 0.0;
        double lastEventExportTicks = 0.0;
        
        // Process all MIDI events from all tracks in the clip
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (!event) continue;
                
                auto message = event->message;
                
                // CRITICAL: Apply drum library remapping for note events
                if (message.isNoteOnOrOff() && clip->sourceLibrary != DrumLibrary::Unknown)
                {
                    DrumLibrary targetLib = getTargetLibrary();
                    
                    // Only remap if target library is not Bypass and source != target
                    if (targetLib != DrumLibrary::Bypass && clip->sourceLibrary != targetLib)
                    {
                        uint8_t originalNote = static_cast<uint8_t>(message.getNoteNumber());
                        uint8_t remappedNote = processor.drumLibraryManager.mapNoteToLibrary(
                            originalNote, 
                            clip->sourceLibrary, 
                            targetLib
                        );
                        
                        // Create new message with remapped note
                        if (message.isNoteOn())
                        {
                            message = juce::MidiMessage::noteOn(
                                message.getChannel(),
                                remappedNote,
                                static_cast<juce::uint8>(message.getVelocity())
                            );
                        }
                        else // Note off
                        {
                            message = juce::MidiMessage::noteOff(
                                message.getChannel(),
                                remappedNote,
                                static_cast<juce::uint8>(message.getVelocity())
                            );
                        }
                        
                        // Restore original timestamp (will be adjusted below)
                        message.setTimeStamp(event->message.getTimeStamp());
                    }
                }
                
                // Skip tempo and time signature events (we set our own)
                if (message.isTempoMetaEvent() || message.isTimeSignatureMetaEvent())
                    continue;
                
                // Skip other meta events (except end of track)
                if (message.isMetaEvent() && !message.isEndOfTrackMetaEvent())
                    continue;
                
                double originalTicks = message.getTimeStamp();
                
                // Filter events beyond the actual MIDI duration (in original tick space)
                if (originalTicks > maxEventTimeInTicks + 100.0)  // +100 tick tolerance
                    continue;
                
                // EXACT same conversion as createCombinedMidiFile:
                // Step 1: Convert TPQN if needed
                double convertedTicks = originalTicks * tpqnConversionFactor;
                
                // Step 2: Scale for BPM (same as GrooveBrowser)
                double scaledTicks = convertedTicks * tempoScale;
                
                // Step 3: Add clip start offset
                double finalTicks = clipStartTicks + scaledTicks;
                
                // Skip negative times (shouldn't happen with proper startOffset)
                if (finalTicks < 0.0)
                {
                    DBG("WARNING: Skipping event with negative time: " + juce::String(finalTicks));
                    continue;
                }
                
                // Debug the first note to verify calculation
                if (!firstEventLogged && message.isNoteOn())
                {
                    DBG("  === FIRST NOTE CALCULATION ===");
                    DBG("    Original ticks: " + juce::String(originalTicks, 2));
                    DBG("    TPQN converted: " + juce::String(convertedTicks, 2));
                    DBG("    BPM scaled: " + juce::String(scaledTicks, 2));
                    DBG("    Clip start ticks: " + juce::String(clipStartTicks, 2));
                    DBG("    Final ticks: " + juce::String(finalTicks, 2));
                    DBG("    Verification: " + juce::String((finalTicks / outputTPQN) * (60.0 / headerBPM), 4) + "s");
                    firstEventLogged = true;
                }
                
                // Track last event
                if (message.isNoteOn() || message.isNoteOff())
                {
                    lastEventTicks = originalTicks;
                    lastEventExportTicks = finalTicks;
                }
                
                // Create the event with scaled timing (CRITICAL: set timestamp on message itself!)
                juce::MidiMessage exportMessage = message;
                exportMessage.setTimeStamp(finalTicks);
                trackSequence.addEvent(exportMessage);
                
                eventCount++;
            }
        }
        
        DBG("  === CLIP EXPORT SUMMARY ===");
        DBG("    Last event original ticks: " + juce::String(lastEventTicks, 2));
        DBG("    Last event export ticks: " + juce::String(lastEventExportTicks, 2));
        DBG("    Last event export time: " + juce::String((lastEventExportTicks / outputTPQN) * (60.0 / headerBPM), 4) + "s");
        DBG("    Expected timeline end: " + juce::String((clip->startTime - startOffset) + (actualMidiDurationSeconds * tempoScale), 4) + "s");
        DBG("  Added " + juce::String(eventCount) + " events from this clip");
    }
    
    // Sort the sequence and update matched pairs (same as combined export)
    trackSequence.sort();
    trackSequence.updateMatchedPairs();
    
    // Add the track to the MIDI file
    midiFile.addTrack(trackSequence);
    
    DBG("=== Track Export Complete ===");
    DBG("Total events: " + juce::String(trackSequence.getNumEvents()));
    
    return midiFile;
}

//==============================================================================
// FIXED: Correct BPM time conversion using clipOriginalBPM
juce::MidiFile TimelineManager::createCombinedMidiFile() const
{
    
    juce::MidiFile midiFile;
    // Don't set TPQN yet - we'll use the first clip's TPQN
    
    double headerBPM = getHeaderBPM();
    DrumLibrary targetLibrary = getTargetLibrary();  // NEW: Get target library for remapping
    
    
    // Collect all clips from all tracks
    struct ClipInfo {
        const MidiClip* clip;
        double trackBPM;
        int trackIndex;
    };
    std::vector<ClipInfo> allClips;
    
    for (int trackIdx = 0; trackIdx < container->getNumTracks(); ++trackIdx)
    {
        double trackBPM = container->getTrackBPM(trackIdx);
        auto clips = container->getTrackClips(trackIdx);
        
        for (const auto* clip : clips)
        {
            if (!clip || !clip->file.existsAsFile())
                continue;
            
            ClipInfo info;
            info.clip = clip;
            info.trackBPM = trackBPM;
            info.trackIndex = trackIdx;
            allClips.push_back(info);
            
               
        }
    }
    
    if (allClips.empty())
    {
        midiFile.setTicksPerQuarterNote(960);  // Default TPQN for empty file
        juce::MidiMessageSequence emptySequence;
        // Add tempo event even for empty file
        int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / headerBPM);
        emptySequence.addEvent(juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote), 0.0);
        emptySequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
        midiFile.addTrack(emptySequence);
        return midiFile;
    }
    
    // Sort clips by start time
    std::sort(allClips.begin(), allClips.end(),
        [](const ClipInfo& a, const ClipInfo& b) {
            return a.clip->startTime < b.clip->startTime;
        });
    
    // Get TPQN from the first clip to preserve original timing resolution
    int outputTPQN = 960;  // Default
    if (!allClips.empty())
    {
        juce::MidiFile firstClipFile;
        juce::FileInputStream firstStream(allClips[0].clip->file);
        if (firstStream.openedOk() && firstClipFile.readFrom(firstStream))
        {
            outputTPQN = firstClipFile.getTimeFormat();
            if (outputTPQN <= 0) outputTPQN = 960;
        }
    }
    
    midiFile.setTicksPerQuarterNote(outputTPQN);
    
    // Create a single sequence to hold all events
    juce::MidiMessageSequence combinedSequence;
    
    // CRITICAL: Always use header BPM as the export tempo (reference BPM for the MIDI file)
    // Each track's clips will be scaled by their individual track BPMs, but the final
    // MIDI file tempo is always the header BPM
    int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / headerBPM);
    combinedSequence.addEvent(juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote), 0.0);
    
    // Add time signature at tick 0
    combinedSequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
    
    
    // Process each clip: convert using clip's original BPM, then apply time-stretch, then to header BPM
    for (const auto& clipInfo : allClips)
    {
        const auto* clip = clipInfo.clip;
        double trackBPM = clipInfo.trackBPM;
        
        // Load the MIDI file
        juce::MidiFile clipMidiFile;
        juce::FileInputStream stream(clip->file);
        if (!stream.openedOk() || !clipMidiFile.readFrom(stream))
        {
            continue;
        }
        
        // FIXED: Use the stored originalBPM from the clip, not from the MIDI file
        // This ensures consistency with how the clip was added to the timeline
        double clipOriginalBPM = clip->originalBPM;
        
        // DEBUGGING: Also read BPM from file to compare
        double bpmFromFile = 120.0;
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (event && event->message.isTempoMetaEvent())
                {
                    bpmFromFile = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
            if (bpmFromFile != 120.0) break;
        }
        
        if (std::abs(clipOriginalBPM - bpmFromFile) > 0.01)
        {
        }
        
        // CRITICAL FIX: Calculate actual MIDI file duration to properly filter events
        // Find the last event timestamp in the MIDI file
        double maxEventTimeInTicks = 0.0;
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (!event) continue;
                
                // Skip meta events when finding max timestamp
                if (!event->message.isMetaEvent() || event->message.isEndOfTrackMetaEvent())
                {
                    maxEventTimeInTicks = juce::jmax(maxEventTimeInTicks, event->message.getTimeStamp());
                }
            }
        }
        
        // Convert max timestamp to seconds at original BPM
        double ticksPerQuarterNote = clipMidiFile.getTimeFormat();
        if (ticksPerQuarterNote <= 0) ticksPerQuarterNote = 960.0;
        double actualMidiDurationSeconds = (maxEventTimeInTicks / ticksPerQuarterNote) * (60.0 / clipOriginalBPM);
        
            
        
        int eventCount = 0;
        bool firstEventLogged = false;
        double lastEventTicks = 0.0;
        double lastEventExportTicks = 0.0;
        
        // CRITICAL FIX: Use GrooveBrowser's approach - scale ticks directly!
        // When trackBPM > originalBPM: clip plays FASTER, so we need FEWER ticks
        // When trackBPM < originalBPM: clip plays SLOWER, so we need MORE ticks
        double tempoScale = clipOriginalBPM / trackBPM;  // e.g., 120/400 = 0.3 (fewer ticks = faster)
        
        // Calculate clip start time in ticks at EXPORT BPM using the OUTPUT TPQN
        double clipStartTicks = clip->startTime * (headerBPM / 60.0) * outputTPQN;
        
        
        // If input and output TPQN differ, we need to convert
        double tpqnConversionFactor = static_cast<double>(outputTPQN) / ticksPerQuarterNote;
        
        // Process all MIDI events from all tracks in the clip
        for (int t = 0; t < clipMidiFile.getNumTracks(); ++t)
        {
            const auto* track = clipMidiFile.getTrack(t);
            if (!track) continue;
            
            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto* event = track->getEventPointer(e);
                if (!event) continue;
                
                auto message = event->message;
                
                // CRITICAL: Apply drum library remapping for note events
                if (message.isNoteOnOrOff() && clip->sourceLibrary != DrumLibrary::Unknown)
                {
                    DrumLibrary targetLib = getTargetLibrary();
                    
                    // Only remap if target library is not Bypass and source != target
                    if (targetLib != DrumLibrary::Bypass && clip->sourceLibrary != targetLib)
                    {
                        uint8_t originalNote = static_cast<uint8_t>(message.getNoteNumber());
                        uint8_t remappedNote = processor.drumLibraryManager.mapNoteToLibrary(
                            originalNote, 
                            clip->sourceLibrary, 
                            targetLib
                        );
                        
                        // Log remapping for first note
                        if (!firstEventLogged)
                        {
                        }
                        
                        // Create new message with remapped note
                        if (message.isNoteOn())
                        {
                            message = juce::MidiMessage::noteOn(
                                message.getChannel(),
                                remappedNote,
                                static_cast<juce::uint8>(message.getVelocity())
                            );
                        }
                        else // Note off
                        {
                            message = juce::MidiMessage::noteOff(
                                message.getChannel(),
                                remappedNote,
                                static_cast<juce::uint8>(message.getVelocity())
                            );
                        }
                        
                        // Restore original timestamp (will be adjusted below)
                        message.setTimeStamp(event->message.getTimeStamp());
                    }
                }
                
                // Skip tempo and time signature events (we set our own)
                if (message.isTempoMetaEvent() || message.isTimeSignatureMetaEvent())
                    continue;
                
                // Skip other meta events (except end of track)
                if (message.isMetaEvent() && !message.isEndOfTrackMetaEvent())
                    continue;
                
                double originalTicks = message.getTimeStamp();
                
                // Filter events beyond the actual MIDI duration (in original tick space)
                if (originalTicks > maxEventTimeInTicks + 100.0)  // +100 tick tolerance
                    continue;
                
                // FIXED: Apply BOTH tempo scaling AND TPQN conversion!
                // Step 1: Convert TPQN if needed
                double convertedTicks = originalTicks * tpqnConversionFactor;
                
                // Step 2: Scale for BPM like GrooveBrowser does
                double scaledTicks = convertedTicks * tempoScale;
                
                // Step 3: Add clip start offset
                double finalTicks = clipStartTicks + scaledTicks;
                
                // Debug the first note to verify calculation
                if (!firstEventLogged && message.isNoteOn())
                {
                       
                    firstEventLogged = true;
                }
                
                // Track last event
                if (message.isNoteOn() || message.isNoteOff())
                {
                    lastEventTicks = originalTicks;
                    lastEventExportTicks = finalTicks;
                }
                
                // Create the event with scaled timing
                juce::MidiMessage exportMessage = message;
                exportMessage.setTimeStamp(finalTicks); 
                combinedSequence.addEvent(exportMessage);
                
                eventCount++;
            }
        }
        
        
    }
    
    // Sort the sequence and update matched pairs
    combinedSequence.sort();
    combinedSequence.updateMatchedPairs();
    
    // Add the combined track to the MIDI file
    midiFile.addTrack(combinedSequence);
    
    
    return midiFile;
}

bool TimelineManager::checkForOverlapsWithDifferentBPM(const std::vector<ClipBoundary>& boundaries, juce::String& errorMessage) const
{
    // Overlaps are now allowed - clips are converted to header BPM
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
        obj->setProperty("sourceLibrary", static_cast<int>(clip->sourceLibrary));  // NEW: Add source library
        
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
    
    // Get target library for remapping
    DrumLibrary targetLib = getTargetLibrary();
    DBG("Target library for drag: " + juce::String(static_cast<int>(targetLib)));
    
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
            
            // NEW: Get source library for this clip
            DrumLibrary sourceLib = static_cast<DrumLibrary>(static_cast<int>(obj->getProperty("sourceLibrary")));
            
            // Determine if note remapping is needed for this clip
            bool needsRemapping = (targetLib != DrumLibrary::Bypass && 
                                   sourceLib != DrumLibrary::Unknown && 
                                   sourceLib != targetLib);
            
            if (needsRemapping)
            {
                DBG("Clip remapping needed: Source " + juce::String(static_cast<int>(sourceLib)) + 
                    " -> Target " + juce::String(static_cast<int>(targetLib)));
            }
            
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
                                    
                                    // NEW: Apply note remapping if needed
                                    if (needsRemapping && msg.isNoteOnOrOff())
                                    {
                                        uint8_t originalNote = static_cast<uint8_t>(msg.getNoteNumber());
                                        uint8_t remappedNote = processor.drumLibraryManager.mapNoteToLibrary(
                                            originalNote, 
                                            sourceLib, 
                                            targetLib);
                                        
                                        if (remappedNote != originalNote)
                                        {
                                            msg.setNoteNumber(remappedNote);
                                            DBG("Remapped note: " + juce::String(originalNote) + " -> " + juce::String(remappedNote));
                                        }
                                    }
                                    
                                    trackSequences.getReference(trackNum).addEvent(msg);
                                }
                            }
                        }
                        
                        
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
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NEW: Check if folder is empty
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
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NEW: Clear all contents of a folder
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
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NEW: Show confirmation dialog for non-empty folder
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
        message += "  - " + juce::String(fileCount) + " file" + (fileCount > 1 ? "s" : "") + "\n";
    if (folderCount > 0)
        message += "  - " + juce::String(folderCount) + " folder" + (folderCount > 1 ? "s" : "") + "\n";
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