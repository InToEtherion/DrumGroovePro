#include "GrooveBrowser.h"
#include "DrumPartsColumn.h"
#include "../../PluginProcessor.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include <cstdlib>  // for std::getenv

#if JUCE_LINUX
#include <sys/stat.h>  // for chmod
#endif

//==============================================================================
// DraggableListItemOverlay implementation
//==============================================================================

void DraggableListItemOverlay::mouseDown(const juce::MouseEvent& e)
{
    isDragging = false;

    // Only handle CTRL clicks, pass everything else through
    if (!e.mods.isCtrlDown())
    {
        // Pass the event to the ListBox below by not consuming it
        e.eventComponent->getParentComponent()->mouseDown(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
    }
}

void DraggableListItemOverlay::mouseDrag(const juce::MouseEvent& e)
{
    #if JUCE_LINUX
    DBG("mouseDrag: Ctrl=" + juce::String(e.mods.isCtrlDown()) +
    " Alt=" + juce::String(e.mods.isAltDown()) +
    " Shift=" + juce::String(e.mods.isShiftDown()) +
    " Distance=" + juce::String(e.getDistanceFromDragStart()));
    #endif

    if (e.mods.isCtrlDown())
    {
        // CTRL+Drag = External drag to DAW
        if (!isDragging && e.getDistanceFromDragStart() > 5)
        {
            isDragging = true;
            DBG("DraggableListItemOverlay: External drag detected for row " + juce::String(row));

            if (parentColumn)
            {
                parentColumn->startExternalDrag(row);
            }
        }
    }
    else
    {
        // Pass through to ListBox for normal internal drag
        e.eventComponent->getParentComponent()->mouseDrag(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
    }
}

void DraggableListItemOverlay::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mods.isCtrlDown())
    {
        // Pass through to ListBox
        e.eventComponent->getParentComponent()->mouseUp(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
    }

    isDragging = false;
}

void DraggableListItemOverlay::mouseDoubleClick(const juce::MouseEvent& e)
{
    // CRITICAL FIX: Forward double-click events to the ListBox
    // This allows normal double-click functionality to work
    if (parentColumn)
    {
        // Get the ListBox and trigger its double-click handler
        parentColumn->listBoxItemDoubleClicked(row, e);
    }
}

// BrowserColumn implementation
BrowserColumn::BrowserColumn(const juce::String& columnName, DrumGrooveProcessor& proc)
: columnTitle(columnName), selectedRow(-1), processor(proc)
{
    setModel(this);
    setRowHeight(24);
    // Semi-transparent background (0.7f alpha) for navigation columns
    setColour(juce::ListBox::backgroundColourId, ColourPalette::mainBackground.withAlpha(0.75f));
    setMultipleSelectionEnabled(false);
    loadIcons();
}

juce::Component* BrowserColumn::refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    // Create transparent overlay component for each row that handles CTRL+Drag
    auto* overlay = dynamic_cast<DraggableListItemOverlay*>(existingComponentToUpdate);

    if (overlay == nullptr)
    {
        overlay = new DraggableListItemOverlay(this);
    }

    overlay->setRow(rowNumber);

    return overlay;
}
// Add destructor to clean up temp files:
BrowserColumn::~BrowserColumn()
{
    // Clean up any temporary drag files
    if (lastTempDragFile.existsAsFile())
    {
        lastTempDragFile.deleteFile();
    }
}

bool BrowserColumn::keyPressed(const juce::KeyPress& key)
{
    // Forward arrow keys to parent GrooveBrowser for MIDI navigation
    if (key.getKeyCode() == juce::KeyPress::upKey ||
        key.getKeyCode() == juce::KeyPress::downKey)
    {
        // Get the parent GrooveBrowser
        if (auto* parent = findParentComponentOfClass<GrooveBrowser>())
        {
            // Forward the key to the parent
            return parent->keyPressed(key);
        }
    }

    // Let ListBox handle other keys (left/right for column navigation, etc.)
    return ListBox::keyPressed(key);
}

void BrowserColumn::loadIcons()
{
    // Load actual icons from Resources folder
    // Try multiple paths to find the resources
    juce::File resourcesDir;

    // Try relative to executable
    resourcesDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
    .getParentDirectory()
    .getChildFile("Resources");

    if (!resourcesDir.exists())
    {
        // Try in VST3 bundle structure
        resourcesDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getParentDirectory()
        .getChildFile("Resources");
    }

    // Load folder icon
    auto folderIconFile = resourcesDir.getChildFile("icons/folder/16x16/folder.png");
    if (folderIconFile.existsAsFile())
    {
        folderIcon = juce::ImageCache::getFromFile(folderIconFile);
    }
    else
    {
        // Create fallback icon if resource not found
        folderIcon = juce::Image(juce::Image::RGB, 16, 16, true);
        juce::Graphics g(folderIcon);
        g.setColour(ColourPalette::warningOrange);
        g.fillRect(0, 4, 14, 10);
    }

    // Load MIDI icon
    auto midiIconFile = resourcesDir.getChildFile("icons/midi/16x16/midi.png");
    if (midiIconFile.existsAsFile())
    {
        midiIcon = juce::ImageCache::getFromFile(midiIconFile);
    }
    else
    {
        // Create fallback icon if resource not found
        midiIcon = juce::Image(juce::Image::RGB, 16, 16, true);
        juce::Graphics g2(midiIcon);
        g2.setColour(ColourPalette::primaryBlue);
        g2.fillEllipse(4, 4, 8, 8);
    }
}

int BrowserColumn::getNumRows()
{
    return items.size();
}

void BrowserColumn::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                     int width, int height, bool rowIsSelected)
{
    if (rowNumber >= items.size())
        return;

    if (rowIsSelected)
    {
        g.fillAll(ColourPalette::primaryBlue);
        g.setColour(ColourPalette::primaryText);
    }
    else
    {
        // 100% transparent background - only draw text and icons
        g.setColour(ColourPalette::secondaryText);

        if (isMouseOver())
        {
            auto mousePos = getMouseXYRelative();
            auto itemBounds = getRowPosition(rowNumber, true);
            if (itemBounds.contains(mousePos))
            {
                // Optional: Add a subtle highlight on hover (semi-transparent)
                g.fillAll(ColourPalette::secondaryBackground.withAlpha(0.3f));
                g.setColour(ColourPalette::primaryText);
            }
        }
    }

    // Draw icon
    int iconX = 4;
    int iconY = (height - 16) / 2;

    if (itemIsFolder[rowNumber])
    {
        g.drawImageAt(folderIcon, iconX, iconY);
    }
    else
    {
        g.drawImageAt(midiIcon, iconX, iconY);
    }

    // Draw text (just the filename, no BPM info)
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    g.setFont(lnf.getNormalFont().withHeight(13.0f));

    juce::String text = items[rowNumber];
    g.drawText(text, 24, 0, width - 28, height, juce::Justification::centredLeft);
}

void BrowserColumn::selectedRowsChanged(int lastRow)
{
    selectedRow = lastRow;
    if (onSelectionChange)
        onSelectionChange();
}

void BrowserColumn::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick(row);
}

void BrowserColumn::setItems(const juce::StringArray& newItems,
                             const juce::Array<bool>& newIsFolder,
                             const juce::Array<juce::File>& filePaths)
{
    items = newItems;
    itemIsFolder = newIsFolder;
    itemFiles = filePaths;
    updateContent();
}

void BrowserColumn::clearItems()
{
    items.clear();
    itemIsFolder.clear();
    itemFiles.clear();
    selectedRow = -1;
    updateContent();
}

juce::String BrowserColumn::getSelectedItem() const
{
    if (selectedRow >= 0 && selectedRow < items.size())
        return items[selectedRow];
    return {};
}

bool BrowserColumn::isSelectedItemFolder() const
{
    if (selectedRow >= 0 && selectedRow < itemIsFolder.size())
        return itemIsFolder[selectedRow];
    return false;
}

juce::File BrowserColumn::getSelectedFile() const
{
    if (selectedRow >= 0 && selectedRow < itemFiles.size())
        return itemFiles[selectedRow];
    return {};
}

juce::var BrowserColumn::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.size() > 0)
    {
        int row = selectedRows[0];
        if (row >= 0 && row < items.size())
        {
            juce::String filename = items[row];

            if (itemIsFolder[row])
            {
                // Folder drag
                juce::String fullPath = itemFiles[row].getFullPathName();
                return juce::var(filename + "|FOLDER|" + fullPath);
            }
            else
            {
                // File drag - include current header BPM for track inheritance
                juce::String fullPath;
                if (row < itemFiles.size() && itemFiles[row].existsAsFile())
                {
                    fullPath = itemFiles[row].getFullPathName();
                }
                else
                {
                    fullPath = filename;
                }

                // Get current header BPM (sync to host or manual)
                bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
                double headerBPM = syncToHost ?
                processor.getHostBPM() :
                processor.parameters.getRawParameterValue("manualBPM")->load();

                // Format: filename|fullPath|headerBPM
                return juce::var(filename + "|" + fullPath + "|" + juce::String(headerBPM, 2));
            }
        }
    }
    return {};
}

void BrowserColumn::startExternalDrag(int rowNumber)
{
    if (isExternalDragActive)
        return;

    isExternalDragActive = true;

    DBG("=== STARTING EXTERNAL DRAG FROM ROW " + juce::String(rowNumber) + " ===");

    // Validate selection
    if (rowNumber < 0 || rowNumber >= items.size())
    {
        DBG("ERROR: Invalid row number");
        isExternalDragActive = false;
        return;
    }

    if (itemIsFolder[rowNumber])
    {
        DBG("ERROR: Cannot drag folders");
        isExternalDragActive = false;
        return;
    }

    juce::File originalMidiFile = itemFiles[rowNumber];
    if (!originalMidiFile.existsAsFile())
    {
        DBG("ERROR: File doesn't exist: " + originalMidiFile.getFullPathName());
        isExternalDragActive = false;
        return;
    }

    DBG("File: " + originalMidiFile.getFileName());
    DBG("Path: " + originalMidiFile.getFullPathName());

    // Get the GrooveBrowser (which is the DragAndDropContainer)
    auto* grooveBrowser = findParentComponentOfClass<GrooveBrowser>();
    if (!grooveBrowser)
    {
        DBG("ERROR: No GrooveBrowser parent found!");
        isExternalDragActive = false;
        return;
    }
    auto* dragContainer = dynamic_cast<juce::DragAndDropContainer*>(grooveBrowser);
    if (!dragContainer)
    {
        DBG("ERROR: GrooveBrowser is not a DragAndDropContainer!");
        isExternalDragActive = false;
        return;
    }

    DBG("Found DragAndDropContainer");

    // Get current BPM
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    double currentBPM = syncToHost ?
    processor.getHostBPM() :
    processor.parameters.getRawParameterValue("manualBPM")->load();

    DBG("Current BPM: " + juce::String(currentBPM, 2));

    // Read original MIDI to check BPM
    juce::MidiFile originalMidi;
    juce::FileInputStream inputStream(originalMidiFile);
    if (!inputStream.openedOk() || !originalMidi.readFrom(inputStream))
    {
        DBG("ERROR: Cannot read MIDI file");
        isExternalDragActive = false;
        return;
    }

    // Get original BPM from tempo track
    double originalBPM = 120.0;
    bool foundBPM = false;
    for (int track = 0; track < originalMidi.getNumTracks(); ++track)
    {
        auto* tempoTrack = originalMidi.getTrack(track);
        for (int i = 0; i < tempoTrack->getNumEvents(); ++i)
        {
            auto& event = tempoTrack->getEventPointer(i)->message;
            if (event.isTempoMetaEvent())
            {
                originalBPM = 60000000.0 / event.getTempoSecondsPerQuarterNote() / 1000000.0;
                foundBPM = true;
                DBG("Found original BPM: " + juce::String(originalBPM, 2));
                break;
            }
        }
        if (foundBPM) break;
    }

    juce::File fileToDrag;

    // Only create temp file if BPM adjustment is needed
    if (std::abs(originalBPM - currentBPM) > 0.01)
    {
        DBG("BPM adjustment needed: " + juce::String(originalBPM, 2) + " -> " + juce::String(currentBPM, 2));

        // Create temp file with unique name
        juce::String tempFileName = "DrumGroovePro_drag_" +
        juce::String(juce::Random::getSystemRandom().nextInt64()) + ".mid";
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(tempFileName);

        DBG("Creating temp file: " + tempFile.getFullPathName());

        // Adjust BPM: scale = originalBPM / currentBPM
        double tempoScale = originalBPM / currentBPM;
        juce::MidiFile adjustedMidi;

        // Copy all tracks and scale timestamps
        for (int track = 0; track < originalMidi.getNumTracks(); ++track)
        {
            auto* sourceTrack = originalMidi.getTrack(track);
            juce::MidiMessageSequence newTrack;

            for (int i = 0; i < sourceTrack->getNumEvents(); ++i)
            {
                auto& midiEvent = sourceTrack->getEventPointer(i)->message;
                double oldTimestamp = sourceTrack->getEventTime(i);
                double newTimestamp = oldTimestamp * tempoScale;

                // Update tempo events with new BPM
                if (midiEvent.isTempoMetaEvent())
                {
                    double microsecondsPerQuarterNote = 60000000.0 / currentBPM;
                    auto tempoEvent = juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerQuarterNote));
                    tempoEvent.setTimeStamp(newTimestamp);
                    newTrack.addEvent(tempoEvent);
                }
                else
                {
                    auto copiedMessage = midiEvent;
                    copiedMessage.setTimeStamp(newTimestamp);
                    newTrack.addEvent(copiedMessage);
                }
            }

            newTrack.updateMatchedPairs();
            adjustedMidi.addTrack(newTrack);
        }

        // Set time format to match original
        adjustedMidi.setTicksPerQuarterNote(originalMidi.getTimeFormat());

        // Write to temp file - WITH EXPLICIT FLUSHING
        {
            juce::FileOutputStream outputStream(tempFile);
            if (!outputStream.openedOk())
            {
                DBG("ERROR: Cannot open temp file for writing!");
                DBG("Path: " + tempFile.getFullPathName());
                isExternalDragActive = false;
                return;
            }

            if (!adjustedMidi.writeTo(outputStream))
            {
                DBG("ERROR: Failed to write MIDI data to temp file!");
                isExternalDragActive = false;
                return;
            }

            // CRITICAL: Explicitly flush and close the stream
            outputStream.flush();
        } // Stream goes out of scope and closes here

        #if JUCE_LINUX
        // CRITICAL: Set file permissions on Linux for Reaper compatibility
        // Reaper might reject files without proper read permissions
        chmod(tempFile.getFullPathName().toRawUTF8(), 0644);  // rw-r--r--
        DBG("Set Linux file permissions: 0644");
        #endif

        // Wait a tiny bit for filesystem to sync
        juce::Thread::sleep(50);

        // Verify the file was created and has content
        if (!tempFile.existsAsFile())
        {
            DBG("ERROR: Temp file doesn't exist after writing!");
            isExternalDragActive = false;
            return;
        }

        juce::int64 fileSize = tempFile.getSize();
        if (fileSize == 0)
        {
            DBG("ERROR: Temp file is empty (0 bytes)!");
            isExternalDragActive = false;
            return;
        }

        fileToDrag = tempFile;
        DBG("Temp file created successfully:");
        DBG("  Path: " + tempFile.getFullPathName());
        DBG("  Size: " + juce::String(fileSize) + " bytes");

        // Clean up previous temp file
        if (lastTempDragFile.existsAsFile())
            lastTempDragFile.deleteFile();
        lastTempDragFile = tempFile;
    }
    else
    {
        #if JUCE_LINUX
        // CRITICAL: On Linux, always create a temp copy even without BPM adjustment
        // Reaper may not have access to the original file location
        DBG("Linux: Creating temp copy for Reaper compatibility");

        // Use the ORIGINAL filename in temp directory
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile(originalMidiFile.getFileName());

        DBG("Temp file path: " + tempFile.getFullPathName());

        // Copy original file to temp location
        if (!originalMidiFile.copyFileTo(tempFile))
        {
            DBG("ERROR: Failed to copy file to temp location");
            isExternalDragActive = false;
            return;
        }

        // Set proper permissions for Reaper
        chmod(tempFile.getFullPathName().toRawUTF8(), 0644);
        DBG("Temp copy created with 0644 permissions");
        DBG("Original: " + originalMidiFile.getFullPathName());
        DBG("Temp:     " + tempFile.getFullPathName());

        fileToDrag = tempFile;

        // Clean up previous temp file
        if (lastTempDragFile.existsAsFile() && lastTempDragFile != tempFile)
            lastTempDragFile.deleteFile();
        lastTempDragFile = tempFile;
        #else
        // Windows/Mac: Can use original file directly
        fileToDrag = originalMidiFile;
        DBG("No BPM adjustment needed, using original file");
        #endif
    }

    // Final validation before drag
    if (!fileToDrag.existsAsFile())
    {
        DBG("ERROR: Final file check failed - file doesn't exist");
        isExternalDragActive = false;
        return;
    }

    juce::int64 finalSize = fileToDrag.getSize();
    if (finalSize == 0)
    {
        DBG("ERROR: File is empty (0 bytes)");
        isExternalDragActive = false;
        return;
    }

    DBG("=== CALLING performExternalDragDropOfFiles ===");
    DBG("File: " + fileToDrag.getFullPathName());
    DBG("Size: " + juce::String(finalSize) + " bytes");

    #if JUCE_LINUX
    DBG("Linux file path: " + fileToDrag.getFullPathName());
    DBG("File exists: " + juce::String(fileToDrag.existsAsFile() ? "YES" : "NO"));
    DBG("File URL: " + fileToDrag.getFullPathName().toRawUTF8());
    #endif

    // Use absolute path
    juce::StringArray files;
    files.add(fileToDrag.getFullPathName());

    dragContainer->performExternalDragDropOfFiles(files, true, this, [this, fileToDrag]()
    {
        DBG("=== DRAG COMPLETED ===");
        isExternalDragActive = false;

        // Cleanup temp files after a delay
        if (fileToDrag.getFileName().startsWith("DrumGroovePro_drag_"))
        {
            juce::Timer::callAfterDelay(3000, [fileToDrag]()
            {
                if (fileToDrag.existsAsFile())
                {
                    fileToDrag.deleteFile();
                    DBG("Temp file cleaned up");
                }
            });
        }
    });

    DBG("performExternalDragDropOfFiles returned - drag should be active now!");
}

//==============================================================================
// GrooveBrowser implementation

GrooveBrowser::GrooveBrowser(DrumGrooveProcessor& p)
: processor(p),
browserPlaybackActive(false),
currentlyPlayingFile(juce::File()),
currentSourceLibrary(DrumLibrary::Unknown),
isHandlingTargetLibraryChange(false)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    // Target Library controls - positioned at top of browser
    targetLibraryLabel.setText("Target Drum Library:", juce::dontSendNotification);
    targetLibraryLabel.setFont(lnf.getNormalFont().withHeight(13.0f));
    targetLibraryLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(targetLibraryLabel);

    // Populate combo box with library names (in alphabetical order)
    auto libraryNames = DrumLibraryManager::getAllLibraryNames();
    for (int i = 0; i < libraryNames.size(); ++i)
    {
        targetLibraryCombo.addItem(libraryNames[i], i + 1);
    }

    addAndMakeVisible(targetLibraryCombo);

    // Load saved library ENUM from config
    DrumLibrary savedLibrary = processor.drumLibraryManager.getLastSelectedTargetLibrary();
    juce::String savedLibraryName = DrumLibraryManager::getLibraryName(savedLibrary);

    DBG("=== GrooveBrowser Constructor ===");
    DBG("Loaded from config:");
    DBG("  Enum value: " + juce::String(static_cast<int>(savedLibrary)));
    DBG("  Library name: " + savedLibraryName);

    // Find this library name in the ComboBox and get its ID
    int comboIdToSelect = 0;
    for (int i = 0; i < libraryNames.size(); ++i)
    {
        if (libraryNames[i] == savedLibraryName)
        {
            comboIdToSelect = i + 1;  // ComboBox IDs are 1-based
            DBG("  Found in ComboBox at index " + juce::String(i) + ", ID: " + juce::String(comboIdToSelect));
            break;
        }
    }

    if (comboIdToSelect == 0)
    {
        DBG("  WARNING: Library not found in ComboBox, defaulting to General MIDI");
        savedLibraryName = "General MIDI";
        savedLibrary = DrumLibrary::GeneralMIDI;

        // Find General MIDI in the list
        for (int i = 0; i < libraryNames.size(); ++i)
        {
            if (libraryNames[i] == "General MIDI")
            {
                comboIdToSelect = i + 1;
                break;
            }
        }
    }

    // Set ComboBox selection BEFORE creating attachment
    targetLibraryCombo.setSelectedId(comboIdToSelect, juce::dontSendNotification);

    DBG("ComboBox set to ID: " + juce::String(comboIdToSelect) + ", text: " + targetLibraryCombo.getText());

    // Now update the parameter to match
    // The parameter stores the index (0-based) in the alphabetical list
    int paramIndex = comboIdToSelect - 1;  // Convert 1-based ID to 0-based index

    auto* targetLibParam = processor.parameters.getParameter("targetLibrary");
    if (targetLibParam)
    {
        float normalizedValue = processor.parameters.getParameterRange("targetLibrary").convertTo0to1(paramIndex);
        targetLibParam->setValueNotifyingHost(normalizedValue);

        DBG("Set parameter to index: " + juce::String(paramIndex) + " (normalized: " + juce::String(normalizedValue) + ")");
    }

    // Create parameter attachment
    libraryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "targetLibrary", targetLibraryCombo);

    // Add listeners AFTER attachment
    targetLibraryCombo.addListener(this);
    processor.parameters.addParameterListener("targetLibrary", this);

    DBG("GrooveBrowser initialized successfully");
    DBG("  Final ComboBox text: " + targetLibraryCombo.getText());

    // Setup viewport
    viewport.setViewedComponent(&columnsContainer, false);
    viewport.setScrollBarsShown(false, true);
    addAndMakeVisible(viewport);

    // NEW: Enable keyboard focus so space bar and arrow keys work
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    startTimer(100);
}


GrooveBrowser::~GrooveBrowser()
{
    stopTimer();
    targetLibraryCombo.removeListener(this);
    processor.parameters.removeParameterListener("targetLibrary", this);
    removeKeyListener(this);
}

void GrooveBrowser::paint(juce::Graphics& g)
{
    // 100% transparent - no background fill
    // Background image from MainComponent will show through completely
}

void GrooveBrowser::resized()
{
    auto bounds = getLocalBounds();

    // Target Library at top right - REDUCED SPACING
    auto topBar = bounds.removeFromTop(35);
    auto rightSection = topBar.removeFromRight(350); // Adjusted width
    targetLibraryCombo.setBounds(rightSection.removeFromRight(200).reduced(0, 5));
    rightSection.removeFromRight(2); // REDUCED: Small gap between label and combo (was 5, now 2)
    targetLibraryLabel.setBounds(rightSection.reduced(0, 5));

    // Viewport fills rest
    viewport.setBounds(bounds);

    updateColumnsLayout();
}

// NEW: ComboBox::Listener implementation
void GrooveBrowser::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &targetLibraryCombo)
    {
        DBG("=== ComboBox Changed ===");

        // Get the selected text and convert to enum
        juce::String selectedText = targetLibraryCombo.getText();
        DrumLibrary selectedLibrary = DrumLibraryManager::getLibraryFromName(selectedText);

        DBG("  Selected text: " + selectedText);
        DBG("  Converted to enum: " + juce::String(static_cast<int>(selectedLibrary)));
        DBG("  Enum name: " + DrumLibraryManager::getLibraryName(selectedLibrary));

        // Save the ENUM VALUE to config (not the ComboBox ID!)
        processor.drumLibraryManager.setLastSelectedTargetLibrary(selectedLibrary);

        DBG("  Saved to config.xml: " + juce::String(static_cast<int>(selectedLibrary)));

        // Handle the library change
        handleTargetLibraryChange();
    }
}

// NEW: AudioProcessorValueTreeState::Listener implementation
void GrooveBrowser::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "targetLibrary")
    {
        DBG("=== parameterChanged: targetLibrary ===");
        DBG("  New value: " + juce::String(newValue));

        // Sync combo box with parameter value
        syncComboBoxWithParameter();

        // Handle the library change
        handleTargetLibraryChange();
    }
}

// NEW: Handle target library change
void GrooveBrowser::handleTargetLibraryChange()
{
    // Prevent recursive calls
    if (isHandlingTargetLibraryChange)
        return;

    isHandlingTargetLibraryChange = true;

    DrumLibrary newTargetLibrary = getCurrentTargetLibrary();

    DBG("=== Target Library Change ===");
    DBG("New Target: " + juce::String(static_cast<int>(newTargetLibrary)) +
    " (" + targetLibraryCombo.getText() + ")");

    // If we have a current MIDI file loaded, re-dissect it immediately
    if (currentMidiFile.existsAsFile() && !currentDrumParts.isEmpty())
    {
        DBG("Re-dissecting current file in real-time");
        redissectCurrentMidiFile();
    }

    isHandlingTargetLibraryChange = false;
}

void GrooveBrowser::syncComboBoxWithParameter()
{
    // Get current parameter value
    int paramValue = static_cast<int>(processor.parameters.getRawParameterValue("targetLibrary")->load());

    // Convert to combo box ID (param is 0-based index, combo is 1-based ID)
    int comboId = paramValue + 1;

    // Get current combo box selection
    int currentComboId = targetLibraryCombo.getSelectedId();

    if (currentComboId != comboId)
    {
        // Update combo box without triggering listener
        targetLibraryCombo.removeListener(this);
        targetLibraryCombo.setSelectedId(comboId, juce::dontSendNotification);
        targetLibraryCombo.addListener(this);

        DBG("syncComboBoxWithParameter: Updated combo box");
        DBG("  Param value: " + juce::String(paramValue));
        DBG("  Combo ID: " + juce::String(comboId));
        DBG("  Combo text: " + targetLibraryCombo.getText());
    }
    else
    {
        DBG("syncComboBoxWithParameter: Already in sync (ID=" + juce::String(comboId) + ")");
    }
}

// NEW: Re-dissect current MIDI file with new target library
void GrooveBrowser::redissectCurrentMidiFile()
{
    if (!currentMidiFile.existsAsFile())
        return;

    DrumLibrary newTargetLibrary = getCurrentTargetLibrary();

    DBG("Re-dissecting " + currentMidiFile.getFileName() +
    " with target library: " + juce::String(static_cast<int>(newTargetLibrary)) +
    " (" + DrumLibraryManager::getLibraryName(newTargetLibrary) + ")");

    // CRITICAL: Always perform full re-dissection to ensure accurate real-time updates
    // This guarantees that all drum parts are correctly categorized and remapped
    currentDrumParts = midiDissector.dissectMidiFileWithLibraryManager(
        currentMidiFile,
        currentSourceLibrary,
        newTargetLibrary,
        processor.drumLibraryManager);

    // Update source library in parts
    for (auto& part : currentDrumParts)
    {
        part.sourceLibrary = currentSourceLibrary;
    }

    // Update the parts column immediately for real-time visual feedback
    if (partsColumn && !currentDrumParts.isEmpty())
    {
        partsColumn->setDrumParts(currentDrumParts, currentMidiFile);
        DBG("Parts column updated with " + juce::String(currentDrumParts.size()) + " parts");
    }

    DBG("Re-dissection complete");
}

// NEW: Get current target library from combo box
DrumLibrary GrooveBrowser::getCurrentTargetLibrary() const
{
    // Get the selected text from combo box
    juce::String selectedText = targetLibraryCombo.getText();

    if (selectedText.isEmpty())
    {
        DBG("WARNING: ComboBox text is empty, defaulting to General MIDI");
        return DrumLibrary::GeneralMIDI;
    }

    // Convert the text to enum using the library manager
    DrumLibrary library = DrumLibraryManager::getLibraryFromName(selectedText);

    DBG("getCurrentTargetLibrary:");
    DBG("  ComboBox text: " + selectedText);
    DBG("  Enum value: " + juce::String(static_cast<int>(library)));

    return library;
}

void GrooveBrowser::loadFolderContents(const juce::File& folder)
{
    if (!folder.exists())
        return;

    folderColumns.clear();
    removePartsColumn(); // Clear any existing parts column
    navigationPath.clear();
    currentPath = folder;

    navigateToFolder(folder, 0);
}

void GrooveBrowser::handleFileSelection(const juce::File& file)
{
    DBG("File selected: " + file.getFullPathName());

    // Notify main component about file selection
    if (onFileSelected)
        onFileSelected(file);
}

void GrooveBrowser::handleFileDoubleClick(const juce::File& file)
{
    if (file.existsAsFile() && file.hasFileExtension(".mid;.midi"))
    {
        // Stop and clear current playback
        processor.midiProcessor.stop();
        processor.midiProcessor.clearAllClips();

        // Find source library for this file
        DrumLibrary sourceLib = DrumLibrary::Unknown;
        auto& library = processor.drumLibraryManager;

        for (int i = 0; i < library.getNumRootFolders(); ++i)
        {
            auto rootFolder = library.getRootFolder(i);
            if (file.getFullPathName().startsWith(rootFolder.getFullPathName()))
            {
                sourceLib = library.getRootFolderSourceLibrary(i);
                break;
            }
        }

        // Get the header BPM (user's desired playback speed)
        double headerBPM = 120.0;
        bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;

        if (syncToHost)
        {
            headerBPM = processor.getHostBPM();
        }
        else
        {
            headerBPM = processor.parameters.getRawParameterValue("manualBPM")->load();
        }

        // Get the actual duration of the MIDI file for accurate loop length
        juce::MidiFile midiFile;
        juce::FileInputStream stream(file);
        if (midiFile.readFrom(stream))
        {
            double ticksPerQuarterNote = midiFile.getTimeFormat();
            if (ticksPerQuarterNote <= 0)
            {
                ticksPerQuarterNote = 480.0;
            }

            // Read actual BPM from file
            double fileBPM = 120.0;
            // Read BPM and time signature
            int timeSignatureNumerator = 4;
            int timeSignatureDenominator = 4;

            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                auto* track = const_cast<juce::MidiMessageSequence*>(midiFile.getTrack(t));
                if (!track) continue;

                track->updateMatchedPairs();

                for (int e = 0; e < track->getNumEvents(); ++e)
                {
                    const auto* event = track->getEventPointer(e);
                    if (!event) continue;

                    if (event->message.isTempoMetaEvent())
                    {
                        fileBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                    }
                    else if (event->message.isTimeSignatureMetaEvent())
                    {
                        event->message.getTimeSignatureInfo(timeSignatureNumerator, timeSignatureDenominator);
                    }
                }
            }

            // Find max time across ALL tracks and ALL events (including note-offs)
            double maxTimeInTicks = 0;

            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                auto* track = const_cast<juce::MidiMessageSequence*>(midiFile.getTrack(t));
                if (!track) continue;

                track->updateMatchedPairs();

                for (int e = 0; e < track->getNumEvents(); ++e)
                {
                    double eventTime = track->getEventTime(e);
                    maxTimeInTicks = juce::jmax(maxTimeInTicks, eventTime);

                    // Also check note-off times
                    auto* eventHolder = track->getEventPointer(e);
                    if (eventHolder && eventHolder->noteOffObject != nullptr)
                    {
                        double noteOffTime = eventHolder->noteOffObject->message.getTimeStamp();
                        maxTimeInTicks = juce::jmax(maxTimeInTicks, noteOffTime);
                    }
                }
            }

            // Calculate ticks per bar and round UP to complete bars
            double ticksPerBar = ticksPerQuarterNote * (4.0 / timeSignatureDenominator) * timeSignatureNumerator;
            double numBars = std::ceil(maxTimeInTicks / ticksPerBar);
            double roundedTicks = numBars * ticksPerBar;

            double fileDurationInSeconds = (roundedTicks / ticksPerQuarterNote) * (60.0 / fileBPM);

            // Ensure minimum duration
            fileDurationInSeconds = juce::jmax(0.1, fileDurationInSeconds);

            // NEW: Enable loop by default for browser playback
            // Add the clip with proper duration IN SECONDS
            processor.midiProcessor.addMidiClip(file, 0.0, sourceLib, 120.0, headerBPM, 0,
                                                fileDurationInSeconds,
                                                "preview_" + juce::Uuid().toString());
            processor.midiProcessor.setPlayheadPosition(0.0);

            // NEW: Set loop range to file duration IN SECONDS and enable looping
            processor.midiProcessor.setLoopRange(0.0, fileDurationInSeconds);
            processor.midiProcessor.setLoopEnabled(true);

            processor.midiProcessor.play();

            // Track browser playback state
            browserPlaybackActive = true;
            currentlyPlayingFile = file;

            DBG("Playing file in LOOP: " + file.getFullPathName() +
            " at " + juce::String(headerBPM, 2) + " BPM" +
            ", ticks: " + juce::String(maxTimeInTicks, 0) +
            ", PPQN: " + juce::String(ticksPerQuarterNote, 0) +
            ", duration: " + juce::String(fileDurationInSeconds, 3) + "s");
        }
    }
}


void GrooveBrowser::handleColumnDoubleClick(int columnIndex, int row)
{
    if (columnIndex < 0 || columnIndex >= folderColumns.size())
        return;

    auto* column = folderColumns[columnIndex];

    // CRITICAL FIX: Ignore double-click on folders - do nothing
    if (column->isSelectedItemFolder())
    {
        return;  // Only single-click navigation for folders
    }

    // Double-click on MIDI file = play
    juce::File selectedFile = column->getSelectedFile();
    if (selectedFile.existsAsFile() && selectedFile.hasFileExtension(".mid;.midi"))
    {
        handleFileDoubleClick(selectedFile);
    }
}

void GrooveBrowser::handleMidiFileSelection(const juce::File& midiFile)
{
    if (!midiFile.existsAsFile() || !midiFile.hasFileExtension(".mid;.midi"))
    {
        removePartsColumn();
        return;
    }

    currentMidiFile = midiFile;

    // Find source library for this file
    DrumLibrary sourceLib = DrumLibrary::Unknown;
    auto& library = processor.drumLibraryManager;

    for (int i = 0; i < library.getNumRootFolders(); ++i)
    {
        auto rootFolder = library.getRootFolder(i);
        if (midiFile.getFullPathName().startsWith(rootFolder.getFullPathName()))
        {
            sourceLib = library.getRootFolderSourceLibrary(i);
            break;
        }
    }

    // Store the source library for future remapping
    currentSourceLibrary = sourceLib;

    // Get the current target library
    DrumLibrary targetLib = getCurrentTargetLibrary();

    // Dissect the MIDI file with BOTH source and target libraries AND library manager
    currentDrumParts = midiDissector.dissectMidiFileWithLibraryManager(midiFile, sourceLib, targetLib, processor.drumLibraryManager);

    if (!currentDrumParts.isEmpty())
    {
        // Add or update parts column
        if (!partsColumn)
        {
            addPartsColumn();
        }

        partsColumn->setDrumParts(currentDrumParts, midiFile);
        updateColumnsLayout();

        DBG("MIDI file dissected: " + midiFile.getFileName() +
        " -> " + juce::String(currentDrumParts.size()) + " parts (Source: " +
        juce::String(static_cast<int>(sourceLib)) + ", Target: " +
        juce::String(static_cast<int>(targetLib)) + ")");
    }
    else
    {
        removePartsColumn();
        DBG("No drum parts found in: " + midiFile.getFileName());
    }

    // FIX: Notify main component about file selection to update path display
    handleFileSelection(midiFile);
}

void GrooveBrowser::handleDrumPartSelection(const DrumPart& part)
{
    DBG("Drum part selected: " + part.displayName +
    " (" + juce::String(part.eventCount) + " events)");
}

void GrooveBrowser::handleDrumPartDoubleClick(const DrumPart& part)
{
    DBG("Playing drum part: " + part.displayName);
    // The DrumPartsColumn handles the actual playback
}

juce::File GrooveBrowser::getCurrentFileForRow(int columnIndex, int row)
{
    if (columnIndex >= 0 && columnIndex < folderColumns.size())
    {
        BrowserColumn* column = folderColumns[columnIndex];
        if (row >= 0 && row < column->itemFiles.size() && !column->itemIsFolder[row])
        {
            return column->itemFiles[row];
        }
    }
    return juce::File();
}

bool GrooveBrowser::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return keyPressed(key);
}

bool GrooveBrowser::keyPressed(const juce::KeyPress& key)
{
    // Only handle keyboard events if GrooveBrowser or its children have focus
    if (!hasKeyboardFocus(true))
        return false;

    // Handle space bar to stop/resume playback (only when GrooveBrowser has focus)
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (processor.midiProcessor.isPlaying())
        {
            // Stop playback
            processor.midiProcessor.pause();
            browserPlaybackActive = false;
            DBG("GrooveBrowser: Space bar pressed - Pausing playback");
        }
        else if (browserPlaybackActive && currentlyPlayingFile.existsAsFile())
        {
            // Resume playback
            processor.midiProcessor.play();
            DBG("GrooveBrowser: Space bar pressed - Resuming playback");
        }
        return true;  // Consume the event so it doesn't reach DAW
    }

    // Handle arrow keys for navigation
    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        navigateToNextMidiFile();
        return true;  // Consume the event
    }

    if (key.getKeyCode() == juce::KeyPress::upKey)
    {
        navigateToPreviousMidiFile();
        return true;  // Consume the event
    }

    return false;
}

void GrooveBrowser::timerCallback()
{
    // Update hover states
    for (auto* column : folderColumns)
    {
        column->repaint();
    }

    if (partsColumn)
    {
        partsColumn->repaint();
    }

    // Check if BPM changed during playback
    if (processor.midiProcessor.isPlaying())
    {
        double currentBPM = 120.0;
        bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;

        if (syncToHost)
        {
            currentBPM = processor.getHostBPM();
        }
        else
        {
            currentBPM = processor.parameters.getRawParameterValue("manualBPM")->load();
        }

        // If BPM changed, update track 0 (browser playback uses track 0)
        if (std::abs(currentBPM - lastKnownBPM) > 0.01)
        {
            processor.midiProcessor.updateTrackBPM(0, currentBPM);
            lastKnownBPM = currentBPM;
            DBG("GrooveBrowser: BPM changed to " + juce::String(currentBPM, 2) + " BPM");
        }
    }
}

void GrooveBrowser::restoreNavigationState(const juce::File& folder, const juce::Array<juce::File>& path)
{
    currentPath = folder;
    navigationPath = path;

    if (folder.exists())
    {
        loadFolderContents(folder);
    }
}

void GrooveBrowser::addFolderColumn(const juce::String& title, bool isFileColumn)
{
    auto* column = new BrowserColumn(title, processor);

    column->onSelectionChange = [this, column]() {
        int columnIndex = folderColumns.indexOf(column);
        handleColumnSelection(columnIndex);
    };

    column->onDoubleClick = [this, column](int row) {
        int columnIndex = folderColumns.indexOf(column);
        handleColumnDoubleClick(columnIndex, row);
    };

    // CRITICAL FIX: Set up right-click callback for folders
    column->onRightClickFolder = [this](const juce::File& folder) {
        // The showContextMenu in BrowserColumn already handles this
        // This callback is just to enable the functionality
    };

    column->setSize(isFileColumn ? FILE_COLUMN_WIDTH : FOLDER_COLUMN_WIDTH, COLUMN_HEIGHT_MIN);

    folderColumns.add(column);
    columnsContainer.addAndMakeVisible(column);

    updateColumnsLayout();
}

void GrooveBrowser::removeFolderColumnsAfter(int index)
{
    while (folderColumns.size() > index + 1)
    {
        folderColumns.removeLast();
    }

    // Also remove from navigation path
    while (navigationPath.size() > index + 1)
    {
        navigationPath.removeLast();
    }

    // Remove parts column when navigating away
    removePartsColumn();

    updateColumnsLayout();
}

void GrooveBrowser::addPartsColumn()
{
    if (partsColumn)
        return; // Already exists

        partsColumn = std::make_unique<DrumPartsColumn>(processor, "Drum Parts");

    // Set up callbacks
    partsColumn->onPartSelected = [this](const DrumPart& part) {
        handleDrumPartSelection(part);
    };

    partsColumn->onPartDoubleClicked = [this](const DrumPart& part) {
        handleDrumPartDoubleClick(part);
    };

    columnsContainer.addAndMakeVisible(partsColumn.get());

    updateColumnsLayout();  // ADD THIS LINE
}

void GrooveBrowser::removePartsColumn()
{
    if (partsColumn)
    {
        columnsContainer.removeChildComponent(partsColumn.get());
        partsColumn.reset();
        currentMidiFile = juce::File();
        currentDrumParts.clear();
        currentSourceLibrary = DrumLibrary::Unknown; // Reset source library tracking
        updateColumnsLayout();
    }
}

void GrooveBrowser::updateColumnsLayout()
{
    int totalWidth = 0;
    int currentX = 0;

    // Layout folder columns
    for (auto* column : folderColumns)
    {
        int width = (column == folderColumns.getLast() && !partsColumn) ? FILE_COLUMN_WIDTH : FOLDER_COLUMN_WIDTH;
        column->setBounds(currentX, 0, width, getHeight() - 35); // Account for top bar
        currentX += width;
        totalWidth += width;
    }

    // Layout parts column if it exists
    if (partsColumn)
    {
        partsColumn->setBounds(currentX, 0, PARTS_COLUMN_WIDTH, getHeight() - 35);
        totalWidth += PARTS_COLUMN_WIDTH;
    }

    columnsContainer.setBounds(0, 0, totalWidth, getHeight() - 35);
}

void GrooveBrowser::scanFolder(const juce::File& folder, BrowserColumn* column)
{
    juce::StringArray items;
    juce::Array<bool> isFolder;
    juce::Array<juce::File> filePaths;

    // Get subdirectories
    auto subdirs = folder.findChildFiles(juce::File::findDirectories, false);
    subdirs.sort();

    for (const auto& dir : subdirs)
    {
        items.add(dir.getFileName());
        isFolder.add(true);
        filePaths.add(dir);  // Add directory path
    }

    // Get MIDI files - NO BPM formatting, just plain filename
    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi");
    files.sort();

    for (const auto& file : files)
    {
        items.add(file.getFileNameWithoutExtension()); // Just the filename without extension, no BPM info
        isFolder.add(false);
        filePaths.add(file);  // Add actual file path
    }

    column->setItems(items, isFolder, filePaths);
}

void GrooveBrowser::navigateToFolder(const juce::File& folder, int columnIndex)
{
    // Remove columns after this index
    removeFolderColumnsAfter(columnIndex - 1);

    // Add new column for this folder
    addFolderColumn(folder.getFileName(), false);

    // Scan and populate the column
    BrowserColumn* newColumn = folderColumns.getLast();
    scanFolder(folder, newColumn);

    // Update navigation path
    while (navigationPath.size() > columnIndex)
        navigationPath.removeLast();

    if (columnIndex < navigationPath.size())
        navigationPath.set(columnIndex, folder);
    else
        navigationPath.add(folder);

    updateColumnsLayout();
}

void GrooveBrowser::handleColumnSelection(int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= folderColumns.size())
        return;

    auto* selectedColumn = folderColumns[columnIndex];
    juce::String selectedItem = selectedColumn->getSelectedItem();
    bool isFolder = selectedColumn->isSelectedItemFolder();
    juce::File selectedFile = selectedColumn->getSelectedFile();

    if (isFolder)
    {
        // Navigate into folder - create NEXT column (columnIndex + 1)
        navigateToFolder(selectedFile, columnIndex + 1);
    }
    else if (selectedFile.existsAsFile() && selectedFile.hasFileExtension(".mid"))
    {
        // MIDI file selected - dissect it
        currentMidiFile = selectedFile;

        // FIX: Call handleFileSelection FIRST before dissection
        // This ensures the file path display updates immediately
        handleFileSelection(selectedFile);

        // Determine source library from root folder
        DrumLibrary sourceLib = DrumLibrary::Unknown;
        for (int i = 0; i < processor.drumLibraryManager.getNumRootFolders(); ++i)
        {
            auto rootFolder = processor.drumLibraryManager.getRootFolder(i);
            if (selectedFile.getFullPathName().startsWith(rootFolder.getFullPathName()))
            {
                sourceLib = processor.drumLibraryManager.getRootFolderSourceLibrary(i);
                currentSourceLibrary = sourceLib;
                break;
            }
        }

        DrumLibrary targetLib = getCurrentTargetLibrary();

        // Use dissectMidiFileWithLibraryManager
        currentDrumParts = midiDissector.dissectMidiFileWithLibraryManager(currentMidiFile, sourceLib, targetLib, processor.drumLibraryManager);

        if (!currentDrumParts.isEmpty())
        {
            // Add or update parts column
            if (!partsColumn)
            {
                addPartsColumn();
            }

            partsColumn->setDrumParts(currentDrumParts, currentMidiFile);
            updateColumnsLayout();

            DBG("MIDI file dissected: " + currentMidiFile.getFileName() +
            " -> " + juce::String(currentDrumParts.size()) + " parts (Source: " +
            juce::String(static_cast<int>(sourceLib)) + ", Target: " +
            juce::String(static_cast<int>(targetLib)) + ")");
        }
        else
        {
            removePartsColumn();
            DBG("No drum parts found in: " + currentMidiFile.getFileName());
        }
    }
}

void GrooveBrowser::showFolderContextMenu(const juce::File& folder)
{
    // This function is now handled directly in BrowserColumn::showContextMenu
    // for better positioning at the mouse click location
}

juce::String GrooveBrowser::formatFileName(const juce::String& filename, bool isMidiFile) const
{
    // Just return the filename as-is (no BPM formatting)
    return filename;
}

int GrooveBrowser::extractBPMFromFilename(const juce::String& filename) const
{
    // Extract BPM from filename if present
    // This is probably used elsewhere in the original code
    return 120; // Default BPM
}



// ============================================================================
// REPLACE ENTIRE mouseDrag FUNCTION in GrooveBrowser.cpp
// BPM MATH CORRECTED: originalBPM / currentBPM (was backwards before)
// ============================================================================



void BrowserColumn::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) // Right-click
    {
        showContextMenu(row, e.getMouseDownPosition());
    }
}

void BrowserColumn::showContextMenu(int row, const juce::Point<int>& position)
{
    if (row < 0 || row >= items.size())
        return;

    // Get actual mouse screen position
    auto mousePos = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();

    // Handle folders - show folder context menu
    if (itemIsFolder[row])
    {
        juce::File folder = itemFiles[row];
        if (folder.exists() && folder.isDirectory() && onRightClickFolder)
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Add to Favorites");
            menu.addSeparator();
            menu.addItem(2, "Show in Explorer");

            // Show menu at the actual mouse position
            menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetScreenArea(juce::Rectangle<int>(static_cast<int>(mousePos.x),
                                                       static_cast<int>(mousePos.y), 1, 1)),
                               [this, folder](int result)
                               {
                                   if (result == 1) // Add to Favorites
                                   {
                                       // Show dialog to optionally rename
                                       juce::AlertWindow w("Add to Favorites",
                                                           "Enter a name for this favorite folder:",
                                                           juce::AlertWindow::NoIcon);
                                       w.addTextEditor("name", folder.getFileName());
                                       w.addButton("Add", 1, juce::KeyPress(juce::KeyPress::returnKey));
                                       w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                                       if (w.runModalLoop() == 1)
                                       {
                                           auto name = w.getTextEditorContents("name");
                                           processor.favoritesManager.addFavorite(folder, name);
                                       }
                                   }
                                   else if (result == 2) // Show in Explorer
                                   {
                                       folder.revealToUser();
                                   }
                               });
        }
        return;
    }

    // Handle MIDI files
    juce::File midiFile = itemFiles[row];
    if (!midiFile.existsAsFile())
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Export to Desktop...");
    menu.addSeparator();
    menu.addItem(2, "Show in Explorer");

    // Show menu at actual mouse position
    menu.showMenuAsync(juce::PopupMenu::Options()
    .withTargetScreenArea(juce::Rectangle<int>(static_cast<int>(mousePos.x),
                                               static_cast<int>(mousePos.y), 1, 1)),
                       [this, midiFile](int result)
                       {
                           if (result == 1)
                           {
                               exportFileToDesktop(midiFile);
                           }
                           else if (result == 2)
                           {
                               midiFile.revealToUser();
                           }
                       });
}

void BrowserColumn::exportFileToDesktop(const juce::File& originalMidiFile)
{
    if (!originalMidiFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Export Error", "File doesn't exist!", "OK");
        return;
    }

    // Get current plugin BPM (header BPM)
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    double currentBPM = syncToHost ? processor.getHostBPM()
    : processor.parameters.getRawParameterValue("manualBPM")->load();

    // Read original MIDI file
    juce::MidiFile originalMidi;
    juce::FileInputStream inputStream(originalMidiFile);
    if (!inputStream.openedOk() || !originalMidi.readFrom(inputStream))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Export Error", "Failed to read MIDI file!", "OK");
        return;
    }

    // Get original BPM from MIDI file
    double originalBPM = 120.0;
    for (int t = 0; t < originalMidi.getNumTracks(); ++t)
    {
        const auto* track = originalMidi.getTrack(t);
        if (track)
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto* event = track->getEventPointer(i);
                if (event && event->message.isTempoMetaEvent())
                {
                    originalBPM = 60.0 / event->message.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
            if (originalBPM != 120.0) break;
        }
    }

    // Get Desktop directory
    auto desktopDir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);

    // Create unique filename on Desktop
    juce::String baseName = originalMidiFile.getFileNameWithoutExtension();

    // Add BPM to filename for clarity
    baseName += "_" + juce::String(static_cast<int>(currentBPM)) + "bpm";

    juce::File exportFile = desktopDir.getChildFile(baseName + ".mid");

    // Ensure unique filename if file already exists
    int counter = 1;
    while (exportFile.existsAsFile())
    {
        exportFile = desktopDir.getChildFile(baseName + "_" + juce::String(counter) + ".mid");
        counter++;
    }

    // Check if BPM adjustment is needed
    bool needsAdjustment = std::abs(currentBPM - originalBPM) > 0.1;

    if (needsAdjustment)
    {

        // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CORRECTED: originalBPM / currentBPM (was backwards!)
        double timeStretchRatio = originalBPM / currentBPM;

        // Create adjusted MIDI file
        juce::MidiFile adjustedMidi;
        adjustedMidi.setTicksPerQuarterNote(originalMidi.getTimeFormat() > 0 ?
        originalMidi.getTimeFormat() : 480);

        for (int t = 0; t < originalMidi.getNumTracks(); ++t)
        {
            const auto* originalTrack = originalMidi.getTrack(t);
            if (!originalTrack) continue;

            juce::MidiMessageSequence newTrack;

            // Add tempo event to first track
            if (t == 0)
            {
                int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / currentBPM);
                newTrack.addEvent(juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote), 0.0);
            }

            // Process all events in the track
            for (int i = 0; i < originalTrack->getNumEvents(); ++i)
            {
                const auto* event = originalTrack->getEventPointer(i);
                if (!event) continue;

                auto message = event->message;

                // Skip tempo events (we set our own)
                if (message.isTempoMetaEvent())
                    continue;

                // Apply time stretch to event timestamp
                double originalTimestamp = message.getTimeStamp();
                double newTimestamp = originalTimestamp * timeStretchRatio;
                message.setTimeStamp(newTimestamp);

                newTrack.addEvent(message, message.getTimeStamp());
            }

            newTrack.updateMatchedPairs();
            adjustedMidi.addTrack(newTrack);
        }

        // Write adjusted MIDI to Desktop
        juce::FileOutputStream outputStream(exportFile);
        if (outputStream.openedOk())
        {
            adjustedMidi.writeTo(outputStream);
            outputStream.flush();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Error",
                "Could not write MIDI file to Desktop.\nPlease check permissions.",
                "OK");
            return;
        }
    }
    else
    {
        // No BPM adjustment needed - just copy the file

        bool success = originalMidiFile.copyFileTo(exportFile);

        if (!success)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Error",
                "Could not export MIDI file to Desktop.\nPlease check permissions.",
                "OK");
            return;
        }
    }

    // Verify export was successful
    if (exportFile.existsAsFile() && exportFile.getSize() > 0)
    {

        // Show success message with BPM info
        juce::String message = "MIDI file exported to Desktop";
        if (needsAdjustment)
        {
            message += "\n\nBPM adjusted: " + juce::String(originalBPM, 1) + " ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ " + juce::String(currentBPM, 1);
        }
        message += "\n\nFile: " + exportFile.getFileName();

        // Show success message with option to reveal file
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
            .withIconType(juce::AlertWindow::InfoIcon)
            .withTitle("Export Successful")
            .withMessage(message)
            .withButton("OK")
            .withButton("Show in Explorer"),
                                     [exportFile](int result)
                                     {
                                         if (result == 2) // "Show in Explorer" button
                                         {
                                             exportFile.revealToUser();
                                         }
                                     });
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Error",
            "Export failed - file is empty or could not be created.",
            "OK");
    }
}

// NEW: Helper to get the active file column (rightmost column with MIDI files)
BrowserColumn* GrooveBrowser::getActiveFileColumn()
{
    // Find the rightmost column that contains MIDI files (not folders)
    for (int i = folderColumns.size() - 1; i >= 0; --i)
    {
        auto* column = folderColumns[i];
        // Check if this column has any MIDI files
        for (int j = 0; j < column->itemFiles.size(); ++j)
        {
            if (!column->itemIsFolder[j] && column->itemFiles[j].hasFileExtension(".mid;.midi"))
            {
                return column;  // Found a column with MIDI files
            }
        }
    }
    return nullptr;
}

// NEW: Helper to play a file at a specific row in a column
void GrooveBrowser::playFileAtRow(BrowserColumn* column, int row)
{
    if (!column || row < 0 || row >= column->itemFiles.size())
        return;

    if (column->itemIsFolder[row])
        return;  // Don't play folders

        juce::File file = column->itemFiles[row];
    if (file.existsAsFile() && file.hasFileExtension(".mid;.midi"))
    {
        // Select the row
        column->selectRow(row);

        // Play the file
        handleFileDoubleClick(file);

        DBG("GrooveBrowser: Arrow key navigation - Playing: " + file.getFileName());
    }
}

// NEW: Navigate to next MIDI file using down arrow
void GrooveBrowser::navigateToNextMidiFile()
{
    auto* activeColumn = getActiveFileColumn();
    if (!activeColumn)
        return;

    int currentRow = activeColumn->getSelectedRow();
    int numRows = activeColumn->getNumRows();

    // Find next MIDI file (skip folders)
    for (int i = currentRow + 1; i < numRows; ++i)
    {
        if (!activeColumn->itemIsFolder[i] &&
            activeColumn->itemFiles[i].hasFileExtension(".mid;.midi"))
        {
            playFileAtRow(activeColumn, i);
            return;
        }
    }

    // If we reached the end, wrap to the first MIDI file
    for (int i = 0; i <= currentRow; ++i)
    {
        if (!activeColumn->itemIsFolder[i] &&
            activeColumn->itemFiles[i].hasFileExtension(".mid;.midi"))
        {
            playFileAtRow(activeColumn, i);
            return;
        }
    }
}

// NEW: Navigate to previous MIDI file using up arrow
void GrooveBrowser::navigateToPreviousMidiFile()
{
    auto* activeColumn = getActiveFileColumn();
    if (!activeColumn)
        return;

    int currentRow = activeColumn->getSelectedRow();

    // Find previous MIDI file (skip folders)
    for (int i = currentRow - 1; i >= 0; --i)
    {
        if (!activeColumn->itemIsFolder[i] &&
            activeColumn->itemFiles[i].hasFileExtension(".mid;.midi"))
        {
            playFileAtRow(activeColumn, i);
            return;
        }
    }

    // If we reached the beginning, wrap to the last MIDI file
    int numRows = activeColumn->getNumRows();
    for (int i = numRows - 1; i >= currentRow; --i)
    {
        if (!activeColumn->itemIsFolder[i] &&
            activeColumn->itemFiles[i].hasFileExtension(".mid;.midi"))
        {
            playFileAtRow(activeColumn, i);
            return;
        }
    }
}

void GrooveBrowser::mouseDown(const juce::MouseEvent& e)
{
    // Grab keyboard focus when user clicks in the GrooveBrowser
    grabKeyboardFocus();
    DBG("GrooveBrowser: Grabbed keyboard focus");
}
