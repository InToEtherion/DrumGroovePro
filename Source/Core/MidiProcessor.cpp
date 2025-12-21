#include "MidiProcessor.h"
#include "DrumLibraryManager.h"
#include <cmath>  // For std::round and std::lround

MidiProcessor::MidiProcessor(DrumLibraryManager& drumLibManager)
: drumLibraryManager(drumLibManager)
{
    sampleRate = 44100.0;
    samplesPerBlock = 512;
    currentBPM = 120.0;

    // Atomics are initialized in header with default values
    // playheadPosition.store(0.0);  // Already initialized to 0.0 in header
    // playing.store(false);         // Already initialized to false in header

    loopEnabled = false;
    loopStart = 0.0;
    loopEnd = 4.0;
    playbackSpeed = 1.0;
}

MidiProcessor::~MidiProcessor()
{
    clearAllClips();
}

void MidiProcessor::prepareToPlay(double sr, int spb)
{
    sampleRate = sr;
    samplesPerBlock = spb;
}

void MidiProcessor::releaseResources()
{
    clearAllClips();
}

void MidiProcessor::processBlock(juce::MidiBuffer& midiMessages, double bpm, DrumLibrary targetLibrary)
{
    // THREAD-SAFE: Read atomic playing flag once at start
    if (!playing.load())
    {
        // Add preview notes even when not playing
        const juce::ScopedLock sl(previewLock);
        if (!previewNoteBuffer.isEmpty())
        {
            midiMessages.addEvents(previewNoteBuffer, 0, -1, 0);
            previewNoteBuffer.clear();
        }
        return;
    }

    currentBPM = bpm;

    double secondsPerBlock = static_cast<double>(samplesPerBlock) / sampleRate;
    double adjustedSecondsPerBlock = secondsPerBlock * playbackSpeed;

    // THREAD-SAFE: Read playhead position once at the start of the block
    double blockStartTime = playheadPosition.load();
    double blockEndTime = blockStartTime + adjustedSecondsPerBlock;

    // ============================================================================
    // CRITICAL FIX: Handle loop restart BEFORE processing to avoid skipping boundary notes
    // ============================================================================
    bool loopRestarted = false;
    double overrunToProcess = 0.0;

    if (loopEnabled && blockEndTime >= loopEnd)
    {
        // Calculate how much we overshoot the loop end
        overrunToProcess = blockEndTime - loopEnd;

        // First, process up to the loop end point
        blockEndTime = loopEnd;
        loopRestarted = true;

        DBG("MidiProcessor: Loop boundary detected - processing up to loopEnd=" +
        juce::String(loopEnd, 3) + "s, overrun=" + juce::String(overrunToProcess, 3) + "s");
    }

    // ============================================================================
    // CRITICAL SECTION: Lock only while accessing activeClips vector
    // File I/O and heavy operations are done OUTSIDE this lock
    // Atomic operations don't need this lock
    // ============================================================================
    {
        juce::ScopedLock sl(clipLock);

        // Process the current block (up to loop end if looping)
        for (auto& clip : activeClips)
        {
            processClipWithSampleAccuracy(*clip, midiMessages, blockStartTime, blockEndTime, bpm, targetLibrary);
        }

        // Handle loop restart if needed
        if (loopRestarted)
        {
            DBG("MidiProcessor: Loop restarting - seeking all clips to loopStart=" +
            juce::String(loopStart, 3) + "s");

            // Reactivate and seek all clips to loop start
            for (auto& clip : activeClips)
            {
                double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
                double scaledDuration = clip->duration * visualScaleFactor;
                double clipEndTime = clip->startTime + scaledDuration;

                // Check if clip should be active at loop start
                if (loopStart >= clip->startTime && loopStart < clipEndTime)
                {
                    // CRITICAL: Seek to exact loop boundary (0 tolerance)
                    seekClipToTime(*clip, loopStart);
                    clip->isActive = true;

                    DBG("MidiProcessor: Clip " + clip->id + " reactivated at loop start, eventIndex=" +
                    juce::String(clip->currentEventIndex) + ", unscaledTime=" +
                    juce::String(clip->unscaledLocalTime, 3) + "s");
                }
                else
                {
                    clip->isActive = false;
                }
            }

            // Process the overrun from loop start
            if (overrunToProcess > 0.0)
            {
                double overrunStartTime = loopStart;
                double overrunEndTime = loopStart + overrunToProcess;

                DBG("MidiProcessor: Processing overrun from " + juce::String(overrunStartTime, 3) +
                "s to " + juce::String(overrunEndTime, 3) + "s");

                for (auto& clip : activeClips)
                {
                    processClipWithSampleAccuracy(*clip, midiMessages, overrunStartTime, overrunEndTime, bpm, targetLibrary);
                }
            }
        }
    } // Lock released here - atomic operations below don't need it

    // OPTIMIZATION: Atomic operations done OUTSIDE critical section
    // These don't need the clipLock since they're already thread-safe
    playheadPosition.store(blockEndTime);

    if (loopRestarted)
    {
        // Update playhead position after loop restart
        double finalPosition = (overrunToProcess > 0.0) ? (loopStart + overrunToProcess) : loopStart;
        playheadPosition.store(finalPosition);
    }

    // Add preview notes
    {
        const juce::ScopedLock sl(previewLock);
        if (!previewNoteBuffer.isEmpty())
        {
            midiMessages.addEvents(previewNoteBuffer, 0, -1, 0);
            previewNoteBuffer.clear();
        }
    }
}

void MidiProcessor::addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib,
                                double referenceBPM, double targetBPM, int trackNum, const juce::String& clipId)
{
    if (!file.existsAsFile())
        return;

    // ============================================================================
    // OPTIMIZATION: File I/O and MIDI parsing done OUTSIDE critical section
    // This prevents blocking the audio thread during disk operations
    // ============================================================================
    auto clip = std::make_unique<MidiClipPlayback>();
    clip->id = clipId;
    clip->startTime = startTime;
    clip->sourceLibrary = sourceLib;
    clip->referenceBPM = referenceBPM;
    clip->targetBPM = targetBPM;
    clip->trackNumber = trackNum;
    clip->unscaledLocalTime = 0.0;

    // Load and parse MIDI file - potentially slow, done OUTSIDE lock
    if (!loadMidiFileWithPrecision(file, *clip))
        return;

    if (clip->sequence.getNumEvents() == 0)
        return;

    // ============================================================================
    // CRITICAL SECTION: Lock only when adding to activeClips vector
    // This is the minimal scope needed - fast operation, won't block audio thread
    // ============================================================================
    juce::ScopedLock sl(clipLock);

    // THREAD-SAFE: Read playhead position atomically
    double currentPosition = playheadPosition.load();
    seekClipToTime(*clip, currentPosition);
    activeClips.push_back(std::move(clip));

    DBG("MidiProcessor: Added clip with ID: " + clipId);
}

void MidiProcessor::addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib,
                                double referenceBPM, double targetBPM, int trackNum, double explicitDuration, const juce::String& clipId)
{
    DBG("=== addMidiClip called ===");
    DBG("File: " + file.getFullPathName());
    DBG("File exists: " + juce::String(file.existsAsFile() ? "YES" : "NO"));
    DBG("File size: " + juce::String(file.getSize()) + " bytes");

    if (!file.existsAsFile())
    {
        DBG("ERROR: File does not exist!");
        return;
    }

    // ============================================================================
    // OPTIMIZATION: File I/O and MIDI parsing done OUTSIDE critical section
    // This prevents blocking the audio thread during disk operations
    // ============================================================================
    auto clip = std::make_unique<MidiClipPlayback>();
    clip->id = clipId;
    clip->startTime = startTime;
    clip->sourceLibrary = sourceLib;
    clip->referenceBPM = referenceBPM;
    clip->targetBPM = targetBPM;
    clip->trackNumber = trackNum;
    clip->unscaledLocalTime = 0.0;

    DBG("Calling loadMidiFileWithPrecision...");

    // Load and parse MIDI file - potentially slow, done OUTSIDE lock
    if (!loadMidiFileWithPrecision(file, *clip))
    {
        DBG("ERROR: loadMidiFileWithPrecision FAILED!");
        return;
    }

    DBG("loadMidiFileWithPrecision SUCCESS - Events: " + juce::String(clip->sequence.getNumEvents()));

    if (clip->sequence.getNumEvents() == 0)
    {
        DBG("ERROR: Clip has NO events after loading!");
        return;
    }

    clip->duration = explicitDuration;

    DBG("MidiProcessor: Clip loaded successfully - " + juce::String(clip->sequence.getNumEvents()) + " events");
    DBG("MidiProcessor: Added clip with explicit duration: " + juce::String(explicitDuration, 3) +
    "s, referenceBPM: " + juce::String(referenceBPM, 2) + ", targetBPM: " + juce::String(targetBPM, 2) +
    ", ID: " + clipId);

    // ============================================================================
    // CRITICAL SECTION: Lock only when adding to activeClips vector
    // This is the minimal scope needed - fast operation, won't block audio thread
    // ============================================================================
    juce::ScopedLock sl(clipLock);

    // THREAD-SAFE: Read playhead position atomically
    double currentPosition = playheadPosition.load();
    seekClipToTime(*clip, currentPosition);
    activeClips.push_back(std::move(clip));

    DBG("Clip added to activeClips vector. Total clips: " + juce::String(activeClips.size()));
}



void MidiProcessor::updateTrackBPM(int trackNumber, double newBPM)
{
    juce::ScopedLock sl(clipLock);

    // THREAD-SAFE: Read playhead position atomically once
    double currentPosition = playheadPosition.load();

    for (auto& clip : activeClips)
    {
        if (clip->trackNumber == trackNumber && clip->targetBPM != newBPM)
        {
            clip->targetBPM = newBPM;

            double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
            double scaledDuration = clip->duration * visualScaleFactor;
            double clipEndTime = clip->startTime + scaledDuration;

            if (currentPosition >= clip->startTime && currentPosition < clipEndTime)
            {
                clip->isActive = true;
            }
            else
            {
                clip->isActive = false;
            }
        }
    }
}

void MidiProcessor::updateClipBoundaries(const juce::String& clipId, double newStartTime, double newDuration)
{
    juce::ScopedLock sl(clipLock);

    // THREAD-SAFE: Read playhead position atomically once
    double currentPosition = playheadPosition.load();

    for (auto& clip : activeClips)
    {
        if (clip->id == clipId)
        {
            clip->startTime = newStartTime;
            clip->duration = newDuration;

            double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
            double scaledDuration = newDuration * visualScaleFactor;
            double clipEndTime = newStartTime + scaledDuration;

            if (currentPosition >= newStartTime && currentPosition < clipEndTime)
            {
                seekClipToTime(*clip, currentPosition);
                clip->isActive = true;
            }
            else if (currentPosition < newStartTime)
            {
                seekClipToTime(*clip, newStartTime);
                clip->isActive = false;
            }
            else
            {
                clip->isActive = false;
            }

            break;
        }
    }
}

void MidiProcessor::clearAllClips()
{
    juce::ScopedLock sl(clipLock);
    activeClips.clear();
}

void MidiProcessor::clearClip(const juce::String& clipId)
{
    juce::ScopedLock sl(clipLock);

    activeClips.erase(
        std::remove_if(activeClips.begin(), activeClips.end(),
                       [&clipId](const std::unique_ptr<MidiClipPlayback>& clip) {
                           return clip->id == clipId;
                       }),
                      activeClips.end()
    );
}

bool MidiProcessor::loadMidiFileWithPrecision(const juce::File& file, MidiClipPlayback& clip)
{
    DBG("loadMidiFileWithPrecision: Starting - File: " + file.getFullPathName());
    DBG("  File size: " + juce::String(file.getSize()) + " bytes");

    juce::FileInputStream fileStream(file);
    if (!fileStream.openedOk())
    {
        DBG("ERROR: FileInputStream failed to open!");
        return false;
    }

    DBG("  FileInputStream opened OK");

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(fileStream))
    {
        DBG("ERROR: MidiFile.readFrom() failed!");
        return false;
    }

    DBG("  MidiFile.readFrom() SUCCESS");

    double ticksPerQuarterNote = midiFile.getTimeFormat();
    DBG("  TPQN: " + juce::String(ticksPerQuarterNote));

    if (ticksPerQuarterNote <= 0)
        ticksPerQuarterNote = 480.0;

    clip.originalBPM = 120.0;
    double currentTempo = 120.0;

    int numTracks = midiFile.getNumTracks();
    DBG("  Number of tracks: " + juce::String(numTracks));

    juce::Array<juce::MidiMessageSequence::MidiEventHolder*> allEvents;

    for (int t = 0; t < numTracks; ++t)
    {
        const juce::MidiMessageSequence* track = midiFile.getTrack(t);
        if (track)
        {
            DBG("  Track " + juce::String(t) + " has " + juce::String(track->getNumEvents()) + " events");

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                juce::MidiMessageSequence::MidiEventHolder* event = track->getEventPointer(i);
                if (event->message.isTempoMetaEvent())
                {
                    currentTempo = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                    clip.originalBPM = currentTempo;
                }
                allEvents.add(event);
            }
        }
    }

    DBG("  Total events collected: " + juce::String(allEvents.size()));

    // Sort events by timestamp using JUCE-compatible comparator
    struct EventComparator
    {
        static int compareElements(juce::MidiMessageSequence::MidiEventHolder* first,
                                   juce::MidiMessageSequence::MidiEventHolder* second)
        {
            if (first->message.getTimeStamp() < second->message.getTimeStamp())
                return -1;
            if (first->message.getTimeStamp() > second->message.getTimeStamp())
                return 1;
            return 0;
        }
    };

    EventComparator comparator;
    allEvents.sort(comparator);

    int noteCount = 0;
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
                noteCount++;
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

    DBG("  Note-on events added: " + juce::String(noteCount));

    clip.sequence.sort();
    clip.sequence.updateMatchedPairs();

    DBG("  Final sequence events: " + juce::String(clip.sequence.getNumEvents()));

    if (clip.sequence.getNumEvents() > 0)
    {
        double lastEventTime = clip.sequence.getEndTime();
        clip.duration = lastEventTime + 0.1;
        DBG("  Duration: " + juce::String(clip.duration, 3) + "s");
    }
    else
    {
        clip.duration = 1.0;
        DBG("  WARNING: No events, using default duration");
    }

    DBG("loadMidiFileWithPrecision: SUCCESS");
    return true;
}

void MidiProcessor::processClipWithSampleAccuracy(MidiClipPlayback& clip, juce::MidiBuffer& buffer,
                                                  double blockStartTime, double blockEndTime,
                                                  double bpm, DrumLibrary targetLib)
{
    double visualScaleFactor = clip.referenceBPM / clip.targetBPM;
    double scaledDuration = clip.duration * visualScaleFactor;
    double clipEndTime = clip.startTime + scaledDuration;

    constexpr double EPSILON = 0.001; // 1ms tolerance

    // If playhead is completely before or after the clip, deactivate
    if (blockStartTime >= (clipEndTime + EPSILON) || blockEndTime <= (clip.startTime - EPSILON))
    {
        clip.isActive = false;
        return;
    }

    double unscaledDuration = clip.duration;

    // ============================================================================
    // CRITICAL FOR GROOVEBROWSER LOOPING:
    // Check if this clip should be looping (clip range overlaps with loop range)
    // ============================================================================
    bool clipShouldLoop = false;
    if (loopEnabled)
    {
        // Check if clip overlaps with loop range
        // Clip should loop if: clip.startTime is within loop range OR clip extends into loop range
        bool clipOverlapsLoop = (clip.startTime >= loopStart && clip.startTime < loopEnd) ||
        (clipEndTime > loopStart && clip.startTime <= loopStart);

        if (clipOverlapsLoop)
        {
            clipShouldLoop = true;
        }
    }

    // ============================================================================
    // Clip activation logic with AGGRESSIVE reactivation for looping
    // ============================================================================
    if (!clip.isActive)
    {
        // If clip should be looping, ALWAYS reactivate it when inactive
        if (clipShouldLoop)
        {
            clip.isActive = true;
            seekClipToTime(clip, blockStartTime);
            DBG("MidiProcessor: Clip " + clip.id + " reactivated for loop playback");
        }
        else
        {
            // Standard activation logic for non-looping clips
            double localBlockStart = blockStartTime - clip.startTime;
            double unscaledLocalBlockStart = localBlockStart / visualScaleFactor;

            if (clip.unscaledLocalTime >= unscaledDuration)
            {
                // Clip finished and not looping - stay inactive
                return;
            }
            else
            {
                // Clip hasn't finished, activate normally
                clip.isActive = true;
                double seekTime = juce::jmax(clip.startTime, blockStartTime);
                seekClipToTime(clip, seekTime);
            }
        }
    }

    double localBlockStartTime = blockStartTime - clip.startTime;
    double localBlockEndTime = blockEndTime - clip.startTime;

    double unscaledLocalStart = localBlockStartTime / visualScaleFactor;
    double unscaledLocalEnd = localBlockEndTime / visualScaleFactor;

    // Clamp to clip's actual duration
    unscaledLocalEnd = juce::jmin(unscaledLocalEnd, unscaledDuration);

    // ============================================================================
    // CRITICAL: Expand the start window to catch notes at exact boundaries
    // For notes at exactly time 0, we need to ensure they're included
    // ============================================================================
    double unscaledLocalStartWithTolerance;

    if (unscaledLocalStart < EPSILON * 2.0) // Very close to start (within 0.2ms)
    {
        // For times very close to 0, start from exactly 0 to catch boundary notes
        unscaledLocalStartWithTolerance = 0.0;
    }
    else
    {
        // For other times, use epsilon tolerance
        unscaledLocalStartWithTolerance = juce::jmax(0.0, unscaledLocalStart - EPSILON);
    }

    // Process MIDI events within this block
    while (clip.currentEventIndex < clip.sequence.getNumEvents())
    {
        juce::MidiMessageSequence::MidiEventHolder* eventHolder =
        clip.sequence.getEventPointer(clip.currentEventIndex);

        if (!eventHolder)
            break;

        double originalEventTime = eventHolder->message.getTimeStamp();

        // Stop processing if event is beyond clip's duration
        if (originalEventTime >= unscaledDuration)
        {
            // CRITICAL: Don't mark inactive if looping - let loop restart handle it
            if (!clipShouldLoop)
            {
                clip.isActive = false;
            }
            break;
        }

        // Skip events that are before this block's start time (with tolerance)
        // BUT ONLY if the event would have a NEGATIVE sample offset
        if (originalEventTime < unscaledLocalStartWithTolerance)
        {
            // CRITICAL FIX: Check if this note would actually have a valid sample offset
            // before skipping it. At high BPM, notes can cluster together.
            double scaledEventTime = originalEventTime * visualScaleFactor;
            double absoluteEventTime = clip.startTime + scaledEventTime;
            double relativeTime = absoluteEventTime - blockStartTime;
            int testOffset = static_cast<int>(std::lround(relativeTime * sampleRate));

            // Only skip if the sample offset is truly negative (before this block)
            if (testOffset < 0)
            {
                clip.currentEventIndex++;
                continue;
            }
            // Otherwise, fall through and process this event
        }

        // Stop if event is after this block
        if (originalEventTime >= unscaledLocalEnd)
        {
            break;
        }

        // Calculate when this event should fire
        double scaledEventTime = originalEventTime * visualScaleFactor;
        double absoluteEventTime = clip.startTime + scaledEventTime;

        if (absoluteEventTime >= clipEndTime)
        {
            // CRITICAL: Don't mark inactive if looping
            if (!clipShouldLoop)
            {
                clip.isActive = false;
            }
            break;
        }

        double relativeTime = absoluteEventTime - blockStartTime;

        // ============================================================================
        // CRITICAL FIX: Use std::lround instead of static_cast<int> for proper rounding
        // static_cast<int> truncates (3.9 -> 3), causing cumulative timing drift
        // std::lround rounds properly (3.9 -> 4, 3.4 -> 3)
        // This is especially important for fast double kick patterns at high BPM
        // ============================================================================
        int sampleOffset = static_cast<int>(std::lround(relativeTime * sampleRate));

        // Clamp negative offsets (rounding errors) to 0
        if (sampleOffset < 0)
            sampleOffset = 0;

        // Only add events within the current audio block
        if (sampleOffset >= 0 && sampleOffset < samplesPerBlock)
        {
            juce::MidiMessage message = eventHolder->message;

            // Apply drum library remapping
            if (message.isNoteOnOrOff())
            {
                int originalNote = message.getNoteNumber();
                int remappedNote = drumLibraryManager.mapNoteToLibrary(originalNote, clip.sourceLibrary, targetLib);

                if (remappedNote != originalNote)
                {
                    if (message.isNoteOn())
                        message = juce::MidiMessage::noteOn(message.getChannel(), remappedNote,
                                                            static_cast<juce::uint8>(message.getVelocity()));
                        else
                            message = juce::MidiMessage::noteOff(message.getChannel(), remappedNote,
                                                                 static_cast<juce::uint8>(message.getVelocity()));
                }
            }

            buffer.addEvent(message, sampleOffset);
        }

        clip.currentEventIndex++;
    }

    // Update clip's local time position
    clip.unscaledLocalTime = unscaledLocalEnd;

    // ============================================================================
    // CRITICAL FOR LOOPING: Only mark inactive if NOT looping
    // ============================================================================
    if (!clipShouldLoop)
    {
        // Standard end-of-clip logic for non-looping clips
        if (clip.unscaledLocalTime >= unscaledDuration)
        {
            clip.isActive = false;
        }
    }
    // If looping, the loop restart logic in processBlock handles reactivation
}


// ============================================================================
// CRITICAL: seekClipToTime must handle boundary notes correctly
// ============================================================================
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

    // ============================================================================
    // IMPROVED: Special handling for seeking to time 0 or very close to it
    // When looping restarts at time 0, we MUST position at index 0 to catch
    // notes at exactly time 0.0
    // ============================================================================
    constexpr double NEAR_ZERO_THRESHOLD = 0.001; // 1ms

    if (unscaledLocalTime < NEAR_ZERO_THRESHOLD)
    {
        // Seeking to start or very close to start - always use index 0
        clip.currentEventIndex = 0;
        clip.unscaledLocalTime = 0.0;

        DBG("MidiProcessor::seekClipToTime - Clip " + clip.id +
        " seeking to time near zero (unscaled=" + juce::String(unscaledLocalTime, 6) +
        "s), setting index=0");
        return;
    }

    // For seeks away from the start, use binary search with small backward epsilon
    // to ensure we don't skip events at exact seek points
    constexpr double SEEK_EPSILON = 0.0001; // 0.1ms - position slightly before target
    double seekTime = juce::jmax(0.0, unscaledLocalTime - SEEK_EPSILON);
    clip.unscaledLocalTime = unscaledLocalTime; // Store actual target time

    // Binary search to find the last event at or before seekTime
    int low = 0;
    int high = clip.sequence.getNumEvents() - 1;
    int result = 0;

    while (low <= high)
    {
        int mid = (low + high) / 2;
        juce::MidiMessageSequence::MidiEventHolder* event = clip.sequence.getEventPointer(mid);

        if (!event)
            break;

        double eventTime = event->message.getTimeStamp();

        if (eventTime <= seekTime)
        {
            result = mid;
            low = mid + 1;
        }
        else
        {
            high = mid - 1;
        }
    }

    clip.currentEventIndex = result;

    DBG("MidiProcessor::seekClipToTime - Clip " + clip.id +
    " seeking to globalTime=" + juce::String(globalTime, 3) +
    "s, unscaledTime=" + juce::String(unscaledLocalTime, 3) +
    "s, eventIndex=" + juce::String(result));
}

void MidiProcessor::play()
{
    // THREAD-SAFE: Atomic write
    playing.store(true);

    juce::ScopedLock sl(clipLock);

    // Read current playhead position atomically
    double currentPosition = playheadPosition.load();

    DBG("MidiProcessor::play() - Active clips: " + juce::String(activeClips.size()));
    for (const auto& clip : activeClips)
    {
        seekClipToTime(*clip, currentPosition);
        DBG("  Clip " + clip->id + ": " + juce::String(clip->sequence.getNumEvents()) + " events, startTime=" + juce::String(clip->startTime, 3) + "s");
    }
}

void MidiProcessor::stop()
{
    // THREAD-SAFE: Atomic writes
    playing.store(false);
    playheadPosition.store(0.0);

    juce::ScopedLock sl(clipLock);

    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, 0.0);
    }
}

void MidiProcessor::pause()
{
    // THREAD-SAFE: Atomic write
    playing.store(false);
}

void MidiProcessor::setPlayheadPosition(double timeInSeconds)
{
    // THREAD-SAFE: Atomic write with clamping
    double clampedTime = juce::jmax(0.0, timeInSeconds);
    playheadPosition.store(clampedTime);

    juce::ScopedLock sl(clipLock);

    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, clampedTime);
    }
}

void MidiProcessor::addPreviewNote(const juce::MidiMessage& noteOn, const juce::MidiMessage& noteOff)
{
    const juce::ScopedLock sl(previewLock);

    previewNoteBuffer.clear();
    previewNoteBuffer.addEvent(noteOn, 0);
    previewNoteBuffer.addEvent(noteOff, static_cast<int>(sampleRate * 0.1)); // 100ms duration
}
