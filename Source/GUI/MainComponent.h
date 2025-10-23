#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

// Forward declarations
class DrumGrooveProcessor;
class HeaderSection;
class FolderPanel;
class GrooveBrowser;
class MultiTrackContainer;
class TimelineControls;
class FilePathDisplay;

class MainComponent : public juce::Component, public juce::ChangeListener
{
public:
    explicit MainComponent(DrumGrooveProcessor& processor);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateBPMDisplay();
    FolderPanel* getFolderPanel() const { return folderPanel.get(); }
    MultiTrackContainer* getMultiTrackContainer() { return multiTrackContainer.get(); }
    
    // ChangeListener interface
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // GUI state persistence - simplified and reliable
    struct GuiState
    {
        juce::File currentBrowserFolder;
        juce::Array<juce::File> browserNavigationPath;
        juce::File selectedFile;

        // Default constructor
        GuiState() = default;

        // Copy constructor and assignment operator
        GuiState(const GuiState& other) = default;
        GuiState& operator=(const GuiState& other) = default;

        // Move constructor and assignment operator
        GuiState(GuiState&& other) noexcept = default;
        GuiState& operator=(GuiState&& other) noexcept = default;

        // Serialization methods (keeping for compatibility)
        juce::XmlElement* createXml() const;
        void restoreFromXml(const juce::XmlElement& xml);

        // Validation method
        bool isValid() const;
    };

    GuiState saveGuiState() const;
    void restoreGuiState(const GuiState& state);

private:
    DrumGrooveProcessor& processor;

    std::unique_ptr<HeaderSection> headerSection;
    std::unique_ptr<FolderPanel> folderPanel;
    std::unique_ptr<GrooveBrowser> grooveBrowser;
    std::unique_ptr<FilePathDisplay> filePathDisplay;
    std::unique_ptr<MultiTrackContainer> multiTrackContainer;
    std::unique_ptr<TimelineControls> timelineControls;
    
    // Background image
    juce::Image backgroundImage;

    // Event handlers
    void handleFileSelected(const juce::File& file);
    void handleTimelineClipSelected(const juce::File& file);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};