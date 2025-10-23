#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/MidiDissector.h"

// Forward declarations
class DrumGrooveProcessor;

class DrumPartsColumn : public juce::ListBox,
                       public juce::ListBoxModel,
                       public juce::DragAndDropContainer
{
public:
    DrumPartsColumn(DrumGrooveProcessor& processor, const juce::String& columnName);
    ~DrumPartsColumn() override;

    // ListBoxModel implementation
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int newRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;  // NEW: For right-click detection

    // Drag and drop
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // Set drum parts to display
    void setDrumParts(const juce::Array<DrumPart>& parts, const juce::File& sourceFile);
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

private:
    DrumGrooveProcessor& processor;
    juce::String columnTitle;
    juce::Array<DrumPart> drumParts;
    juce::File originalMidiFile;
    juce::File lastTempFile;
    int selectedRow = -1;

    // Visual elements
    void drawPartItem(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds, bool isSelected, int rowNumber);
    void drawDrumPatternDots(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds); 
    void drawNoteMapping(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds);

    // Playback helpers
    void playPart(const DrumPart& part);
    void createTempMidiFile(const DrumPart& part, juce::File& tempFile);

    // NEW: Context menu for export
    void showContextMenu(int row, const juce::Point<int>& position);
    void exportPartToDesktop(const DrumPart& part);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumPartsColumn)
};