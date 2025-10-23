#include "MainComponent.h"
#include "../PluginProcessor.h"
#include "Components/HeaderSection.h"
#include "Components/FolderPanel.h"
#include "Components/GrooveBrowser.h"
#include "Components/MultiTrackContainer.h"
#include "Components/TimelineControls.h"
#include "Components/FilePathDisplay.h"
#include "LookAndFeel/ColourPalette.h"
#include "LookAndFeel/DrumGrooveLookAndFeel.h"
#include <algorithm>
#include <utility>

MainComponent::MainComponent(DrumGrooveProcessor& p)
: processor(p)
{
    setOpaque(true);

    headerSection = std::make_unique<HeaderSection>(processor);
    folderPanel = std::make_unique<FolderPanel>(processor);
    grooveBrowser = std::make_unique<GrooveBrowser>(processor);
    filePathDisplay = std::make_unique<FilePathDisplay>();
    multiTrackContainer = std::make_unique<MultiTrackContainer>(processor);  // Replaces Timeline
    multiTrackContainer->addChangeListener(this);  // Listen for selection changes
    timelineControls = std::make_unique<TimelineControls>(processor, *multiTrackContainer);
    
    // Set timeline controls reference in multi-track container
    multiTrackContainer->setTimelineControls(timelineControls.get());

    // Connect folder panel to groove browser
    folderPanel->onFolderSelected = [this](const juce::File& folder) {
        if (grooveBrowser)
            grooveBrowser->loadFolderContents(folder);
    };

    // Connect groove browser file selection to file path display
    grooveBrowser->onFileSelected = [this](const juce::File& file) {
        handleFileSelected(file);
    };

    // Connect multi-track container clip selection to file path display
    multiTrackContainer->onClipSelected = [this](const juce::File& file) {
        handleTimelineClipSelected(file);
    };

    addAndMakeVisible(headerSection.get());
    addAndMakeVisible(folderPanel.get());
    addAndMakeVisible(grooveBrowser.get());
    addAndMakeVisible(filePathDisplay.get());
    addAndMakeVisible(timelineControls.get());
    addAndMakeVisible(multiTrackContainer.get());  // Multi-track container
    
    // Load background image from Resources folder - try multiple paths
    juce::Array<juce::File> searchPaths;
    
    // Path 1: Next to executable (for development)
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    searchPaths.add(executableFile.getParentDirectory().getChildFile("Resources").getChildFile("background").getChildFile("background.png"));
    
    // Path 2: VST3 bundle structure (for installed VST3)
    searchPaths.add(executableFile.getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("background").getChildFile("background.png"));
    
    // Path 3: Alternative VST3 structure
    searchPaths.add(executableFile.getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("background").getChildFile("background.png"));
    
    // Path 4: Current working directory (fallback)
    searchPaths.add(juce::File::getCurrentWorkingDirectory().getChildFile("Resources").getChildFile("background").getChildFile("background.png"));
    
    bool imageLoaded = false;
    for (const auto& path : searchPaths)
    {
        DBG("Trying background path: " + path.getFullPathName());
        if (path.existsAsFile())
        {
            backgroundImage = juce::ImageCache::getFromFile(path);
            if (backgroundImage.isValid())
            {
                DBG("Background image loaded successfully from: " + path.getFullPathName());
                imageLoaded = true;
                break;
            }
        }
    }
    
    if (!imageLoaded)
    {
        DBG("Background image not found in any of the search paths");
        DBG("Executable location: " + executableFile.getFullPathName());
    }
}

MainComponent::~MainComponent() = default;

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);

    // Get title area bounds (first 50px)
    auto titleBounds = getLocalBounds().removeFromTop(50);

    // Paint title "DrumGroovePro" centered
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    auto font = lnf.getTitleFont().withHeight(42.0f);
    g.setFont(font);

    // Use GlyphArrangement for JUCE 8
    auto drumGrooveWidth = juce::GlyphArrangement::getStringWidthInt(font, "DrumGroove");
    auto proWidth = juce::GlyphArrangement::getStringWidthInt(font, "Pro");
    auto totalWidth = drumGrooveWidth + proWidth;
    
    // FIXED: Use titleBounds.getWidth() instead of getWidth() for proper centering
    auto startX = titleBounds.getX() + (titleBounds.getWidth() - totalWidth) / 2;

    // Center vertically in the 50px title area
    int titleY = titleBounds.getY() + (titleBounds.getHeight() - 42) / 2;

    // Draw "DrumGroove" in white
    g.setColour(ColourPalette::primaryText);
    g.drawText("DrumGroove", startX, titleY, drumGrooveWidth, 42,
               juce::Justification::left);

    // Draw "Pro" in cyan
    g.setColour(ColourPalette::cyanAccent);
    g.drawText("Pro", startX + drumGrooveWidth, titleY,
               proWidth, 42, juce::Justification::left);
}

void MainComponent::updateBPMDisplay()
{
    if (headerSection)
        headerSection->updateBPMDisplay();
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Title area - 50px at top
    bounds.removeFromTop(50);

    // Header section (only BPM controls) - 40px
    int headerHeight = 40;
    headerSection->setBounds(bounds.removeFromTop(headerHeight));

    // GrooveBrowser and FolderPanel area - everything EXCEPT timeline at bottom
    // Calculate how much space timeline needs: 30 (ruler) + 40 (controls) + 25 (file path) + space for 3 tracks
    int minTimelineHeight = 30 + 40 + 25 + (3 * 80);  // = 335px minimum for 3 tracks
    
    // Reserve space for timeline at bottom - use minTimelineHeight
    auto timelineBounds = bounds.removeFromBottom(minTimelineHeight);
    
    // NOW split the REMAINING space between folder panel and groove browser
    int folderPanelWidth = 270;
    folderPanel->setBounds(bounds.removeFromLeft(folderPanelWidth));
    grooveBrowser->setBounds(bounds);  // Takes all remaining vertical space
    
    // Timeline section at bottom
    // File path display above timeline controls
    int filePathHeight = 25;
    filePathDisplay->setBounds(timelineBounds.removeFromTop(filePathHeight));

    // Timeline controls
    timelineControls->setBounds(timelineBounds.removeFromTop(40));

    // Multi-track container uses remaining timeline space
    multiTrackContainer->setBounds(timelineBounds);

    repaint();
}

void MainComponent::handleFileSelected(const juce::File& file)
{
    if (filePathDisplay)
    {
        filePathDisplay->setFilePath(file);
    }
}

void MainComponent::handleTimelineClipSelected(const juce::File& file)
{
    if (filePathDisplay)
    {
        filePathDisplay->setFilePath(file);
    }
}

// GUI State Persistence Implementation - UPDATED for processor-based storage
juce::XmlElement* MainComponent::GuiState::createXml() const
{
    auto* element = new juce::XmlElement("MainComponentState");

    // Save current browser folder
    if (currentBrowserFolder.exists())
    {
        element->setAttribute("currentBrowserFolder", currentBrowserFolder.getFullPathName());
    }

    // Save selected file
    if (selectedFile.exists())
    {
        element->setAttribute("selectedFile", selectedFile.getFullPathName());
    }

    // Save navigation path
    if (!browserNavigationPath.isEmpty())
    {
        auto* pathElement = element->createNewChildElement("NavigationPath");
        for (int i = 0; i < browserNavigationPath.size(); ++i)
        {
            if (browserNavigationPath[i].exists())
            {
                auto* folderElement = pathElement->createNewChildElement("Folder");
                folderElement->setAttribute("path", browserNavigationPath[i].getFullPathName());
                folderElement->setAttribute("index", i);
            }
        }
    }

    return element;
}

void MainComponent::GuiState::restoreFromXml(const juce::XmlElement& xml)
{
    if (!xml.hasTagName("MainComponentState"))
        return;

    // Restore current browser folder
    auto folderPath = xml.getStringAttribute("currentBrowserFolder");
    if (folderPath.isNotEmpty())
    {
        juce::File folder(folderPath);
        if (folder.exists())
            currentBrowserFolder = folder;
    }

    // Restore selected file
    auto filePath = xml.getStringAttribute("selectedFile");
    if (filePath.isNotEmpty())
    {
        juce::File file(filePath);
        if (file.exists())
            selectedFile = file;
    }

    // Restore navigation path
    browserNavigationPath.clear();
    if (auto* pathElement = xml.getChildByName("NavigationPath"))
    {
        // First, collect all valid folders with their indices
        juce::Array<std::pair<int, juce::File>> indexedFolders;

        for (auto* folderElement : pathElement->getChildIterator())
        {
            if (folderElement->hasTagName("Folder"))
            {
                auto path = folderElement->getStringAttribute("path");
                int index = folderElement->getIntAttribute("index", -1);

                juce::File folder(path);
                if (folder.exists() && index >= 0)
                {
                    indexedFolders.add({index, folder});
                }
            }
        }

        // Sort by index and rebuild the array
        std::sort(indexedFolders.begin(), indexedFolders.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& indexedFolder : indexedFolders)
        {
            browserNavigationPath.add(indexedFolder.second);
        }
    }
}

bool MainComponent::GuiState::isValid() const
{
    // State is valid if at least one meaningful component is set
    return currentBrowserFolder.exists() ||
    selectedFile.exists() ||
    !browserNavigationPath.isEmpty();
}

MainComponent::GuiState MainComponent::saveGuiState() const
{
    GuiState state;

    // Save browser state
    if (grooveBrowser)
    {
        state.currentBrowserFolder = grooveBrowser->getCurrentFolder();
        state.browserNavigationPath = grooveBrowser->getNavigationPath();
    }

    // Save selected file
    if (filePathDisplay)
    {
        state.selectedFile = filePathDisplay->getCurrentFile();
    }
    
    // CRITICAL: Save MultiTrackContainer state (tracks, clips, BPM, etc.)
    if (multiTrackContainer)
    {
        processor.saveCompleteGuiState(multiTrackContainer.get());
    }

    return state;
}

void MainComponent::restoreGuiState(const GuiState& state)
{
    // Validate the state before applying
    if (!state.isValid())
        return;

    // Restore browser navigation
    if (grooveBrowser && state.currentBrowserFolder.exists())
    {
        grooveBrowser->restoreNavigationState(state.currentBrowserFolder, state.browserNavigationPath);
    }

    // Restore selected file display
    if (filePathDisplay && state.selectedFile.exists())
    {
        filePathDisplay->setFilePath(state.selectedFile);
    }
    
    // Note: MultiTrackContainer state is restored separately in PluginEditor
    // to ensure proper timing and component initialization
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // Update timeline controls when selection changes in MultiTrackContainer
    if (source == multiTrackContainer.get() && timelineControls)
    {
        if (multiTrackContainer->hasSelection())
        {
            double startTime = multiTrackContainer->getSelectionStart();
            double endTime = multiTrackContainer->getSelectionEnd();
            
            // Update timeline control fields
            timelineControls->setLoopStartTime(startTime);
            timelineControls->setLoopEndTime(endTime);
        }
    }
}
