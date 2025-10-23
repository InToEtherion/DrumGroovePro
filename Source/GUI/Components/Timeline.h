#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../PluginProcessor.h"
#include "../../Core/MidiDissector.h"
#include <deque>

struct MidiClip
{
    juce::String name;
    juce::File file;
    double startTime = 0.0;
    double duration = 4.0;
    double originalBPM = 120.0;
    juce::Colour colour;
    bool isSelected = false;
    juce::String id = juce::Uuid().toString();
};

// Forward declarations for command classes
class TimelineCommand;
class AddClipCommand;
class DeleteClipsCommand;
class MoveClipsCommand;
class ResizeClipsCommand;
class TrackHeader;

class Timeline : public juce::Component,
public juce::DragAndDropTarget,
    public juce::ChangeBroadcaster,
        public juce::KeyListener,
            private juce::Timer
            {
            public:
                Timeline(DrumGrooveProcessor& processor);
                ~Timeline() override;

                void paint(juce::Graphics& g) override;
                void resized() override;

                // Mouse handling with multi-selection support (NO double-click playback)
                void mouseDown(const juce::MouseEvent& e) override;
                void mouseDrag(const juce::MouseEvent& e) override;
                void mouseUp(const juce::MouseEvent& e) override;
                void mouseMove(const juce::MouseEvent& e) override;
                void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

                // Keyboard handling
                bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
                bool keyPressed(const juce::KeyPress& key) override;

                // Drag and drop with BPM inheritance
                bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
                void itemDragEnter(const SourceDetails& dragSourceDetails) override;
                void itemDragMove(const SourceDetails& dragSourceDetails) override;
                void itemDragExit(const SourceDetails& dragSourceDetails) override;
                void itemDropped(const SourceDetails& dragSourceDetails) override;

                // Playback control with precise timing
                void play();
                void pause();
                void stop();
                bool isPlaying() const { return playing; }
                double getPlayheadPosition() const { return playheadPosition; }
                void setPlayheadPosition(double timeInSeconds);

                // Timeline control
                void setZoom(float pixelsPerSecond);
                float getZoom() const { return zoomLevel; }
                void fitToContent();
                void setAutoScroll(bool enabled) { autoScrollEnabled = enabled; }

                // Selection control (independent from loop)
                bool hasSelection() const { return selectionValid || isSettingSelection; }
                bool hasValidSelection() const { return selectionValid; }
                double getSelectionStart() const { return selectionStart; }
                double getSelectionEnd() const { return selectionEnd; }
                void setSelectionStart(double time);
                void setSelectionEnd(double time);
                void clearSelection();

                // Loop control (uses selection if available)
                void toggleLoop();
                bool isLoopEnabled() const { return loopEnabled; }

                // Time formatting
                juce::String formatTime(double seconds) const;
                double parseTime(const juce::String& timeStr) const;

                // Grid
                void setGridInterval(double seconds) { gridInterval = seconds; repaint(); }
                double getGridInterval() const { return gridInterval; }

                // Clip management with undo/redo
                void addClip(const MidiClip& clip, bool recordUndo = true);
                void removeSelectedClips();
                void selectAll();
                void deselectAll();
                void deleteSelectedClips();
                void clearAllClips();

                // Undo/Redo
                void undo();
                void redo();
                bool canUndo() const { return currentUndoIndex > 0; }
                bool canRedo() const { return currentUndoIndex < undoStack.size(); }

                // Time conversion with consistent BPM scaling
                double pixelsToTime(float pixels) const;
                float timeToPixels(double time) const;

                // Selection helpers
                void selectClipsInRange(double startTime, double endTime);
                std::vector<MidiClip*> getSelectedClips();

                // Track BPM handling
                void onTrackBPMChanged();
                double getTrackBPM() const;
                bool isTrackMuted() const;
                bool hasClips() const { return !clips.empty(); }
                void inheritBPMFromHeader(); // Called when first clip is added

                // Context menu
                void showRightClickMenu(const juce::Point<int>& position);

                // Clip selection callback
                std::function<void(const juce::File&)> onClipSelected;

                // Public access for Timeline command classes
                std::vector<std::unique_ptr<MidiClip>> clips;

            private:
                DrumGrooveProcessor& processor;

                // Track header component
                std::unique_ptr<TrackHeader> trackHeader;

                std::unique_ptr<MidiClip> ghostClip;

                // Playback state with high-precision timing
                bool playing = false;
                double playheadPosition = 0.0;
                double lastPlaybackTime = 0.0;
                bool autoScrollEnabled = true;

                // View state
                float zoomLevel = 100.0f;
                double viewStartTime = 0.0;
                double gridInterval = 1.0;

                // Selection state (independent from loop)
                bool selectionValid = false;
                double selectionStart = 0.0;
                double selectionEnd = 0.0;
                bool isSettingSelection = false;
                double selectionDragStart = 0.0;

                // Loop state (uses selection when enabled)
                bool loopEnabled = false;

                // Interaction state
                bool isDragging = false;
                bool isResizing = false;
                bool isResizingLeft = false;
                bool isSelecting = false;
                bool isAddingToSelection = false;
                bool isRangeSelecting = false;
                juce::Point<float> dragStartPoint;
                juce::Rectangle<float> selectionBox;
                float dropIndicatorX = -1;
                juce::String lastSelectedClipId;

                // Resizing state
                MidiClip* resizingClip = nullptr;
                double resizeStartTime = 0.0;
                double resizeStartDuration = 0.0;

                // Dragging state
                std::vector<std::pair<juce::String, double>> draggedClips;

                // Undo/Redo system
                std::vector<std::unique_ptr<TimelineCommand>> undoStack;
                size_t currentUndoIndex = 0;
                static constexpr size_t MAX_UNDO_LEVELS = 50;

                // Layout constants
                static constexpr int RULER_HEIGHT = 30;
                static constexpr int TRACK_HEIGHT = 80;
                static constexpr int RESIZE_HANDLE_WIDTH = 8;
                static constexpr int TRACK_HEADER_WIDTH = 200;

                // High-precision timing
                void startHighPrecisionTimer();

                // Drawing methods
                void drawRuler(juce::Graphics& g);
                void drawGrid(juce::Graphics& g);
                void drawClips(juce::Graphics& g);
                void drawPlayhead(juce::Graphics& g);
                void drawSelectionRegion(juce::Graphics& g);
                void drawDropIndicator(juce::Graphics& g);
                void drawGhostClip(juce::Graphics& g);
                void drawSelectionBox(juce::Graphics& g);
                void drawTooltip(juce::Graphics& g);
                void drawMidiDotsInClip(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> clipBounds);

                // Timer callback for playback and auto-scroll
                void timerCallback() override;

                // Helper methods
                MidiClip* getClipAt(const juce::Point<float>& point);
                double snapToGrid(double time) const;
                void updateAutoScroll();
                void addUndoCommand(std::unique_ptr<TimelineCommand> command);
                double getMaxTime() const;

                // BPM scaling helpers
                double getVisualScaleFactor() const;
                juce::Rectangle<int> getTimelineArea() const;

                // Drag and drop handlers with BPM inheritance
                void handleDrumPartDrop(const juce::StringArray& parts, const juce::Point<int>& position);
                void handleMidiFileDrop(const juce::StringArray& parts, const juce::Point<int>& position);
                bool createDrumPartMidiFile(const juce::File& originalFile, DrumPartType partType, juce::File& outputFile);
                bool calculateMidiFileDuration(const juce::File& midiFile, double& duration);
                void scheduleTemporaryFileCleanup(const juce::File& tempFile);

                // Tooltip support
                juce::String currentTooltip;
                juce::Point<int> tooltipPosition;

                JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Timeline)
            };

            // Command base class
            class TimelineCommand
            {
            public:
                virtual ~TimelineCommand() = default;
                virtual void execute() = 0;
                virtual void undo() = 0;
            };