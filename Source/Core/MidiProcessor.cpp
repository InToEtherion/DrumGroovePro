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
    if (!playing)
        return;

    currentBPM = bpm;

    double secondsPerBlock = static_cast<double>(samplesPerBlock) / sampleRate;
    double adjustedSecondsPerBlock = secondsPerBlock * playbackSpeed;
    
    double blockStartTime = playheadPosition;
    double blockEndTime = playheadPosition + adjustedSecondsPerBlock;

    juce::ScopedLock sl(clipLock);

    for (auto& clip : activeClips)
    {
        processClipWithSampleAccuracy(*clip, midiMessages, blockStartTime, blockEndTime, bpm, targetLibrary);
    }

    playheadPosition = blockEndTime;

    if (loopEnabled && playheadPosition >= loopEnd)
	{
    double overrun = playheadPosition - loopEnd;
    playheadPosition = loopStart + overrun;

		for (auto& clip : activeClips)
		{
			seekClipToTime(*clip, playheadPosition);
        
			double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
			double scaledDuration = clip->duration * visualScaleFactor;
			double clipEndTime = clip->startTime + scaledDuration;
        
			if (playheadPosition >= clip->startTime && playheadPosition < clipEndTime)
			{
            clip->isActive = true;
			}
		}
	}
}

void MidiProcessor::addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib, 
                                double referenceBPM, double targetBPM, int trackNum, const juce::String& clipId)
{
    if (!file.existsAsFile())
        return;
    
    auto clip = std::make_unique<MidiClipPlayback>();
    clip->id = clipId;  // USE THE PROVIDED ID
    clip->startTime = startTime;
    clip->sourceLibrary = sourceLib;
    clip->referenceBPM = referenceBPM;
    clip->targetBPM = targetBPM;
    clip->trackNumber = trackNum;
    clip->unscaledLocalTime = 0.0;

    if (!loadMidiFileWithPrecision(file, *clip))
        return;
    
    if (clip->sequence.getNumEvents() == 0)
        return;

    juce::ScopedLock sl(clipLock);
    seekClipToTime(*clip, playheadPosition);
    activeClips.push_back(std::move(clip));
    
    DBG("MidiProcessor: Added clip with ID: " + clipId);
}

void MidiProcessor::addMidiClip(const juce::File& file, double startTime, DrumLibrary sourceLib, 
                                double referenceBPM, double targetBPM, int trackNum, double explicitDuration, const juce::String& clipId)
{
    if (!file.existsAsFile())
        return;
    
    auto clip = std::make_unique<MidiClipPlayback>();
    clip->id = clipId;  // USE THE PROVIDED ID
    clip->startTime = startTime;
    clip->sourceLibrary = sourceLib;
    clip->referenceBPM = referenceBPM;
    clip->targetBPM = targetBPM;
    clip->trackNumber = trackNum;
    clip->unscaledLocalTime = 0.0;

    if (!loadMidiFileWithPrecision(file, *clip))
        return;
    
    if (clip->sequence.getNumEvents() == 0)
        return;
    
    clip->duration = explicitDuration;
    
    DBG("MidiProcessor: Added clip with explicit duration: " + juce::String(explicitDuration, 3) + 
        "s, referenceBPM: " + juce::String(referenceBPM, 2) + ", targetBPM: " + juce::String(targetBPM, 2) +
        ", ID: " + clipId);

    juce::ScopedLock sl(clipLock);
    seekClipToTime(*clip, playheadPosition);
    activeClips.push_back(std::move(clip));
}



void MidiProcessor::updateTrackBPM(int trackNumber, double newBPM)
{
    juce::ScopedLock sl(clipLock);
    
    // CRITICAL FIX: Update BPM and recalculate clip activation states in real-time
    for (auto& clip : activeClips)
    {
        if (clip->trackNumber == trackNumber && clip->targetBPM != newBPM)
        {
            clip->targetBPM = newBPM;
            
            // Recalculate if this clip should be active at current playhead position
            // BPM changes affect visual duration, so we need to recheck boundaries
            double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
            double scaledDuration = clip->duration * visualScaleFactor;
            double clipEndTime = clip->startTime + scaledDuration;
            
            // Update active state based on current playhead position
            if (playheadPosition >= clip->startTime && playheadPosition < clipEndTime)
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
    
    for (auto& clip : activeClips)
    {
        if (clip->id == clipId)
        {
            clip->startTime = newStartTime;
            clip->duration = newDuration;
           
            double visualScaleFactor = clip->referenceBPM / clip->targetBPM;
            double scaledDuration = newDuration * visualScaleFactor;
            double clipEndTime = newStartTime + scaledDuration;
            
            // CRITICAL FIX: Re-seek clip to current playhead position when boundaries change
            if (playheadPosition >= newStartTime && playheadPosition < clipEndTime)
            {
                seekClipToTime(*clip, playheadPosition);
                clip->isActive = true;
                
                DBG("MidiProcessor: Clip " + clipId + " repositioned - now active at playhead " + 
                    juce::String(playheadPosition, 3) + "s (clip: " + 
                    juce::String(newStartTime, 3) + "s to " + 
                    juce::String(clipEndTime, 3) + "s)");
            }
            else if (playheadPosition < newStartTime)
            {
                seekClipToTime(*clip, newStartTime);
                clip->isActive = false;
                
                DBG("MidiProcessor: Clip " + clipId + " repositioned - inactive (playhead before clip)");
            }
            else
            {
                clip->isActive = false;
                
                DBG("MidiProcessor: Clip " + clipId + " repositioned - inactive (playhead after clip)");
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
    juce::FileInputStream fileStream(file);
    if (!fileStream.openedOk())
        return false;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(fileStream))
        return false;

    double ticksPerQuarterNote = midiFile.getTimeFormat();
    if (ticksPerQuarterNote <= 0)
        ticksPerQuarterNote = 480.0;
    
    clip.originalBPM = 120.0;
    double currentTempo = 120.0;
    
    int numTracks = midiFile.getNumTracks();
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
                    
                    if (event->message.isTempoMetaEvent())
                    {
                        currentTempo = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                        clip.originalBPM = currentTempo;
                    }
                }
            }
        }
    }

    std::sort(allEvents.begin(), allEvents.end(),
              [](const juce::MidiMessageSequence::MidiEventHolder* a,
                 const juce::MidiMessageSequence::MidiEventHolder* b) {
                  return a->message.getTimeStamp() < b->message.getTimeStamp();
              });

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

    clip.sequence.sort();
    clip.sequence.updateMatchedPairs();

    if (clip.sequence.getNumEvents() > 0)
    {
        double lastEventTime = clip.sequence.getEndTime();
        clip.duration = lastEventTime + 0.1;
    }
    else
    {
        clip.duration = 1.0;
    }

    return true;
}

void MidiProcessor::processClipWithSampleAccuracy(MidiClipPlayback& clip, juce::MidiBuffer& buffer,
                                                  double blockStartTime, double blockEndTime,
                                                  double bpm, DrumLibrary targetLib)
{
    double visualScaleFactor = clip.referenceBPM / clip.targetBPM;
    double scaledDuration = clip.duration * visualScaleFactor;
    double clipEndTime = clip.startTime + scaledDuration;
    
    // CRITICAL FIX: Strict boundary checking - don't play outside visual bounds
    // If playhead is completely before or after the clip, deactivate and return
    if (blockStartTime >= clipEndTime || blockEndTime <= clip.startTime)
    {
        clip.isActive = false;
        return;
    }
    
    // CRITICAL FIX: If clip already finished (reached its duration), keep it inactive
    double unscaledDuration = clip.duration;
    if (clip.unscaledLocalTime >= unscaledDuration)
    {
        clip.isActive = false;
        return;
    }
    
    // Activate clip if it's within bounds and hasn't finished
    if (!clip.isActive)
    {
        clip.isActive = true;
        seekClipToTime(clip, blockStartTime);
    }

    double blockDuration = blockEndTime - blockStartTime;
    double unscaledBlockDuration = blockDuration / visualScaleFactor;

    double localBlockStartTime = blockStartTime - clip.startTime;
    double localBlockEndTime = blockEndTime - clip.startTime;
    
    double unscaledLocalStart = localBlockStartTime / visualScaleFactor;
    double unscaledLocalEnd = localBlockEndTime / visualScaleFactor;
    
    // CRITICAL: Clamp to clip's actual duration - don't process beyond visual bounds
    unscaledLocalEnd = juce::jmin(unscaledLocalEnd, unscaledDuration);

    // Process MIDI events within this block
    while (clip.currentEventIndex < clip.sequence.getNumEvents())
    {
        juce::MidiMessageSequence::MidiEventHolder* eventHolder = 
            clip.sequence.getEventPointer(clip.currentEventIndex);
        
        if (!eventHolder)
            break;

        double originalEventTime = eventHolder->message.getTimeStamp();

        // CRITICAL: Stop processing if event is beyond clip's duration
        if (originalEventTime >= unscaledDuration)
        {
            clip.isActive = false;
            break;
        }

        // Skip events before this block
        if (originalEventTime < unscaledLocalStart)
        {
            clip.currentEventIndex++;
            continue;
        }
        
        // Stop if event is after this block
        if (originalEventTime >= unscaledLocalEnd)
        {
            break;
        }

        // Calculate when this event should fire in the current block
        double scaledEventTime = originalEventTime * visualScaleFactor;
        double absoluteEventTime = clip.startTime + scaledEventTime;
        
        // CRITICAL: Double-check event is within clip visual bounds
        if (absoluteEventTime >= clipEndTime)
        {
            clip.isActive = false;
            break;
        }
        
        double relativeTime = absoluteEventTime - blockStartTime;
        int sampleOffset = static_cast<int>(relativeTime * sampleRate);
        
        // Only add events that are within the current audio block
        if (sampleOffset >= 0 && sampleOffset < samplesPerBlock)
        {
            juce::MidiMessage message = eventHolder->message;
            
            // Apply drum library remapping if needed
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
    
    // CRITICAL: Mark clip as inactive if it has finished or reached its duration
    if (clip.currentEventIndex >= clip.sequence.getNumEvents() || 
        clip.unscaledLocalTime >= unscaledDuration ||
        blockEndTime >= clipEndTime)
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
    int result = 0;
    
    while (low <= high)
    {
        int mid = (low + high) / 2;
        juce::MidiMessageSequence::MidiEventHolder* event = clip.sequence.getEventPointer(mid);
        
        if (!event)
            break;
            
        double eventTime = event->message.getTimeStamp();
        
        if (eventTime <= unscaledLocalTime)
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
}

void MidiProcessor::play()
{
    playing = true;

    juce::ScopedLock sl(clipLock);

    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, playheadPosition);
    }
}

void MidiProcessor::stop()
{
    playing = false;
    playheadPosition = 0.0;

    juce::ScopedLock sl(clipLock);

    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, 0.0);
    }
}

void MidiProcessor::pause()
{
    playing = false;
}

void MidiProcessor::setPlayheadPosition(double timeInSeconds)
{
    playheadPosition = juce::jmax(0.0, timeInSeconds);
    
    juce::ScopedLock sl(clipLock);
    
    for (auto& clip : activeClips)
    {
        seekClipToTime(*clip, playheadPosition);
    }
}