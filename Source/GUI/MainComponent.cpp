#include "MainComponent.h"
#include "../PluginProcessor.h"
#include "Components/HeaderSection.h"
#include "Components/FolderPanel.h"
#include "Components/GrooveBrowser.h"
#include "Components/MultiTrackContainer.h"
#include "Components/TimelineControls.h"
#include "Components/FilePathDisplay.h"
#include "Components/SectionBar.h"
#include "LookAndFeel/ColourPalette.h"
#include "LookAndFeel/DrumGrooveLookAndFeel.h"
#include <algorithm>
#include <utility>

MainComponent::MainComponent(DrumGrooveProcessor& p)
: processor(p)
{
    // Don't setOpaque - we want to paint the background image
    setOpaque(false);

    headerSection = std::make_unique<HeaderSection>(processor);
    folderPanel = std::make_unique<FolderPanel>(processor);
    grooveBrowser = std::make_unique<GrooveBrowser>(processor);
    filePathDisplay = std::make_unique<FilePathDisplay>();
    multiTrackContainer = std::make_unique<MultiTrackContainer>(processor);  // Replaces Timeline
    multiTrackContainer->addChangeListener(this);  // Listen for selection changes
    timelineControls = std::make_unique<TimelineControls>(processor, *multiTrackContainer);
    sectionBar = std::make_unique<SectionBar>(processor, processor.sectionManager);

    // Set timeline controls reference in multi-track container
    multiTrackContainer->setTimelineControls(timelineControls.get());

    headerSection->onBPMChanged = [this](double newBPM) {
        // Update Global BPM for BAR mode (always)
        processor.sectionManager.setGlobalBPM(newBPM);

        // Update empty tracks ONLY if bypass is not enabled
        if (multiTrackContainer && !headerSection->isBypassTrackBPMSync())
            multiTrackContainer->updateEmptyTracksBPM(newBPM);

        // NEW: Update groove browser loop duration when BPM changes
        if (grooveBrowser)
            grooveBrowser->updateLoopDurationForBPMChange();
    };

    // Connect track BPM control state changes
    headerSection->onTrackBPMControlStateChanged = [this](bool enabled) {
        // Update all track BPM controls enabled/disabled state
        if (multiTrackContainer)
            multiTrackContainer->updateAllTrackBPMControlsState(enabled);
    };

    // Initialize track BPM control states based on current settings
    if (headerSection && multiTrackContainer)
    {
        headerSection->updateTrackBPMControlsState();
    }

    sectionBar = std::make_unique<SectionBar>(processor, processor.sectionManager);

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
    addAndMakeVisible(sectionBar.get());
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
    // Fill with black background first
    g.fillAll(juce::Colours::black);

    // Draw the background image centered if loaded
    if (backgroundImage.isValid())
    {
        auto bounds = getLocalBounds();
        int imageWidth = backgroundImage.getWidth();
        int imageHeight = backgroundImage.getHeight();

        // Calculate centered position
        int x = (bounds.getWidth() - imageWidth) / 2;
        int y = (bounds.getHeight() - imageHeight) / 2;

        // Draw image centered - black will show on sides if GUI is larger
        g.drawImageAt(backgroundImage, x, y);
    }
    else
    {
        // Fallback if image not loaded - use dark background
        g.fillAll(ColourPalette::mainBackground);
    }

    // Get title area bounds (first 50px)
    auto titleBounds = getLocalBounds().removeFromTop(50);

    // Paint title "DrumGroovePro" centered on top of background
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    auto font = lnf.getTitleFont().withHeight(42.0f);
    g.setFont(font);

    // Use GlyphArrangement for JUCE 8
    auto drumGrooveWidth = juce::GlyphArrangement::getStringWidthInt(font, "DrumGroove");
    auto proWidth = juce::GlyphArrangement::getStringWidthInt(font, "Pro");
    auto totalWidth = drumGrooveWidth + proWidth;

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

    // Title area - 75px at top
    bounds.removeFromTop(75);

    // Header section (BPM controls) - 40px (FIXED)
    int headerHeight = 40;
    headerSection->setBounds(bounds.removeFromTop(headerHeight));

    // Define minimum heights
    int folderPanelMinHeight = 400; // Minimum for Library Folders + Favorites
    int timelineControlsHeight = 25 + 40 + 40; // FilePathDisplay + TimelineControls + SectionBar
    int minTimelineTracksHeight = 240; // Minimum 3 tracks
    int minTimelineHeight = timelineControlsHeight + minTimelineTracksHeight;

    int availableHeight = bounds.getHeight();

    // Calculate how to split: give priority to timeline expansion
    int timelineHeight = minTimelineHeight;
    int topSectionHeight = availableHeight - timelineHeight;

    // If we have extra space, give it to the timeline
    if (topSectionHeight > folderPanelMinHeight)
    {
        timelineHeight = availableHeight - folderPanelMinHeight;
        topSectionHeight = folderPanelMinHeight;
    }

    // Top section: FolderPanel (left, 270px) + GrooveBrowser (right)
    auto topSection = bounds.removeFromTop(topSectionHeight);
    int folderPanelWidth = 270;
    folderPanel->setBounds(topSection.removeFromLeft(folderPanelWidth));
    grooveBrowser->setBounds(topSection);

    // Timeline section at bottom
    auto timelineBounds = bounds; // All remaining space

    // Timeline controls (fixed height)
    int filePathHeight = 25;
    filePathDisplay->setBounds(timelineBounds.removeFromTop(filePathHeight));
    timelineControls->setBounds(timelineBounds.removeFromTop(40));

    int sectionBarHeight = 40;
    sectionBar->setBounds(timelineBounds.removeFromTop(sectionBarHeight));

    // Multi-track container uses ALL remaining space
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
    if (!state.isValid())
    {
        DBG("=== MainComponent::restoreGuiState - INVALID STATE ===");
        return;
    }

    DBG("=== MainComponent::restoreGuiState ===");
    DBG("Current Folder: " + state.currentBrowserFolder.getFullPathName());
    DBG("Selected File: " + state.selectedFile.getFullPathName());
    DBG("Nav Path Count: " + juce::String(state.browserNavigationPath.size()));

    if (grooveBrowser && state.currentBrowserFolder.exists())
    {
        DBG("Calling grooveBrowser->restoreNavigationState...");
        grooveBrowser->restoreNavigationState(state.currentBrowserFolder, state.browserNavigationPath);
    }
    else
    {
        DBG(juce::String("grooveBrowser exists: ") + (grooveBrowser ? "yes" : "no"));
        DBG(juce::String("currentBrowserFolder exists: ") + (state.currentBrowserFolder.exists() ? "yes" : "no"));
    }

    if (filePathDisplay && state.selectedFile.exists())
    {
        DBG("Setting file path display...");
        filePathDisplay->setFilePath(state.selectedFile);
    }
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

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    if (multiTrackContainer && multiTrackContainer->getBounds().contains(e.getPosition()))
    {
        multiTrackContainer->grabKeyboardFocus();
    }
}
