#include "DrumGridView.h"
#include "../../PluginProcessor.h"

DrumGridView::DrumGridView(EditableMidiClip& midiClip, DrumLibraryManager& libManager, DrumGrooveProcessor& proc)
: clip(midiClip), drumLibraryManager(libManager), processor(proc), horizontalScrollbar(false)
{
    addAndMakeVisible(horizontalScrollbar);
    horizontalScrollbar.addListener(this);

    updateVisibleDrumParts();
    updateScrollbarRanges();
}

DrumGridView::~DrumGridView()
{
    horizontalScrollbar.removeListener(this);
}

void DrumGridView::updateVisibleDrumParts()
{
    visibleDrumParts.clear();

    auto sourceLib = clip.getSourceLibrary();

    // Check all possible drum parts
    for (int i = 0; i < static_cast<int>(DrumPartType::COUNT); ++i)
    {
        DrumPartType part = static_cast<DrumPartType>(i);

        // Only add parts that have valid mappings for this library
        if (drumLibraryManager.hasValidMapping(part, sourceLib))
        {
            visibleDrumParts.push_back(part);
        }
    }

    DBG("Visible drum parts for library " + juce::String(static_cast<int>(sourceLib)) +
    ": " + juce::String(visibleDrumParts.size()));
}

void DrumGridView::refreshForNewClip()
{
    updateVisibleDrumParts();
    updateScrollbarRanges();
    repaint();
}

void DrumGridView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF1A1A1A));

    auto labelArea = bounds.reduced(0, 16).removeFromLeft(LABEL_WIDTH);
    auto gridArea = bounds.reduced(0, 16).withTrimmedLeft(LABEL_WIDTH).withTrimmedRight(16);

    drawGrid(g, gridArea);
    drawRowLabels(g, labelArea);
    drawNotes(g, gridArea);
    drawLoopRegion(g, gridArea);
    drawPlayhead(g, gridArea);
    drawSelectionBox(g, gridArea);
}

void DrumGridView::resized()
{
    auto bounds = getLocalBounds();

    int scrollbarSize = 16;
    horizontalScrollbar.setBounds(bounds.removeFromBottom(scrollbarSize)
    .reduced(LABEL_WIDTH, 0)
    .withTrimmedRight(scrollbarSize));

    updateScrollbarRanges();
}

void DrumGridView::mouseDown(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds();
    auto gridArea = bounds.reduced(0, 16).withTrimmedLeft(LABEL_WIDTH).withTrimmedRight(16);

    if (!gridArea.contains(event.getPosition()))
        return;

    // Check if clicking on loop markers
    bool overStart = false, overEnd = false;
    if (isOverLoopMarker(event.x, event.y, overStart, overEnd))
    {
        isDraggingLoopStart = overStart;
        isDraggingLoopEnd = overEnd;
        return;
    }

    // CRITICAL: Ignore editing in read-only mode (allow Select tool only)
    if (isReadOnly && currentTool != EditorTool::Select)
        return;

    int x = event.x - LABEL_WIDTH;
    int y = event.y;

    switch (currentTool)
    {
        case EditorTool::Pencil:
            handlePencilTool(x, y);
            break;

        case EditorTool::Eraser:
            handleEraserTool(x, y);
            break;

        case EditorTool::Select:
        {
            auto* note = getNoteAtPosition(x, y);
            if (note)
            {
                if (!event.mods.isShiftDown() && !note->isSelected)
                    clip.clearSelection();

                note->isSelected = true;
                startNoteDrag(note->id, x, y);
            }
            else
            {
                if (!event.mods.isShiftDown())
                    clip.clearSelection();
                startBoxSelection(x, y);
            }
            break;
        }
    }

    repaint();
}

void DrumGridView::mouseDrag(const juce::MouseEvent& event)
{
    int x = event.x - LABEL_WIDTH;
    int y = event.y;

    // Handle loop marker dragging
    if (isDraggingLoopStart || isDraggingLoopEnd)
    {
        double newTime = xToTime(x);
        newTime = juce::jlimit(0.0, getEffectiveTimelineDuration(), newTime);

        if (isDraggingLoopStart)
        {
            loopStartQN = juce::jmin(newTime, loopEndQN - 0.25);
        }
        else if (isDraggingLoopEnd)
        {
            loopEndQN = juce::jmax(newTime, loopStartQN + 0.25);
        }

        if (onLoopRegionChanged)
            onLoopRegionChanged(loopStartQN, loopEndQN);

        repaint();
        return;
    }

    if (isDragging)
    {
        updateNoteDrag(x, y);
    }
    else if (isBoxSelecting)
    {
        updateBoxSelection(x, y);
    }
}

void DrumGridView::mouseUp(const juce::MouseEvent& event)
{
    // Clear loop dragging flags
    isDraggingLoopStart = false;
    isDraggingLoopEnd = false;

    if (isDragging)
    {
        finishNoteDrag();
    }
    else if (isBoxSelecting)
    {
        finishBoxSelection();
    }
}

void DrumGridView::mouseMove(const juce::MouseEvent& event)
{
    // Update cursor when over loop markers
    bool overStart = false, overEnd = false;
    if (isOverLoopMarker(event.x, event.y, overStart, overEnd))
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void DrumGridView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCtrlDown())
    {
        // Ctrl + Wheel = Zoom
        float zoomDelta = wheel.deltaY * 0.5f;
        setZoomLevel(zoomFactor + zoomDelta);
    }
    else
    {
        // Normal wheel = Horizontal scroll
        // Allow scrolling to the full timeline duration (MAX_TIMELINE_DURATION)
        double scrollAmount = wheel.deltaY * -2.0;
        scrollOffsetX = juce::jlimit(0.0, getEffectiveTimelineDuration(), scrollOffsetX + scrollAmount);

        int gridWidth = getWidth() - LABEL_WIDTH - 16;
        double visibleDuration = gridWidth / (GRID_WIDTH_PER_QN * zoomFactor);

        horizontalScrollbar.setCurrentRange(scrollOffsetX,
                                            juce::jmin(scrollOffsetX + visibleDuration, getEffectiveTimelineDuration()),
                                            juce::dontSendNotification);
        repaint();
    }
}

void DrumGridView::setCurrentTool(EditorTool tool)
{
    currentTool = tool;
}

void DrumGridView::setGridResolution(double resolution)
{
    gridResolution = resolution;
    repaint();
}

void DrumGridView::setZoomLevel(float zoom)
{
    zoomFactor = juce::jlimit(0.1f, 4.0f, zoom);
    updateScrollbarRanges();
    repaint();
}

void DrumGridView::zoomIn()
{
    setZoomLevel(zoomFactor * 1.2f);
}

void DrumGridView::zoomOut()
{
    setZoomLevel(zoomFactor / 1.2f);
}

void DrumGridView::setPlayheadPosition(double positionInQuarterNotes)
{
    playheadPosition = positionInQuarterNotes;
    repaint();
}

void DrumGridView::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart)
{
    if (scrollBar == &horizontalScrollbar)
    {
        scrollOffsetX = newRangeStart;
        repaint();
    }
}

void DrumGridView::drawGrid(juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    // Background
    g.setColour(juce::Colour(0xFF252525));
    g.fillRect(gridArea);

    // Calculate visible time range - extend to effective timeline duration
    double viewEndTime = scrollOffsetX + (gridArea.getWidth() / (GRID_WIDTH_PER_QN * zoomFactor));
    viewEndTime = juce::jmin(viewEndTime, getEffectiveTimelineDuration());

    // Draw vertical grid lines (quarter notes)
    g.setColour(juce::Colour(0xFF404040));
    int startQN = static_cast<int>(std::floor(scrollOffsetX));
    int endQN = static_cast<int>(std::ceil(viewEndTime));

    for (int qn = startQN; qn <= endQN; ++qn)
    {
        int x = timeToX(qn);
        if (x >= 0 && x < gridArea.getWidth())
        {
            g.drawVerticalLine(gridArea.getX() + x, gridArea.getY(), gridArea.getBottom());
        }
    }

    // Draw grid resolution lines (lighter)
    g.setColour(juce::Colour(0xFF303030));
    double currentTime = startQN;
    while (currentTime <= viewEndTime)
    {
        int x = timeToX(currentTime);
        if (x >= 0 && x < gridArea.getWidth())
        {
            g.drawVerticalLine(gridArea.getX() + x, gridArea.getY(), gridArea.getBottom());
        }
        currentTime += gridResolution;
    }

    // Draw horizontal row lines
    g.setColour(juce::Colour(0xFF303030));
    for (int row = 0; row <= getNumVisibleNotes(); ++row)
    {
        int y = row * ROW_HEIGHT;
        g.drawHorizontalLine(gridArea.getY() + y, gridArea.getX(), gridArea.getRight());
    }

    // Draw a visual indicator showing the original clip duration
    double clipDuration = clip.getDurationInQuarterNotes();
    if (clipDuration < getEffectiveTimelineDuration())
    {
        int clipEndX = timeToX(clipDuration);
        if (clipEndX >= 0 && clipEndX < gridArea.getWidth())
        {
            g.setColour(juce::Colour(0xFF404040).withAlpha(0.5f));
            g.fillRect(gridArea.getX() + clipEndX - 1, gridArea.getY(), 2, gridArea.getHeight());
        }
    }
}

void DrumGridView::drawRowLabels(juce::Graphics& g, juce::Rectangle<int> labelArea)
{
    g.setColour(juce::Colour(0xFF2A2A2A));
    g.fillRect(labelArea);

    g.setColour(juce::Colour(0xFF808080));
    g.setFont(12.0f);

    for (int i = 0; i < getNumVisibleNotes(); ++i)
    {
        DrumPartType part = visibleDrumParts[i];
        juce::String label = drumLibraryManager.getDrumPartName(part, clip.getSourceLibrary());

        int y = labelArea.getY() + i * ROW_HEIGHT;
        g.drawText(label, labelArea.getX() + 5, y, labelArea.getWidth() - 10, ROW_HEIGHT,
                   juce::Justification::centredLeft, true);
    }
}

void DrumGridView::drawNotes(juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    const auto& notes = clip.getNotes();

    for (const auto& note : notes)
    {
        // Cull notes outside visible area
        if (note.startTime + note.duration < scrollOffsetX)
            continue;

        double viewEndTime = scrollOffsetX + (gridArea.getWidth() / (GRID_WIDTH_PER_QN * zoomFactor));
        if (note.startTime > viewEndTime)
            continue;

        int x = timeToX(note.startTime);
        int width = juce::jmax(2, timeToX(note.startTime + note.duration) - x);
        int row = drumPartToRow(note.drumPart);
        int y = row * ROW_HEIGHT;

        // Note rectangle
        auto noteRect = juce::Rectangle<int>(
            gridArea.getX() + x,
                                             gridArea.getY() + y + 2,
                                             width,
                                             ROW_HEIGHT - 4
        );

        // Color based on drum part and selection
        if (note.isSelected)
        {
            g.setColour(juce::Colour(0xFF00AAFF));  // Selected = bright blue
        }

        else
        {
            DrumLibrary targetLib = processor.getTargetLibrary();

            juce::Colour noteColour = MidiDissector::getColourForNote(
                note.noteNumber,
                targetLib,
                &drumLibraryManager);

            float brightness = 0.4f + (note.velocity / 127.0f) * 0.4f;
            noteColour = noteColour.withBrightness(brightness);
            g.setColour(noteColour);
        }

        g.fillRect(noteRect);

        // Border
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawRect(noteRect, 1);
    }
}

void DrumGridView::drawPlayhead(juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    int x = timeToX(playheadPosition);

    if (x >= 0 && x < gridArea.getWidth())
    {
        g.setColour(juce::Colour(0xFFFF6600));
        g.fillRect(gridArea.getX() + x - 1, gridArea.getY(), 2, gridArea.getHeight());

        // Draw playhead triangle at top
        juce::Path triangle;
        int triSize = 8;
        triangle.addTriangle(
            gridArea.getX() + x - triSize / 2, gridArea.getY(),
                             gridArea.getX() + x + triSize / 2, gridArea.getY(),
                             gridArea.getX() + x, gridArea.getY() + triSize
        );
        g.fillPath(triangle);
    }
}

void DrumGridView::drawSelectionBox(juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    if (!isBoxSelecting)
        return;

    int x1 = juce::jmin(selectionStart.x, selectionEnd.x);
    int y1 = juce::jmin(selectionStart.y, selectionEnd.y);
    int x2 = juce::jmax(selectionStart.x, selectionEnd.x);
    int y2 = juce::jmax(selectionStart.y, selectionEnd.y);

    auto box = juce::Rectangle<int>(gridArea.getX() + x1, gridArea.getY() + y1,
                                    x2 - x1, y2 - y1);

    g.setColour(juce::Colour(0x33FFFFFF));
    g.fillRect(box);

    g.setColour(juce::Colour(0xFFFFFFFF));
    g.drawRect(box, 1);
}

void DrumGridView::drawLoopRegion(juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    if (!loopRegionDragEnabled)
        return;

    int loopStartX = timeToX(loopStartQN);
    int loopEndX = timeToX(loopEndQN);

    // Skip if loop region is completely outside visible area
    if (loopEndX < 0 || loopStartX > gridArea.getWidth())
        return;

    // Draw semi-transparent overlay
    g.setColour(juce::Colour(0x1100FF00));
    g.fillRect(gridArea.getX() + loopStartX, gridArea.getY(),
               loopEndX - loopStartX, gridArea.getHeight());

    // Draw loop start marker
    g.setColour(juce::Colour(0xFF00FF00));
    g.fillRect(gridArea.getX() + loopStartX - 2, gridArea.getY(), 4, gridArea.getHeight());

    // Draw loop end marker
    g.setColour(juce::Colour(0xFFFF6600));
    g.fillRect(gridArea.getX() + loopEndX - 2, gridArea.getY(), 4, gridArea.getHeight());

    // Draw handles at top for dragging
    int handleSize = 12;
    int handleY = gridArea.getY() + 5;

    // Start handle (triangle pointing right)
    juce::Path startHandle;
    startHandle.addTriangle(
        gridArea.getX() + loopStartX - 2, handleY,
                            gridArea.getX() + loopStartX - 2, handleY + handleSize,
                            gridArea.getX() + loopStartX + handleSize - 2, handleY + handleSize / 2
    );
    g.setColour(juce::Colour(0xFF00FF00));
    g.fillPath(startHandle);

    // End handle (triangle pointing left)
    juce::Path endHandle;
    endHandle.addTriangle(
        gridArea.getX() + loopEndX + 2, handleY,
                          gridArea.getX() + loopEndX + 2, handleY + handleSize,
                          gridArea.getX() + loopEndX - handleSize + 2, handleY + handleSize / 2
    );
    g.setColour(juce::Colour(0xFFFF6600));
    g.fillPath(endHandle);
}

bool DrumGridView::isOverLoopMarker(int x, int y, bool& isStart, bool& isEnd)
{
    if (!loopRegionDragEnabled)
        return false;

    auto bounds = getLocalBounds();
    auto gridArea = bounds.reduced(0, 16).withTrimmedLeft(LABEL_WIDTH).withTrimmedRight(16);

    // Check if Y is within grid area
    if (y < gridArea.getY() || y > gridArea.getBottom())
        return false;

    int loopStartX = gridArea.getX() + timeToX(loopStartQN);
    int loopEndX = gridArea.getX() + timeToX(loopEndQN);

    int tolerance = 10;

    isStart = std::abs(x - loopStartX) < tolerance;
    isEnd = std::abs(x - loopEndX) < tolerance;

    return isStart || isEnd;
}

void DrumGridView::setLoopRegion(double startQN, double endQN)
{
    loopStartQN = startQN;
    loopEndQN = endQN;
    repaint();
}

int DrumGridView::timeToX(double quarterNotes) const
{
    return static_cast<int>((quarterNotes - scrollOffsetX) * GRID_WIDTH_PER_QN * zoomFactor);
}

double DrumGridView::xToTime(int x) const
{
    return scrollOffsetX + (x / (GRID_WIDTH_PER_QN * zoomFactor));
}

int DrumGridView::noteToRow(int noteNumber) const
{
    return getNumVisibleNotes() - 1 - (noteNumber % getNumVisibleNotes());
}

int DrumGridView::rowToNote(int row) const
{
    DrumPartType part = rowToDrumPart(row);
    return drumLibraryManager.getNoteNumberForDrumPart(part, clip.getSourceLibrary());
}

DrumPartType DrumGridView::rowToDrumPart(int row) const
{
    if (row >= 0 && row < static_cast<int>(visibleDrumParts.size()))
        return visibleDrumParts[row];

    return DrumPartType::Other;
}

int DrumGridView::drumPartToRow(DrumPartType part) const
{
    // Find the row index for this drum part
    for (size_t i = 0; i < visibleDrumParts.size(); ++i)
    {
        if (visibleDrumParts[i] == part)
            return static_cast<int>(i);
    }

    return 0;
}

double DrumGridView::snapToGrid(double time) const
{
    if (gridResolution <= 0.0)
        return time;

    return std::round(time / gridResolution) * gridResolution;
}

MidiNote* DrumGridView::getNoteAtPosition(int x, int y)
{
    double clickTime = xToTime(x);
    int clickRow = y / ROW_HEIGHT;
    DrumPartType clickPart = rowToDrumPart(clickRow);

    auto& notes = clip.getNotes();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& note = notes[i];
        if (note.drumPart == clickPart)
        {
            if (clickTime >= note.startTime && clickTime <= note.startTime + note.duration)
            {
                return const_cast<MidiNote*>(&note);
            }
        }
    }

    return nullptr;
}

void DrumGridView::handlePencilTool(int x, int y)
{
    if (isReadOnly) return;

    double time = snapToGrid(xToTime(x));
    int row = (y - ROW_HEIGHT) / ROW_HEIGHT;

    if (row < 0 || row >= getNumVisibleNotes())
        return;

    DrumPartType part = rowToDrumPart(row);
    int noteNumber = rowToNote(row);

    // Check if note already exists at this position
    const auto& notes = clip.getNotes();
    for (const auto& note : notes)
    {
        if (std::abs(note.startTime - time) < 0.001 && note.noteNumber == noteNumber)
            return; // Note already exists
    }

    // Create and add note
    MidiNote newNote;
    newNote.noteNumber = noteNumber;
    newNote.startTime = time;
    newNote.duration = gridResolution;
    newNote.velocity = 100;
    newNote.drumPart = part;
    newNote.id = juce::Uuid().toString();

    if (undoManager)
    {
        undoManager->perform(std::make_unique<AddNoteCommand>(clip, newNote));
    }
    else
    {
        clip.addNote(newNote);
    }

    // Preview the note
    if (previewNotesEnabled && onPlayPreviewNote)
        onPlayPreviewNote(newNote.noteNumber, newNote.velocity);

    if (onClipModified)
        onClipModified();

    repaint();
}

void DrumGridView::handleEraserTool(int x, int y)
{
    if (isReadOnly) return;

    MidiNote* note = getNoteAtPosition(x, y);
    if (note)
    {
        if (undoManager)
        {
            undoManager->perform(std::make_unique<RemoveNoteCommand>(clip, *note));
        }
        else
        {
            clip.removeNote(note->id);
        }

        if (onClipModified)
            onClipModified();

        repaint();
    }
}

void DrumGridView::handleSelectTool(int x, int y, bool isShiftDown)
{
    auto* note = getNoteAtPosition(x, y);

    if (note)
    {
        if (!isShiftDown && !note->isSelected)
            clip.clearSelection();

        note->isSelected = true;

        // Preview the note when selected
        if (previewNotesEnabled && onPlayPreviewNote)
            onPlayPreviewNote(note->noteNumber, note->velocity);
    }
    else if (!isShiftDown)
    {
        clip.clearSelection();
    }
}

void DrumGridView::startNoteDrag(const juce::String& noteId, int x, int y)
{
    isDragging = true;
    draggedNoteId = noteId;
    dragStartX = x;
    dragStartY = y;

    auto* note = clip.findNote(noteId);
    if (note)
    {
        dragStartTime = note->startTime;
        dragStartNote = note->noteNumber;
    }
}

void DrumGridView::updateNoteDrag(int x, int y)
{
    auto* note = clip.findNote(draggedNoteId);
    if (!note)
        return;

    double newTime = snapToGrid(xToTime(x));
    // Allow dragging to the extended timeline duration
    newTime = juce::jlimit(0.0, getEffectiveTimelineDuration() - note->duration, newTime);

    int newRow = y / ROW_HEIGHT;
    newRow = juce::jlimit(0, getNumVisibleNotes() - 1, newRow);

    DrumPartType newPart = rowToDrumPart(newRow);
    int newNoteNumber = drumLibraryManager.getNoteNumberForDrumPart(newPart, clip.getSourceLibrary());

    clip.moveNote(draggedNoteId, newTime, newNoteNumber);

    // Extend clip duration if the note goes beyond current clip duration
    double noteEndTime = newTime + note->duration;
    if (noteEndTime > clip.getDurationInQuarterNotes())
    {
        // Calculate how many bars are needed
        int barsNeeded = static_cast<int>(std::ceil(noteEndTime / clip.getTimeSignatureNumerator()));
        clip.setLengthInBars(barsNeeded);
        updateScrollbarRanges();
    }

    repaint();
}

void DrumGridView::finishNoteDrag()
{
    if (!isDragging || draggingNoteId.isEmpty() || isReadOnly)
    {
        isDragging = false;
        draggingNoteId = juce::String();
        return;
    }

    MidiNote* note = clip.findNote(draggingNoteId);
    if (note && undoManager)
    {
        // Store original position before drag started
        double newTime = note->startTime;
        int newNote = note->noteNumber;

        // Restore to original position temporarily
        clip.moveNote(draggingNoteId, dragStartTime, dragStartNote);

        // Use undo command to move to new position
        undoManager->perform(std::make_unique<MoveNoteCommand>(
            clip, draggingNoteId, dragStartTime, dragStartNote, newTime, newNote));
    }

    isDragging = false;
    draggingNoteId = juce::String();

    if (onClipModified)
        onClipModified();

    repaint();
}

void DrumGridView::startBoxSelection(int x, int y)
{
    isBoxSelecting = true;
    selectionStart = juce::Point<int>(x, y);
    selectionEnd = selectionStart;
}

void DrumGridView::updateBoxSelection(int x, int y)
{
    selectionEnd = juce::Point<int>(x, y);
    repaint();
}

void DrumGridView::finishBoxSelection()
{
    if (!isBoxSelecting)
        return;

    // Calculate selection rectangle
    int minX = juce::jmin(selectionStart.x, selectionEnd.x);
    int maxX = juce::jmax(selectionStart.x, selectionEnd.x);
    int minY = juce::jmin(selectionStart.y, selectionEnd.y);
    int maxY = juce::jmax(selectionStart.y, selectionEnd.y);

    double minTime = xToTime(minX);
    double maxTime = xToTime(maxX);
    int minRow = minY / ROW_HEIGHT;
    int maxRow = maxY / ROW_HEIGHT;

    // Select all notes in rectangle
    auto& notes = clip.getNotes();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& note = const_cast<MidiNote&>(notes[i]);
        int noteRow = drumPartToRow(note.drumPart);

        if (note.startTime >= minTime && note.startTime <= maxTime &&
            noteRow >= minRow && noteRow <= maxRow)
        {
            note.isSelected = true;
        }
    }

    isBoxSelecting = false;
    repaint();
}

double DrumGridView::getEffectiveTimelineDuration() const
{
    // Return the maximum timeline duration for extended scrolling
    return MAX_TIMELINE_DURATION;
}

void DrumGridView::updateScrollbarRanges()
{
    // Use extended timeline duration instead of just clip duration
    double timelineDuration = getEffectiveTimelineDuration();
    int gridWidth = getWidth() - LABEL_WIDTH - 16;

    double visibleDuration = gridWidth / (GRID_WIDTH_PER_QN * zoomFactor);

    horizontalScrollbar.setRangeLimits(0.0, timelineDuration);
    horizontalScrollbar.setCurrentRange(scrollOffsetX,
                                        juce::jmin(scrollOffsetX + visibleDuration, timelineDuration));
}
