#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/DrumLibraryManager.h"
#include "GUI/LookAndFeel/ColourPalette.h"

// Forward declaration
class DrumGrooveProcessor;

class OriginLibraryEditor : public juce::Component,
public juce::ListBoxModel,
    public juce::Button::Listener
    {
    public:
        OriginLibraryEditor(DrumLibraryManager& manager, DrumGrooveProcessor* processor = nullptr);
        ~OriginLibraryEditor() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

        // Button::Listener
        void buttonClicked(juce::Button* button) override;

        static void showEditor(DrumLibraryManager& manager, DrumGrooveProcessor* processor, juce::Component* parent, std::function<void()> onLibrariesChanged = nullptr);

        std::function<void()> onLibrariesChanged;

    private:
        DrumLibraryManager& drumLibraryManager;

        DrumGrooveProcessor* processor;

        juce::Label titleLabel;
        juce::Label originLibrariesLabel;
        juce::Label mappingsLabel;

        juce::ListBox originLibrariesList;
        juce::StringArray allOriginLibraries;
        int selectedOriginIndex = -1;

        juce::TextButton addOriginButton;
        juce::TextButton addMappingButton;
        juce::TextButton pasteFromClipboardButton;
        juce::TextButton saveButton;
        juce::TextButton cancelButton;

        // Mappings display
        juce::Viewport mappingsViewport;
        juce::Component mappingsContainer;

        // =========================================================================
        // PENDING CHANGES SYSTEM - Changes only committed on Save
        // =========================================================================

        struct PendingMapping
        {
            uint8_t originNote;
            uint8_t gmNote;
            juce::String description;

            PendingMapping() : originNote(0), gmNote(0) {}

            bool operator==(const PendingMapping& other) const
            {
                return originNote == other.originNote && gmNote == other.gmNote;
            }
        };

        // Working copy of mappings per library: libraryName -> vector of mappings
        std::map<juce::String, std::vector<PendingMapping>> workingMappings;

        // Track which libraries were added
        std::set<juce::String> pendingAddedLibraries;

        // Track which libraries were deleted
        std::set<juce::String> pendingDeletedLibraries;

        // Track which libraries should also be added as target libraries on save
        std::set<std::string> pendingAddAsTargetLibraries;

        // Flag to track if there are unsaved changes
        bool hasUnsavedChanges = false;

        // Load current state from DrumLibraryManager into working copy
        void loadWorkingCopy();

        // Get working mappings for a library
        std::vector<PendingMapping>& getWorkingMappingsForLibrary(const juce::String& libraryName);

        // Check if a library is protected (read-only mappings)
        // Only General MIDI is protected for origin libraries
        bool isProtectedLibrary(const juce::String& libraryName) const;

        // =========================================================================

        // MappingRow - displays: Origin Note -> GM Note -> Description
        struct MappingRow : public juce::Component,
        public juce::Button::Listener
        {
            juce::Label originNoteLabel;
            juce::Label arrowLabel;
            juce::Label gmNoteLabel;
            juce::Label drumNameLabel;
            juce::TextButton editButton;
            juce::TextButton playButton;
            juce::DrawableButton deleteButton;

            uint8_t currentOriginNote;
            uint8_t currentGMNote;
            juce::String currentDescription;
            bool isReadOnly;

            juce::Image deleteIcon;
            juce::Image arrowIcon;
            std::unique_ptr<juce::DrawableImage> deleteNormalImage;
            std::unique_ptr<juce::DrawableImage> deleteOverImage;

            std::function<void()> onDelete;
            std::function<void()> onEdit;
            std::function<void()> onPlay;
            std::function<void()> onValueChanged;

            MappingRow(uint8_t originNote, uint8_t gmNote, const juce::String& description,
                       bool readOnly);
            ~MappingRow() override;

            void buttonClicked(juce::Button* button) override;
            void paint(juce::Graphics& g) override;
            void resized() override;

            uint8_t getOriginNote() const { return currentOriginNote; }
            uint8_t getGMNote() const { return currentGMNote; }
            juce::String getDescription() const { return drumNameLabel.getText(); }
        };

        juce::OwnedArray<MappingRow> mappingRows;

        juce::Image deleteIcon;
        juce::Image arrowIcon;

        void updateOriginLibrariesList();
        void updateMappingsForSelectedOrigin();
        void showAddOriginLibraryDialog();
        void showAddNoteMappingDialog();
        void showEditMappingDialog(MappingRow* row);
        void deleteOriginLibrary(int index);
        void deleteMapping(uint8_t originNote);
        void commitChanges();
        void pasteFromClipboard();

        void saveCustomMappings();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OriginLibraryEditor)
    };
