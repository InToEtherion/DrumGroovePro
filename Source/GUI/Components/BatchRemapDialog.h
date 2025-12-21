#pragma once

#include <JuceHeader.h>
#include "DrumLibraryManager.h"

class BatchRemapDialog : public juce::DocumentWindow
{
public:
    BatchRemapDialog(DrumLibraryManager& libManager);
    ~BatchRemapDialog() override;

    void closeButtonPressed() override;

    std::function<void()> onDialogClosed;

private:
    class DialogContent : public juce::Component
    {
    public:
        DialogContent(DrumLibraryManager& libManager);
        ~DialogContent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        DrumLibraryManager& drumLibraryManager;

        // UI Components
        juce::Label sourceLibraryLabel;
        juce::ComboBox sourceLibraryCombo;

        juce::Label targetLibraryLabel;
        juce::ComboBox targetLibraryCombo;

        juce::Label sourcePathLabel;
        juce::Label sourcePathDisplay;
        juce::TextButton browseSourceButton;
        juce::ToggleButton singleFileRadio;
        juce::ToggleButton folderRadio;

        juce::Label destinationPathLabel;
        juce::Label destinationPathDisplay;
        juce::TextButton browseDestinationButton;

        juce::TextButton convertButton;
        juce::TextButton cancelButton;

        juce::Label progressLabel;
        juce::ProgressBar progressBar;

        // State
        juce::File sourceFile;
        juce::File destinationFolder;
        bool isProcessing = false;
        double progress = 0.0;

        // Methods
        void loadAvailableMappings();
        void browseSource();
        void browseDestination();
        void startConversion();
        void processFiles();
        void processSingleFile(const juce::File& inputFile, const juce::File& outputFolder, 
                              const juce::String& relativePath);
        void processFolder(const juce::File& inputFolder, const juce::File& outputFolder, 
                          const juce::String& relativePath);
        bool isDestinationReadOnly(const juce::File& folder);
        juce::String sanitizeFileName(const juce::String& name);
        void updateProgress(double newProgress, const juce::String& message);
        void showCompletionDialog(int filesProcessed, int filesSkipped);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DialogContent)
    };

    DrumLibraryManager& drumLibraryManager;
    std::unique_ptr<DialogContent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchRemapDialog)
};
