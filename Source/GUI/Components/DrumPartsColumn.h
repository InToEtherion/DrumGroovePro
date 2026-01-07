#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/MidiDissector.h"

// Forward declarations
class DrumGrooveProcessor;
class DrumPartsColumn;

// Overlay component to handle CTRL+Drag for external drag to DAW
class DrumPartDragOverlay : public juce::Component
{
public:
    DrumPartDragOverlay(DrumPartsColumn* parent) : parentColumn(parent) {}

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setRow(int rowNum) { row = rowNum; }

private:
    DrumPartsColumn* parentColumn = nullptr;
    int row = -1;
    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumPartDragOverlay)
};

class DrumPartsColumn : public juce::ListBox,
public juce::ListBoxModel,
    public juce::DragAndDropContainer,
        private juce::Timer
        {
        public:
            DrumPartsColumn(DrumGrooveProcessor& processor, const juce::String& columnName);
            ~DrumPartsColumn() override;

            // Component override
            void resized() override;

            // ListBoxModel implementation
            int getNumRows() override;
            void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
            juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
            void selectedRowsChanged(int newRowSelected) override;
            void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
            void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

            // Drag and drop
            juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

            // Set drum parts to display
            void setDrumParts(const juce::Array<DrumPart>& parts, const juce::File& sourceFile,DrumLibrary srcLib);
            void clearParts();

            // Getters
            const juce::Array<DrumPart>& getDrumParts() const { return drumParts; }
            const DrumPart* getSelectedPart() const;
            int getSelectedRow() const { return selectedRow; }

            // Callbacks
            std::function<void(const DrumPart&)> onPartSelected;
            std::function<void(const DrumPart&)> onPartDoubleClicked;

            // Playback control
            void playSelectedPart();
            void stopPlayback();

            // External drag method (called by overlay)
            void startExternalDrag(int row);

            // Timer callback
            void timerCallback() override;

        private:
            DrumGrooveProcessor& processor;
            double currentPartReferenceBPM = 120.0;
            float playbackProgress = 0.0f;
            juce::String columnTitle;
            juce::Array<DrumPart> drumParts;
            juce::File originalMidiFile;
            DrumLibrary sourceLibrary = DrumLibrary::Unknown;
            juce::File lastTempFile;
            int selectedRow = -1;
            bool isExternalDragActive = false;

            // Visual elements
            void drawPartItem(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds, bool isSelected, int rowNumber);
            void drawDrumPatternDots(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds);
            void drawNoteMapping(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds);

            // Playback helpers
            void playPart(const DrumPart& part);
            void createTempMidiFile(const DrumPart& part, juce::File& tempFile);

            // Context menu for export
            void showContextMenu(int row, const juce::Point<int>& position);
            void exportPartToDesktop(const DrumPart& part);

            // Track current preview state for BPM updates
            bool isPreviewPlaying { false };
            double currentPreviewDuration { 0.0 };
            double currentPreviewBPM { 120.0 };
            double lastKnownBPM { 120.0 };

            // Additional preview state tracking
            double actualDuration { 0.0 };
            double currentBPM { 120.0 };

            // Store MIDI ticks for loop recalculation
            double currentPlaybackMidiTicks { 0.0 };
            double currentPlaybackTPQN { 480.0 };

            // BPM synchronization helpers
            void updatePreviewForBPMChange();
            void updateLoopDurationForBPMChange();

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumPartsColumn)
        };
