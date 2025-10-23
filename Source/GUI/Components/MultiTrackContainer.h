#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include "Track.h"
#include "TrackHeader.h"
#include "../../PluginProcessor.h"
#include "TimelineManager.h"

// Forward declaration
class DrumGrooveProcessor;

// TimelineContent - scrollable content that holds tracks (WITHOUT headers)
class TimelineContent : public juce::Component
{
public:
    explicit TimelineContent(DrumGrooveProcessor& processor);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateSize(double maxTime, float zoomLevel);
    
    // Store reference to tracks for rendering
    void setTracks(std::vector<std::unique_ptr<Track>>* trackList);
    
    // Store zoom level for resizing
    void setZoomLevel(float zoom) { zoomLevel = zoom; }
    
    // Set container reference for selection rendering
    void setContainer(class MultiTrackContainer* cont) { container = cont; }
    
private:
    DrumGrooveProcessor& processor;
    std::vector<std::unique_ptr<Track>>* tracks = nullptr;
    class MultiTrackContainer* container = nullptr;
    float zoomLevel = 100.0f;
    
    // Guard to prevent infinite resize loops
    bool isUpdating = false;  // ADD THIS
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineContent)
};

// FixedHeaderColumn - non-scrolling column for track headers
class FixedHeaderColumn : public juce::Component
{
public:
    explicit FixedHeaderColumn(DrumGrooveProcessor& processor);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setHeaders(std::vector<std::unique_ptr<TrackHeader>>* headerList);
    void updateSize();  // Update size based on number of headers
    
private:
    DrumGrooveProcessor& processor;
    std::vector<std::unique_ptr<TrackHeader>>* headers = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FixedHeaderColumn)
};

//==============================================================================
// FixedRulerRow - non-scrolling ruler that stays at top
class FixedRulerRow : public juce::Component
{
public:
    explicit FixedRulerRow(DrumGrooveProcessor& processor);
    
    void paint(juce::Graphics& g) override;
    void setZoomLevel(float newZoomLevel);
    void setViewportX(int x);
    void setContentWidth(int width);
    
    // Mouse handling for playhead and region selection
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    
    // Set container reference for playhead control
    void setContainer(MultiTrackContainer* cont) { container = cont; }
    
private:
    void drawRuler(juce::Graphics& g);
    
    DrumGrooveProcessor& processor;
    MultiTrackContainer* container = nullptr;
    float zoomLevel = 100.0f;
    int viewportX = 0;
    int contentWidth = 0;
    
    // Region selection
    bool isDraggingRegion = false;
    double regionStartTime = 0.0;
    double regionEndTime = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FixedRulerRow)
};

class MultiTrackContainer : public juce::Component,
                            public juce::Timer,
                            public juce::KeyListener,
                            public juce::DragAndDropTarget,
                            public juce::ChangeBroadcaster,
                            public juce::ScrollBar::Listener
{
public:
    explicit MultiTrackContainer(DrumGrooveProcessor& processor);
    ~MultiTrackContainer() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Keyboard handling
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    
    // ScrollBar listener
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    // State persistence - production requirement
    juce::ValueTree saveGuiState() const;
    void restoreGuiState(const juce::ValueTree& state);
    
    // File menu operations
    void saveTimelineState();
    void loadTimelineState();
    void exportTimelineAsMidi();
    void exportTimelineAsSeparateMidis();
    void beginDragOfSelectedClips(const juce::MouseEvent& e);
    
    // Drag and drop to external DAWs
    void exportSelectedClipsForDragDrop(juce::DragAndDropContainer& dragContainer);

    // Drag and drop
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    // Playback control
    void play();
    void pause();
    void stop();
    void setPlayheadPosition(double timeInSeconds);
    void setLoopStart(double timeInSeconds);
    void setLoopEnd(double timeInSeconds);
    void setTimelineControls(class TimelineControls* controls) { timelineControls = controls; }
    double getPlayheadPosition() const { return playheadPosition; }
    bool isPlaying() const { return playing; }

    // Timeline control
    void setZoom(float pixelsPerSecond);
    float getZoom() const { return zoomLevel; }
    void fitToContent();
    void updateTimelineSize();
    void updateGridInterval();
    
    // Scroll position management
    void setScrollPosition(int horizontalPos, int verticalPos);
    juce::Point<int> getScrollPosition() const;
    

    // Selection control
    void setSelectionStart(double time);
    void setSelectionEnd(double time);
    void clearSelection();
    bool hasSelection() const { return selectionValid; }
    double getSelectionStart() const { return selectionStart; }
    double getSelectionEnd() const { return selectionEnd; }
    
    // Track control
    void handleSoloChange(int soloedTrackIndex);
    bool isTrackSoloed(int trackIndex) const;
    void updateTrackPlaybackStates();
    double getTrackBPM(int trackIndex) const;

    // Loop control
    void toggleLoop();
    bool isLoopEnabled() const { return loopEnabled; }

    // Time formatting
    juce::String formatTime(double seconds) const;
    double parseTime(const juce::String& timeStr) const;

    // Time conversion
    double pixelsToTime(float pixels) const;
    float timeToPixels(double time) const;
    double visualPixelsToTime(float pixels) const;
    float visualTimeToPixels(double time) const;
    double snapToGrid(double time) const;

    // Track management
    Track* getTrack(int index) const;
    int getNumTracks() const { return static_cast<int>(tracks.size()); }
    void addTrack();  // Add a new track dynamically
    void removeTrack(int trackIndex);  // Remove a track or clear it if index < 3
    void selectTrack(int trackIndex, bool multiSelect = false, bool toggleMode = false);  // Select a specific track
    void clearTrackSelection();  // Clear all track selections
    int getSelectedTrackIndex() const { return selectedTrackIndex; }

    // Track state queries - ADDED for compilation fix
    bool isTrackMuted(int trackIndex) const;
       
       // Track and clip access for TimelineManager
       std::vector<const MidiClip*> getTrackClips(int trackIndex) const;
       std::vector<const MidiClip*> getSelectedClips(int trackIndex) const;
       double getMasterBPM() const;
    
    // Viewport access for Track components - ADDED for compilation fix
    int getViewportX() const { return viewport.getViewPositionX(); }

    // Global operations
    void selectAllClips();
    void deselectAllClips();
    void deleteSelectedClips();
    void clearAllTracks();
    
    // Clipboard operations
    void copySelectedClips();
    void cutSelectedClips();
    void pasteClips();

    // BPM handling
    double getCurrentBPM() const;
    double getVisualScaleFactor() const;
    void onTrackBPMChanged();
	
	juce::String getTrackName(int trackIndex) const;

    // Callback for clip selection
    std::function<void(const juce::File&)> onClipSelected;

    // Constants
    static constexpr int TRACK_HEADER_WIDTH = 180;
    static constexpr int TRACK_HEIGHT = 80;
    static constexpr int RULER_HEIGHT = 30;
    static constexpr double MIN_TIMELINE_WIDTH_SECONDS = 120.0;
    static constexpr double BUFFER_TIME = 60.0;

private:
    DrumGrooveProcessor& processor;
    class TimelineControls* timelineControls = nullptr;  // Reference to timeline controls
    
    // SEPARATED: Headers and tracks
    std::unique_ptr<FixedHeaderColumn> fixedHeaderColumn;  // Fixed header column
    std::unique_ptr<FixedRulerRow> fixedRulerRow;          // Fixed ruler row
    std::unique_ptr<TimelineContent> timelineContent;      // Scrollable timeline
    juce::Viewport viewport;                                // Main timeline viewport
    juce::Viewport headerViewport;                          // Header column viewport
    
    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<std::unique_ptr<TrackHeader>> trackHeaders;  // Separate headers
    
    // Playback state
    bool playing = false;
    double playheadPosition = 0.0;
    double lastPlaybackTime = 0.0;
    bool autoScrollEnabled = true;

    // View state
    float zoomLevel = 100.0f;
    double gridInterval = 0.5;

    // Selection state
    bool selectionValid = false;
    bool isSettingSelection = false;
    double selectionStart = 0.0;
    int selectedTrackIndex = -1;
    std::set<int> selectedTrackIndices;
    
    // Clipboard for inter-track operations
    std::vector<MidiClip> clipboardClips;
    bool clipboardIsCut = false;
    
    // Scrollbar configuration
    static constexpr int SCROLLBAR_THICKNESS = 12;
    
    // State persistence
    void writeState(juce::ValueTree& state) const;
    void readState(const juce::ValueTree& state);
    juce::ValueTree saveCompleteState() const;
    
    // Timeline manager for file operations
    std::unique_ptr<TimelineManager> timelineManager;
    void restoreCompleteState(const juce::ValueTree& state);  // For multi-selection  // Currently selected track (-1 = none)
    double selectionEnd = 0.0;
    double selectionDragStart = 0.0;

    // Loop state
    bool loopEnabled = false;

    // Ghost clip for global drag/drop
    std::unique_ptr<MidiClip> globalGhostClip;
    double originalGhostDuration = 0.0;
    int currentTargetTrack = -1;

    // Helper methods
    void timerCallback() override;
    void updateAutoScroll();
    double getMaxTime() const;
    double getTimelineWidthInSeconds() const;
    
    void showRightClickMenu(const juce::Point<int>& position);
    void drawGrid(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void drawSelectionRegion(juce::Graphics& g);
    void drawGlobalGhostClip(juce::Graphics& g);
	
	// Manual vertical scrollbar (separate from viewport's automatic one)
    juce::ScrollBar manualVerticalScrollbar{true};  // true = vertical
    bool needsManualVerticalScrollbar = false;
	void updateScrollbarVisibility();
	
	// Overlay horizontal scrollbar (positioned over content, not below)
	juce::ScrollBar overlayHorizontalScrollbar{false};  // false = horizontal
	bool shouldShowVerticalScrollbar = false;
	
	bool isUpdatingLayout = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiTrackContainer)
};