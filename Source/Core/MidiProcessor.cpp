#include "MidiProcessor.h"
#include "DrumLibraryManager.h"

MidiProcessor::MidiProcessor(DrumLibraryManager& drumLibManager)
    : drumLibraryManager(drumLibManager)
{
    sampleRate = 44100.0;
    samplesPerBlock = 512;
    currentBPM = 120.0;
    playheadPosition = 0.0;
    playing = false;
    loopEnabled = false;
    loopStart = 0.0;
    loopEnd = 4.0;
}

MidiProcessor::~MidiProcessor()
{
    clearAllClips();
}

void MidiProcessor::prepareToPlay(double sr, int spb)
{
    sampleRate = sr;
    samplesPerBlock = spb;
    
    DBG("MidiProcessor: Prepare to play - Sample Rate: " + juce::String(sampleRate) + 
        ", Samples per Block: " + juce::String(samplesPerBlock));
}

void MidiProcessor::releaseResources()
{
    clearAllClips();
}

void MidiProcessor::processBlock(juce::MidiBuffer& midiMessages, double bpm, DrumLibrary targetLibrary)
{
    if (!playing)
        return;

    currentBPM = bpm;

    // Calculate precise timing for this block
    double secondsPerBlock = static_cast<double>(samplesPerBlock) / sampleRate;
    double blockStartTime = playheadPosition;
    double blockEndTime = playheadPosition + secondsPerBlock;

    juce::ScopedLock sl(clipLock);

    // Process all active clips with sample-accurate timing
    for (auto& clip : activeClips)
    {
        processClipWithSampleAccuracy(*clip, midiMessages, blockStartTime, blockEndTime, bpm, targetLibrary);
    }

    // Update playhead position
    playheadPosition = blockEndTime;

    // Handle looping with sample accuracy
    if (loopEnabled && playheadPosition >= loopEnd)
    {
        // Calculate exact loop point
        double overrun = playheadPosition - loopEnd;
        playheadPosition = loopStart + overrun;

        // Reset clips for loop with precise positioning
        for (auto& clip : activeClips)
        {
            if (clip->startTime >= loopStart && clip->startTime < loopEnd)
            {
                seekClipToTime(*clip, playheadPosition);
            }
        }
    }
}

void MidiProcessor::addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib, 
                                double referenceBPM, double targetBPM, int trackNum)
{
    DBG("=== addMidiClip ===");
    DBG("File: " + file.getFullPathName());
    
    if (!file.existsAsFile())
    {
        DBG("ERROR: File doesn't exist!");
        return;
    }
    
    auto clip = std::make_unique<MidiClipPlayback>();
    clip->id = file.getFileNameWithoutExtension() + "_" + juce::String(juce::Random::getSystemRandom().nextInt());
    clip->startTime = startTime;
    clip->sourceLibrary = sourceLib;
    clip->referenceBPM = referenceBPM;
    clip->targetBPM = targetBPM;
    clip->trackNumber = trackNum;
    clip->unscaledLocalTime = 0.0;

    if (!loadMidiFileWithPrecision(file, *clip))
    {
        DBG("ERROR: Failed to load MIDI file!");
        return;  // ADD THIS CHECK
    }
    
    if (clip->sequence.getNumEvents() == 0)
    {
        DBG("ERROR: No events in sequence!");
        return;  // ADD THIS CHECK
    }

    juce::ScopedLock sl(clipLock);
    seekClipToTime(*clip, playheadPosition);
    activeClips.push_back(std::move(clip));
    
    DBG("Clip added successfully");
}

void MidiProcessor::updateTrackBPM(int trackNumber, double newBPM)
{
    juce::ScopedLock sl(clipLock);
    
    for (auto& clip : activeClips)
    {
        if (clip->trackNumber == trackNumber && clip->targetBPM != newBPM)
        {
            // CRITICAL FIX: Just update the BPM without re-seeking
            // The unscaledLocalTime maintains position, so playback continues smoothly
            clip->targetBPM = newBPM;
            
            DBG("Updated clip " + clip->id + " BPM from " + juce::String(clip->targetBPM, 2) + 
                " to " + juce::String(newBPM, 2) + " on track " + juce::String(trackNumber) +
                " (unscaled time: " + juce::String(clip->unscaledLocalTime, 6) + ")");
        }
    }
}

void MidiProcessor::updateClipBoundaries(const juce::String& clipId, double newStartTime, double newDuration)
{
    if (!playing)
        return;  // Only update during playback
        
    juce::ScopedLock sl(clipLock);
    
    for (auto& clip : activeClips)
    {
        if (clip->id == clipId)
        {
            double oldStartTime = clip->startTime;
            clip->startTime = newStartTime;
            clip->duration = newDuration;
            
            // If clip position changed significantly, re-seek to maintain sync
            if (std::abs(newStartTime - oldStartTime) > 0.001)
            {
                // Only re-seek if playhead is within the clip's range
                if (playheadPosition >= newStartTime)
                {
                    double localTime = playheadPosition - newStartTime;
                    double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
                    clip->unscaledLocalTime = localTime / visualScaleFactor;
                    
                    // Update event index to match new position
                    int eventIndex = 0;
                    while (eventIndex < clip->sequence.getNumEvents())
                    {
                        auto* eventPtr = clip->sequence.getEventPointer(eventIndex);
                        if (eventPtr && eventPtr->message.getTimeStamp() > clip->unscaledLocalTime)
                            break;
                        eventIndex++;
                    }
                    clip->currentEventIndex = eventIndex;
                }
            }
            
            DBG("Updated clip boundaries: " + clipId + 
                " | Start: " + juce::String(newStartTime, 6) + 
                " | Duration: " + juce::String(newDuration, 6));
            return;
        }
    }
}


void MidiProcessor::clearAllClips()
{
    juce::ScopedLock sl(clipLock);
    activeClips.clear();
    DBG("Cleared all MIDI clips");
}

void MidiProcessor::clearClip(const juce::String& clipId)
{
    juce::ScopedLock sl(clipLock);
    activeClips.erase(
        std::remove_if(activeClips.begin(), activeClips.end(),
                      [&clipId](const std::unique_ptr<MidiClipPlayback>& clip) {
                          return clip->id == clipId;
                      }),
        activeClips.end());
}

bool MidiProcessor::loadMidiFileWithPrecision(const juce::File& file, MidiClipPlayback& clip)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        DBG("Failed to open MIDI file: " + file.getFullPathName());
        return false;
    }

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
    {
        DBG("Failed to read MIDI file: " + file.getFullPathName());
        return false;
    }

    clip.sequence.clear();

    // Get precise tempo information
    double ticksPerQuarterNote = midiFile.getTimeFormat();
    if (ticksPerQuarterNote <= 0)
    {
        ticksPerQuarterNote = 480.0;
        DBG("Using default MIDI resolution: 480 PPQN");
    }
    
    clip.originalBPM = 120.0;
    double currentTempo = 120.0;
    
    // Convert MIDI file to sequence with precise timing
    int numTracks = midiFile.getNumTracks();
    
    // First pass: collect all events with timing
    juce::Array<juce::MidiMessageSequence::MidiEventHolder*> allEvents;
    
    for (int t = 0; t < numTracks; ++t)
    {
        const juce::MidiMessageSequence* track = midiFile.getTrack(t);
        if (track)
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                juce::MidiMessageSequence::MidiEventHolder* event = track->getEventPointer(i);
                if (event)
                {
                    allEvents.add(event);
                    
                    // Check for tempo changes
                    if (event->message.isTempoMetaEvent())
                    {
                        currentTempo = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                        clip.originalBPM = currentTempo;
                        DBG("Found tempo: " + juce::String(currentTempo, 2) + " BPM");
                    }
                }
            }
        }
    }

    // Sort events by timestamp
    std::sort(allEvents.begin(), allEvents.end(),
              [](const juce::MidiMessageSequence::MidiEventHolder* a,
                 const juce::MidiMessageSequence::MidiEventHolder* b) {
                  return a->message.getTimeStamp() < b->message.getTimeStamp();
              });

    // Convert ticks to seconds with precise timing
    for (auto* eventHolder : allEvents)
    {
        if (eventHolder->message.isNoteOn() || eventHolder->message.isNoteOff() ||
            eventHolder->message.isController() || eventHolder->message.isProgramChange())
        {
            double timeInSeconds = (eventHolder->message.getTimeStamp() / ticksPerQuarterNote) * (60.0 / clip.originalBPM);
            
            juce::MidiMessage timedEvent = eventHolder->message;
            timedEvent.setTimeStamp(timeInSeconds);
            
            if (timedEvent.isNoteOn() && timedEvent.getVelocity() > 0)
            {
                clip.sequence.addEvent(timedEvent);
            }
            else if (timedEvent.isNoteOff() || (timedEvent.isNoteOn() && timedEvent.getVelocity() == 0))
            {
                clip.sequence.addEvent(timedEvent);
            }
            else if (timedEvent.isController() || timedEvent.isProgramChange())
            {
                clip.sequence.addEvent(timedEvent);
            }
        }
    }

    // Sort by time and update matched pairs
    clip.sequence.sort();
    clip.sequence.updateMatchedPairs();

    // Calculate precise duration
    if (clip.sequence.getNumEvents() > 0)
    {
        double lastEventTime = clip.sequence.getEndTime();
        clip.duration = lastEventTime;
        
        // Add small buffer for note-off events
        clip.duration += 0.1;
    }
    else
    {
        clip.duration = 1.0;
    }

    DBG("Loaded MIDI file with " + juce::String(clip.sequence.getNumEvents()) + 
        " events, Original BPM: " + juce::String(clip.originalBPM, 2) + 
        ", Duration: " + juce::String(clip.duration, 6) + "s");

    return true;
}

void MidiProcessor::processClipWithSampleAccuracy(MidiClipPlayback& clip, juce::MidiBuffer& buffer,
                                                  double blockStartTime, double blockEndTime,
                                                  double bpm, DrumLibrary targetLib)
{
    // Calculate visual scaling for display purposes
    double visualScaleFactor = clip.referenceBPM / clip.targetBPM;
    double scaledDuration = clip.duration * visualScaleFactor;
    double clipEndTime = clip.startTime + scaledDuration;
    
    // âœ… CRITICAL FIX: Check if playhead has exceeded clip boundaries
    // This ensures clip stops when it should, even during real-time changes
    if (blockStartTime >= clipEndTime || blockEndTime <= clip.startTime)
    {
        clip.isActive = false;
        return;
    }
    
    // Additional check: if we're past the unscaled duration, stop the clip
    double unscaledDuration = clip.duration;
    if (clip.unscaledLocalTime >= unscaledDuration)
    {
        clip.isActive = false;
        return;
    }

    clip.isActive = true;

    // Calculate how much unscaled time passes this block
    double blockDuration = blockEndTime - blockStartTime;
    double unscaledBlockDuration = blockDuration / visualScaleFactor;

    // Track the unscaled time within the clip
    double localBlockStartTime = blockStartTime - clip.startTime;
    double localBlockEndTime = blockEndTime - clip.startTime;
    
    // Convert to unscaled times
    double unscaledLocalStart = localBlockStartTime / visualScaleFactor;
    double unscaledLocalEnd = localBlockEndTime / visualScaleFactor;
    
    // âœ… CRITICAL: Clamp to actual clip duration to prevent playing beyond boundaries
    unscaledLocalEnd = juce::jmin(unscaledLocalEnd, unscaledDuration);

    // Process events within this block
    while (clip.currentEventIndex < clip.sequence.getNumEvents())
    {
        juce::MidiMessageSequence::MidiEventHolder* eventHolder = 
            clip.sequence.getEventPointer(clip.currentEventIndex);
        
        if (!eventHolder)
            break;

        double originalEventTime = eventHolder->message.getTimeStamp();

        // Check if event is past the clip's duration
        if (originalEventTime >= unscaledDuration)
        {
            clip.isActive = false;
            break;
        }

        // Check if event is before this block (skip it)
        if (originalEventTime < unscaledLocalStart)
        {
            clip.currentEventIndex++;
            continue;
        }
        
        // Check if event is after this block (stop processing)
        if (originalEventTime >= unscaledLocalEnd)
        {
            break;
        }

        // Event is within this block - calculate scaled time for output
        double scaledEventTime = originalEventTime * visualScaleFactor;
        double absoluteEventTime = clip.startTime + scaledEventTime;
        
        // Calculate sample-accurate position within block
        double relativeTime = absoluteEventTime - blockStartTime;
        int sampleOffset = static_cast<int>(relativeTime * sampleRate);
        
        if (sampleOffset >= 0 && sampleOffset < samplesPerBlock)
        {
            juce::MidiMessage message = eventHolder->message;
            
            // Remap note if needed
            if (message.isNoteOnOrOff())
            {
                int originalNote = message.getNoteNumber();
                int remappedNote = drumLibraryManager.mapNoteToLibrary(originalNote, clip.sourceLibrary, targetLib);
                
                if (remappedNote != originalNote)
                {
                    if (message.isNoteOn())
                        message = juce::MidiMessage::noteOn(message.getChannel(), remappedNote, static_cast<juce::uint8>(message.getVelocity()));
                    else
                        message = juce::MidiMessage::noteOff(message.getChannel(), remappedNote, static_cast<juce::uint8>(message.getVelocity()));
                }
            }
            
            buffer.addEvent(message, sampleOffset);
        }

        clip.currentEventIndex++;
    }
    
    // Update unscaled local time for next block
    clip.unscaledLocalTime = unscaledLocalEnd;
    
    // âœ… CRITICAL: Check if clip has finished playing
    if (clip.currentEventIndex >= clip.sequence.getNumEvents() || clip.unscaledLocalTime >= unscaledDuration)
    {
        clip.isActive = false;
    }
}

void MidiProcessor::seekClipToTime(MidiClipPlayback& clip, double globalTime)
{
    double localTime = globalTime - clip.startTime;
    
    if (localTime < 0.0 || clip.sequence.getNumEvents() == 0)
    {
        clip.currentEventIndex = 0;
        clip.unscaledLocalTime = 0.0;
        return;
    }

    double visualScaleFactor = clip.referenceBPM / clip.targetBPM;
    double unscaledLocalTime = localTime / visualScaleFactor;
    clip.unscaledLocalTime = unscaledLocalTime;
    
    int low = 0;
    int high = clip.sequence.getNumEvents() - 1;
    
    while (low <= high)
    {
        int mid = (low + high) / 2;
        auto* eventPtr = clip.sequence.getEventPointer(mid);
        
        if (!eventPtr)  // ADD NULL CHECK
        {
            DBG("ERROR: Null event pointer at index " + juce::String(mid));
            break;
        }
        
        double eventTime = eventPtr->message.getTimeStamp();
        
        if (eventTime <= unscaledLocalTime)
            low = mid + 1;
        else
            high = mid - 1;
    }
    
    clip.currentEventIndex = low;
}

void MidiProcessor::play()
{
    playing = true;

    juce::ScopedLock sl(clipLock);

    // Position all clips to current playhead position
    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, playheadPosition);
    }
    
    DBG("MidiProcessor: Started playback at position " + juce::String(playheadPosition, 6));
}

void MidiProcessor::stop()
{
    playing = false;
    playheadPosition = 0.0;

    juce::ScopedLock sl(clipLock);

    // Reset all clips to beginning
    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, 0.0);
    }
    
    DBG("MidiProcessor: Stopped playback");
}

void MidiProcessor::pause()
{
    playing = false;
    DBG("MidiProcessor: Paused playback at position " + juce::String(playheadPosition, 6));
}

void MidiProcessor::setPlayheadPosition(double timeInSeconds)
{
    playheadPosition = juce::jmax(0.0, timeInSeconds);
    
    juce::ScopedLock sl(clipLock);
    
    // Update all clip positions with sample accuracy
    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, playheadPosition);
    }
    
    DBG("MidiProcessor: Set playhead position to " + juce::String(playheadPosition, 6));
}

void MidiProcessor::syncPlayheadPosition(double timeInSeconds)
{
    // Light sync without clip repositioning (for smooth timeline updates)
    playheadPosition = juce::jmax(0.0, timeInSeconds);
}