#include "Timeline.h"
#include "TrackHeader.h"
#include "../../Core/DrumGrooveProcessor.h"
#include "../../Core/MidiProcessor.h"
#include "../../Core/MidiDissector.h"
#include "../../Utils/TimelineUtils.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../LookAndFeel/ColourPalette.h"

// Command classes for undo/redo
class AddClipCommand : public TimelineCommand
{
public:
    AddClipCommand(Timeline* tl, const MidiClip& clip)
        : timeline(tl), clip(clip) {}

    void execute() override {
        timeline->clips.push_back(std::make_unique<MidiClip>(clip));
        timeline->repaint();
    }

    void undo() override {
        timeline->clips.erase(
            std::remove_if(timeline->clips.begin(), timeline->clips.end(),
                [this](const std::unique_ptr<MidiClip>& c) {
                    return c->id == clip.id;
                }),
            timeline->clips.end());
        timeline->repaint();
    }

private:
    Timeline* timeline;
    MidiClip clip;
};

class DeleteClipsCommand : public TimelineCommand
{
public:
    DeleteClipsCommand(Timeline* tl, const std::vector<MidiClip>& clips)
        : timeline(tl), deletedClips(clips) {}

    void execute() override {
        for (const auto& clip : deletedClips) {
            timeline->clips.erase(
                std::remove_if(timeline->clips.begin(), timeline->clips.end(),
                    [&clip](const std::unique_ptr<MidiClip>& c) {
                        return c->id == clip.id;
                    }),
                timeline->clips.end());
        }
        timeline->repaint();
    }

    void undo() override {
        for (const auto& clip : deletedClips) {
            timeline->clips.push_back(std::make_unique<MidiClip>(clip));
        }
        timeline->repaint();
    }

private:
    Timeline* timeline;
    std::vector<MidiClip> deletedClips;
};

class MoveClipsCommand : public TimelineCommand
{
public:
    MoveClipsCommand(Timeline* tl, const std::vector<std::pair<juce::String, double>>& moves)
        : timeline(tl), clipMoves(moves) {}

    void execute() override {
        if (oldPositions.empty()) {
            for (const auto& [id, newTime] : clipMoves) {
                for (auto& clip : timeline->clips) {
                    if (clip->id == id) {
                        oldPositions.push_back({id, clip->startTime});
                        clip->startTime = newTime;
                        break;
                    }
                }
            }
        } else {
            for (const auto& [id, newTime] : clipMoves) {
                for (auto& clip : timeline->clips) {
                    if (clip->id == id) {
                        clip->startTime = newTime;
                        break;
                    }
                }
            }
        }
        timeline->repaint();
    }

    void undo() override {
        for (const auto& [id, oldTime] : oldPositions) {
            for (auto& clip : timeline->clips) {
                if (clip->id == id) {
                    clip->startTime = oldTime;
                    break;
                }
            }
        }
        timeline->repaint();
    }

private:
    Timeline* timeline;
    std::vector<std::pair<juce::String, double>> clipMoves;
    std::vector<std::pair<juce::String, double>> oldPositions;
};

class ResizeClipsCommand : public TimelineCommand
{
public:
    ResizeClipsCommand(Timeline* tl, const juce::String& clipId, double newDuration)
    : timeline(tl), clipId(clipId), newDuration(newDuration) {}

    void execute() override {
        for (auto& clip : timeline->clips) {
            if (clip->id == clipId) {
                oldDuration = clip->duration;
                clip->duration = newDuration;
                break;
            }
        }
        timeline->repaint();
    }

    void undo() override {
        for (auto& clip : timeline->clips) {
            if (clip->id == clipId) {
                clip->duration = oldDuration;
                break;
            }
        }
        timeline->repaint();
    }

private:
    Timeline* timeline;
    juce::String clipId;
    double newDuration = 0.0;
    double oldDuration = 0.0;
};

// Timeline implementation
Timeline::Timeline(DrumGrooveProcessor& p)
    : processor(p)
{
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    loopEnabled = false;
    selectionValid = false;
    selectionStart = 0.0;
    selectionEnd = 0.0;
    
    // FIXED: Initialize grid interval based on initial zoom level
    zoomLevel = 100.0f; // Default zoom
    gridInterval = TimelineUtils::calculateOptimalGridInterval(zoomLevel);
    
    trackHeader = std::make_unique<TrackHeader>(processor, *this);
    addAndMakeVisible(trackHeader.get());

    lastPlaybackTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
}

Timeline::~Timeline()
{
    stopTimer();
    removeKeyListener(this);
}

void Timeline::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);

    drawGrid(g);
    drawRuler(g);
    drawClips(g);

    if (selectionValid || isSettingSelection)
        drawSelectionRegion(g);

    if (ghostClip)
        drawGhostClip(g);

    if (isSelecting)
        drawSelectionBox(g);

    drawPlayhead(g);

    if (dropIndicatorX >= 0)
        drawDropIndicator(g);

    if (currentTooltip.isNotEmpty())
        drawTooltip(g);
}

void Timeline::resized()
{
    auto bounds = getLocalBounds();

    if (trackHeader)
        trackHeader->setBounds(bounds.removeFromLeft(TRACK_HEADER_WIDTH));
}

void Timeline::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showRightClickMenu(e.position.toInt());
        return;
    }

    if (e.x < TRACK_HEADER_WIDTH)
        return;

    MidiClip* clickedClip = nullptr;
    float clickX = static_cast<float>(e.x);
    bool clickedOnResizeHandle = false;
    bool clickedOnLeftResizeHandle = false;

    for (auto& clip : clips)
    {
        float clipX = timeToPixels(clip->startTime);
        float clipWidth = static_cast<float>(clip->duration * zoomLevel * getVisualScaleFactor());
        float clipEndX = clipX + clipWidth;

        if (clickX >= clipX && clickX <= clipEndX)
        {
            clickedClip = clip.get();

            if (clickX <= clipX + RESIZE_HANDLE_WIDTH)
            {
                clickedOnLeftResizeHandle = true;
            }
            else if (clickX >= clipEndX - RESIZE_HANDLE_WIDTH)
            {
                clickedOnResizeHandle = true;
            }
            break;
        }
    }

    if (clickedOnLeftResizeHandle && clickedClip)
    {
        isResizingLeft = true;
        resizingClip = clickedClip;
        resizeStartTime = clickedClip->startTime;
        resizeStartDuration = clickedClip->duration;
    }
    else if (clickedOnResizeHandle && clickedClip)
    {
        isResizing = true;
        resizingClip = clickedClip;
        resizeStartDuration = clickedClip->duration;
    }
    else if (clickedClip)
    {
        if (!e.mods.isCtrlDown() && !e.mods.isShiftDown())
        {
            for (auto& clip : clips)
                clip->isSelected = false;
        }

        clickedClip->isSelected = !clickedClip->isSelected;

        if (clickedClip->isSelected)
        {
            isDragging = true;
            dragStartPoint = e.position;
            draggedClips.clear();

            for (const auto& clip : clips)
            {
                if (clip->isSelected)
                {
                    draggedClips.push_back({clip->id, clip->startTime});
                }
            }
        }
    }
    else
    {
        if (e.mods.isAltDown())
        {
            isSettingSelection = true;
            selectionStart = pixelsToTime(static_cast<float>(e.x));
            selectionEnd = selectionStart;
        }
        else
        {
            if (!e.mods.isCtrlDown() && !e.mods.isShiftDown())
            {
                for (auto& clip : clips)
                    clip->isSelected = false;
            }

            isSelecting = true;
            isAddingToSelection = e.mods.isCtrlDown() || e.mods.isShiftDown();
            dragStartPoint = e.position;
            selectionBox = juce::Rectangle<float>();
        }
    }

    repaint();
}

void Timeline::mouseDrag(const juce::MouseEvent& e)
{
    if (isSettingSelection)
    {
        selectionEnd = pixelsToTime(static_cast<float>(e.x));
        selectionEnd = juce::jmax(selectionStart + 0.001, selectionEnd);
        selectionValid = true;

        if (processor.transportCallback)
            processor.transportCallback();

        sendChangeMessage();
        repaint();
    }
    else if (isResizing && resizingClip)
    {
        double newEndTime = pixelsToTime(static_cast<float>(e.x));
        double newDuration = newEndTime - resizingClip->startTime;

        newDuration = juce::jmax(0.1, newDuration);

        if (!e.mods.isAltDown())
            newDuration = snapToGrid(resizingClip->startTime + newDuration) - resizingClip->startTime;

        resizingClip->duration = newDuration;
        
        // CRITICAL FIX: Notify MidiProcessor of clip boundary changes during playback
        if (playing)
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );
        }
        
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
        
        // CRITICAL FIX: Notify MidiProcessor of clip boundary changes during playback
        if (playing)
        {
            processor.midiProcessor.updateClipBoundaries(
                resizingClip->id,
                resizingClip->startTime,
                resizingClip->duration
            );
        }
        
        repaint();
    }
    else if (isDragging)
    {
        double deltaTime = pixelsToTime(static_cast<float>(e.x)) - pixelsToTime(static_cast<float>(dragStartPoint.x));

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
                        
                        // CRITICAL FIX: Notify MidiProcessor of clip position changes during playback
                        if (playing)
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
            float clipX = timeToPixels(clip->startTime);
            float clipWidth = static_cast<float>(clip->duration * zoomLevel * getVisualScaleFactor());
            juce::Rectangle<float> clipBounds(clipX, RULER_HEIGHT + 10.0f, clipWidth, TRACK_HEIGHT - 20.0f);

            if (selectionBox.intersects(clipBounds))
                clip->isSelected = true;
            else if (!isAddingToSelection)
                clip->isSelected = false;
        }
    }

    repaint();
}

void Timeline::mouseUp(const juce::MouseEvent&)
{
    if (isSettingSelection)
    {
        isSettingSelection = false;

        if (selectionEnd - selectionStart > 0.001)
        {
            if (processor.transportCallback)
                processor.transportCallback();
        }
    }
    else if (isResizing && resizingClip)
    {
        if (std::abs(resizingClip->duration - resizeStartDuration) > 0.001)
        {
            addUndoCommand(std::make_unique<ResizeClipsCommand>(
                this, resizingClip->id, resizingClip->duration));
        }
        isResizing = false;
        resizingClip = nullptr;
    }
    else if (isResizingLeft && resizingClip)
    {
        if (std::abs(resizingClip->startTime - resizeStartTime) > 0.001 ||
            std::abs(resizingClip->duration - resizeStartDuration) > 0.001)
        {
            std::vector<std::pair<juce::String, double>> moves;
            moves.push_back({resizingClip->id, resizingClip->startTime});
            addUndoCommand(std::make_unique<MoveClipsCommand>(this, moves));
        }
        isResizingLeft = false;
        resizingClip = nullptr;
    }
    else if (isDragging && !draggedClips.empty())
    {
        std::vector<std::pair<juce::String, double>> finalPositions;
        bool moved = false;

        for (const auto& clip : clips)
        {
            if (clip->isSelected)
            {
                for (const auto& [id, originalTime] : draggedClips)
                {
                    if (id == clip->id && std::abs(clip->startTime - originalTime) > 0.001)
                    {
                        finalPositions.push_back({id, clip->startTime});
                        moved = true;
                        break;
                    }
                }
            }
        }

        if (moved)
        {
            addUndoCommand(std::make_unique<MoveClipsCommand>(this, finalPositions));
        }

        isDragging = false;
        draggedClips.clear();
    }
    else if (isSelecting)
    {
        isSelecting = false;
        selectionBox = juce::Rectangle<float>();
    }

    repaint();
}

void Timeline::mouseMove(const juce::MouseEvent& e)
{
    if (e.x < TRACK_HEADER_WIDTH)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    for (const auto& clip : clips)
    {
        float clipX = timeToPixels(clip->startTime);
        float clipWidth = static_cast<float>(clip->duration * zoomLevel * getVisualScaleFactor());
        float clipEndX = clipX + clipWidth;

        if (e.position.x >= clipX && e.position.x <= clipEndX)
        {
            if (std::abs(e.position.x - clipEndX) < RESIZE_HANDLE_WIDTH ||
                std::abs(e.position.x - clipX) < RESIZE_HANDLE_WIDTH)
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
}

// REMOVED: mouseDoubleClick method - no double-click playback as requested

void Timeline::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        float newZoom = zoomLevel * (1.0f + wheel.deltaY * 0.5f);
        setZoom(newZoom);
    }
    else
    {
        viewStartTime -= wheel.deltaX * 10.0 / zoomLevel;
        viewStartTime = juce::jmax(0.0, viewStartTime);
        repaint();
    }
}

bool Timeline::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return keyPressed(key);
}

bool Timeline::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (playing)
            pause();
        else
            play();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey ||
        key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        deleteSelectedClips();
        return true;
    }

    if (key.isKeyCode('A') && key.getModifiers().isCtrlDown())
    {
        selectAll();
        return true;
    }

    if (key.isKeyCode('Z') && key.getModifiers().isCtrlDown())
    {
        undo();
        return true;
    }

    if ((key.isKeyCode('Y') && key.getModifiers().isCtrlDown()) ||
        (key.isKeyCode('Z') && key.getModifiers().isCtrlDown() && key.getModifiers().isShiftDown()))
    {
        redo();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        clearSelection();
        return true;
    }

    return false;
}

bool Timeline::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isString();
}

void Timeline::itemDragEnter(const SourceDetails& details)
{
    dropIndicatorX = 0;

    ghostClip = std::make_unique<MidiClip>();
    ghostClip->name = details.description.toString();
    ghostClip->startTime = 0;
    
    // FIXED: Set realistic duration instead of hardcoded 2.0
    // Parse the drop description to determine if it's a MIDI file or drum part
    juce::String description = details.description.toString();
    juce::StringArray parts = juce::StringArray::fromTokens(description, "|", "");
    
    if (parts.size() >= 2 && parts[1] == "PART")
    {
        // Drum part - shorter duration
        ghostClip->duration = 1.0;
    }
    else if (parts.size() >= 2)
    {
        // Full MIDI file - calculate realistic duration
        juce::File midiFile(parts[1]);
        if (midiFile.existsAsFile())
        {
            double duration = 4.0; // Default
            if (calculateMidiFileDuration(midiFile, duration))
            {
                ghostClip->duration = duration;
            }
            else
            {
                ghostClip->duration = 4.0; // Fallback
            }
        }
        else
        {
            ghostClip->duration = 4.0; // Default for unknown files
        }
    }
    else
    {
        ghostClip->duration = 2.0; // Fallback
    }
    
    ghostClip->colour = ColourPalette::primaryBlue.withAlpha(0.3f);

    repaint();
}

void Timeline::itemDragMove(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.localPosition.x >= TRACK_HEADER_WIDTH)
    {
        if (ghostClip)
        {
            // Calculate ghost clip position
            double mouseTime = pixelsToTime(static_cast<float>(dragSourceDetails.localPosition.x));
            double halfClipDuration = ghostClip->duration * 0.5;
            double centeredTime = mouseTime - halfClipDuration;
            ghostClip->startTime = snapToGrid(centeredTime);
            
            // CRITICAL: Set drop indicator to exact same position as ghost clip
            // Both ghost clip and drop indicator draw at same X coordinate
            dropIndicatorX = timeToPixels(ghostClip->startTime);
            
            // DEBUG: Verify positions
            DBG("Timeline - Ghost start: " + juce::String(ghostClip->startTime, 6) + 
                ", Drop X: " + juce::String(dropIndicatorX) + 
                ", Recalc X: " + juce::String(timeToPixels(ghostClip->startTime)));
        }
        else
        {
            dropIndicatorX = static_cast<float>(dragSourceDetails.localPosition.x);
        }
    }
    else
    {
        dropIndicatorX = -1;
    }

    repaint();
}

void Timeline::itemDragExit(const SourceDetails&)
{
    dropIndicatorX = -1;
    ghostClip.reset();
    repaint();
}

void Timeline::itemDropped(const SourceDetails& details)
{
    dropIndicatorX = -1;

    if (details.localPosition.x < TRACK_HEADER_WIDTH)
    {
        ghostClip.reset();
        repaint();
        return;
    }

    // FIXED: BPM inheritance logic - if timeline is empty, consider track BPM
    double targetBPM = getTrackBPM();
    double dropTime = pixelsToTime(static_cast<float>(details.localPosition.x));
    
    // FIXED: Center the clip on drop position
    double halfClipDuration = ghostClip ? ghostClip->duration * 0.5 : 2.0;
    double centeredTime = dropTime - halfClipDuration;
    double snappedTime = snapToGrid(centeredTime);

    juce::String description = details.description.toString();
    juce::StringArray parts = juce::StringArray::fromTokens(description, "|", "");

    if (parts.size() < 2)
    {
        ghostClip.reset();
        repaint();
        return;
    }

    if (parts[1] == "PART")
    {
        if (parts.size() >= 4)
        {
            handleDrumPartDrop(parts, snappedTime);
        }
    }
    else
    {
        handleMidiFileDrop(parts, snappedTime);
    }

    ghostClip.reset();
    repaint();
}

// Playback methods
void Timeline::play()
{
    // CRITICAL FIX: Clear all existing clips before adding new ones
    // This prevents stale clips from previous play sessions
    processor.midiProcessor.clearAllClips();
    
    // Get current track BPM for all clips
    double trackBPM = getTrackBPM();
    
    // Add all clips with current track BPM
    for (const auto& clip : clips)
    {
        processor.midiProcessor.addMidiClip(
            clip->file, 
            clip->startTime, 
            DrumLibrary::Unknown,
            clip->referenceBPM,
            trackBPM,
            0  // Track number (0 for main timeline)
        );
    }

    if (loopEnabled && selectionValid)
    {
        processor.midiProcessor.setLoopEnabled(true);
        processor.midiProcessor.setLoopRange(selectionStart, selectionEnd);

        if (playheadPosition < selectionStart || playheadPosition > selectionEnd)
        {
            setPlayheadPosition(selectionStart);
        }
    }
    else
    {
        processor.midiProcessor.setLoopEnabled(false);

        if (playheadPosition >= getMaxTime() && getMaxTime() > 0)
        {
            setPlayheadPosition(0.0);
        }
    }

    processor.midiProcessor.setPlayheadPosition(playheadPosition);
    processor.midiProcessor.play();

    playing = true;
    startHighPrecisionTimer();
}

void Timeline::pause()
{
    playing = false;
    processor.midiProcessor.pause();
    stopTimer();
}

void Timeline::stop()
{
    playing = false;
    setPlayheadPosition(0.0);
    processor.midiProcessor.stop();
    processor.midiProcessor.clearAllClips();
    stopTimer();
    repaint();
}

void Timeline::setPlayheadPosition(double timeInSeconds)
{
    playheadPosition = juce::jmax(0.0, timeInSeconds);

    processor.midiProcessor.setPlayheadPosition(playheadPosition);

    lastPlaybackTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if (autoScrollEnabled)
    {
        updateAutoScroll();
    }

    repaint();
}

void Timeline::toggleLoop()
{
    loopEnabled = !loopEnabled;

    if (playing)
    {
        processor.midiProcessor.setLoopEnabled(loopEnabled);
        if (loopEnabled && selectionValid)
        {
            processor.midiProcessor.setLoopRange(selectionStart, selectionEnd);
        }
    }

    repaint();
}

void Timeline::clearSelection()
{
    selectionValid = false;
    selectionStart = 0.0;
    selectionEnd = 0.0;
    sendChangeMessage();
    repaint();
}

void Timeline::setSelectionStart(double time)
{
    selectionStart = juce::jmax(0.0, time);
    if (selectionEnd <= selectionStart)
        selectionEnd = selectionStart + 0.001;
    selectionValid = true;
    sendChangeMessage();
    repaint();
}

void Timeline::setSelectionEnd(double time)
{
    selectionEnd = juce::jmax(selectionStart + 0.001, time);
    selectionValid = true;
    sendChangeMessage();
    repaint();
}

juce::String Timeline::formatTime(double seconds) const
{
    return TimelineUtils::formatTime(seconds);
}

double Timeline::parseTime(const juce::String& timeStr) const
{
    return TimelineUtils::parseTime(timeStr);
}

void Timeline::setZoom(float pixelsPerSecond)
{
    // Clamp zoom level to reasonable bounds
    float newZoomLevel = juce::jlimit(10.0f, 500.0f, pixelsPerSecond);
    
    // Store the time position that's currently at the center of the view
    float viewWidth = static_cast<float>(getWidth() - TRACK_HEADER_WIDTH);
    float centerX = viewWidth / 2.0f;
    double centerTime = pixelsToTime(centerX + TRACK_HEADER_WIDTH);
    
    // Update zoom level
    zoomLevel = newZoomLevel;
    
    // Update grid interval based on new zoom
    gridInterval = TimelineUtils::calculateOptimalGridInterval(zoomLevel);
    
    // Recalculate viewStartTime to keep the same time at the center
    // This ensures clips stay at their correct time positions
    double newViewWidth = viewWidth / zoomLevel;  // Width in seconds at new zoom
    viewStartTime = centerTime - (newViewWidth / 2.0);
    
    // Don't allow scrolling before time 0
    viewStartTime = juce::jmax(0.0, viewStartTime);
    
    repaint();
}

void Timeline::fitToContent()
{
    if (clips.empty())
        return;

    double maxTime = getMaxTime();

    if (maxTime > 0)
    {
        zoomLevel = static_cast<float>(getWidth() - TRACK_HEADER_WIDTH) / static_cast<float>(maxTime);
        viewStartTime = 0;
        repaint();
    }
}

double Timeline::getMaxTime() const
{
    double maxTime = 0;
    for (const auto& clip : clips)
        maxTime = juce::jmax(maxTime, clip->startTime + clip->duration);
    return maxTime;
}

void Timeline::addClip(const MidiClip& clip, bool recordUndo)
{
    if (recordUndo)
    {
        addUndoCommand(std::make_unique<AddClipCommand>(this, clip));
    }
    else
    {
        clips.push_back(std::make_unique<MidiClip>(clip));
        repaint();
    }
}

void Timeline::removeSelectedClips()
{
    deleteSelectedClips();
}

void Timeline::deleteSelectedClips()
{
    std::vector<MidiClip> toDelete;
    for (const auto& clip : clips)
    {
        if (clip->isSelected)
            toDelete.push_back(*clip);
    }

    if (!toDelete.empty())
    {
        addUndoCommand(std::make_unique<DeleteClipsCommand>(this, toDelete));
    }
}

void Timeline::selectAll()
{
    for (auto& clip : clips)
        clip->isSelected = true;
    repaint();
}

void Timeline::deselectAll()
{
    for (auto& clip : clips)
        clip->isSelected = false;
    repaint();
}

void Timeline::selectClipsInRange(double startTime, double endTime)
{
    for (auto& clip : clips)
    {
        if (clip->startTime >= startTime && clip->startTime <= endTime)
            clip->isSelected = true;
    }
    repaint();
}

std::vector<MidiClip*> Timeline::getSelectedClips()
{
    std::vector<MidiClip*> selected;
    for (auto& clip : clips)
    {
        if (clip->isSelected)
            selected.push_back(clip.get());
    }
    return selected;
}

void Timeline::clearAllClips()
{
    clips.clear();
    repaint();
}

void Timeline::undo()
{
    if (canUndo())
    {
        currentUndoIndex--;
        undoStack[currentUndoIndex]->undo();
    }
}

void Timeline::redo()
{
    if (canRedo())
    {
        undoStack[currentUndoIndex]->execute();
        currentUndoIndex++;
    }
}

void Timeline::addUndoCommand(std::unique_ptr<TimelineCommand> command)
{
    if (currentUndoIndex < undoStack.size())
    {
        undoStack.erase(undoStack.begin() + currentUndoIndex, undoStack.end());
    }

    command->execute();

    undoStack.push_back(std::move(command));
    currentUndoIndex = undoStack.size();

    if (undoStack.size() > MAX_UNDO_LEVELS)
    {
        undoStack.erase(undoStack.begin());
        currentUndoIndex--;
    }
}

void Timeline::onTrackBPMChanged()
{
    repaint();
}

double Timeline::getTrackBPM() const
{
    if (trackHeader)
        return trackHeader->getTrackBPM();
    return 120.0;
}

bool Timeline::isTrackMuted() const
{
    if (trackHeader)
        return trackHeader->isMuted();
    return false;
}

double Timeline::getVisualScaleFactor() const
{
    return TimelineUtils::getVisualScaleFactor(getTrackBPM());
}

juce::Rectangle<int> Timeline::getTimelineArea() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(TRACK_HEADER_WIDTH);
    return bounds;
}

void Timeline::showRightClickMenu(const juce::Point<int>&)
{
    if (trackHeader)
        trackHeader->showContextMenu();
}

// FIX: Time conversion methods with proper view range handling
double Timeline::pixelsToTime(float pixels) const
{
    // Ensure we account for track header offset
    if (pixels < TRACK_HEADER_WIDTH)
        return viewStartTime;
    
    // Convert pixels to time: subtract header width, divide by zoom, add view start
    return viewStartTime + ((pixels - TRACK_HEADER_WIDTH) / zoomLevel);
}

float Timeline::timeToPixels(double time) const
{
    // Convert time to pixels: subtract view start, multiply by zoom, add header width
    return TRACK_HEADER_WIDTH + static_cast<float>((time - viewStartTime) * zoomLevel);
}

void Timeline::startHighPrecisionTimer()
{
    lastPlaybackTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    startTimer(16);
}

void Timeline::timerCallback()
{
    if (playing)
    {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double deltaTime = currentTime - lastPlaybackTime;
        lastPlaybackTime = currentTime;

        playheadPosition += deltaTime;

        // âœ… CRITICAL FIX: Use setPlayheadPosition instead of syncPlayheadPosition
        // This ensures clips are repositioned correctly during playback
        processor.midiProcessor.setPlayheadPosition(playheadPosition);

        if (loopEnabled && selectionValid && playheadPosition >= selectionEnd)
        {
            playheadPosition = selectionStart;
            lastPlaybackTime = currentTime;
            processor.midiProcessor.setPlayheadPosition(selectionStart);
        }

        double maxTime = getMaxTime();
        if (!loopEnabled && maxTime > 0 && playheadPosition >= maxTime)
        {
            stop();
            return;
        }

        if (autoScrollEnabled)
        {
            updateAutoScroll();
        }

        repaint();
    }
}

void Timeline::updateAutoScroll()
{
    float playheadX = timeToPixels(playheadPosition);
    float viewWidth = static_cast<float>(getWidth() - TRACK_HEADER_WIDTH);

    if (playheadX > (TRACK_HEADER_WIDTH + viewWidth * 0.9f))
    {
        viewStartTime = playheadPosition - (viewWidth * 0.1f / zoomLevel);
    }
    else if (playheadX < (TRACK_HEADER_WIDTH + viewWidth * 0.1f))
    {
        viewStartTime = playheadPosition - (viewWidth * 0.9f / zoomLevel);
    }

    viewStartTime = juce::jmax(0.0, viewStartTime);
}

void Timeline::drawRuler(juce::Graphics& g)
{
    auto timelineArea = getTimelineArea();
    auto rulerBounds = timelineArea.removeFromTop(RULER_HEIGHT);

    // Draw ruler background
    g.setColour(juce::Colour(0xff454545));
    g.fillRect(rulerBounds);

    g.setColour(juce::Colour(0xff3c3c3c));
    g.drawLine(static_cast<float>(rulerBounds.getX()), 
               static_cast<float>(RULER_HEIGHT),
               static_cast<float>(rulerBounds.getRight()), 
               static_cast<float>(RULER_HEIGHT));

    g.setColour(juce::Colour(0xff969696));
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    g.setFont(lnf.getSmallFont());

    // Calculate visible time range
    double startTime = viewStartTime;
    double endTime = pixelsToTime(static_cast<float>(getWidth()));
    
    // Use appropriate time step based on zoom
    double timeStep = gridInterval;
    if (gridInterval < 0.1)
        timeStep = 0.1;
    else if (gridInterval < 0.25)
        timeStep = 0.25;
    else if (gridInterval < 0.5)
        timeStep = 0.5;
    else if (gridInterval < 1.0)
        timeStep = 1.0;

    // Draw ruler marks for visible range
    for (double time = std::floor(startTime / timeStep) * timeStep; time <= endTime; time += timeStep)
    {
        float x = timeToPixels(time);
        
        // Only draw if within visible bounds
        if (x >= TRACK_HEADER_WIDTH && x <= getWidth())
        {
            g.drawLine(x, static_cast<float>(RULER_HEIGHT - 10), 
                      x, static_cast<float>(RULER_HEIGHT));

            // Format time display
            juce::String timeText;
            if (timeStep >= 1.0)
            {
                int minutes = static_cast<int>(time) / 60;
                int seconds = static_cast<int>(time) % 60;
                timeText = juce::String::formatted("%d:%02d", minutes, seconds);
            }
            else
            {
                int minutes = static_cast<int>(time) / 60;
                int seconds = static_cast<int>(time) % 60;
                int millis = static_cast<int>((time - static_cast<int>(time)) * 1000);
                timeText = juce::String::formatted("%d:%02d.%03d", minutes, seconds, millis);
            }
            
            g.drawText(timeText, 
                      static_cast<int>(x - 30), 
                      0, 
                      60, 
                      RULER_HEIGHT - 10, 
                      juce::Justification::centred);
        }
    }
}


void Timeline::drawGrid(juce::Graphics& g)
{
    // Draw grid background to prevent black areas
    auto gridArea = getTimelineArea();
    gridArea.removeFromTop(RULER_HEIGHT);
    
    g.setColour(ColourPalette::secondaryBackground);
    g.fillRect(gridArea);
    
    g.setColour(ColourPalette::timelineGrid);

    // Calculate visible range using the same coordinate system as clips
    double startTime = viewStartTime;
    double endTime = pixelsToTime(static_cast<float>(getWidth()));
    
    double gridStep = gridInterval;

    // Draw grid lines for the visible time range
    for (double time = std::floor(startTime / gridStep) * gridStep; time <= endTime; time += gridStep)
    {
        float x = timeToPixels(time);
        
        // Only draw if within visible bounds
        if (x >= static_cast<float>(TRACK_HEADER_WIDTH) && x <= getWidth())
        {
            g.drawVerticalLine(static_cast<int>(x), 
                             static_cast<float>(RULER_HEIGHT), 
                             static_cast<float>(getHeight()));
        }
    }
}

void Timeline::drawClips(juce::Graphics& g)
{
    // Calculate the visible time range based on current viewport
    // This must match the coordinate system used by timeToPixels/pixelsToTime
    double visibleStartTime = viewStartTime;
    double visibleEndTime = pixelsToTime(static_cast<float>(getWidth()));
    
    // Add a small buffer to catch clips at the edges
    const double BUFFER_SECONDS = 0.5;
    visibleStartTime -= BUFFER_SECONDS;
    visibleEndTime += BUFFER_SECONDS;
    
    for (const auto& clip : clips)
    {
        // Calculate when the clip ends
        double clipEndTime = clip->startTime + clip->duration;
        
        // First visibility check: is the clip within the visible time range?
        // A clip is visible if any part of it overlaps with [visibleStartTime, visibleEndTime]
        if (clipEndTime < visibleStartTime || clip->startTime > visibleEndTime)
            continue;

        // Calculate the X position using consistent coordinate transformation
        // This ensures clips always appear at their correct time position
        float x = timeToPixels(clip->startTime);
        
        // Calculate width: duration * zoom * BPM scale factor
        // The BPM scale factor affects how long the clip appears visually,
        // but NOT where it starts
        float width = static_cast<float>(clip->duration * zoomLevel * getVisualScaleFactor());
        
        // Second visibility check: is the clip actually on screen in pixel space?
        // This catches edge cases where time-based check might be imprecise
        if (x + width < TRACK_HEADER_WIDTH || x > getWidth())
            continue;
        
        // Ensure clips are always at least 2 pixels wide for visibility
        width = juce::jmax(2.0f, width);

        // Define the clip's bounding rectangle
        auto clipBounds = juce::Rectangle<float>(x, RULER_HEIGHT + 10.0f, width, TRACK_HEIGHT - 20.0f);

        // Apply muting effect if track is muted
        auto clipColour = clip->colour;
        if (isTrackMuted())
            clipColour = clipColour.withSaturation(0.3f);

        // Draw clip background (brighter if selected)
        g.setColour(clip->isSelected ? 
                   clipColour.brighter(0.3f) : 
                   clipColour);
        g.fillRoundedRectangle(clipBounds, 4.0f);

        // Draw clip border (yellow if selected)
        g.setColour(clip->isSelected ? 
                   juce::Colours::yellow : 
                   juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(clipBounds, 4.0f, 2.0f);

        // Draw clip name if there's enough space
        if (clipBounds.getWidth() > 40)
        {
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            auto& lnf = DrumGrooveLookAndFeel::getInstance();
            g.setFont(lnf.getSmallFont());
            
            auto textBounds = clipBounds.reduced(6.0f, 4.0f);
            g.drawText(clip->name, textBounds, juce::Justification::topLeft, false);
        }

        // Draw MIDI note dots if there's space
        if (clipBounds.getWidth() > 20)
        {
            drawMidiDotsInClip(g, *clip, clipBounds);
        }
    }
}


void Timeline::drawMidiDotsInClip(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> clipBounds)
{
    if (clipBounds.getWidth() < 30 || clipBounds.getHeight() < 20)
        return;

    if (!clip.file.existsAsFile())
        return;

    auto dotArea = clipBounds.reduced(4.0f, 2.0f);
    dotArea.removeFromTop(16.0f);

    g.setColour(clip.colour.darker(0.6f));
    g.fillRoundedRectangle(dotArea, 2.0f);

    g.setColour(clip.colour.darker(0.8f));

    int gridDivisions = juce::jmax(4, static_cast<int>(dotArea.getWidth() / 15));
    for (int i = 0; i <= gridDivisions; ++i)
    {
        float x = dotArea.getX() + (i * dotArea.getWidth() / static_cast<float>(gridDivisions));
        g.drawVerticalLine(static_cast<int>(x), dotArea.getY(), dotArea.getBottom());
    }

    juce::FileInputStream stream(clip.file);
    if (!stream.openedOk())
        return;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return;

    // Get proper MIDI file timing information
    double ticksPerQuarterNote = midiFile.getTimeFormat();
    if (ticksPerQuarterNote <= 0)
        ticksPerQuarterNote = 480.0;

    // Calculate actual MIDI file duration properly
    double maxTimeStamp = 0;
    juce::MidiMessageSequence allNotes;
    
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
                    allNotes.addEvent(message);
                    maxTimeStamp = juce::jmax(maxTimeStamp, message.getTimeStamp());
                }
            }
        }
    }

    allNotes.sort();

    // Calculate duration in seconds using proper BPM conversion
    double midiDurationInSeconds = (maxTimeStamp / ticksPerQuarterNote) * (60.0 / 120.0);
    double visualDuration = juce::jmax(0.1, clip.duration);

    // Draw note events with dissection colors
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
                float noteY = dotArea.getY() + (1.0f - (noteNumber / 127.0f)) * dotArea.getHeight();
                
                juce::Colour dotColour = MidiDissector::getColorForDrumNote(noteNumber);
                
                float velocity = event.getVelocity() / 127.0f;
                float dotSize = 2.0f + velocity * 4.0f;
                
                g.setColour(dotColour.withAlpha(0.6f + velocity * 0.4f));
                g.fillEllipse(dotX - dotSize * 0.5f, noteY - dotSize * 0.5f, dotSize, dotSize);
            }
        }
    }
}

void Timeline::drawGhostClip(juce::Graphics& g)
{
    if (!ghostClip)
        return;

    // Use exact same coordinate calculation as drop indicator
    float x = timeToPixels(ghostClip->startTime);
    float width = static_cast<float>(ghostClip->duration * zoomLevel * getVisualScaleFactor());

    auto clipBounds = juce::Rectangle<float>(x, RULER_HEIGHT + 10.0f, width, TRACK_HEIGHT - 20.0f);

    g.setColour(ghostClip->colour);
    g.fillRoundedRectangle(clipBounds, 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(clipBounds, 4.0f, 2.0f);
}

void Timeline::drawSelectionBox(juce::Graphics& g)
{
    if (isSelecting && !selectionBox.isEmpty())
    {
        g.setColour(ColourPalette::primaryBlue.withAlpha(0.2f));
        g.fillRect(selectionBox);

        g.setColour(ColourPalette::primaryBlue.withAlpha(0.8f));
        g.drawRect(selectionBox, 1.0f);
    }
}

void Timeline::drawTooltip(juce::Graphics& g)
{
    if (currentTooltip.isNotEmpty())
    {
        auto& lnf = DrumGrooveLookAndFeel::getInstance();
        g.setFont(lnf.getSmallFont());

        auto textWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), currentTooltip) + 10;
        auto tooltipBounds = juce::Rectangle<int>(tooltipPosition.x, tooltipPosition.y - 25, textWidth, 20);

        g.setColour(ColourPalette::panelBackground);
        g.fillRoundedRectangle(tooltipBounds.toFloat(), 3.0f);

        g.setColour(ColourPalette::borderColour);
        g.drawRoundedRectangle(tooltipBounds.toFloat(), 3.0f, 1.0f);

        g.setColour(ColourPalette::primaryText);
        g.drawText(currentTooltip, tooltipBounds, juce::Justification::centred);
    }
}

void Timeline::drawPlayhead(juce::Graphics& g)
{
    float x = timeToPixels(playheadPosition);

    if (x >= TRACK_HEADER_WIDTH && x <= getWidth())
    {
        g.setColour(ColourPalette::yellowPlayhead);
        g.drawLine(x, 0.0f, x, static_cast<float>(getHeight()), 2.0f);

        juce::Path triangle;
        triangle.addTriangle(x - 6.0f, 0.0f, x + 6.0f, 0.0f, x, 10.0f);
        g.fillPath(triangle);
    }
}

void Timeline::drawSelectionRegion(juce::Graphics& g)
{
    if (!selectionValid && !isSettingSelection)
        return;

    float startX = timeToPixels(selectionStart);
    float endX = timeToPixels(selectionEnd);

    if (endX > startX)
    {
        startX = juce::jmax(static_cast<float>(TRACK_HEADER_WIDTH), startX);
        endX = juce::jmin(static_cast<float>(getWidth()), endX);

        float alpha;
        if (loopEnabled)
        {
            alpha = 0.3f;
            g.setColour(ColourPalette::cyanAccent.withAlpha(alpha));
        }
        else
        {
            alpha = 0.2f;
            g.setColour(ColourPalette::primaryBlue.withAlpha(alpha));
        }

        g.fillRect(juce::Rectangle<float>(startX, RULER_HEIGHT, endX - startX, getHeight() - RULER_HEIGHT));

        g.setColour(loopEnabled ? ColourPalette::cyanAccent : ColourPalette::primaryBlue);
        g.drawLine(startX, RULER_HEIGHT, startX, static_cast<float>(getHeight()), 2.0f);
        g.drawLine(endX, RULER_HEIGHT, endX, static_cast<float>(getHeight()), 2.0f);
    }
}

void Timeline::drawDropIndicator(juce::Graphics& g)
{
    if (dropIndicatorX > 0)
    {
        g.setColour(ColourPalette::cyanAccent);
        g.drawLine(dropIndicatorX, RULER_HEIGHT, dropIndicatorX, static_cast<float>(getHeight()), 2.0f);
    }
}

// Helper methods
double Timeline::snapToGrid(double time) const
{
    return juce::roundToInt(time / gridInterval) * gridInterval;
}

void Timeline::handleMidiFileDrop(const juce::StringArray& parts, double dropTime)
{
    MidiClip newClip;
    newClip.name = parts[0];
    newClip.startTime = dropTime;
    newClip.colour = ColourPalette::primaryBlue.withAlpha(0.7f);
    
    double trackBPM = getTrackBPM();
    newClip.duration = 4.0 * (120.0 / trackBPM);

    newClip.file = juce::File(parts[1]);
    
    if (!newClip.file.existsAsFile())
        return;

    if (calculateMidiFileDuration(newClip.file, newClip.duration))
    {
        newClip.duration = newClip.duration * (120.0 / trackBPM);
    }

    addClip(newClip);
}

void Timeline::handleDrumPartDrop(const juce::StringArray& parts, double dropTime)
{
    juce::String partName = parts[0];
    int drumNote = parts[2].getIntValue();
    juce::File sourceFile(parts[3]);

    if (!sourceFile.existsAsFile())
        return;

    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("drum_part_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".mid");

    if (MidiDissector::extractDrumPartToFile(sourceFile, drumNote, tempFile))
    {
        MidiClip newClip;
        newClip.name = partName;
        newClip.startTime = dropTime;
        newClip.file = tempFile;
        newClip.colour = MidiDissector::getColorForDrumNote(drumNote).withAlpha(0.7f);
        
        double trackBPM = getTrackBPM();
        newClip.duration = 1.0 * (120.0 / trackBPM);

        addClip(newClip);
    }
}

bool Timeline::calculateMidiFileDuration(const juce::File& file, double& duration)
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