#include "Track.h"
#include "TrackHeader.h"
#include "MultiTrackContainer.h"
#include "../../Utils/TimelineUtils.h"
#include "../../Core/MidiDissector.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../LookAndFeel/ColourPalette.h"
#include "GrooveBrowser.h"
#include "../../PluginEditor.h"


// ===== UNDO/REDO COMMAND CLASSES =====
class TrackAddClipCommand : public TrackCommand
{
public:
    TrackAddClipCommand(Track* t, const MidiClip& c)
    : track(t), clip(c) {}

    void execute() override {
        track->clips.push_back(std::make_unique<MidiClip>(clip));
        track->container.updateTimelineSize();
        track->repaint();
    }

    void undo() override {
        track->clips.erase(
            std::remove_if(track->clips.begin(), track->clips.end(),
                           [this](const std::unique_ptr<MidiClip>& c) {
                               return c->id == clip.id;
                           }),
                           track->clips.end());
        track->container.updateTimelineSize();
        track->repaint();
    }

private:
    Track* track;
    MidiClip clip;
};

class TrackDeleteClipsCommand : public TrackCommand
{
public:
    TrackDeleteClipsCommand(Track* t, const std::vector<MidiClip>& clips)
    : track(t), deletedClips(clips) {}

    void execute() override {
        for (const auto& clip : deletedClips) {
            // Notify MidiProcessor to clear this clip
            track->processor.midiProcessor.clearClip(clip.id);

            track->clips.erase(
                std::remove_if(track->clips.begin(), track->clips.end(),
                               [&clip](const std::unique_ptr<MidiClip>& c) {
                                   return c->id == clip.id;
                               }),
                               track->clips.end());
        }
        track->container.updateTimelineSize();
        track->repaint();
    }

    void undo() override {
        for (const auto& clip : deletedClips) {
            track->clips.push_back(std::make_unique<MidiClip>(clip));
        }
        track->container.updateTimelineSize();
        track->repaint();
    }

private:
    Track* track;
    std::vector<MidiClip> deletedClips;
};

class TrackMoveClipsCommand : public TrackCommand
{
public:
    TrackMoveClipsCommand(Track* t,
                          const std::vector<std::pair<juce::String, double>>& oldPos,
                          const std::vector<std::pair<juce::String, double>>& newPos)
    : track(t), oldPositions(oldPos), newPositions(newPos) {}

    void execute() override {
        // Apply new positions
        for (const auto& [id, newTime] : newPositions) {
            for (auto& clip : track->clips) {
                if (clip->id == id) {
                    clip->startTime = newTime;
                    break;
                }
            }
        }
        track->container.updateTimelineSize();
        track->repaint();
    }

    void undo() override {
        // Restore old positions
        for (const auto& [id, oldTime] : oldPositions) {
            for (auto& clip : track->clips) {
                if (clip->id == id) {
                    clip->startTime = oldTime;
                    break;
                }
            }
        }
        track->container.updateTimelineSize();
        track->repaint();
    }

private:
    Track* track;
    std::vector<std::pair<juce::String, double>> oldPositions;
    std::vector<std::pair<juce::String, double>> newPositions;
};

class TrackResizeClipCommand : public TrackCommand
{
public:
    TrackResizeClipCommand(Track* t, const juce::String& id,
                           double oldStart, double oldDur,
                           double newStart, double newDur)
    : track(t), clipId(id),
    oldStartTime(oldStart), oldDuration(oldDur),
    newStartTime(newStart), newDuration(newDur) {}

    void execute() override {
        for (auto& clip : track->clips) {
            if (clip->id == clipId) {
                clip->startTime = newStartTime;
                clip->duration = newDuration;
                break;
            }
        }
        track->repaint();
    }

    void undo() override {
        for (auto& clip : track->clips) {
            if (clip->id == clipId) {
                clip->startTime = oldStartTime;
                clip->duration = oldDuration;
                break;
            }
        }
        track->repaint();
    }

private:
    Track* track;
    juce::String clipId;
    double oldStartTime;
    double oldDuration;
    double newStartTime;
    double newDuration;
};
// ===== END UNDO/REDO COMMAND CLASSES =====


Track::Track(DrumGrooveProcessor& p, MultiTrackContainer& c, int trackNum)
: processor(p), container(c), trackNumber(trackNum)
{
    setWantsKeyboardFocus(true);
    addKeyListener(this);
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
    // Background - transparent to show drum background
    g.fillAll(ColourPalette::secondaryBackground.withAlpha(0.6f));  // Reduced from 0.8f to see drums better

    // Right separator line (red for debug, should be removed or changed)
    g.setColour(juce::Colours::red.withAlpha(0.5f));
    g.drawLine(static_cast<float>(getWidth() - 1), 0.0f,
               static_cast<float>(getWidth() - 1), static_cast<float>(getHeight()), 2.0f);

    // Draw clips
    drawClips(g);

    if (ghostClip)
        drawGhostClip(g);

    if (isSelecting)
        drawSelectionBox(g);

    drawDropIndicator(g);

    // CRITICAL: Draw gray overlay AFTER clips for mute/solo visual feedback
    // Check if track should be visually disabled (muted or inactive due to solo)
    bool isVisuallyDisabled = isMuted();
    
    // Check if inactive due to another track being soloed
    if (!isSoloed())
    {
        for (int i = 0; i < container.getNumTracks(); ++i)
        {
            if (i != (trackNumber - 1) && container.isTrackSoloed(i))
            {
                isVisuallyDisabled = true;
                break;
            }
        }
    }
    
    // Draw gray overlay ON TOP of everything when track is disabled
    if (isVisuallyDisabled)
    {
        g.fillAll(ColourPalette::mainBackground.withAlpha(0.7f));  // Strong dark overlay on top
    }

    // Bottom separator line
    g.setColour(ColourPalette::separator);
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1),
               static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1));
}


void Track::resized()
{
}

void Track::mouseDown(const juce::MouseEvent& e)
{
    // Grab keyboard focus so Ctrl+Z works
    grabKeyboardFocus();

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
        return;
    }

    auto* clip = getClipAt(e.position);

    if (clip)
    {
        // CRITICAL FIX: Use absolute position for resize handle detection
        float clipX = static_cast<float>(clip->startTime * container.getZoom());
        double trackBPM = getTrackBPM();
        double visualScaleFactor = clip->referenceBPM / trackBPM;
        float clipWidth = static_cast<float>(clip->duration * container.getZoom() * visualScaleFactor);
        float clipEndX = clipX + clipWidth;

        if (std::abs(e.position.x - clipEndX) < RESIZE_HANDLE_WIDTH)
        {
            isResizing = true;
            resizingClip = clip;
            resizeStartDuration = clip->duration;
        }
        else if (std::abs(e.position.x - clipX) < RESIZE_HANDLE_WIDTH)
        {
            isResizingLeft = true;
            resizingClip = clip;
            resizeStartTime = clip->startTime;
            resizeStartDuration = clip->duration;
        }
        else
        {
            if (!e.mods.isShiftDown())
            {
                container.deselectAllClips();
            }

            clip->isSelected = !clip->isSelected;

            if (clip->isSelected)
            {
                isDragging = true;
                dragStartPoint = e.position;

                draggedClips.clear();
                for (const auto& c : clips)
                {
                    if (c->isSelected)
                        draggedClips.push_back({c->id, c->startTime});
                }
            }

            // Always trigger file path display callback when clicking a clip
            if (onClipSelected && clip->file.existsAsFile())
                onClipSelected(clip->file);
        }

        repaint();
    }
    else
    {
        if (!e.mods.isShiftDown())
        {
            container.deselectAllClips();
        }

        isSelecting = true;
        dragStartPoint = e.position;
        selectionBox = juce::Rectangle<float>();
        repaint();
    }
}

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
                    uint8_t noteNumber = static_cast<uint8_t>(event.getNoteNumber());
                    DrumLibrary sourceLib = clip.sourceLibrary;

                    noteColour = MidiDissector::getColourForNote(
                        noteNumber,
                        sourceLib,
                        &processor.drumLibraryManager).brighter(0.3f);
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

    // CRITICAL: Detect source library from file path
    DrumLibrary sourceLib = DrumLibrary::Unknown;
    auto& library = processor.drumLibraryManager;

    for (int i = 0; i < library.getNumRootFolders(); ++i)
    {
        auto rootFolder = library.getRootFolder(i);
        if (file.getFullPathName().startsWith(rootFolder.getFullPathName()))
        {
            sourceLib = library.getRootFolderSourceLibrary(i);
            DBG("Detected source library: " + DrumLibraryManager::getLibraryName(sourceLib));
            break;
        }
    }

    // Read the original BPM from the MIDI file
    double originalBPM = 120.0;
    {
        juce::FileInputStream bpmStream(file);
        if (bpmStream.openedOk())
        {
            juce::MidiFile tempMidiFile;
            if (tempMidiFile.readFrom(bpmStream))
            {
                for (int t = 0; t < tempMidiFile.getNumTracks(); ++t)
                {
                    const auto* track = tempMidiFile.getTrack(t);
                    if (!track) continue;

                    for (int e = 0; e < track->getNumEvents(); ++e)
                    {
                        const auto* event = track->getEventPointer(e);
                        if (event && event->message.isTempoMetaEvent())
                        {
                            originalBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                            break;
                        }
                    }
                    if (originalBPM != 120.0) break;
                }
            }
        }
    }

    // NEW: Extract header BPM from drag description if available (from GrooveBrowser)
    // Format: filename|fullPath|headerBPM
    double headerBPM = originalBPM;  // Default to file's BPM if not provided
    if (parts.size() >= 3)
    {
        headerBPM = parts[2].getDoubleValue();
        if (headerBPM > 0)
        {
            DBG("Using header BPM from GrooveBrowser: " + juce::String(headerBPM, 2));
        }
        else
        {
            headerBPM = originalBPM;  // Fallback if parse fails
        }
    }

    // Create new clip
    MidiClip newClip;
    newClip.name = filename;
    newClip.file = file;
    newClip.startTime = dropTime;
    newClip.colour = ColourPalette::primaryBlue.withAlpha(0.7f);

    // Store the MIDI file's original BPM and header BPM for inheritance
    newClip.originalBPM = originalBPM;
    newClip.referenceBPM = originalBPM;
    newClip.sourceLibrary = sourceLib;


    // Store header BPM in a temporary variable for inheritance
    newClip.headerBPM = headerBPM;

    // Calculate actual duration from MIDI file
    double duration = 4.0;
    if (calculateMidiFileDuration(file, duration))
    {
        newClip.duration = duration;
        DBG("Calculated MIDI duration: " + juce::String(duration, 3) + "s at " +
        juce::String(originalBPM, 2) + " BPM");
    }
    else
    {
        newClip.duration = 4.0;
        DBG("Failed to calculate duration, using default 4.0s");
    }

    DBG("Dropped MIDI: " + filename);
    DBG("  Original BPM: " + juce::String(originalBPM, 2));
    DBG("  Header BPM: " + juce::String(headerBPM, 2));
    DBG("  Duration: " + juce::String(newClip.duration, 3) + "s");
    DBG("  Source Library: " + DrumLibraryManager::getLibraryName(sourceLib));

    clips.push_back(std::make_unique<MidiClip>(newClip));

    // Add clip to MidiProcessor if playing
    if (container.isPlaying())
    {
        double trackBPM = getTrackBPM();
        processor.midiProcessor.addMidiClip(
            file,
            dropTime,
            sourceLib,
            originalBPM,
            trackBPM,
            trackNumber,
            newClip.duration,
            newClip.id
        );

        DBG("Added to MidiProcessor - Original: " + juce::String(originalBPM, 2) +
        " BPM, Track: " + juce::String(trackBPM, 2) + " BPM");
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

    // FIXED: Read the original BPM from the dissected MIDI file
    double originalBPM = 120.0;
    {
        juce::FileInputStream bpmStream(outputFile);
        if (bpmStream.openedOk())
        {
            juce::MidiFile tempMidiFile;
            if (tempMidiFile.readFrom(bpmStream))
            {
                for (int t = 0; t < tempMidiFile.getNumTracks(); ++t)
                {
                    const auto* track = tempMidiFile.getTrack(t);
                    if (!track) continue;

                    for (int e = 0; e < track->getNumEvents(); ++e)
                    {
                        const auto* event = track->getEventPointer(e);
                        if (event && event->message.isTempoMetaEvent())
                        {
                            originalBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                            break;
                        }
                    }
                    if (originalBPM != 120.0) break;
                }
            }
        }
    }

    MidiClip newClip;
    newClip.name = partName;
    newClip.file = outputFile;
    newClip.startTime = dropTime;
    newClip.colour = MidiDissector::getPartColour(partType).withAlpha(0.7f);

    // FIXED: Store the MIDI file's original BPM and source library
    newClip.originalBPM = originalBPM;
    newClip.referenceBPM = originalBPM;  // Reference is the original BPM, not track BPM
    newClip.sourceLibrary = sourceLib;  // NEW: Store source library from drum part

    DBG("Dropped drum part: " + partName + " - Original BPM: " + juce::String(originalBPM, 2) +
    ", Source Library: " + DrumLibraryManager::getLibraryName(sourceLib));

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

    // ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¦ CRITICAL FIX: Add clip to MidiProcessor immediately if playing
    if (container.isPlaying())
    {
        double trackBPM = getTrackBPM();
        processor.midiProcessor.addMidiClip(
            outputFile,
            dropTime,
            sourceLib,
            originalBPM,
            trackBPM,
            trackNumber,
            newClip.duration,
            newClip.id
        );

        DBG("Added drum part to MidiProcessor - Original: " + juce::String(originalBPM, 2) +
        " BPM, Track: " + juce::String(trackBPM, 2) + " BPM");
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

    // CRITICAL FIX: Read the tempo from the original file BEFORE dissection
    double originalBPM = 120.0;
    {
        juce::FileInputStream bpmStream(originalFile);
        if (bpmStream.openedOk())
        {
            juce::MidiFile tempMidiFile;
            if (tempMidiFile.readFrom(bpmStream))
            {
                for (int t = 0; t < tempMidiFile.getNumTracks(); ++t)
                {
                    const auto* track = tempMidiFile.getTrack(t);
                    if (!track) continue;

                    for (int e = 0; e < track->getNumEvents(); ++e)
                    {
                        const auto* event = track->getEventPointer(e);
                        if (event && event->message.isTempoMetaEvent())
                        {
                            originalBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                            DBG("Found original BPM: " + juce::String(originalBPM, 2));
                            break;
                        }
                    }
                    if (originalBPM != 120.0) break;
                }
            }
        }
    }

    MidiDissector dissector;
    DrumLibrary targetLib = DrumLibrary::GeneralMIDI; // Default
	auto* grooveBrowser = processor.getGrooveBrowser();
	if (grooveBrowser)
	{
		targetLib = grooveBrowser->getCurrentTargetLibrary();
		DBG("Using target library from GrooveBrowser: " + juce::String(static_cast<int>(targetLib)) + 
			" (" + DrumLibraryManager::getLibraryName(targetLib) + ")");
	}
	else
	{
		// Fallback if GrooveBrowser not available (shouldn't happen in normal use)
		targetLib = processor.getTargetLibrary();
		DBG("WARNING: GrooveBrowser not available, using parameter fallback: " + 
			juce::String(static_cast<int>(targetLib)));
	}

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

            // CRITICAL: Get original file's TPQN for proper tick conversion
            int originalTPQN = 480;  // Default
            {
                juce::FileInputStream inputStream(originalFile);
                if (inputStream.openedOk())
                {
                    juce::MidiFile tempMidi;
                    if (tempMidi.readFrom(inputStream))
                    {
                        originalTPQN = tempMidi.getTimeFormat();
                        DBG("Original MIDI file TPQN: " + juce::String(originalTPQN));
                    }
                }
            }

            juce::MidiFile midiFileToSave;
            int newTPQN = 480;
            midiFileToSave.setTicksPerQuarterNote(newTPQN);

            juce::MidiMessageSequence trackCopy;

			// CRITICAL FIX: Add tempo meta event FIRST to preserve original BPM
			double secondsPerQuarterNote = 60.0 / originalBPM;
			juce::MidiMessage tempoEvent = juce::MidiMessage::tempoMetaEvent(
				static_cast<int>(secondsPerQuarterNote * 1000000.0)
			);
			tempoEvent.setTimeStamp(0.0);  // Add at the beginning
			trackCopy.addEvent(tempoEvent, 0.0);
			DBG("Added tempo meta event: " + juce::String(originalBPM, 2) + " BPM");
			
			// CRITICAL FIX: Scale timestamps from original TPQN to new TPQN
			// The dissected part.sequence has timestamps in the ORIGINAL file's tick format
			// We need to scale them to match our new TPQN (480)
			// Formula: newTicks = oldTicks * (newTPQN / originalTPQN)
			double tickScaleFactor = static_cast<double>(newTPQN) / static_cast<double>(originalTPQN);
			
			DBG("Tick scale factor: " + juce::String(tickScaleFactor, 4) + 
			    " (from " + juce::String(originalTPQN) + " to " + juce::String(newTPQN) + " TPQN)");
			
			// Copy all note events from the dissected part with tick scaling
			for (int i = 0; i < part.sequence.getNumEvents(); ++i)
			{
				auto originalEvent = part.sequence.getEventPointer(i)->message;
				double oldTicks = originalEvent.getTimeStamp();
				double newTicks = oldTicks * tickScaleFactor;
				
				// Create new message with scaled timestamp
				auto convertedEvent = originalEvent;
				convertedEvent.setTimeStamp(newTicks);
				
				trackCopy.addEvent(convertedEvent);
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

    // Read BPM and time signature
    double midiFileBPM = 120.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* track = const_cast<juce::MidiMessageSequence*>(midiFile.getTrack(t));
        if (!track) continue;

        track->updateMatchedPairs();

        for (int e = 0; e < track->getNumEvents(); ++e)
        {
            const auto* event = track->getEventPointer(e);
            if (!event) continue;

            if (event->message.isTempoMetaEvent())
            {
                midiFileBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
            }
            else if (event->message.isTimeSignatureMetaEvent())
            {
                event->message.getTimeSignatureInfo(timeSignatureNumerator, timeSignatureDenominator);
            }
        }
    }

    // Find max time across ALL tracks and ALL events (including note-offs)
    double maxTimeInTicks = 0;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* track = const_cast<juce::MidiMessageSequence*>(midiFile.getTrack(t));
        if (!track) continue;

        track->updateMatchedPairs();

        for (int e = 0; e < track->getNumEvents(); ++e)
        {
            double eventTime = track->getEventTime(e);
            maxTimeInTicks = juce::jmax(maxTimeInTicks, eventTime);

            auto* eventHolder = track->getEventPointer(e);
            if (eventHolder && eventHolder->noteOffObject != nullptr)
            {
                double noteOffTime = eventHolder->noteOffObject->message.getTimeStamp();
                maxTimeInTicks = juce::jmax(maxTimeInTicks, noteOffTime);
            }
        }
    }

    // Calculate ticks per bar
    double ticksPerBar = ticksPerQuarterNote * (4.0 / timeSignatureDenominator) * timeSignatureNumerator;

    // Round UP to the nearest complete bar
    double numBars = std::ceil(maxTimeInTicks / ticksPerBar);
    double roundedTicks = numBars * ticksPerBar;

    // Convert to seconds
    duration = (roundedTicks / ticksPerQuarterNote) * (60.0 / midiFileBPM);

    return duration > 0;
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
    // In TIME mode, use zoom-based time intervals
    if (!container.isBarMode())
    {
        // Get current zoom level from container
        float zoomLevel = container.getZoomLevel();

        double gridInterval;
        if (zoomLevel >= 300.0f)
        {
            gridInterval = 0.01; // 10ms at 300% zoom or higher
        }
        else if (zoomLevel >= 200.0f)
        {
            gridInterval = 0.05; // 50ms at 200% zoom
        }
        else
        {
            gridInterval = 0.1; // 100ms at 100% zoom (default)
        }

        return std::round(time / gridInterval) * gridInterval;
    }

    // In BAR mode, use section-aware, DIV-based snapping
    auto& sectionMgr = processor.sectionManager;

    // Find which section this time falls into
    const Section* section = sectionMgr.getSectionAtTime(time, container.getMasterBPM());
    if (!section)
    {
        DBG("ERROR: No section found at time " + juce::String(time));
        return time; // No section, return unsnapped
    }
    DBG("Snap: time=" + juce::String(time, 3) +
    ", section bars=" + juce::String(section->numBars) +
    ", numerator=" + juce::String(section->numerator));

    // Get track's DIV setting from header
    auto* header = container.getTrackHeader(trackNumber - 1);
    if (!header)
        return time; // No header, return unsnapped

        NoteDivision div = header->getNoteDivision();
    int divDenominator = static_cast<int>(div); // 4, 8, 16, 32, or 128

    // Calculate snap parameters
    double sectionBPM = (section->bpm > 0.0) ? section->bpm : container.getMasterBPM();
    double secondsPerBeat = 60.0 / sectionBPM;
    double secondsPerBar = secondsPerBeat * section->numerator;

    // Formula: Snap Points per Bar = Beats per Bar × (DIV denominator ÷ Time Signature denominator)
    double snapPointsPerBar = section->numerator * (static_cast<double>(divDenominator) / section->denominator);
    double secondsPerSnap = secondsPerBar / snapPointsPerBar;
	
    
    // Find section start time
    double sectionStartTime = sectionMgr.getSectionStartTime(
        sectionMgr.getSectionIndex(section)
    );
    
    // Calculate position within section
    double timeInSection = time - sectionStartTime;
    
    // Snap to nearest grid point within section
    int snapIndex = static_cast<int>(std::round(timeInSection / secondsPerSnap));
    snapIndex = juce::jmax(0, snapIndex);
    
    return sectionStartTime + (snapIndex * secondsPerSnap);
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
        // CRITICAL FIX: Use absolute position
        float clipX = static_cast<float>(clip->startTime * container.getZoom());
        double trackBPM = getTrackBPM();
        double visualScaleFactor = clip->referenceBPM / trackBPM;
        float width = static_cast<float>(clip->duration * container.getZoom() * visualScaleFactor);

        if (point.x >= clipX && point.x <= clipX + width)
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

void Track::mouseDrag(const juce::MouseEvent& e)
{
    //  CTRL+ALT drag = External drag to DAW with track BPM
    if (e.mods.isCtrlDown() && e.mods.isAltDown())
    {
        if (e.getDistanceFromDragStart() > 10 && !isExternalDragActive)
        {
            // Check if we have selected clips
            auto selectedClips = getSelectedClips();
            if (!selectedClips.empty())
            {
                startExternalDrag();
                return;
            }
        }
        return;  // Don't process other drag logic when CTRL+ALT is pressed
    }

    if (isResizing && resizingClip)
    {
        float mouseX = static_cast<float>(e.position.x);
        float clipStartX = static_cast<float>(resizingClip->startTime * container.getZoom());
        float newWidth = mouseX - clipStartX;

        double trackBPM = getTrackBPM();
        double visualScaleFactor = resizingClip->referenceBPM / trackBPM;

        double newDuration = (newWidth / container.getZoom()) / visualScaleFactor;
        newDuration = juce::jmax(0.1, newDuration);

        if (!e.mods.isAltDown())
            newDuration = container.snapToGrid(newDuration);

        resizingClip->duration = newDuration;

        if (container.isPlaying())
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );

            DBG("Updated clip duration in real-time: " + resizingClip->name);
        }

        repaint();
    }
    else if (isResizingLeft && resizingClip)
    {
        float mouseX = static_cast<float>(e.position.x);
        double newStartTime = mouseX / container.getZoom();

        if (!e.mods.isAltDown())
            newStartTime = container.snapToGrid(newStartTime);

        double endTime = resizeStartTime + resizeStartDuration;
        newStartTime = juce::jmin(newStartTime, endTime - 0.1);
        newStartTime = juce::jmax(0.0, newStartTime);

        resizingClip->startTime = newStartTime;
        resizingClip->duration = endTime - newStartTime;

        if (container.isPlaying())
        {
            double trackBPM = getTrackBPM();
            double visualScaleFactor = resizingClip->referenceBPM / trackBPM;

            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );

            DBG("Updated clip position in real-time: " + resizingClip->name);
        }

        repaint();
    }
    else if (isDragging && !draggedClips.empty())
    {
        float deltaX = e.position.x - dragStartPoint.x;
        double deltaTime = deltaX / container.getZoom();

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

                        if (container.isPlaying())
                        {
                            processor.midiProcessor.updateClipBoundaries(
                                clip->id,
                                clip->startTime,
                                clip->duration
                            );
                        }

                        break;
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
            // CRITICAL FIX: Use absolute position for selection
            float clipX = static_cast<float>(clip->startTime * container.getZoom());
            double trackBPM = getTrackBPM();
            double visualScaleFactor = clip->referenceBPM / trackBPM;
            float clipWidth = static_cast<float>(clip->duration * container.getZoom() * visualScaleFactor);

            juce::Rectangle<float> clipBounds(clipX, 10.0f, clipWidth, TRACK_HEIGHT - 20.0f);

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

    // Record undo for resizing (right handle)
    if (isResizing && resizingClip)
    {
        resizingClip->duration = juce::jmax(0.1, resizingClip->duration);
        resizingClip->duration = snapToGrid(resizingClip->duration);

        // Check if duration actually changed
        if (std::abs(resizingClip->duration - resizeStartDuration) > 0.001)
        {
            addUndoCommand(std::make_unique<TrackResizeClipCommand>(
                this, resizingClip->id,
                resizingClip->startTime, resizeStartDuration,  // old values
                resizingClip->startTime, resizingClip->duration),  // new values
                false);  // Don't execute - already applied during drag
        }

        // Update MidiProcessor
        if (container.isPlaying())
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );
            DBG("Updated clip duration in real-time: " + resizingClip->name);
        }

        isResizing = false;
        resizingClip = nullptr;
    }
    // Record undo for resizing (left handle - changes both position and duration)
    else if (isResizingLeft && resizingClip)
    {
        resizingClip->startTime = juce::jmax(0.0, resizingClip->startTime);
        resizingClip->duration = juce::jmax(0.1, resizingClip->duration);

        resizingClip->startTime = snapToGrid(resizingClip->startTime);
        resizingClip->duration = snapToGrid(resizingClip->duration);

        // Check if position or duration actually changed
        if (std::abs(resizingClip->startTime - resizeStartTime) > 0.001 ||
            std::abs(resizingClip->duration - resizeStartDuration) > 0.001)
        {
            addUndoCommand(std::make_unique<TrackResizeClipCommand>(
                this, resizingClip->id,
                resizeStartTime, resizeStartDuration,  // old values
                resizingClip->startTime, resizingClip->duration),  // new values
                false);  // Don't execute - already applied during drag
        }

        // Update MidiProcessor
        if (container.isPlaying())
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );
            DBG("Updated clip left resize in real-time: " + resizingClip->name);
        }

        isResizingLeft = false;
        resizingClip = nullptr;
    }
    // Record undo for moving clips
    else if (isDragging && !draggedClips.empty())
    {
        // Snap final positions
        for (auto& clip : clips)
        {
            if (clip->isSelected)
            {
                clip->startTime = snapToGrid(clip->startTime);

                // Update MidiProcessor if playing
                if (container.isPlaying())
                {
                    processor.midiProcessor.updateClipBoundaries(
                        clip->id,
                        clip->startTime,
                        clip->duration
                    );
                    DBG("Updated clip position in real-time: " + clip->name);
                }
            }
        }

        // Check if any clips actually moved
        std::vector<std::pair<juce::String, double>> oldPositions;
        std::vector<std::pair<juce::String, double>> newPositions;
        bool moved = false;

        for (const auto& clip : clips)
        {
            if (clip->isSelected)
            {
                for (const auto& [id, originalTime] : draggedClips)
                {
                    if (id == clip->id && std::abs(clip->startTime - originalTime) > 0.001)
                    {
                        oldPositions.push_back({id, originalTime});  // Save ORIGINAL position
                        newPositions.push_back({id, clip->startTime});  // Save NEW position
                        moved = true;
                        break;
                    }
                }
            }
        }

        if (moved)
        {
            addUndoCommand(std::make_unique<TrackMoveClipsCommand>(this, oldPositions, newPositions), false);
        }

        isDragging = false;
        draggedClips.clear();
    }
    // Handle track-to-track drag
    else if (!draggedClips.empty())
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

    // Clean up all drag states
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

void Track::mouseDoubleClick(const juce::MouseEvent& event)
{
    // Check if double-clicked on a MIDI clip
    auto* clip = getClipAt(event.position);

    if (clip && clip->file.existsAsFile())
    {
        // Open MIDI editor
        auto* editor = findParentComponentOfClass<DrumGrooveEditor>();
        if (editor)
        {
            editor->openMidiEditor(clip->file, clip->sourceLibrary);
        }
    }
}

void Track::mouseMove(const juce::MouseEvent& e)
{
    auto trackArea = getTrackArea();
    if (!trackArea.contains(e.getPosition()))
        return;

    auto* clip = getClipAt(e.position);
    if (clip)
    {
        // CRITICAL FIX: Use absolute position for cursor detection
        float clipX = static_cast<float>(clip->startTime * container.getZoom());
        double trackBPM = getTrackBPM();
        double visualScaleFactor = clip->referenceBPM / trackBPM;
        float clipWidth = static_cast<float>(clip->duration * container.getZoom() * visualScaleFactor);
        float clipEndX = clipX + clipWidth;

        if (std::abs(e.position.x - clipEndX) < RESIZE_HANDLE_WIDTH ||
            std::abs(e.position.x - clipX) < RESIZE_HANDLE_WIDTH)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
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
    double fileBPM = 120.0;
    juce::File midiFile;

    // ====================================================================
    // FIX: Handle dissected parts correctly
    // ====================================================================
    if (parts.size() >= 2 && parts[1] == "PART")
    {
        // This is a dissected drum part
        // Format: partName|PART|originalFilePath|partType|sourceLibrary

        if (parts.size() >= 3)
        {
            juce::File originalFile(parts[2]);

            if (originalFile.existsAsFile())
            {
                // Get the original file's BPM
                fileBPM = getBPMFromMidiFile(originalFile);

                // CRITICAL: We need to create the temp MIDI file to get accurate duration
                // But we can't do that here without the part type info
                // So we'll use a reasonable estimate based on typical drum part lengths

                // Read the part type to estimate duration
                if (parts.size() >= 4)
                {
                    int partTypeInt = parts[3].getIntValue();
                    DrumPartType partType = static_cast<DrumPartType>(partTypeInt);

                    // Estimate duration based on part type
                    // Most drum parts are 1-4 bars at the original BPM
                    baseDuration = 2.0; // Default 2 bars worth at 120 BPM

                    // Calculate more accurate duration by reading the original file
                    juce::FileInputStream inputStream(originalFile);
                    if (inputStream.openedOk())
                    {
                        juce::MidiFile originalMidi;
                        if (originalMidi.readFrom(inputStream))
                        {
                            double ticksPerQuarterNote = originalMidi.getTimeFormat();
                            if (ticksPerQuarterNote <= 0)
                                ticksPerQuarterNote = 480.0;

                            // Find max time for this part type
                            double maxTimeInTicks = 0;
                            for (int t = 0; t < originalMidi.getNumTracks(); ++t)
                            {
                                auto* track = const_cast<juce::MidiMessageSequence*>(originalMidi.getTrack(t));
                                if (!track) continue;

                                for (int e = 0; e < track->getNumEvents(); ++e)
                                {
                                    const auto* event = track->getEventPointer(e);
                                    if (!event || !event->message.isNoteOn()) continue;

                                    // Check if this note belongs to the part type
                                    int noteNumber = event->message.getNoteNumber();
                                    DrumLibrary sourceLib = DrumLibrary::GeneralMIDI;
                                    if (parts.size() >= 5)
                                    {
                                        sourceLib = static_cast<DrumLibrary>(parts[4].getIntValue());
                                    }

                                    DrumPartType notePartType = MidiDissector::getPartTypeFromNote(noteNumber, sourceLib);
                                    if (notePartType == partType)
                                    {
                                        double eventTime = event->message.getTimeStamp();
                                        maxTimeInTicks = juce::jmax(maxTimeInTicks, eventTime);
                                    }
                                }
                            }

                            if (maxTimeInTicks > 0)
                            {
                                // Round up to complete bars
                                double ticksPerBar = ticksPerQuarterNote * 4.0; // Assume 4/4
                                double numBars = std::ceil(maxTimeInTicks / ticksPerBar);
                                double roundedTicks = numBars * ticksPerBar;

                                // Calculate duration at file's BPM
                                baseDuration = (roundedTicks / ticksPerQuarterNote) * (60.0 / fileBPM);
                            }
                        }
                    }
                }

                DBG("Ghost MIDI for drum part - Duration: " + juce::String(baseDuration, 3) +
                "s at " + juce::String(fileBPM, 2) + " BPM");
            }
        }
    }
    else if (parts.size() >= 2)
    {
        // Regular MIDI file
        midiFile = juce::File(parts[1]);
        if (midiFile.existsAsFile())
        {
            if (!calculateMidiFileDuration(midiFile, baseDuration))
                baseDuration = 4.0;

            // Read the actual BPM from the MIDI file
            fileBPM = getBPMFromMidiFile(midiFile);
        }
    }

    // Set the ghost clip's BPM to the file's actual BPM
    ghostClip->originalBPM = fileBPM;
    ghostClip->referenceBPM = fileBPM;
    ghostClip->duration = baseDuration;

    // The visual scaling will be applied in drawing code based on track BPM
    ghostClip->colour = ColourPalette::primaryBlue.withAlpha(0.3f);

    DBG("Ghost clip entered track " + juce::String(trackNumber) +
    " - Duration: " + juce::String(ghostClip->duration, 3) + "s" +
    " at " + juce::String(fileBPM, 2) + " BPM");

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

double Track::getBPMFromMidiFile(const juce::File& file) const
{
    if (!file.existsAsFile())
        return 120.0;

    juce::FileInputStream inputStream(file);
    if (!inputStream.openedOk())
        return 120.0;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(inputStream))
        return 120.0;

    // Read BPM from tempo events
    double bpm = 120.0;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack(t);
        if (!track) continue;

        for (int e = 0; e < track->getNumEvents(); ++e)
        {
            const auto* event = track->getEventPointer(e);
            if (event && event->message.isTempoMetaEvent())
            {
                bpm = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                break;
            }
        }
        if (bpm != 120.0) break;
    }

    return bpm;
}

void Track::itemDragExit(const SourceDetails&)
{
    dropIndicatorX = -1;
    ghostClip.reset();
    repaint();
}

// DRAWING METHODS

void Track::drawClips(juce::Graphics& g)
{
    double trackBPM = getTrackBPM();
    float zoom = container.getZoom();

    // Check if track should be visually disabled (muted or inactive due to solo)
    bool isVisuallyDisabled = isMuted();
    
    // Check if inactive due to another track being soloed
    if (!isSoloed())
    {
        for (int i = 0; i < container.getNumTracks(); ++i)
        {
            if (i != (trackNumber - 1) && container.isTrackSoloed(i))
            {
                isVisuallyDisabled = true;
                break;
            }
        }
    }

    for (const auto& clip : clips)
    {
        // CRITICAL FIX: Track is INSIDE TimelineContent (the scrollable content)
        // So we draw at ABSOLUTE positions, NOT relative to viewport
        // The viewport scrolling is handled by JUCE automatically

        // Calculate absolute pixel position in timeline content
        float clipX = static_cast<float>(clip->startTime * zoom);

        // Calculate visual scale factor for this specific clip
        double visualScaleFactor = clip->referenceBPM / trackBPM;

        // Calculate width with BPM scaling
        float width = static_cast<float>(clip->duration * zoom * visualScaleFactor);

        // Skip if clip is completely outside track bounds
        if (clipX + width < 0 || clipX > getWidth())
            continue;

        // Ensure minimum width for visibility
        width = juce::jmax(2.0f, width);

        auto clipBounds = juce::Rectangle<float>(clipX, 10.0f, width, static_cast<float>(TRACK_HEIGHT - 20));

        // Draw clip background with appropriate color
        // Darken if track is visually disabled (muted or inactive due to solo)
        juce::Colour clipColour = clip->colour;
        if (isVisuallyDisabled)
            clipColour = clipColour.darker(0.6f);

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

        // Draw clip name
        if (clipBounds.getWidth() > 40)
        {
            auto& lnf = DrumGrooveLookAndFeel::getInstance();
            g.setFont(lnf.getSmallFont().withHeight(11.0f));
            
            // Dim text if track is disabled
            float textAlpha = isVisuallyDisabled ? 0.5f : 0.9f;
            g.setColour(juce::Colours::white.withAlpha(textAlpha));
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

    // CRITICAL FIX: Draw at absolute position, not relative to viewport
    float clipX = static_cast<float>(ghostClip->startTime * container.getZoom());

    double trackBPM = getTrackBPM();
    double visualScaleFactor = ghostClip->referenceBPM / trackBPM;
    float width = static_cast<float>(ghostClip->duration * container.getZoom() * visualScaleFactor);

    auto clipBounds = juce::Rectangle<float>(clipX, 10.0f, width, TRACK_HEIGHT - 20.0f);

    // Draw ghost clip with transparency
    g.setColour(ghostClip->colour.withAlpha(0.5f));
    g.fillRoundedRectangle(clipBounds, 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRoundedRectangle(clipBounds, 4.0f, 2.0f);
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
    // When first MIDI is dropped on empty track, update track BPM to match MIDI's header BPM
    if (!clips.empty())
    {
        const auto& firstClip = clips.front();
        // Use headerBPM from GrooveBrowser (the BPM user was listening to it at)
        double midiBPM = firstClip->headerBPM;

        DBG("Track " + juce::String(trackNumber) + " inheriting BPM from first MIDI header: " +
        juce::String(midiBPM, 2) + " BPM");

        // Update track header's BPM controls
        if (trackNumber > 0 && trackNumber <= container.getNumTracks())
        {
            auto* header = container.getTrackHeader(trackNumber - 1);
            if (header)
            {
                header->setTrackBPM(midiBPM);
                DBG("Updated track " + juce::String(trackNumber) + " BPM to " + juce::String(midiBPM, 2));
            }
        }
    }
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

// ===== UNDO/REDO SYSTEM IMPLEMENTATION =====

void Track::addClip(const MidiClip& clip, bool recordUndo)
{
    if (recordUndo)
    {
        addUndoCommand(std::make_unique<TrackAddClipCommand>(this, clip));
    }
    else
    {
        clips.push_back(std::make_unique<MidiClip>(clip));
        container.updateTimelineSize();
        repaint();
    }
	if (auto* mtc = findParentComponentOfClass<MultiTrackContainer>())
    {
        mtc->invalidateBarWidthCache();
    }
}

void Track::removeSelectedClips()
{
    std::vector<MidiClip> toDelete;
    for (const auto& clip : clips)
    {
        if (clip->isSelected)
            toDelete.push_back(*clip);
    }

    if (!toDelete.empty())
    {
        addUndoCommand(std::make_unique<TrackDeleteClipsCommand>(this, toDelete));
    }
	if (auto* mtc = findParentComponentOfClass<MultiTrackContainer>())
    {
        mtc->invalidateBarWidthCache();
    }
}

void Track::clearAllClips(bool showConfirmation)
{
    if (clips.empty())
        return;

    // Collect all clips for undo command
    std::vector<MidiClip> allClips;
    for (const auto& clip : clips)
    {
        allClips.push_back(*clip);
    }
    
    if (!allClips.empty())
    {
        // Clear all clips using the delete command - fully undoable with Ctrl+Z
        addUndoCommand(std::make_unique<TrackDeleteClipsCommand>(this, allClips));
        DBG("Cleared " + juce::String(allClips.size()) + " clips from track (undoable)");
    }
	if (auto* mtc = findParentComponentOfClass<MultiTrackContainer>())
    {
        mtc->invalidateBarWidthCache();
    }
}

void Track::undo()
{
    container.undo();
}

void Track::redo()
{
    container.redo();
}

bool Track::canUndo() const
{
    return container.canUndo();
}

bool Track::canRedo() const
{
    return container.canRedo();
}

void Track::addUndoCommand(std::unique_ptr<TrackCommand> command, bool executeNow)
{
    container.addUndoCommand(std::move(command), executeNow);
}

void Track::clearUndoHistory()
{
    container.clearUndoHistory();
}

bool Track::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Handle Delete/Backspace
    if (key.getKeyCode() == juce::KeyPress::deleteKey ||
        key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        removeSelectedClips();
        return true;
    }

    // Ctrl+A = Select All
    if (key.getKeyCode() == 'a' && key.getModifiers().isCtrlDown())
    {
        selectAll();
        return true;
    }

    // Ctrl+Z = Undo - forward to container
    if (key.getKeyCode() == 'z' && key.getModifiers().isCtrlDown() && !key.getModifiers().isShiftDown())
    {
        container.undo();
        return true;
    }

    // Ctrl+Y or Ctrl+Shift+Z = Redo - forward to container
    if ((key.getKeyCode() == 'y' && key.getModifiers().isCtrlDown()) ||
        (key.getKeyCode() == 'z' && key.getModifiers().isCtrlDown() && key.getModifiers().isShiftDown()))
    {
        container.redo();
        return true;
    }

    return false;
}
