#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
class DrumGrooveProcessor;

class AddFolderDialog : public juce::DialogWindow,
                        public juce::Button::Listener,
                        public juce::ComboBox::Listener,
                        private juce::Timer
{
public:
    AddFolderDialog(DrumGrooveProcessor& processor);
    ~AddFolderDialog() override;
    void closeButtonPressed() override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    std::function<void()> onFolderAdded;

private:
    class AddFolderComponent : public juce::Component
    {
    public:
        AddFolderComponent(DrumGrooveProcessor& processor);
        ~AddFolderComponent() override;
        void paint(juce::Graphics& g) override;
        void resized() override;
        juce::Label folderPathLabel, sourceLibraryLabel, sourceHelpLabel, libraryNameLabel;
        juce::TextEditor folderPathEditor, libraryNameEditor;
        juce::TextButton browseButton, addButton, cancelButton;
        juce::ComboBox sourceLibraryCombo;
        juce::ProgressBar progressBar;
        juce::Label statusLabel;
        double progress = 0.0;
    private:
        DrumGrooveProcessor& processor;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddFolderComponent)
    };
    
    DrumGrooveProcessor& processor;
    std::unique_ptr<AddFolderComponent> component;
    juce::File selectedFolder;
    int selectedSourceLibrary = 0;
    juce::String libraryName;
    juce::Array<juce::File> midiFiles;
    int currentChunkIndex = 0;
    bool isProcessing = false;
    bool processingCancelled = false;
    static constexpr int CHUNK_SIZE = 10;
    void timerCallback() override;
    void startProcessing();
    void processNextChunk();
    void finishProcessing();
    void setProcessingState(bool processing);
    void updateAddButtonState();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddFolderDialog)
};