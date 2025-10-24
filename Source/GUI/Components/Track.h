#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../PluginProcessor.h"
#include "../../Core/MidiDissector.h"
#include "../../Utils/TimelineUtils.h"
#include "../LookAndFeel/ColourPalette.h"
#include <memory>

// Forward declarations
class MultiTrackContainer;

struct MidiClip
{
    juce::String name;
    juce::File file;
    double startTime = 0.0;
    double duration = 4.0;
    double originalBPM = 120.0;
    double referenceBPM = 120.0;  // NEW: Track BPM when clip was added
    juce::Colour colour;
    bool isSelected = false;
    juce::String id = juce::Uuid().toString();
};
class Track : public juce::Component,
              public juce::DragAndDropTarget
{
public:
    Track(DrumGrooveProcessor& processor, MultiTrackContainer& container, int trackNumber);
    ~Track() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // Drag and drop
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    // Clip management
    void addClip(const MidiClip& clip);
    void removeSelectedClips();
    void selectAll();
    void deselectAll();
    void clearAllClips();
    std::vector<MidiClip*> getSelectedClips();
    const std::vector<std::unique_ptr<MidiClip>>& getClips() const { return clips; }
    
    // Inter-track operations
    void copySelectedClipsToTrack(Track* targetTrack);
    void moveSelectedClipsToTrack(Track* targetTrack);
    MidiClip createClipForTrack(const MidiClip& sourceClip, double targetBPM);

    // Track properties
    bool isMuted() const;
    bool isSoloed() const;
    double getTrackBPM() const;
    juce::String getTrackName() const;

    // Callback
    std::function<void(const juce::File&)> onClipSelected;

    // Constants
    static constexpr int TRACK_HEIGHT = 80;
    static constexpr float RESIZE_HANDLE_WIDTH = 8.0f;

private:
    DrumGrooveProcessor& processor;
    MultiTrackContainer& container;
    int trackNumber;

    std::vector<std::unique_ptr<MidiClip>> clips;
    std::unique_ptr<MidiClip> ghostClip;

    // Interaction state - ALL MEMBER VARIABLES NEEDED
    bool isDragging = false;
    bool isResizing = false;
    bool isResizingLeft = false;
    bool isSelecting = false;
    bool isExternalDragging = false;                              
    juce::Point<float> dragStartPoint;
    std::vector<std::pair<juce::String, double>> draggedClips;
    MidiClip* resizingClip = nullptr;
    double resizeStartTime = 0.0;
    double resizeStartDuration = 0.0;
    juce::Rectangle<float> selectionBox;
    float dropIndicatorX = -1;
	
	    // For external drag
    bool isExternalDragActive = false;
    juce::File lastTempDragFile;
    
    // External drag method
    void startExternalDrag();

    // Helper methods for drawing
    void drawClips(juce::Graphics& g);
    void drawGhostClip(juce::Graphics& g);
    void drawSelectionBox(juce::Graphics& g);
    void drawDropIndicator(juce::Graphics& g);
    void drawMidiDotsInClip(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> clipBounds);

    // Helper methods for interaction
    MidiClip* getClipAt(const juce::Point<float>& point);
    juce::Rectangle<int> getTrackArea() const;
    double pixelsToTime(float pixels) const;
    float timeToPixels(double time) const;
    double snapToGrid(double time) const;
    
	void handleMidiFileDrop(const juce::StringArray& parts, const juce::Point<int>& position);
    void handleDrumPartDrop(const juce::StringArray& parts, const juce::Point<int>& position);
    void adjustGhostClipToTrackBPM();
    void inheritBPMFromHeader();
    double getVisualScaleFactor() const;
    void showTrackContextMenu(const juce::Point<int>& position);
	
    
    // Drum part creation
    bool createDrumPartMidiFile(const juce::File& originalFile, 
                               DrumPartType partType, 
                               DrumLibrary sourceLib,
                               juce::File& outputFile);
                               
    bool calculateMidiFileDuration(const juce::File& midiFile, double& duration) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};