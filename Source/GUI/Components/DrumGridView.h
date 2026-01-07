#pragma once

#include <JuceHeader.h>
#include "EditableMidiClip.h"
#include "../EditorTools/EditorTool.h"
#include "../EditorTools/MidiNoteCommands.h"
#include "DrumLibraryManager.h"

// Forward declaration
class DrumGrooveProcessor;

class DrumGridView : public juce::Component,
public juce::ScrollBar::Listener
{
public:
    DrumGridView(EditableMidiClip& clip, DrumLibraryManager& libManager, DrumGrooveProcessor& proc);
    ~DrumGridView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Tool and grid settings
    void setCurrentTool(EditorTool tool);  // NOT inline, implemented in .cpp
    EditorTool getCurrentTool() const { return currentTool; }
    void setGridResolution(double resolution);
    double getGridResolution() const { return gridResolution; }

    // Zoom controls
    void setZoomLevel(float zoom);
    float getZoomLevel() const { return zoomFactor; }  // Returns zoomFactor, not zoomLevel
    void zoomIn();
    void zoomOut();

    // Scroll position
    double getScrollOffsetX() const { return scrollOffsetX; }
    juce::ScrollBar& getHorizontalScrollBar() { return horizontalScrollbar; }

    // Grid spacing
    float getPixelsPerQuarterNote() const { return GRID_WIDTH_PER_QN; }

    // Read-only mode
    void setReadOnly(bool readOnly) { isReadOnly = readOnly; }
    bool getReadOnly() const { return isReadOnly; }

    void setUndoManager(MidiEditorUndoManager* manager) { undoManager = manager; }

    void setPreviewNotesEnabled(bool enabled) { previewNotesEnabled = enabled; }

    std::function<void(int, int)> onPlayPreviewNote;

    // Playback
    void setPlayheadPosition(double positionInQuarterNotes);

    // Loop region
    void setLoopRegion(double startQN, double endQN);
    void setLoopRegionDragEnabled(bool enabled) { loopRegionDragEnabled = enabled; repaint(); }

    // Refresh
    void refreshForNewClip();
    void updateVisibleDrumParts();

    // Scrollbar listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    // Callbacks
    std::function<void()> onClipModified;
    std::function<void(double, double)> onLoopRegionChanged;

private:
    EditableMidiClip& clip;
    DrumLibraryManager& drumLibraryManager;
    DrumGrooveProcessor& processor;

    // View parameters
    float zoomFactor = 1.0f;
    double scrollOffsetX = 0.0;
    double gridResolution = 0.25; // 16th note
    EditorTool currentTool = EditorTool::Pencil;
    bool isReadOnly = false;
    bool previewNotesEnabled = false;

    // Layout constants
    static constexpr int ROW_HEIGHT = 30;
    static constexpr int LABEL_WIDTH = 120;
    static constexpr float GRID_WIDTH_PER_QN = 60.0f;  // NOT PIXELS_PER_QUARTER_NOTE!
    static constexpr double MAX_TIMELINE_DURATION = 1000.0; // Maximum quarter notes (250 bars at 4/4)

    // Playhead
    double playheadPosition = 0.0;

    // Loop region
    double loopStartQN = 0.0;
    double loopEndQN = 4.0;
    bool isDraggingLoopStart = false;
    bool isDraggingLoopEnd = false;
    bool loopRegionDragEnabled = false;  // Default OFF

    // Visible drum parts (filtered by library)
    std::vector<DrumPartType> visibleDrumParts;

    // Scrollbars
    juce::ScrollBar horizontalScrollbar{false};  // NOT horizontalScrollBar!

    // Mouse interaction state
    bool isDragging = false;
    bool isBoxSelecting = false;
    juce::Point<int> dragStartPos;
    juce::Point<int> dragCurrentPos;
    juce::String draggingNoteId;
    int dragNoteOffsetTicks = 0;
    int dragRowOffset = 0;

    // Note dragging (additional state)
    juce::String draggedNoteId;
    int dragStartX = 0;
    int dragStartY = 0;
    double dragStartTime = 0.0;
    int dragStartNote = 0;

    // Box selection
    juce::Point<int> selectionStart;
    juce::Point<int> selectionEnd;

    MidiEditorUndoManager* undoManager = nullptr;

    // Helper methods
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> gridArea);
    void drawRowLabels(juce::Graphics& g, juce::Rectangle<int> labelArea);
    void drawNotes(juce::Graphics& g, juce::Rectangle<int> gridArea);
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<int> gridArea);
    void drawSelectionBox(juce::Graphics& g, juce::Rectangle<int> gridArea);
    void drawLoopRegion(juce::Graphics& g, juce::Rectangle<int> gridArea);

    // Loop region helpers
    bool isOverLoopMarker(int x, int y, bool& isStart, bool& isEnd);

    // Coordinate conversion
    int timeToX(double quarterNotes) const;
    double xToTime(int x) const;
    int noteToRow(int noteNumber) const;
    int rowToNote(int row) const;
    DrumPartType rowToDrumPart(int row) const;
    int drumPartToRow(DrumPartType part) const;

    // Visible notes helper
    int getNumVisibleNotes() const { return static_cast<int>(visibleDrumParts.size()); }

    // Grid snapping
    double snapToGrid(double time) const;

    // Hit testing
    MidiNote* getNoteAtPosition(int x, int y);

    // Note manipulation based on current tool
    void handlePencilTool(int x, int y);
    void handleEraserTool(int x, int y);
    void handleSelectTool(int x, int y, bool isShiftDown);
    void startNoteDrag(const juce::String& noteId, int x, int y);
    void updateNoteDrag(int x, int y);
    void finishNoteDrag();

    // Box selection
    void startBoxSelection(int x, int y);
    void updateBoxSelection(int x, int y);
    void finishBoxSelection();

    // Scrollbar setup
    void updateScrollbarRanges();

    // Helper to get effective timeline duration (clip duration or MAX_TIMELINE_DURATION)
    double getEffectiveTimelineDuration() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGridView)
};
