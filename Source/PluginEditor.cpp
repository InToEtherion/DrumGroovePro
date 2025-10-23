#include "PluginEditor.h"
#include "GUI/LookAndFeel/DrumGrooveLookAndFeel.h"
#include "GUI/LookAndFeel/ColourPalette.h"

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
    
    // Base size for 3440x1440 displays
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
        // Use saved size
        targetWidth = guiState.editorWidth;
        targetHeight = guiState.editorHeight;
        
        // Ensure it still fits on screen
        targetWidth = juce::jmin(targetWidth, displayArea.getWidth() - 100);
        targetHeight = juce::jmin(targetHeight, displayArea.getHeight() - 100);
    }
    
    // Set size - simple and direct
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
    
    // Setup OpenGL
    openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::OpenGLVersion::openGL3_2);
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);

    // Restore GUI state (clips, tracks, BPM, etc.) - CRITICAL
    restoreGuiState();
    
    // Restore MultiTrackContainer state with delay to ensure components are ready
    juce::Timer::callAfterDelay(100, [this]()
    {
        processor.restoreCompleteGuiState();
    });

    // Start timer
    startTimer(200);
}

DrumGrooveEditor::~DrumGrooveEditor()
{
    stopTimer();
    
    // Save state as backup - critical for persistence
    // This ensures state is saved even if setVisible(false) wasn't called
    saveStateToProcessor();
    
    openGLContext.detach();
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

// EditorState XML methods for future file-based backup if needed
void DrumGrooveEditor::newOpenGLContextCreated()
{
    // Initialize OpenGL resources for hardware acceleration
}

void DrumGrooveEditor::renderOpenGL()
{
    // OpenGL rendering for hardware acceleration
    juce::OpenGLHelpers::clear(ColourPalette::mainBackground);
}

void DrumGrooveEditor::openGLContextClosing()
{
    // Clean up OpenGL resources
}

void DrumGrooveEditor::timerCallback()
{
    // Update BPM display
    if (mainComponent)
        mainComponent->updateBPMDisplay();
}
