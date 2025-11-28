#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include "Track.h"
#include "TrackHeader.h"
#include "AudioTrack.h"
#include "AudioTrackHeader.h"
#include "../../PluginProcessor.h"
#include "TimelineManager.h"

class DrumGrooveProcessor;

class TimelineContent : public juce::Component
{
public:
	explicit TimelineContent(DrumGrooveProcessor& processor);

	void paint(juce::Graphics& g) override;
	void paintOverChildren(juce::Graphics& g) override;  // ADDED FOR GRID OVER CLIPS
	void resized() override;
	void updateSize(double maxTime, float zoomLevel);

	void setTracks(std::vector<std::unique_ptr<Track>>* trackList);
	void setAudioTracks(std::vector<std::unique_ptr<AudioTrack>>* audioTrackList);
	void setZoomLevel(float zoom) { zoomLevel = zoom; }
	void setContainer(class MultiTrackContainer* cont) { container = cont; }

private:
	DrumGrooveProcessor& processor;
	std::vector<std::unique_ptr<Track>>* tracks = nullptr;
	std::vector<std::unique_ptr<AudioTrack>>* audioTracks = nullptr;
	class MultiTrackContainer* container = nullptr;
	float zoomLevel = 100.0f;
	bool isUpdating = false;

	void drawBarGrid(juce::Graphics& g);   // ADDED FOR GRID OVER CLIPS
	void drawTimeGrid(juce::Graphics& g);  // ADDED FOR GRID OVER CLIPS

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineContent)
};

class FixedHeaderColumn : public juce::Component
{
public:
	explicit FixedHeaderColumn(DrumGrooveProcessor& processor);

	void paint(juce::Graphics& g) override;
	void resized() override;

	void setHeaders(std::vector<std::unique_ptr<TrackHeader>>* headerList);
	void setAudioHeaders(std::vector<std::unique_ptr<AudioTrackHeader>>* audioHeaderList);
	void updateSize();

private:
	DrumGrooveProcessor& processor;
	std::vector<std::unique_ptr<TrackHeader>>* headers = nullptr;
	std::vector<std::unique_ptr<AudioTrackHeader>>* audioHeaders = nullptr;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FixedHeaderColumn)
};

class FixedRulerRow : public juce::Component
{
public:
	explicit FixedRulerRow(DrumGrooveProcessor& processor);

	void paint(juce::Graphics& g) override;
	void setZoomLevel(float newZoomLevel);
	void setViewportX(int x);
	void setContentWidth(int width);

	// NEW: Bar mode support
	void setBarMode(bool isBarMode);
	bool isBarMode() const { return barMode; }

	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
	void mouseMove(const juce::MouseEvent& e) override;
	void mouseDoubleClick(const juce::MouseEvent& e) override;

	void setContainer(MultiTrackContainer* cont) { container = cont; }

private:
	void drawRuler(juce::Graphics& g);
	void drawBarRuler(juce::Graphics& g); // NEW: Draw bar numbers instead of time
	void drawSelectionHandles(juce::Graphics& g);

	bool isMouseOverStartHandle(float mouseX) const;
	bool isMouseOverEndHandle(float mouseX) const;

	DrumGrooveProcessor& processor;
	MultiTrackContainer* container = nullptr;
	float zoomLevel = 100.0f;
	int viewportX = 0;
	int contentWidth = 0;
	bool barMode = false; // NEW: Track whether we're in bar mode

	bool isDraggingRegion = false;
	double regionStartTime = 0.0;
	double regionEndTime = 0.0;

	// Selection handle dragging
	enum class HandleDragMode { None, Start, End };
	HandleDragMode handleDragMode = HandleDragMode::None;
	static constexpr int HANDLE_WIDTH = 12;
	static constexpr int HANDLE_HEIGHT = 14;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FixedRulerRow)
};

class MultiTrackContainer : public juce::Component,
public juce::Timer,
	public juce::KeyListener,
		public juce::DragAndDropTarget,
			public juce::ChangeBroadcaster,
				public juce::ScrollBar::Listener,
					public juce::ChangeListener
					{
					public:
						explicit MultiTrackContainer(DrumGrooveProcessor& processor);
						~MultiTrackContainer() override;

						void paint(juce::Graphics& g) override;
						void paintOverChildren(juce::Graphics& g) override;
						void resized() override;

						void mouseDown(const juce::MouseEvent& e) override;
						void mouseDrag(const juce::MouseEvent& e) override;
						void mouseUp(const juce::MouseEvent& e) override;
						void mouseMove(const juce::MouseEvent& e) override;
						void mouseDoubleClick(const juce::MouseEvent& e) override;
						void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

						bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

						void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

						juce::ValueTree saveGuiState() const;
						void restoreGuiState(const juce::ValueTree& state);

						void saveTimelineState();
						void loadTimelineState();
						void exportTimelineAsMidi();
						void exportTimelineAsSeparateMidis();
						void beginDragOfSelectedClips(const juce::MouseEvent& e);

						void exportSelectedClipsForDragDrop(juce::DragAndDropContainer& dragContainer);

						bool isInterestedInDragSource(const SourceDetails& details) override;
						void itemDragEnter(const SourceDetails& details) override;
						void itemDragMove(const SourceDetails& details) override;
						void itemDragExit(const SourceDetails& details) override;
						void itemDropped(const SourceDetails& details) override;

						void play();
						void pause();
						void stop();
						void setPlayheadPosition(double timeInSeconds);
						void setLoopStart(double timeInSeconds);
						void setLoopEnd(double timeInSeconds);
						void setTimelineControls(class TimelineControls* controls) { timelineControls = controls; }
						double getPlayheadPosition() const;
						bool isPlaying() const { return playing; }

						void setZoom(float pixelsPerSecond, float mouseX = -1.0f);
						float getZoom() const { return zoomLevel; }
						void fitToContent();
						void updateTimelineSize();
						void updateGridInterval();

						void setScrollPosition(int horizontalPos, int verticalPos);
						juce::Point<int> getScrollPosition() const;

						void setSelectionStart(double time);
						void setSelectionEnd(double time);
						void clearSelection();
						bool hasSelection() const { return selectionValid; }
						double getSelectionStart() const { return selectionStart; }
						double getSelectionEnd() const { return selectionEnd; }
						void updateLoopRangeIfPlaying(double start, double end);

						void handleSoloChange(int soloedTrackIndex);
						bool isTrackSoloed(int trackIndex) const;
						void updateTrackPlaybackStates();
						double getTrackBPM(int trackIndex) const;

						void toggleLoop();
						bool isLoopEnabled() const { return loopEnabled; }

						// NEW: Time/Bar Mode Toggle Methods
						void toggleTimeBarMode();
						void setBarMode(bool shouldUseBarMode);
						bool isBarMode() const { return barMode; }

						juce::String formatTime(double seconds) const;
						double parseTime(const juce::String& timeStr) const;

						double pixelsToTime(float pixels) const;
						float timeToPixels(double time) const;
						double visualPixelsToTime(float pixels) const;
						float visualTimeToPixels(double time) const;
						double snapToGrid(double time) const;
						struct DynamicBarInfo
						{
							double startTime;
							double effectiveWidth;
							double standardWidth;
							int numerator;
							int sectionIndex;
							int barNumber;
						};

						std::vector<DynamicBarInfo> calculateDynamicBarWidths() const;
						double getTrackDivision(int trackIndex) const;
						void changeListenerCallback(juce::ChangeBroadcaster* source) override;

						Track* getTrack(int index) const;
						TrackHeader* getTrackHeader(int index);
						const TrackHeader* getTrackHeader(int index) const;

						DrumGrooveProcessor& getProcessor() { return processor; }
						const DrumGrooveProcessor& getProcessor() const { return processor; }

						int getNumTracks() const { return static_cast<int>(tracks.size()); }
						void addAudioTrack(const juce::File& audioFile);
						void addTrack();
						void removeTrack(int trackIndex);
						void selectTrack(int trackIndex, bool multiSelect = false, bool toggleMode = false);
						void clearTrackSelection();
						int getSelectedTrackIndex() const { return selectedTrackIndex; }

						bool isTrackMuted(int trackIndex) const;

						std::vector<const MidiClip*> getTrackClips(int trackIndex) const;
						std::vector<const MidiClip*> getSelectedClips(int trackIndex) const;
						double getMasterBPM() const;

						int getViewportX() const { return viewport.getViewPositionX(); }

						void selectAllClips();
						void deselectAllClips();
						void deleteSelectedClips();
						void clearAllTracks();

						// Global Undo/Redo system
						void undo();
						void redo();
						bool canUndo() const;
						bool canRedo() const;
						void addUndoCommand(std::unique_ptr<TrackCommand> command, bool executeNow = true);
						void clearUndoHistory();

						void copySelectedClips();
						void cutSelectedClips();
						void pasteClips();

						double getCurrentBPM() const;
						double getVisualScaleFactor() const;
						void onTrackBPMChanged();

						void updateEmptyTracksBPM(double newBPM);

						// Update all track BPM control enabled/disabled state
						void updateAllTrackBPMControlsState(bool enabled);

						juce::String getTrackName(int trackIndex) const;

						void setPlaybackSpeed(double speed);
						double getPlaybackSpeed() const { return playbackSpeed; }

						std::function<void(const juce::File&)> onClipSelected;

						// Public method for cache invalidation (so Track.cpp can call it)
						void invalidateBarWidthCache();

						static constexpr int TRACK_HEADER_WIDTH = 180;
						static constexpr int TRACK_HEIGHT = 80;
						static constexpr int RULER_HEIGHT = 30;
						static constexpr double MIN_TIMELINE_WIDTH_SECONDS = 120.0;
						static constexpr double BUFFER_TIME = 300.0;

						float getZoomLevel() const { return zoomLevel; }


					private:

						DrumGrooveProcessor& processor;
						class TimelineControls* timelineControls = nullptr;

						std::unique_ptr<FixedHeaderColumn> fixedHeaderColumn;
						std::unique_ptr<FixedRulerRow> fixedRulerRow;
						std::unique_ptr<TimelineContent> timelineContent;
						juce::Viewport viewport;
						juce::Viewport headerViewport;


						std::vector<std::unique_ptr<AudioTrack>> audioTracks;
						std::vector<std::unique_ptr<AudioTrackHeader>> audioTrackHeaders;
						std::vector<std::unique_ptr<Track>> tracks;
						std::vector<std::unique_ptr<TrackHeader>> trackHeaders;

						bool playing = false;
						double playheadPosition = 0.0;
						double lastPlaybackTime = 0.0;
						double playbackSpeed = 1.0;
						bool autoScrollEnabled = true;

						// Timer optimization - track last playhead position to avoid unnecessary repaints
						double lastRenderedPlayheadPosition = -1.0;
						static constexpr int TIMER_INTERVAL_MS = 33;  // 30 fps (was 16ms = 62fps)

						float zoomLevel = 100.0f;
						double gridInterval = 0.5;

						bool selectionValid = false;
						bool isSettingSelection = false;
						double selectionStart = 0.0;
						int selectedTrackIndex = -1;
						std::set<int> selectedTrackIndices;

						std::vector<MidiClip> clipboardClips;
						bool clipboardIsCut = false;

						// NEW: Time/Bar Mode State
						bool barMode = false; // false = Time mode, true = Bar mode

						static constexpr int SCROLLBAR_THICKNESS = 12;

						void writeState(juce::ValueTree& state) const;
						void readState(const juce::ValueTree& state);
						juce::ValueTree saveCompleteState() const;

						std::unique_ptr<TimelineManager> timelineManager;
						void restoreCompleteState(const juce::ValueTree& state);
						double selectionEnd = 0.0;
						double selectionDragStart = 0.0;

						bool loopEnabled = false;

						// Selection handle dragging
						enum class HandleDragMode { None, Start, End };
						HandleDragMode handleDragMode = HandleDragMode::None;
						static constexpr int HANDLE_WIDTH = 12;
						static constexpr int HANDLE_HEIGHT = 14;

						std::unique_ptr<MidiClip> globalGhostClip;
						double originalGhostDuration = 0.0;
						int currentTargetTrack = -1;

						// Method to update track headers when mode changes
						void updateTrackHeadersForMode();

						void snapAllClipsToBarGrid();

						void timerCallback() override;
						void updateAutoScroll();
						void repaintPlayheadRegion(double oldPosition, double newPosition);
						double getMaxTime() const;
						double getTimelineWidthInSeconds() const;

						void showRightClickMenu(const juce::Point<int>& position);
						void drawGrid(juce::Graphics& g);
						void drawSectionDividers(juce::Graphics& g);
						void drawSectionLabels(juce::Graphics& g);
						void drawPlayhead(juce::Graphics& g);
						void drawSelectionRegion(juce::Graphics& g);
						void drawSelectionHandles(juce::Graphics& g);
						void drawGlobalGhostClip(juce::Graphics& g);

						bool isMouseOverStartHandle(const juce::MouseEvent& e) const;
						bool isMouseOverEndHandle(const juce::MouseEvent& e) const;
						void updateLoopRangeInRealTime();

						juce::ScrollBar manualVerticalScrollbar{true};
						bool needsManualVerticalScrollbar = false;
						void updateScrollbarVisibility();

						juce::ScrollBar overlayHorizontalScrollbar{false};
						bool shouldShowVerticalScrollbar = false;

						bool isUpdatingLayout = false;

						// Global undo/redo system
						std::deque<std::unique_ptr<TrackCommand>> undoStack;
						int currentUndoIndex = 0;
						static constexpr int MAX_UNDO_LEVELS = 100;

						// ADDED: Cache for dynamic bar widths
						mutable std::vector<DynamicBarInfo> cachedBarWidths;
						mutable juce::int64 barWidthsCacheTime = 0;
						static constexpr juce::int64 CACHE_LIFETIME_MS = 100;

						JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiTrackContainer)
					};
