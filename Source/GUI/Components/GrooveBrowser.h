#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Core/MidiDissector.h"

// Forward declarations
class DrumGrooveProcessor;
class DrumPartsColumn;
class BrowserColumn;

// Custom component that handles CTRL+Drag independently from ListBox
class DraggableListItemOverlay : public juce::Component
{
public:
    DraggableListItemOverlay(BrowserColumn* parent) : parentColumn(parent) {}
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;  // ✅ ADD THIS LINE
    
    void setRow(int rowNum) { row = rowNum; }
    
private:
    BrowserColumn* parentColumn;
    int row = -1;
    bool isDragging = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraggableListItemOverlay)
};

class BrowserColumn : public juce::ListBox, public juce::ListBoxModel
{
public:
    explicit BrowserColumn(const juce::String& columnName, DrumGrooveProcessor& proc);
    ~BrowserColumn() override;

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    
    // NEW: Add overlay component to each row for drag handling
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    
    // NEW: Public method called by DraggableListItemOverlay
    void startExternalDrag(int rowNumber);

    void setItems(const juce::StringArray& items, const juce::Array<bool>& isFolder, const juce::Array<juce::File>& filePaths = {});
    void clearItems();
    juce::String getSelectedItem() const;
    bool isSelectedItemFolder() const;
    juce::File getSelectedFile() const;
    int getSelectedRow() const { return selectedRow; }

    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    // Callbacks
    std::function<void()> onSelectionChange;
    std::function<void(int)> onDoubleClick;
    std::function<void(const juce::File&)> onRightClickFolder;

    // Public members for display
    juce::StringArray items;
    juce::Array<bool> itemIsFolder;
    juce::Array<juce::File> itemFiles;

private:
    void loadIcons();
    
    // Context menu methods
    void showContextMenu(int row, const juce::Point<int>& position);
    void exportFileToDesktop(const juce::File& originalFile);
    
    juce::String columnTitle;
    int selectedRow = -1;
    juce::Image folderIcon;
    juce::Image midiIcon;
    
    // Processor reference needed for export functionality
    DrumGrooveProcessor& processor;
    
    // For external drag
    bool isExternalDragActive = false;
    juce::File lastTempDragFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserColumn)
};

class GrooveBrowser : public juce::Component,
                     public juce::DragAndDropContainer,
                     public juce::ComboBox::Listener,
                     public juce::AudioProcessorValueTreeState::Listener,
                     private juce::Timer
{
public:
    explicit GrooveBrowser(DrumGrooveProcessor& processor);
    ~GrooveBrowser() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ComboBox::Listener implementation
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    
    // AudioProcessorValueTreeState::Listener implementation
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void loadFolderContents(const juce::File& folder);
    void handleFileSelection(const juce::File& file);
    void handleFileDoubleClick(const juce::File& file);
    void handleColumnDoubleClick(int columnIndex, int row);
    void handleMidiFileSelection(const juce::File& midiFile);
    void handleDrumPartSelection(const DrumPart& part);
    void handleDrumPartDoubleClick(const DrumPart& part);

    bool keyPressed(const juce::KeyPress& key) override;
    void timerCallback() override;

    // File selection callback for main component
    std::function<void(const juce::File&)> onFileSelected;

    // Get current state for persistence
    juce::File getCurrentFolder() const { return currentPath; }
    juce::Array<juce::File> getNavigationPath() const { return navigationPath; }
    void restoreNavigationState(const juce::File& folder, const juce::Array<juce::File>& path);

private:
    DrumGrooveProcessor& processor;
	
	// BPM monitoring for real-time updates
	double lastKnownBPM = 120.0;

    // Column management
    juce::OwnedArray<BrowserColumn> folderColumns;
    std::unique_ptr<DrumPartsColumn> partsColumn;

    juce::Viewport viewport;
    juce::Component columnsContainer;
    juce::File currentPath;
    juce::Array<juce::File> navigationPath;

    // MIDI dissection
    MidiDissector midiDissector;
    juce::File currentMidiFile;
    juce::Array<DrumPart> currentDrumParts;
    DrumLibrary currentSourceLibrary = DrumLibrary::Unknown;
    
    // Prevent double-processing of target library changes
    bool isHandlingTargetLibraryChange = false;

    // Target library control
    juce::Label targetLibraryLabel;
    juce::ComboBox targetLibraryCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> libraryAttachment;
    
    // Helper methods
    DrumLibrary getCurrentTargetLibrary() const;
    void handleTargetLibraryChange();
    void redissectCurrentMidiFile();
    void showFolderContextMenu(const juce::File& folder);
    void syncComboBoxWithParameter();

    // Column widths
    static constexpr int FOLDER_COLUMN_WIDTH = 220;
    static constexpr int FILE_COLUMN_WIDTH = 300;
    static constexpr int PARTS_COLUMN_WIDTH = 350;
    static constexpr int COLUMN_HEIGHT_MIN = 400;

    // Column management methods
    void addFolderColumn(const juce::String& title, bool isFileColumn = false);
    void removeFolderColumnsAfter(int index);
    void addPartsColumn();
    void removePartsColumn();
    void updateColumnsLayout();

    void scanFolder(const juce::File& folder, BrowserColumn* column);
    void navigateToFolder(const juce::File& folder, int columnIndex);
    void handleColumnSelection(int columnIndex);

    juce::String formatFileName(const juce::String& filename, bool isMidiFile) const;
    int extractBPMFromFilename(const juce::String& filename) const;
    juce::File getCurrentFileForRow(int columnIndex, int row);
	
	bool hasInitializedTargetLibrary = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrooveBrowser)
};