// Track.cpp - PART 1: File with fixes for file path display and MIDI note colors
#include "Track.h"
#include "TrackHeader.h"
#include "MultiTrackContainer.h"
#include "../../Utils/TimelineUtils.h"
#include "../../Core/MidiDissector.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../LookAndFeel/ColourPalette.h"

Track::Track(DrumGrooveProcessor& p, MultiTrackContainer& c, int trackNum)
    : processor(p), container(c), trackNumber(trackNum)
{
}

Track::~Track()
{
    // Clean up any temporary drag files
    if (lastTempDragFile.existsAsFile())
    {
        lastTempDragFile.deleteFile();
        DBG("Track: Cleaned up temp drag file on destruction");
    }
}

void Track::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::secondaryBackground);
    
    g.setColour(juce::Colours::red.withAlpha(0.5f));
    g.drawLine(static_cast<float>(getWidth() - 1), 0.0f, 
               static_cast<float>(getWidth() - 1), static_cast<float>(getHeight()), 2.0f);
    
    drawClips(g);
    
    if (ghostClip)
        drawGhostClip(g);
        
    if (isSelecting)
        drawSelectionBox(g);
        
    drawDropIndicator(g);
    
    g.setColour(ColourPalette::separator);
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1), 
               static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1));
}

void Track::resized()
{
}

void Track::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showTrackContextMenu(e.getPosition());
        return;
    }
    
    // CRITICAL FIX: If Ctrl+Alt is pressed, don't start internal dragging
    // This allows the parent MultiTrackContainer to handle external drag to DAW
    if (e.mods.isCtrlDown() && e.mods.isAltDown())
    {
        auto* clip = getClipAt(e.position);
        if (clip)
        {
            // Select the clip but don't start internal dragging
            if (!e.mods.isShiftDown())
                container.deselectAllClips();
            
            clip->isSelected = true;
            
            // Always trigger file path display callback when clicking a clip
            if (onClipSelected && clip->file.existsAsFile())
                onClipSelected(clip->file);
        }
        repaint();
        return; // Exit early - parent will handle the drag
    }

    auto* clip = getClipAt(e.position);
    
    if (clip)
    {
        float globalX = container.timeToPixels(clip->startTime);
        float localX = globalX - container.getViewportX();
        float clipWidth = static_cast<float>(clip->duration * container.getZoom() * getVisualScaleFactor());
        float clipEndX = localX + clipWidth;

        if (std::abs(e.position.x - clipEndX) < RESIZE_HANDLE_WIDTH)
        {
            isResizing = true;
            resizingClip = clip;
            resizeStartDuration = clip->duration;
        }
        else if (std::abs(e.position.x - localX) < RESIZE_HANDLE_WIDTH)
        {
            isResizingLeft = true;
            resizingClip = clip;
            resizeStartTime = clip->startTime;
            resizeStartDuration = clip->duration;
        }
        else
        {
            if (!e.mods.isShiftDown())
                container.deselectAllClips();
            
            clip->isSelected = true;
            isDragging = true;
            dragStartPoint = e.position;
            draggedClips.clear();

            for (auto& c : clips)
            {
                if (c->isSelected)
                    draggedClips.push_back({c->id, c->startTime});
            }

            // Always trigger file path display callback when clicking a clip
            if (onClipSelected && clip->file.existsAsFile())
                onClipSelected(clip->file);
        }
    }
    else
    {
        if (!e.mods.isShiftDown())
            container.deselectAllClips();

        isSelecting = true;
        dragStartPoint = e.position;
        selectionBox = juce::Rectangle<float>(dragStartPoint, dragStartPoint);
    }
    repaint();
}

// Track.cpp - PART 2: Fixed drawMidiDotsInClip to use correct colors

void Track::drawMidiDotsInClip(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> clipBounds)
{
    if (clipBounds.getWidth() < 20 || clipBounds.getHeight() < 10)
        return;

    if (!clip.file.existsAsFile())
        return;

    auto dotArea = clipBounds;

    // Background - keep darker for contrast
    g.setColour(clip.colour.darker(0.6f));
    g.fillRoundedRectangle(dotArea, 2.0f);

    // Grid lines - keep subtle
    g.setColour(clip.colour.darker(0.8f));
    int gridDivisions = juce::jmax(4, static_cast<int>(dotArea.getWidth() / 15));
    for (int i = 0; i <= gridDivisions; ++i)
    {
        float x = dotArea.getX() + (i * dotArea.getWidth() / static_cast<float>(gridDivisions));
        g.drawVerticalLine(static_cast<int>(x), dotArea.getY(), dotArea.getBottom());
    }

    // Read MIDI file
    juce::FileInputStream stream(clip.file);
    if (!stream.openedOk())
        return;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return;

    double ticksPerQuarterNote = midiFile.getTimeFormat();
    if (ticksPerQuarterNote <= 0)
        ticksPerQuarterNote = 480.0;

    // Determine the actual BPM used by the clip
    double midiFileBPM = 120.0;
    
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const juce::MidiMessageSequence* track = midiFile.getTrack(t);
        if (track)
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                auto* eventHolder = track->getEventPointer(i);
                if (eventHolder && eventHolder->message.isTempoMetaEvent())
                {
                    midiFileBPM = 60.0 / eventHolder->message.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
            if (midiFileBPM != 120.0) break;
        }
    }

    double maxTimeStamp = 0;
    juce::MidiMessageSequence allNotes;
    int minNoteNumber = 127;
    int maxNoteNumber = 0;
    
    // Collect all note events
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const juce::MidiMessageSequence* track = midiFile.getTrack(t);
        if (track)
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                auto* eventHolder = track->getEventPointer(i);
                if (eventHolder)
                {
                    const auto& message = eventHolder->message;
                    
                    if (message.isNoteOn())
                    {
                        allNotes.addEvent(message);
                        int noteNum = message.getNoteNumber();
                        minNoteNumber = juce::jmin(minNoteNumber, noteNum);
                        maxNoteNumber = juce::jmax(maxNoteNumber, noteNum);
                    }
                    
                    maxTimeStamp = juce::jmax(maxTimeStamp, message.getTimeStamp());
                }
            }
        }
    }

    allNotes.sort();
    allNotes.updateMatchedPairs();

    // double midiDurationInSeconds = (maxTimeStamp / ticksPerQuarterNote) * (60.0 / 120.0);
    double visualDuration = juce::jmax(0.1, clip.duration);

    int noteRange = juce::jmax(1, maxNoteNumber - minNoteNumber);

    // Determine coloring strategy based on clip type
    bool isFullMidiFile = (clip.colour == ColourPalette::primaryBlue.withAlpha(0.7f));
    
    // Draw note events with appropriate coloring
    for (int i = 0; i < allNotes.getNumEvents(); ++i)
    {
        const auto& event = allNotes.getEventPointer(i)->message;

        if (event.isNoteOn())
        {
            double noteTime = (event.getTimeStamp() / ticksPerQuarterNote) * (60.0 / 120.0);
            float relativeX = static_cast<float>(noteTime / visualDuration);
            
            if (relativeX >= 0.0f && relativeX <= 1.0f)
            {
                float dotX = dotArea.getX() + relativeX * dotArea.getWidth();
                
                int noteNumber = event.getNoteNumber();
                float relativeY = 1.0f - static_cast<float>(noteNumber - minNoteNumber) / static_cast<float>(noteRange);
                float dotY = dotArea.getY() + relativeY * dotArea.getHeight();

                // Determine color for this individual note
                juce::Colour noteColour;
                if (isFullMidiFile)
                {
                    // For full MIDI files: color each note based on its drum part type
                    DrumPartType notePartType = MidiDissector::getPartTypeFromNote(event.getNoteNumber());
                    noteColour = MidiDissector::getPartColour(notePartType).brighter(0.3f);
                }
                else
                {
                    // For dissected drum parts: use the clip's color brightened
                    noteColour = clip.colour.brighter(0.3f);
                }
                g.setColour(noteColour);
                
                // Draw dot (size based on zoom)
                float dotSize = juce::jmax(1.5f, juce::jmin(3.0f, dotArea.getWidth() / 100.0f));
                g.fillEllipse(dotX - dotSize * 0.5f, dotY - dotSize * 0.5f, dotSize, dotSize);
            }
        }
    }
}

void Track::itemDropped(const SourceDetails& details)
{
    dropIndicatorX = -1;

    auto trackArea = getTrackArea();
    if (!trackArea.contains(details.localPosition))
    {
        ghostClip.reset();
        repaint();
        return;
    }

    bool wasEmpty = clips.empty();

    juce::String description = details.description.toString();
    juce::StringArray parts = juce::StringArray::fromTokens(description, "|", "");

    if (parts.size() >= 2 && parts[1] == "PART")
        handleDrumPartDrop(parts, details.localPosition);
    else
        handleMidiFileDrop(parts, details.localPosition);

    if (wasEmpty && !clips.empty())
        inheritBPMFromHeader();

    // Trigger file path display callback after dropping a file
    // Get the last added clip (the one we just dropped)
    if (!clips.empty() && onClipSelected)
    {
        const auto& lastClip = clips.back();
        if (lastClip->file.existsAsFile())
        {
            onClipSelected(lastClip->file);
        }
    }

    ghostClip.reset();
    repaint();
}

void Track::handleMidiFileDrop(const juce::StringArray& parts, const juce::Point<int>& position)
{
    if (parts.size() < 2)
        return;

    juce::String filename = parts[0];
    juce::File file(parts[1]);

    if (!file.existsAsFile() || !file.hasFileExtension(".mid;.midi"))
        return;

    // Calculate drop position
    auto trackArea = getTrackArea();
    float localX = static_cast<float>(position.x - trackArea.getX());
    double dropTime = container.pixelsToTime(localX + container.getViewportX());
    dropTime = snapToGrid(dropTime);

    // Create new clip
    MidiClip newClip;
    newClip.name = filename;
    newClip.file = file;
    newClip.startTime = dropTime;
    newClip.colour = ColourPalette::primaryBlue.withAlpha(0.7f);
    newClip.referenceBPM = getTrackBPM();

    // Calculate duration
    double duration = 4.0;
    if (calculateMidiFileDuration(file, duration))
    {
        newClip.duration = duration;
    }
    else
    {
        newClip.duration = 4.0;
    }

    clips.push_back(std::make_unique<MidiClip>(newClip));
    
    // âœ… CRITICAL FIX: Add clip to MidiProcessor immediately if playing
    if (container.isPlaying())
    {
        double trackBPM = getTrackBPM();
        processor.midiProcessor.addMidiClip(
            file, 
            dropTime, 
            DrumLibrary::Unknown,
            newClip.referenceBPM,  // Reference BPM (original)
            trackBPM,              // Target BPM (current)
            trackNumber
        );
        
        DBG("Added clip to MidiProcessor in real-time: " + filename);
    }

    container.updateTimelineSize();
    repaint();
}

void Track::handleDrumPartDrop(const juce::StringArray& parts, const juce::Point<int>& position)
{
    if (parts.size() < 4)
        return;

    juce::String partName = parts[0];
    juce::File originalFile(parts[2]);
    int partTypeInt = parts[3].getIntValue();
    DrumPartType partType = static_cast<DrumPartType>(partTypeInt);
    
    int sourceLibInt = parts.size() >= 5 ? parts[4].getIntValue() : 0;
    DrumLibrary sourceLib = static_cast<DrumLibrary>(sourceLibInt);

    if (!originalFile.existsAsFile())
        return;

    auto trackArea = getTrackArea();
    float localX = static_cast<float>(position.x - trackArea.getX());
    double dropTime = container.pixelsToTime(localX + container.getViewportX());
    dropTime = snapToGrid(dropTime);

    juce::File outputFile;
    if (!createDrumPartMidiFile(originalFile, partType, sourceLib, outputFile))
        return;

    MidiClip newClip;
    newClip.name = partName;
    newClip.file = outputFile;
    newClip.startTime = dropTime;
    newClip.colour = MidiDissector::getPartColour(partType).withAlpha(0.7f);
    newClip.referenceBPM = getTrackBPM();

    double duration = 1.0;
    if (calculateMidiFileDuration(outputFile, duration))
    {
        newClip.duration = duration;
    }
    else
    {
        newClip.duration = 1.0;
    }

    clips.push_back(std::make_unique<MidiClip>(newClip));
    
    // âœ… CRITICAL FIX: Add clip to MidiProcessor immediately if playing
    if (container.isPlaying())
    {
        double trackBPM = getTrackBPM();
        processor.midiProcessor.addMidiClip(
            outputFile, 
            dropTime, 
            sourceLib,
            newClip.referenceBPM,  // Reference BPM (original)
            trackBPM,              // Target BPM (current)
            trackNumber
        );
        
        DBG("Added drum part clip to MidiProcessor in real-time: " + partName);
    }

    container.updateTimelineSize();
    repaint();
}

bool Track::createDrumPartMidiFile(const juce::File& originalFile, 
                                   DrumPartType partType, 
                                   DrumLibrary sourceLib,
                                   juce::File& outputFile)
{
    DBG("=== createDrumPartMidiFile ===");
    DBG("Original file: " + originalFile.getFullPathName());
    DBG("Part type: " + juce::String(static_cast<int>(partType)));
    
    MidiDissector dissector;
    DrumLibrary targetLib = processor.getTargetLibrary();

    auto parts = dissector.dissectMidiFileWithLibraryManager(
        originalFile, 
        sourceLib, 
        targetLib,
        processor.drumLibraryManager);

    DBG("Found " + juce::String(parts.size()) + " parts");

    for (const auto& part : parts)
    {
        if (part.type == partType && part.eventCount > 0)
        {
            DBG("Found matching part: " + part.displayName + " with " + juce::String(part.eventCount) + " events");
            
            outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("DrumGroovePro_part_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".mid");

            juce::MidiFile midiFileToSave;
            midiFileToSave.setTicksPerQuarterNote(480);
            
            juce::MidiMessageSequence trackCopy;
            for (int i = 0; i < part.sequence.getNumEvents(); ++i)
            {
                trackCopy.addEvent(part.sequence.getEventPointer(i)->message);
            }
            trackCopy.updateMatchedPairs();
            
            midiFileToSave.addTrack(trackCopy);

            juce::FileOutputStream stream(outputFile);
            if (stream.openedOk())
            {
                midiFileToSave.writeTo(stream);
                stream.flush();
                
                DBG("Temp file created: " + outputFile.getFullPathName());
                
                if (outputFile.existsAsFile() && outputFile.getSize() > 0)
                {
                    DBG("File verified, size: " + juce::String(outputFile.getSize()));
                    return true;
                }
                else
                {
                    DBG("ERROR: File not created or empty!");
                }
            }
            else
            {
                DBG("ERROR: Could not open stream!");
            }
        }
    }
    
    DBG("ERROR: No matching part found or file creation failed");
    return false;
}

bool Track::calculateMidiFileDuration(const juce::File& file, double& duration) const
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return false;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return false;

    double ticksPerQuarterNote = midiFile.getTimeFormat();
    if (ticksPerQuarterNote <= 0)
        ticksPerQuarterNote = 480.0;

    double maxTimeStamp = 0;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const juce::MidiMessageSequence* track = midiFile.getTrack(t);
        if (track && track->getNumEvents() > 0)
        {
            auto lastEvent = track->getEventPointer(track->getNumEvents() - 1);
            if (lastEvent)
            {
                maxTimeStamp = juce::jmax(maxTimeStamp, lastEvent->message.getTimeStamp());
            }
        }
    }

    duration = (maxTimeStamp / ticksPerQuarterNote) * (60.0 / 120.0);
    return duration > 0;
}

void Track::addClip(const MidiClip& clip)
{
    auto newClip = std::make_unique<MidiClip>(clip);
    
    // Deselect all other clips
    for (auto& c : clips)
        c->isSelected = false;
    
    newClip->isSelected = true;
    clips.push_back(std::move(newClip));
    
    container.updateTimelineSize();
    repaint();
}

double Track::pixelsToTime(float pixels) const
{
    // Convert from local track pixels to global container pixels
    float globalX = pixels + container.getViewportX();
    
    // Use track-specific visual scale factor with precision handling
    double scaleFactor = getVisualScaleFactor();
    double zoomLevel = container.getZoom();
    
    // Ensure minimum precision to avoid floating point errors at high BPM
    double effectiveZoom = juce::jmax(0.001, zoomLevel * scaleFactor);
    
    return globalX / effectiveZoom;
}

float Track::timeToPixels(double time) const
{
    // Use track-specific visual scale factor with precision handling
    double scaleFactor = getVisualScaleFactor();
    double zoomLevel = container.getZoom();
    
    // Ensure minimum precision to avoid floating point errors at high BPM
    double effectiveZoom = juce::jmax(0.001, zoomLevel * scaleFactor);
    
    float globalX = static_cast<float>(time * effectiveZoom);
    
    // Convert from global container pixels to local track pixels
    return globalX - container.getViewportX();
}

double Track::snapToGrid(double time) const
{
    return container.snapToGrid(time);
}

double Track::getVisualScaleFactor() const
{
    // Each track uses its OWN BPM for visual scaling, not the container's global BPM
    double trackBPM = getTrackBPM();
    return TimelineUtils::getVisualScaleFactor(trackBPM);
}


juce::Rectangle<int> Track::getTrackArea() const
{
    return getLocalBounds();
}

bool Track::isMuted() const
{
    return container.isTrackMuted(trackNumber - 1);
}

bool Track::isSoloed() const
{
    // Now properly checks the container for solo state
    if (trackNumber > 0 && trackNumber <= container.getNumTracks())
    {
        return container.isTrackSoloed(trackNumber - 1);
    }
    return false;
}

double Track::getTrackBPM() const
{
    return container.getTrackBPM(trackNumber - 1);
}

void Track::showTrackContextMenu(const juce::Point<int>& position)
{
    juce::PopupMenu menu;
    
    menu.addItem(1, "Clear All Clips", !clips.empty());
    menu.addSeparator();
    menu.addItem(2, "Select All Clips", !clips.empty());
    menu.addItem(3, "Delete Selected Clips", !getSelectedClips().empty());
    
    // Convert local position to screen coordinates for proper menu placement
    auto screenPos = localPointToGlobal(position);
    
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
        [this](int result) {
            switch (result)
            {
                case 1: clearAllClips(); break;
                case 2: selectAll(); break;
                case 3: removeSelectedClips(); break;
            }
        });
}

MidiClip* Track::getClipAt(const juce::Point<float>& point)
{
    auto trackArea = getTrackArea();
    if (!trackArea.contains(point.toInt()))
        return nullptr;

    for (auto& clip : clips)
    {
        float globalX = container.timeToPixels(clip->startTime);
        float localX = globalX - container.getViewportX();
        float width = static_cast<float>(clip->duration * container.getZoom() * getVisualScaleFactor());

        if (point.x >= localX && point.x <= localX + width)
            return clip.get();
    }

    return nullptr;
}

std::vector<MidiClip*> Track::getSelectedClips()
{
    std::vector<MidiClip*> selected;
    for (auto& clip : clips)
    {
        if (clip->isSelected)
            selected.push_back(clip.get());
    }
    return selected;
}

void Track::removeSelectedClips()
{
    clips.erase(
        std::remove_if(clips.begin(), clips.end(),
            [](const std::unique_ptr<MidiClip>& clip) { return clip->isSelected; }),
        clips.end());
    
    container.updateTimelineSize();
    repaint();
}

void Track::selectAll()
{
    for (auto& clip : clips)
        clip->isSelected = true;
    repaint();
}

void Track::deselectAll()
{
    for (auto& clip : clips)
        clip->isSelected = false;
    repaint();
}

void Track::clearAllClips()
{
    clips.clear();
    container.updateTimelineSize();
    repaint();
}

void Track::mouseDrag(const juce::MouseEvent& e)
{
    // CRITICAL FIX: If Ctrl+Alt is pressed, trigger external drag to DAW
    if (e.mods.isCtrlDown() && e.mods.isAltDown())
    {
        // Check if we've moved enough distance to start external drag
        if (!isExternalDragging && e.getDistanceFromDragStart() > 5)
        {
            isExternalDragging = true;
            DBG("=== Track: External drag triggered! ===");
            
            // Trigger external drag DIRECTLY from Track
            startExternalDrag();
        }
        
        // Set cursor to indicate external drag mode
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return; // Exit early
    }
    
    if (isResizing && resizingClip)
    {
        double newEndTime = pixelsToTime(static_cast<float>(e.x));
        double newDuration = newEndTime - resizingClip->startTime;
        newDuration = juce::jmax(0.1, newDuration);

        if (!e.mods.isAltDown())
        {
            double snappedEndTime = snapToGrid(resizingClip->startTime + newDuration);
            newDuration = snappedEndTime - resizingClip->startTime;
        }

        resizingClip->duration = newDuration;
        repaint();
    }
    else if (isResizingLeft && resizingClip)
    {
        double newStartTime = pixelsToTime(static_cast<float>(e.x));

        if (!e.mods.isAltDown())
            newStartTime = snapToGrid(newStartTime);

        double endTime = resizeStartTime + resizeStartDuration;
        newStartTime = juce::jmin(newStartTime, endTime - 0.1);
        newStartTime = juce::jmax(0.0, newStartTime);

        resizingClip->startTime = newStartTime;
        resizingClip->duration = endTime - newStartTime;
        repaint();
    }
    else if (isDragging)
    {
        // Check if dragging to another track
        juce::Point<int> globalPoint = localPointToGlobal(e.getPosition());
        juce::Point<int> containerPoint = container.getLocalPoint(nullptr, globalPoint);
        
        // Calculate which track we're over
        int rulerHeight = 30; // RULER_HEIGHT from MultiTrackContainer
        int trackHeight = 80; // TRACK_HEIGHT from MultiTrackContainer
        int targetTrackIndex = (containerPoint.y - rulerHeight) / trackHeight;
        
        // If dragging to a different track
        if (targetTrackIndex >= 0 && 
            targetTrackIndex < container.getNumTracks() && 
            targetTrackIndex != (trackNumber - 1))
        {
            // Show visual feedback for inter-track drag
            setMouseCursor(juce::MouseCursor::CopyingCursor);
        }
        else
        {
            // Normal intra-track dragging
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            
            // Get current track BPM for accurate positioning
            double currentBPM = container.getTrackBPM(trackNumber - 1);
            double scaleFactor = 120.0 / currentBPM;
            
            double currentTime = pixelsToTime(static_cast<float>(e.x));
            double startTime = pixelsToTime(static_cast<float>(dragStartPoint.x));
            double deltaTime = currentTime - startTime;
            deltaTime *= scaleFactor; // Adjust for BPM

            for (auto& clip : clips)
            {
                if (clip->isSelected)
                {
                    for (const auto& [id, originalTime] : draggedClips)
                    {
                        if (id == clip->id)
                        {
                            double newTime = originalTime + deltaTime;

                            if (!e.mods.isAltDown())
                                newTime = snapToGrid(newTime);

                            newTime = juce::jmax(0.0, newTime);
                            clip->startTime = newTime;
                            break;
                        }
                    }
                }
            }
        }
        repaint();
    }
    else if (isSelecting)
    {
        selectionBox = juce::Rectangle<float>(dragStartPoint, e.position);

        for (auto& clip : clips)
        {
            float globalX = container.timeToPixels(clip->startTime);
            float localX = globalX - container.getViewportX();
            float clipWidth = static_cast<float>(clip->duration * container.getZoom() * getVisualScaleFactor());
            
            juce::Rectangle<float> clipBounds(localX, 10.0f, clipWidth, TRACK_HEIGHT - 20.0f);

            if (selectionBox.intersects(clipBounds))
                clip->isSelected = true;
            else if (!e.mods.isShiftDown())
                clip->isSelected = false;
        }
        repaint();
    }
}

void Track::mouseUp(const juce::MouseEvent& e)
{	
	isExternalDragging = false;
    if (isDragging)
    {
        // Snap final positions
        for (auto& clip : clips)
        {
            if (clip->isSelected)
            {
                clip->startTime = snapToGrid(clip->startTime);
                
                // âœ… CRITICAL FIX: Update MidiProcessor if playing
                if (container.isPlaying())
                {
                    double trackBPM = getTrackBPM();
                    double visualScaleFactor = clip->referenceBPM / trackBPM;
                    double visualDuration = clip->duration * visualScaleFactor;
                    
                    processor.midiProcessor.updateClipBoundaries(
                        clip->id,
                        clip->startTime,
                        clip->duration  // Pass original duration, not visual
                    );
                    
                    DBG("Updated clip position in real-time: " + clip->name);
                }
            }
        }
    }
    
    if (isResizing && resizingClip)
    {
        resizingClip->duration = juce::jmax(0.1, resizingClip->duration);
        resizingClip->duration = snapToGrid(resizingClip->duration);
        
        // âœ… CRITICAL FIX: Update MidiProcessor if playing
        if (container.isPlaying())
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );
            
            DBG("Updated clip duration in real-time: " + resizingClip->name);
        }
    }
    
    if (isResizingLeft && resizingClip)
    {
        resizingClip->startTime = juce::jmax(0.0, resizingClip->startTime);
        resizingClip->duration = juce::jmax(0.1, resizingClip->duration);
        
        resizingClip->startTime = snapToGrid(resizingClip->startTime);
        resizingClip->duration = snapToGrid(resizingClip->duration);
        
        // âœ… CRITICAL FIX: Update MidiProcessor if playing
        if (container.isPlaying())
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );
            
            DBG("Updated clip left resize in real-time: " + resizingClip->name);
        }
    }
    
    // Handle track-to-track drag
    if (isDragging && !draggedClips.empty())
    {
        juce::Point<int> screenPos = e.getScreenPosition();
        juce::Point<int> containerPoint = container.getLocalPoint(nullptr, screenPos);
        
        int rulerHeight = 30;
        int trackHeight = 80;
        int targetTrackIndex = (containerPoint.y - rulerHeight) / trackHeight;
        
        if (targetTrackIndex >= 0 && 
            targetTrackIndex < container.getNumTracks() && 
            targetTrackIndex != (trackNumber - 1))
        {
            Track* targetTrack = container.getTrack(targetTrackIndex);
            if (targetTrack)
            {
                moveSelectedClipsToTrack(targetTrack);
                container.updateTimelineSize();
            }
        }
    }
    
    isDragging = false;
    isResizing = false;
    isResizingLeft = false;
    isSelecting = false;
	isExternalDragging = false; 
    resizingClip = nullptr;
    draggedClips.clear();
    selectionBox = juce::Rectangle<float>();
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void Track::mouseMove(const juce::MouseEvent& e)
{
    auto trackArea = getTrackArea();
    if (!trackArea.contains(e.getPosition()))
        return;

    auto* clip = getClipAt(e.position);
    if (clip)
    {
        float globalX = container.timeToPixels(clip->startTime);
        float localX = globalX - container.getViewportX();
        float clipWidth = static_cast<float>(clip->duration * container.getZoom() * getVisualScaleFactor());
        float clipEndX = localX + clipWidth;

        if (std::abs(e.position.x - clipEndX) < RESIZE_HANDLE_WIDTH || 
            std::abs(e.position.x - localX) < RESIZE_HANDLE_WIDTH)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
    }
    else
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

// DRAG AND DROP METHODS (add these)

bool Track::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isString();
}

void Track::itemDragEnter(const SourceDetails& details)
{
    dropIndicatorX = 0;

    ghostClip = std::make_unique<MidiClip>();
    ghostClip->name = details.description.toString();
    ghostClip->startTime = 0;
    
    juce::String description = details.description.toString();
    juce::StringArray parts = juce::StringArray::fromTokens(description, "|", "");
    
    double baseDuration = 4.0;
    
    if (parts.size() >= 2 && parts[1] == "PART")
    {
        baseDuration = 1.0; 
    }
    else if (parts.size() >= 2)
    {
        juce::File midiFile(parts[1]);
        if (midiFile.existsAsFile())
        {
            if (!calculateMidiFileDuration(midiFile, baseDuration))
                baseDuration = 4.0;
        }
    }
    
    ghostClip->originalBPM = 120.0;
    ghostClip->duration = baseDuration;
    
    adjustGhostClipToTrackBPM();
    
    ghostClip->colour = ColourPalette::primaryBlue.withAlpha(0.3f);

    DBG("Ghost clip entered track " + juce::String(trackNumber) + 
        " - Duration: " + juce::String(ghostClip->duration, 3) + "s");

    repaint();
}

void Track::itemDragMove(const SourceDetails& details)
{
    auto trackArea = getTrackArea();
    if (trackArea.contains(details.localPosition))
    {
        if (ghostClip)
        {
            float globalMouseX = static_cast<float>(details.localPosition.x + container.getViewportX());
            double mouseTime = container.pixelsToTime(globalMouseX);
            double snappedMouseTime = snapToGrid(mouseTime);
            
            dropIndicatorX = container.timeToPixels(snappedMouseTime);
            ghostClip->startTime = snappedMouseTime;
            
            DBG("Drag move - Mouse time: " + juce::String(mouseTime, 3) + 
                ", Snapped: " + juce::String(snappedMouseTime, 3) +
                ", Ghost & Drop indicator at: " + juce::String(snappedMouseTime, 3));
        }
        else
        {
            dropIndicatorX = static_cast<float>(details.localPosition.x);
        }
    }
    else
    {
        dropIndicatorX = -1;
    }

    repaint();
}

void Track::itemDragExit(const SourceDetails&)
{
    dropIndicatorX = -1;
    ghostClip.reset();
    repaint();
}

// DRAWING METHODS (add these)

void Track::drawClips(juce::Graphics& g)
{
    // Use track-specific visual scale factor for all clip calculations
    double scaleFactor = getVisualScaleFactor();
    
    for (const auto& clip : clips)
    {
        float globalX = container.timeToPixels(clip->startTime);
        float localX = globalX - container.getViewportX();
        
        // Calculate width using track's own BPM scaling
        float width = static_cast<float>(clip->duration * container.getZoom() * scaleFactor);

        if (localX + width < 0 || localX > getWidth())
            continue;

        auto clipBounds = juce::Rectangle<float>(localX, 10.0f, width, static_cast<float>(TRACK_HEIGHT - 20));

        // Draw clip background with appropriate color
        juce::Colour clipColour = clip->colour;
        if (isMuted())
            clipColour = clipColour.darker(0.5f);

        g.setColour(clipColour);
        g.fillRoundedRectangle(clipBounds, 4.0f);

        // Draw selection highlight
        if (clip->isSelected)
        {
            g.setColour(ColourPalette::primaryBlue.withAlpha(0.3f));
            g.fillRoundedRectangle(clipBounds, 4.0f);
            
            g.setColour(ColourPalette::primaryBlue);
            g.drawRoundedRectangle(clipBounds, 4.0f, 2.0f);
        }
        else
        {
            g.setColour(clipColour.darker(0.3f));
            g.drawRoundedRectangle(clipBounds, 4.0f, 1.0f);
        }

        // Draw MIDI dots
        drawMidiDotsInClip(g, *clip, clipBounds);
		g.setColour(juce::Colours::white.withAlpha(0.8f));

        // Draw clip name
        if (clipBounds.getWidth() > 40)
        {
            auto& lnf = DrumGrooveLookAndFeel::getInstance();
            g.setFont(lnf.getSmallFont().withHeight(11.0f));
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawText(clip->name, clipBounds.reduced(4.0f, 2.0f), 
                      juce::Justification::topLeft, true);
        }

        // Draw resize handles
        if (clip->isSelected)
        {
            g.setColour(ColourPalette::primaryBlue);
            g.fillRect(clipBounds.getX(), clipBounds.getY(), RESIZE_HANDLE_WIDTH, clipBounds.getHeight());
            g.fillRect(clipBounds.getRight() - RESIZE_HANDLE_WIDTH, clipBounds.getY(), 
                      RESIZE_HANDLE_WIDTH, clipBounds.getHeight());
        }
    }
}

void Track::drawGhostClip(juce::Graphics& g)
{
    if (!ghostClip)
        return;

    float globalX = container.timeToPixels(ghostClip->startTime);
    float localX = globalX - container.getViewportX();
    float width = static_cast<float>(ghostClip->duration * container.getZoom() * getVisualScaleFactor());

    auto clipBounds = juce::Rectangle<float>(localX, 10.0f, width, TRACK_HEIGHT - 20.0f);

    g.setColour(ghostClip->colour);
    g.fillRoundedRectangle(clipBounds, 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(clipBounds, 4.0f, 2.0f);
    
    DBG("Drawing ghost clip at local X: " + juce::String(localX) + 
        " (time: " + juce::String(ghostClip->startTime, 3) + "s, global X: " + juce::String(globalX) + ")");
}

void Track::drawSelectionBox(juce::Graphics& g)
{
    if (isSelecting && !selectionBox.isEmpty())
    {
        g.setColour(ColourPalette::primaryBlue.withAlpha(0.2f));
        g.fillRect(selectionBox);

        g.setColour(ColourPalette::primaryBlue.withAlpha(0.8f));
        g.drawRect(selectionBox, 1.0f);
    }
}

void Track::drawDropIndicator(juce::Graphics& g)
{
    if (dropIndicatorX >= 0)
    {
        float localX = dropIndicatorX - container.getViewportX();
        
        if (localX >= 0 && localX <= getWidth())
        {
            g.setColour(juce::Colour(0xff64c864));
            g.drawLine(localX, 0.0f, localX, static_cast<float>(getHeight()), 2.0f);
            
            DBG("Drawing drop indicator at local X: " + juce::String(localX) + 
                " (global: " + juce::String(dropIndicatorX) + ")");
        }
    }
}

// HELPER METHODS (add these)

void Track::adjustGhostClipToTrackBPM()
{
    if (!ghostClip)
        return;
        
    double trackBPM = getTrackBPM();
    ghostClip->duration = ghostClip->duration * (120.0 / trackBPM);
    
    DBG("Adjusted ghost clip to track BPM " + juce::String(trackBPM, 2) + 
        ": duration = " + juce::String(ghostClip->duration, 3) + "s");
}

void Track::inheritBPMFromHeader()
{
    DBG("Track " + juce::String(trackNumber) + " - BPM managed by header");
}

// Inter-track operations
void Track::copySelectedClipsToTrack(Track* targetTrack)
{
    if (!targetTrack)
        return;
    
    auto selectedClips = getSelectedClips();
    double targetBPM = targetTrack->getTrackBPM();
    
    for (auto* clip : selectedClips)
    {
        MidiClip newClip = createClipForTrack(*clip, targetBPM);
        targetTrack->addClip(newClip);
    }
    
    targetTrack->repaint();
}

void Track::moveSelectedClipsToTrack(Track* targetTrack)
{
    if (!targetTrack)
        return;
    
    // First copy the clips
    copySelectedClipsToTrack(targetTrack);
    
    // Then remove them from this track
    removeSelectedClips();
    
    repaint();
}

MidiClip Track::createClipForTrack(const MidiClip& sourceClip, double targetBPM)
{
    MidiClip newClip = sourceClip;
    
    // Recalculate duration based on BPM difference
    double sourceBPM = sourceClip.referenceBPM;
    double bpmRatio = sourceBPM / targetBPM;
    
    // Adjust duration: if target BPM is higher, clip should be shorter
    newClip.duration = sourceClip.duration * bpmRatio;
    newClip.referenceBPM = targetBPM;
    
    // Generate new ID for the copy
    newClip.id = juce::Uuid().toString();
    newClip.isSelected = false;
    
    return newClip;
}

void Track::startExternalDrag()
{
    if (isExternalDragActive)
        return;
        
    isExternalDragActive = true;
    
    std::vector<MidiClip*> selectedClips;
    for (auto& clip : clips)
        if (clip->isSelected)
            selectedClips.push_back(clip.get());
    
    if (selectedClips.empty())
    {
        isExternalDragActive = false;
        return;
    }
    
    double trackBPM = getTrackBPM();
    DBG("Track BPM: " + juce::String(trackBPM, 2));
    
    auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
    auto* dragContainer = editor ? dynamic_cast<juce::DragAndDropContainer*>(editor) : nullptr;
    
    if (!dragContainer)
    {
        isExternalDragActive = false;
        return;
    }
    
    juce::String tempFileName = "DrumGroovePro_track_drag_" + 
        juce::String(juce::Random::getSystemRandom().nextInt64()) + ".mid";
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(tempFileName);
    
    // For single clip - just BPM adjust like GrooveBrowser
    if (selectedClips.size() == 1)
    {
        auto* clip = selectedClips[0];
        
        juce::FileInputStream inputStream(clip->file);
        juce::MidiFile originalMidi;
        if (!inputStream.openedOk() || !originalMidi.readFrom(inputStream))
        {
            isExternalDragActive = false;
            return;
        }
        
        // Get original BPM
        double originalBPM = 120.0;
        for (int t = 0; t < originalMidi.getNumTracks(); ++t)
        {
            auto* track = originalMidi.getTrack(t);
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                if (track->getEventPointer(i)->message.isTempoMetaEvent())
                {
                    originalBPM = 60000000.0 / track->getEventPointer(i)->message.getTempoSecondsPerQuarterNote() / 1000000.0;
                    goto foundBPM;
                }
            }
        }
        foundBPM:
        
        double tempoScale = originalBPM / trackBPM;
        DBG("BPM adjustment: " + juce::String(originalBPM, 2) + " -> " + juce::String(trackBPM, 2) + " (scale: " + juce::String(tempoScale, 4) + ")");
        
        // Create BPM-adjusted MIDI (exactly like GrooveBrowser)
        juce::MidiFile adjustedMidi;
        
        for (int track = 0; track < originalMidi.getNumTracks(); ++track)
        {
            auto* sourceTrack = originalMidi.getTrack(track);
            juce::MidiMessageSequence newTrack;
            
            for (int i = 0; i < sourceTrack->getNumEvents(); ++i)
            {
                auto& midiEvent = sourceTrack->getEventPointer(i)->message;
                double oldTimestamp = sourceTrack->getEventTime(i);
                double newTimestamp = oldTimestamp * tempoScale;
                
                if (midiEvent.isTempoMetaEvent())
                {
                    double microsecondsPerQuarterNote = 60000000.0 / trackBPM;
                    auto tempoEvent = juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerQuarterNote));
                    tempoEvent.setTimeStamp(newTimestamp);
                    newTrack.addEvent(tempoEvent);
                }
                else
                {
                    auto copiedMessage = midiEvent;
                    copiedMessage.setTimeStamp(newTimestamp);
                    newTrack.addEvent(copiedMessage);
                }
            }
            
            newTrack.updateMatchedPairs();
            adjustedMidi.addTrack(newTrack);
        }
        
        adjustedMidi.setTicksPerQuarterNote(originalMidi.getTimeFormat());
        
        juce::FileOutputStream outputStream(tempFile);
        if (!outputStream.openedOk() || !adjustedMidi.writeTo(outputStream))
        {
            isExternalDragActive = false;
            return;
        }
        outputStream.flush();
    }
    else
    {
        // Multiple clips - combine with BPM adjustment
        juce::MidiFile combinedMidi;
        combinedMidi.setTicksPerQuarterNote(480);
        
        double earliestStartTime = std::numeric_limits<double>::max();
        for (auto* clip : selectedClips)
            earliestStartTime = juce::jmin(earliestStartTime, clip->startTime);
        
        juce::Array<juce::MidiMessageSequence> finalTracks;
        
        for (auto* clip : selectedClips)
        {
            if (!clip->file.existsAsFile()) continue;
                
            juce::FileInputStream inputStream(clip->file);
            juce::MidiFile originalMidi;
            if (!inputStream.openedOk() || !originalMidi.readFrom(inputStream)) continue;
            
            double originalBPM = 120.0;
            for (int t = 0; t < originalMidi.getNumTracks(); ++t)
            {
                auto* track = originalMidi.getTrack(t);
                for (int i = 0; i < track->getNumEvents(); ++i)
                {
                    if (track->getEventPointer(i)->message.isTempoMetaEvent())
                    {
                        originalBPM = 60000000.0 / track->getEventPointer(i)->message.getTempoSecondsPerQuarterNote() / 1000000.0;
                        goto foundBPM2;
                    }
                }
            }
            foundBPM2:
            
            double tempoScale = originalBPM / trackBPM;
            double relativeStartTime = clip->startTime - earliestStartTime;
            double offsetTicks = relativeStartTime * 480.0 * (trackBPM / 60.0);
            
            for (int trackNum = 0; trackNum < originalMidi.getNumTracks(); ++trackNum)
            {
                auto* sourceTrack = originalMidi.getTrack(trackNum);
                
                while (trackNum >= finalTracks.size())
                    finalTracks.add(juce::MidiMessageSequence());
                
                for (int i = 0; i < sourceTrack->getNumEvents(); ++i)
                {
                    auto& event = sourceTrack->getEventPointer(i)->message;
                    double adjustedTime = (sourceTrack->getEventTime(i) * tempoScale) + offsetTicks;
                    
                    if (event.isTempoMetaEvent())
                    {
                        double microsecondsPerQuarterNote = 60000000.0 / trackBPM;
                        auto newEvent = juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerQuarterNote));
                        newEvent.setTimeStamp(adjustedTime);
                        finalTracks.getReference(trackNum).addEvent(newEvent);
                    }
                    else
                    {
                        auto newEvent = event;
                        newEvent.setTimeStamp(adjustedTime);
                        finalTracks.getReference(trackNum).addEvent(newEvent);
                    }
                }
            }
        }
        
        for (auto& track : finalTracks)
        {
            track.updateMatchedPairs();
            combinedMidi.addTrack(track);
        }
        
        juce::FileOutputStream outputStream(tempFile);
        if (!outputStream.openedOk() || !combinedMidi.writeTo(outputStream))
        {
            isExternalDragActive = false;
            return;
        }
        outputStream.flush();
    }
    
    juce::Thread::sleep(50);
    
    if (!tempFile.existsAsFile() || tempFile.getSize() == 0)
    {
        isExternalDragActive = false;
        return;
    }
    
    if (lastTempDragFile.existsAsFile())
        lastTempDragFile.deleteFile();
    lastTempDragFile = tempFile;
    
    juce::StringArray files;
    files.add(tempFile.getFullPathName());
    
    dragContainer->performExternalDragDropOfFiles(files, true, this, [this, tempFile]()
    {
        isExternalDragActive = false;
        juce::Timer::callAfterDelay(3000, [tempFile]()
        {
            if (tempFile.existsAsFile())
                tempFile.deleteFile();
        });
    });
}