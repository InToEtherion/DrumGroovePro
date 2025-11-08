#include "PluginEditor.h"
#include "GUI/LookAndFeel/DrumGrooveLookAndFeel.h"
#include "GUI/LookAndFeel/ColourPalette.h"
#include "GUI/Components/MultiTrackContainer.h"

DrumGrooveEditor::DrumGrooveEditor(DrumGrooveProcessor& p)
: AudioProcessorEditor(&p), processor(p)
{
    // Set up look and feel
    setLookAndFeel(&DrumGrooveLookAndFeel::getInstance());

    // Create main component
    mainComponent = std::make_unique<MainComponent>(processor);
    addAndMakeVisible(mainComponent.get());

    // Set constraints
    setResizable(true, true);
    setResizeLimits(900, 600, 2000, 1200);

    // Calculate size based on display 
    auto displays = juce::Desktop::getInstance().getDisplays();
    auto primaryDisplay = displays.getPrimaryDisplay();
    auto displayArea = primaryDisplay->userArea;
    
    // Base size
    int targetWidth = 1300;
    int targetHeight = 900;
    
    // Simple DPI scaling
    float dpiScale = static_cast<float>(primaryDisplay->scale);
    if (dpiScale > 1.0f)
    {
        targetWidth = juce::roundToInt(targetWidth * dpiScale);
        targetHeight = juce::roundToInt(targetHeight * dpiScale);
    }
    
    // Ensure it fits on screen
    targetWidth = juce::jmin(targetWidth, displayArea.getWidth() - 100);
    targetHeight = juce::jmin(targetHeight, displayArea.getHeight() - 100);
    
    // Restore saved state if available
    auto guiState = processor.getGuiState();
    if (guiState.editorWidth > 0 && guiState.editorHeight > 0)
    {
        targetWidth = guiState.editorWidth;
        targetHeight = guiState.editorHeight;
        targetWidth = juce::jmin(targetWidth, displayArea.getWidth() - 100);
        targetHeight = juce::jmin(targetHeight, displayArea.getHeight() - 100);
    }
    
    setSize(targetWidth, targetHeight);
    
    // Restore window position if available
    if (guiState.editorX >= 0 && guiState.editorY >= 0)
    {
        if (auto* peer = getPeer())
        {
            peer->setBounds(juce::Rectangle<int>(guiState.editorX, guiState.editorY, 
                                                  targetWidth, targetHeight), false);
        }
    }

    // Start timer for BPM updates
    startTimer(200);
	
	// SAFE STATE RESTORATION - delayed to prevent crashes
    // Restore clips and tracks from processor's stored state
    juce::Timer::callAfterDelay(100, [safeThis = juce::Component::SafePointer<DrumGrooveEditor>(this)]()
    {
        if (safeThis == nullptr || safeThis->mainComponent == nullptr)
            return;
            
        // Get the MultiTrackContainer
        auto* container = safeThis->mainComponent->getMultiTrackContainer();
        if (container == nullptr)
            return;
            
        // Check if there's saved state to restore
        const auto& stateTree = safeThis->processor.getGuiStateTree();
        if (!stateTree.isValid() || stateTree.getNumChildren() == 0)
            return; // No saved state, start fresh
            
        try
        {
            // Restore the visual state (clips, BPMs, positions)
            container->restoreGuiState(stateTree);
            
            // Force repaint to show restored content
            safeThis->repaint();
            if (safeThis->mainComponent)
                safeThis->mainComponent->repaint();
        }
        catch (...)
        {
            // If restoration fails, just start fresh
            DBG("State restoration failed - starting with empty timeline");
        }
    });
}

DrumGrooveEditor::~DrumGrooveEditor()
{
    stopTimer();
    
    // Save state as backup
    saveStateToProcessor();
    
    setLookAndFeel(nullptr);
}

void DrumGrooveEditor::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);
}

void DrumGrooveEditor::resized()
{
    // Prevent recursive resizing
    if (isResizing) return;
    juce::ScopedValueSetter<bool> resizingGuard(isResizing, true);

    auto bounds = getLocalBounds();

    // Update main component to use FULL window size
    if (mainComponent)
    {
        mainComponent->setBounds(bounds);
        mainComponent->resized();
    }

    // Store current size (no validation, no saving)
    currentEditorState.width = bounds.getWidth();
    currentEditorState.height = bounds.getHeight();

    // Get window position if available
    if (auto* peer = getPeer())
    {
        auto windowBounds = peer->getBounds();
        currentEditorState.x = windowBounds.getX();
        currentEditorState.y = windowBounds.getY();
    }
}

void DrumGrooveEditor::setVisible(bool shouldBeVisible)
{
    if (!shouldBeVisible)
    {
        // Save state when hiding - simple approach
        saveStateToProcessor();
    }

    AudioProcessorEditor::setVisible(shouldBeVisible);
}

void DrumGrooveEditor::saveStateToProcessor()
{
    // Save current GUI state to EditorState first
    saveGuiState();
    
    // Save MultiTrackContainer state (tracks, clips, BPM)
    if (mainComponent)
    {
        if (auto* container = mainComponent->getMultiTrackContainer())
        {
            processor.saveCompleteGuiState(container);
        }
    }

    // Get processor GUI state structure
    auto guiState = processor.getGuiState();

    // Update window state
    guiState.editorWidth = currentEditorState.width;
    guiState.editorHeight = currentEditorState.height;
    guiState.editorX = currentEditorState.x;
    guiState.editorY = currentEditorState.y;

    // Update GUI component state
    if (currentEditorState.guiState.currentBrowserFolder.exists())
    {
        guiState.currentBrowserFolder = currentEditorState.guiState.currentBrowserFolder.getFullPathName();
    }

    if (currentEditorState.guiState.selectedFile.exists())
    {
        guiState.selectedFile = currentEditorState.guiState.selectedFile.getFullPathName();
    }

    // Update navigation path
    guiState.browserNavigationPath.clear();
    for (const auto& file : currentEditorState.guiState.browserNavigationPath)
    {
        if (file.exists())
            guiState.browserNavigationPath.add(file.getFullPathName());
    }

    // Save to processor
    processor.setGuiState(guiState);
}

void DrumGrooveEditor::saveGuiState()
{
    if (mainComponent)
    {
        currentEditorState.guiState = mainComponent->saveGuiState();
    }
}

void DrumGrooveEditor::restoreGuiState()
{
    if (mainComponent && currentEditorState.guiState.isValid())
    {
        mainComponent->restoreGuiState(currentEditorState.guiState);
    }
}

bool DrumGrooveEditor::isPositionOnScreen(int x, int y) const
{
    if (x < 0 || y < 0) return false;

    auto displays = juce::Desktop::getInstance().getDisplays();
    for (int i = 0; i < displays.displays.size(); ++i)
    {
        if (displays.displays.getReference(i).totalArea.contains(x, y))
            return true;
    }
    return false;
}

juce::Rectangle<int> DrumGrooveEditor::getValidWindowBounds(int x, int y, int width, int height) const
{
    auto displays = juce::Desktop::getInstance().getDisplays();
    if (auto* primaryDisplay = displays.getPrimaryDisplay())
    {
        auto workArea = primaryDisplay->userArea;

        // Ensure window fits on screen
        x = juce::jlimit(workArea.getX(),
                         workArea.getRight() - width, x);
        y = juce::jlimit(workArea.getY(),
                         workArea.getBottom() - height, y);
    }

    return juce::Rectangle<int>(x, y, width, height);
}
   // EditorState XML methods for complete state backup
   juce::XmlElement* DrumGrooveEditor::EditorState::createXml() const
   {
       auto* element = new juce::XmlElement("EditorState");
       element->setAttribute("x", x);
       element->setAttribute("y", y);
       element->setAttribute("width", width);
       element->setAttribute("height", height);
       element->setAttribute("zoomLevel", zoomLevel);
       element->setAttribute("horizontalScrollPos", horizontalScrollPos);
       element->setAttribute("verticalScrollPos", verticalScrollPos);
       return element;
   }
   
   void DrumGrooveEditor::EditorState::restoreFromXml(const juce::XmlElement& xml)
   {
       if (!xml.hasTagName("EditorState")) return;
   
       x = xml.getIntAttribute("x", -1);
       y = xml.getIntAttribute("y", -1);
       width = xml.getIntAttribute("width", 1300);
       height = xml.getIntAttribute("height", 900);
       zoomLevel = static_cast<float>(xml.getDoubleAttribute("zoomLevel", 1.0f));
       horizontalScrollPos = xml.getIntAttribute("horizontalScrollPos", 0);
       verticalScrollPos = xml.getIntAttribute("verticalScrollPos", 0);
   }

void DrumGrooveEditor::timerCallback()
{
    // Update BPM display
    if (mainComponent)
        mainComponent->updateBPMDisplay();
}

// CRITICAL FIX: Override focus methods to prevent playback stopping when focus is lost
// By providing empty implementations, we prevent JUCE's default behavior from interfering with playback
void DrumGrooveEditor::focusLost(FocusChangeType)
{
    // DO NOTHING - keep playback running even when focus is lost
    // This allows the plugin to continue playing when user clicks outside the window
}

void DrumGrooveEditor::focusGained(FocusChangeType)
{
    // DO NOTHING - normal behavior continues
    // No special action needed when focus is regained
}

// CRITICAL FIX: Override visibilityChanged to prevent playback stopping when minimized
void DrumGrooveEditor::visibilityChanged()
{
    // Call base class but do NOT stop playback
    // This allows the plugin to keep playing even when minimized or hidden
    Component::visibilityChanged();
    
    // Note: We do NOT call any stop/pause methods here
    // The audio processing continues independently of GUI visibility
}