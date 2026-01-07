#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DrumLibraryManager.h"
#include "ColourPalette.h"

// Forward declaration
class DrumGrooveProcessor;

class DrumLibraryMappingEditor : public juce::Component,
public juce::ListBoxModel,
    public juce::Button::Listener
    {
    public:
        DrumLibraryMappingEditor(DrumLibraryManager& manager, DrumGrooveProcessor* processor = nullptr);
        ~DrumLibraryMappingEditor() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // ListBoxModel implementation
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

        // Button::Listener implementation
        void buttonClicked(juce::Button* button) override;

        // Show the editor as a modal dialog
        static void showEditor(DrumLibraryManager& manager, DrumGrooveProcessor* processor, juce::Component* parent,
                               std::function<void()> onLibrariesChangedCallback = nullptr);

    private:
        DrumLibraryManager& drumLibraryManager;
        DrumGrooveProcessor* processor;

        juce::Image arrowIcon;
        juce::Image crossIcon;

        // UI Components
        juce::Label titleLabel;
        juce::Label productsLabel;
        juce::Label mappingsLabel;

        juce::ListBox productsList;
        juce::Viewport mappingsViewport;
        juce::Component mappingsContainer;

        juce::TextButton addProductButton;
        juce::TextButton addMappingButton;
        juce::TextButton pasteFromClipboardButton;
        juce::TextButton saveButton;
        juce::TextButton cancelButton;

        // Current state
        juce::StringArray allProducts;
        int selectedProductIndex = -1;

        // =========================================================================
        // PENDING CHANGES SYSTEM - Changes only committed on Save
        // =========================================================================

        struct PendingMapping
        {
            uint8_t gmNote;
            uint8_t targetNote;
            juce::String description;
            juce::Colour colour;
            bool hasCustomColour = false;

            PendingMapping() : gmNote(0), targetNote(0), colour(juce::Colours::white), hasCustomColour(false) {}

            bool operator==(const PendingMapping& other) const
            {
                return gmNote == other.gmNote;
            }
        };

        // Working copy of mappings per library: libraryName -> vector of mappings
        std::map<juce::String, std::vector<PendingMapping>> workingMappings;

        // Track which libraries were added
        std::set<juce::String> pendingAddedLibraries;

        // Track which libraries were deleted
        std::set<juce::String> pendingDeletedLibraries;

        // Flag to track if there are unsaved changes
        bool hasUnsavedChanges = false;

        // Load current state from DrumLibraryManager into working copy
        void loadWorkingCopy();

        // Get working mappings for a library
        std::vector<PendingMapping>& getWorkingMappingsForLibrary(const juce::String& libraryName);

        // Check if a library is fully protected (no delete at library level, no edit/delete at row level)
        // ONLY: General MIDI, Bypass (No Remapping), Salamander Drumkit, Muldjord Kit 3
        bool isProtectedLibrary(const juce::String& libraryName) const;

        // =========================================================================

        struct NoteMappingRow : public juce::Component,
        public juce::Button::Listener
        {
            NoteMappingRow(uint8_t gm, uint8_t target, const juce::String& description,
                           bool isReadOnly, const juce::Colour& colour, bool hasCustom);
            ~NoteMappingRow() override;
            void resized() override;
            void paint(juce::Graphics& g) override;
            void buttonClicked(juce::Button* button) override;

            juce::Image deleteIcon;
            juce::Image arrowIcon;
            std::unique_ptr<juce::DrawableImage> deleteNormalImage;
            std::unique_ptr<juce::DrawableImage> deleteOverImage;

            uint8_t gmNote;
            uint8_t targetNote;
            juce::String currentDescription;
            bool readOnly;

            juce::Label gmNoteLabel;
            juce::Label gmNameLabel;
            juce::Label arrowLabel;
            juce::Label targetNoteLabel;
            juce::DrawableButton deleteButton;
            juce::TextButton editButton;
            juce::TextButton playButton;
            juce::TextButton colourButton;
            juce::ToggleButton overwriteCheckbox;

            std::function<void()> onDelete;
            std::function<void()> onEdit;
            std::function<void()> onPlay;
            std::function<void()> onColourChange;
            std::function<void()> onValueChanged;

            uint8_t getGMNote() const { return gmNote; }
            uint8_t getTargetNote() const { return targetNote; }
            juce::String getDescription() const { return gmNameLabel.getText(); }
            void setColour(const juce::Colour& newColour, bool isCustom);
            juce::Colour getColour() const { return currentColour; }
            bool hasCustomColour() const { return hasCustom; }

        private:
            juce::Colour currentColour;
            juce::Colour defaultColour;
            bool hasCustom = false;
            void updateColourButtonState();
        };

        juce::OwnedArray<NoteMappingRow> mappingRows;

        // Helper methods
        void updateProductsList();
        void updateMappingsForSelectedProduct();
        void showAddProductDialog();
        void showAddNoteMappingDialog();
        void showEditMappingDialog(NoteMappingRow* row);
        void deleteProduct(int productIndex);
        void commitChanges();
        void pasteFromClipboard();
        void showColourPicker(NoteMappingRow* row, size_t mappingIndex, const juce::String& libraryName);

        std::function<void()> onLibrariesChanged;

        juce::Image productDeleteIcon;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumLibraryMappingEditor)
    };
