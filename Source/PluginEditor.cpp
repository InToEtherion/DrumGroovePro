#include "PluginEditor.h"
#include "GUI/LookAndFeel/DrumGrooveLookAndFeel.h"
#include "GUI/LookAndFeel/ColourPalette.h"
#include "GUI/Components/MultiTrackContainer.h"
#include "MidiEditorComponent.h"

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

        // Restore browser navigation state
        safeThis->restoreGuiState();
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

        // CRITICAL: Save browser state to processor so it persists
        processor.saveBrowserState(
            currentEditorState.guiState.currentBrowserFolder,
            currentEditorState.guiState.browserNavigationPath,
            currentEditorState.guiState.selectedFile
        );
    }
}

void DrumGrooveEditor::restoreGuiState()
{
    // Restore browser state from processor's persistent storage
    processor.restoreBrowserState(
        currentEditorState.guiState.currentBrowserFolder,
        currentEditorState.guiState.browserNavigationPath,
        currentEditorState.guiState.selectedFile
    );

    if (mainComponent)
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

void DrumGrooveEditor::openMidiEditor(const juce::File& midiFile, DrumLibrary sourceLib)
{
    auto* editor = new MidiEditorComponent(processor.drumLibraryManager, processor.midiProcessor, processor);

    if (editor->openMidiFile(midiFile, sourceLib))
    {
        editor->onClipSaved = [this](const juce::File& savedFile)
        {
            // Optionally refresh browser or timeline
        };

        editor->onEditorClosed = [this, editorPtr = editor]()
        {
            activeEditors.removeObject(editorPtr, true);
        };

        activeEditors.add(editor);
        editor->setVisible(true);
    }
    else
    {
        delete editor;
    }
}

void DrumGrooveEditor::createNewMidiGroove()
{
    // Get list of available origin libraries
    auto libraryNames = processor.drumLibraryManager.getAllSourceLibraryNames();

    if (libraryNames.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Libraries Available",
            "Please add at least one origin library in the settings before creating a new MIDI groove.",
            "OK"
        );
        return;
    }

    // Create library selection dialog
    auto* window = new juce::AlertWindow("Select Origin Library",
                                         "Choose the drum library mapping for this new MIDI groove:",
                                         juce::MessageBoxIconType::QuestionIcon);

    window->addComboBox("library", libraryNames, "Drum Library:");
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    // Set default selection to first item
    window->getComboBoxComponent("library")->setSelectedItemIndex(0);

    window->enterModalState(true,
                            juce::ModalCallbackFunction::create([this, window, libraryNames](int result)
                            {
                                if (result == 1)  // OK pressed
                                {
                                    auto selectedLib = window->getComboBoxComponent("library")->getText();
                                    DrumLibrary sourceLib = processor.drumLibraryManager.getLibraryFromName(selectedLib);

                                    DBG("Creating new MIDI with library: " + selectedLib + " (enum: " + juce::String(static_cast<int>(sourceLib)) + ")");

                                    // Now create the editor with selected library
                                    auto* editor = new MidiEditorComponent(processor.drumLibraryManager,
                                                                           processor.midiProcessor,
                                                                           processor);

                                    double bpm = 120.0;
                                    if (processor.sectionManager.getNumSections() > 0)
                                    {
                                        auto* section = processor.sectionManager.getSection(0);
                                        if (section != nullptr)
                                            bpm = section->bpm;
                                    }

                                    editor->createNewClip(sourceLib, bpm, 4);

                                    editor->onClipSaved = [this](const juce::File& savedFile)
                                    {
                                        // Optionally add to timeline or refresh browser
                                    };

                                    editor->onEditorClosed = [this, editorPtr = editor]()
                                    {
                                        activeEditors.removeObject(editorPtr, true);
                                    };

                                    activeEditors.add(editor);
                                    editor->setVisible(true);
                                }

                                delete window;
                            }), true);
}
