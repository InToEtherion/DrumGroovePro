#include "DrumPartsColumn.h"
#include "../../PluginProcessor.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"

//==============================================================================
// DrumPartDragOverlay Implementation
//==============================================================================

void DrumPartDragOverlay::mouseDown(const juce::MouseEvent& e)
{
    isDragging = false;
    
    // Only handle CTRL clicks, pass everything else through
    if (!e.mods.isCtrlDown())
    {
        // Pass the event to the ListBox below
        e.eventComponent->getParentComponent()->mouseDown(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
    }
}

void DrumPartDragOverlay::mouseDrag(const juce::MouseEvent& e)
{
    
    if (e.mods.isCtrlDown())
    {
        // CTRL+Drag = External drag to DAW
        if (!isDragging && e.getDistanceFromDragStart() > 5)
        {
            isDragging = true;
            
            if (parentColumn)
            {
                parentColumn->startExternalDrag(row);
            }
            else
            {

            }
        }
    }
    else
    {
        // Pass through to ListBox for normal internal drag
        e.eventComponent->getParentComponent()->mouseDrag(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
    }
}

void DrumPartDragOverlay::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mods.isCtrlDown())
    {
        // Pass through to ListBox
        e.eventComponent->getParentComponent()->mouseUp(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
    }
    
    isDragging = false;
}

void DrumPartDragOverlay::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Forward double-click to ListBox
    e.eventComponent->getParentComponent()->mouseDoubleClick(e.getEventRelativeTo(e.eventComponent->getParentComponent()));
}

//==============================================================================
// DrumPartsColumn Implementation
//==============================================================================

DrumPartsColumn::DrumPartsColumn(DrumGrooveProcessor& p, const juce::String& columnName)
    : processor(p), columnTitle(columnName), selectedRow(-1)
{
    setOpaque(false);  
    setModel(this);
    setRowHeight(50);
    
    setColour(juce::ListBox::backgroundColourId, ColourPalette::mainBackground.withAlpha(0.8f));
    setMultipleSelectionEnabled(false);
}

DrumPartsColumn::~DrumPartsColumn()
{
    if (lastTempFile.existsAsFile())
    {
        lastTempFile.deleteFile();
    }
}

int DrumPartsColumn::getNumRows()
{
    return drumParts.size();
}

void DrumPartsColumn::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                       int width, int height, bool rowIsSelected)
{
    if (rowNumber >= drumParts.size())
        return;

    const auto& part = drumParts[rowNumber];
    auto bounds = juce::Rectangle<int>(0, 0, width, height);

    drawPartItem(g, part, bounds, rowIsSelected, rowNumber);
}

juce::Component* DrumPartsColumn::refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    // Create or reuse overlay component for each row
    auto* overlay = dynamic_cast<DrumPartDragOverlay*>(existingComponentToUpdate);
    
    if (overlay == nullptr)
    {
        overlay = new DrumPartDragOverlay(this);
    }
    
    overlay->setRow(rowNumber);
    
    return overlay;
}

void DrumPartsColumn::drawPartItem(juce::Graphics& g, const DrumPart& part,
                                   juce::Rectangle<int> bounds, bool isSelected, int rowNumber)
{
    // Background with 0.8f transparency for MIDI dissection
    if (isSelected)
    {
        g.fillAll(part.colour.withAlpha(0.3f));
        g.setColour(ColourPalette::primaryText);
    }
    else
    {
        // Use 0.8f alpha for non-selected items
        g.fillAll(ColourPalette::mainBackground.withAlpha(0.8f));
        g.setColour(ColourPalette::secondaryText);

        if (isMouseOver())
        {
            auto mousePos = getMouseXYRelative();
            auto itemBounds = getRowPosition(rowNumber, true);
            if (itemBounds.contains(mousePos))
            {
                // Hover state with 0.8f alpha
                g.fillAll(ColourPalette::secondaryBackground.withAlpha(0.8f));
                g.setColour(ColourPalette::primaryText);
            }
        }
    }

    // Color indicator bar on left
    auto colorBar = bounds.removeFromLeft(4);
    g.setColour(part.colour);
    g.fillRect(colorBar);

    bounds.removeFromLeft(8);

    // Part name area (top section)
    auto nameArea = bounds.removeFromTop(25);
    auto nameSection = nameArea.removeFromLeft(120);

    g.setColour(isSelected ? ColourPalette::primaryText : ColourPalette::secondaryText);
    g.setFont(14.0f);
    g.drawText(part.displayName, nameSection, juce::Justification::centredLeft, true);

    // Event count
    auto eventArea = nameArea.removeFromRight(80);
    g.setFont(11.0f);
    g.setColour(ColourPalette::secondaryText);
    g.drawText(juce::String(part.eventCount) + " events", eventArea, juce::Justification::centredRight, true);

    // Note mapping info (bottom section)
    auto mappingArea = bounds.removeFromTop(20);
    drawNoteMapping(g, part, mappingArea);

    // Mini dot pattern preview (remaining space)
    if (bounds.getHeight() > 0)
    {
        drawDrumPatternDots(g, part, bounds);
    }
}

void DrumPartsColumn::drawNoteMapping(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds)
{
    if (part.originalNotes.isEmpty())
        return;

    g.setFont(10.0f);
    g.setColour(ColourPalette::secondaryText.withAlpha(0.8f));

    juce::String mappingText;
    
    // Show original notes
    if (part.originalNotes.size() <= 3)
    {
        juce::StringArray noteStrings;
        for (auto note : part.originalNotes)
        {
            noteStrings.add(juce::String(note));
        }
        mappingText += "Orig: " + noteStrings.joinIntoString(",");
    }
    else
    {
        mappingText += "Orig: " + juce::String(part.originalNotes[0]) + "..." + 
                       juce::String(part.originalNotes.size()) + " notes";
    }

    // Show remapped notes if different
    if (!part.remappedNotes.isEmpty() && part.remappedNotes != part.originalNotes)
    {
        mappingText += " → ";
        
        if (part.remappedNotes.size() <= 3)
        {
            juce::StringArray noteStrings;
            for (auto note : part.remappedNotes)
            {
                noteStrings.add(juce::String(note));
            }
            mappingText += "Target: " + noteStrings.joinIntoString(",");
        }
        else
        {
            mappingText += "Target: " + juce::String(part.remappedNotes[0]) + "..." + 
                           juce::String(part.remappedNotes.size()) + " notes";
        }
        
        // Use a slightly different color when notes are remapped
        g.setColour(ColourPalette::warningOrange.withAlpha(0.7f));
    }

    // Draw the mapping text
    g.drawText(mappingText, bounds.reduced(2), juce::Justification::centredLeft, true);
}

void DrumPartsColumn::drawDrumPatternDots(juce::Graphics& g, const DrumPart& part, juce::Rectangle<int> bounds)
{
    if (part.sequence.getNumEvents() == 0)
        return;

    const int numDots = 16; // 16th notes
    const float dotSize = 3.0f;
    const float spacing = static_cast<float>(bounds.getWidth() - 10) / static_cast<float>(numDots - 1);

    // Calculate the total duration for quantization
    double totalDuration = part.duration;
    if (totalDuration <= 0.0)
        totalDuration = 4.0; // Default to 4 beats

    // Create array to track which dots should be lit
    juce::Array<bool> dotLit;
    dotLit.resize(numDots);
    dotLit.fill(false);

    // Process events to determine which dots to light up
    for (int i = 0; i < part.sequence.getNumEvents(); ++i)
    {
        const auto* event = part.sequence.getEventPointer(i);
        if (event->message.isNoteOn() && event->message.getVelocity() > 0)
        {
            double eventTime = event->message.getTimeStamp();
            double normalizedTime = eventTime / totalDuration;
            
            // Quantize to 16th notes
            int dotIndex = juce::jlimit(0, numDots - 1, 
                                       static_cast<int>(normalizedTime * numDots));
            dotLit.set(dotIndex, true);
        }
    }

    // Draw the dots
    float yCenter = bounds.getY() + bounds.getHeight() * 0.5f;
    
    for (int i = 0; i < numDots; ++i)
    {
        float x = bounds.getX() + 5 + i * spacing;
        
        if (dotLit[i])
        {
            g.setColour(part.colour.brighter(0.3f));
            g.fillEllipse(x - dotSize * 0.5f, yCenter - dotSize * 0.5f, dotSize, dotSize);
        }
        else
        {
            g.setColour(ColourPalette::secondaryText.withAlpha(0.3f));
            g.drawEllipse(x - dotSize * 0.5f, yCenter - dotSize * 0.5f, dotSize, dotSize, 0.5f);
        }
    }
}

void DrumPartsColumn::selectedRowsChanged(int newRowSelected)
{
    selectedRow = newRowSelected;

    if (selectedRow >= 0 && selectedRow < drumParts.size())
    {
        const auto& selectedPart = drumParts[selectedRow];

        if (onPartSelected)
            onPartSelected(selectedPart);
    }
}

void DrumPartsColumn::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < drumParts.size())
    {
        const auto& part = drumParts[row];
        playPart(part);

        if (onPartDoubleClicked)
            onPartDoubleClicked(part);
    }
}

juce::var DrumPartsColumn::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.size() > 0)
    {
        int row = selectedRows[0];
        if (row >= 0 && row < drumParts.size())
        {
            const auto& part = drumParts[row];
            return juce::var(part.getDragDescription(originalMidiFile));
        }
    }
    return {};
}

void DrumPartsColumn::setDrumParts(const juce::Array<DrumPart>& parts, const juce::File& sourceFile)
{
    drumParts = parts;
    originalMidiFile = sourceFile;
    selectedRow = -1;
    
    deselectAllRows();
    
    updateContent();

    DBG("DrumPartsColumn: Set " + juce::String(parts.size()) + " drum parts for " + sourceFile.getFileName());
    
    // Debug: Log note mapping information
    for (int i = 0; i < parts.size(); ++i)
    {
        const auto& part = parts[i];
        DBG("  Part " + juce::String(i) + ": " + part.displayName + 
            " - Original notes: " + juce::String(part.originalNotes.size()) + 
            ", Remapped notes: " + juce::String(part.remappedNotes.size()));
    }
}

void DrumPartsColumn::clearParts()
{
    drumParts.clear();
    originalMidiFile = juce::File();
    selectedRow = -1;
    
    deselectAllRows();
    
    updateContent();
}

const DrumPart* DrumPartsColumn::getSelectedPart() const
{
    if (selectedRow >= 0 && selectedRow < drumParts.size())
        return &drumParts.getReference(selectedRow);
    return nullptr;
}

void DrumPartsColumn::playSelectedPart()
{
    if (auto* part = getSelectedPart())
    {
        playPart(*part);
    }
}

void DrumPartsColumn::playPart(const DrumPart& part)
{
    if (part.sequence.getNumEvents() == 0)
        return;

    processor.midiProcessor.stop();
    processor.midiProcessor.clearAllClips();

    // Create temporary MIDI file with UNIQUE name
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("DrumGroovePro_temp_part_" + 
                                      juce::String(juce::Random::getSystemRandom().nextInt()) + ".mid");

    createTempMidiFile(part, tempFile);

    if (tempFile.existsAsFile())
    {
        // Clean up previous temp file
        if (lastTempFile.existsAsFile() && lastTempFile != tempFile)
        {
            lastTempFile.deleteFile();
        }
        lastTempFile = tempFile;

        // Get current BPM settings
        double bpm = 120.0;
        bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
        
        if (syncToHost)
        {
            bpm = processor.getHostBPM();
        }
        else
        {
            bpm = processor.parameters.getRawParameterValue("manualBPM")->load();
        }

        DrumLibrary targetLib = processor.getTargetLibrary();
        processor.midiProcessor.addMidiClip(tempFile, 0.0, targetLib, bpm, bpm, 0, 10.0, "drumpart_preview_" + juce::Uuid().toString());  // Track 0
        processor.midiProcessor.setPlayheadPosition(0.0);
        processor.midiProcessor.play();

        DBG("Playing drum part: " + part.displayName + " with " + 
            juce::String(part.eventCount) + " events at " + juce::String(bpm, 2) + " BPM");
    }
}

void DrumPartsColumn::createTempMidiFile(const DrumPart& part, juce::File& tempFile)
{
    DBG("=== createTempMidiFile ===");
    DBG("Part: " + part.displayName + ", Events: " + juce::String(part.sequence.getNumEvents()));
    
    if (part.sequence.getNumEvents() == 0)
    {
        DBG("ERROR: Empty sequence!");
        return;
    }
    
    // Get current BPM from processor
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    double currentBPM = syncToHost ? processor.getHostBPM() : processor.parameters.getRawParameterValue("manualBPM")->load();
    
    // Get original file's BPM
    double originalBPM = 120.0;
    juce::FileInputStream inputStream(originalMidiFile);
    if (inputStream.openedOk())
    {
        juce::MidiFile originalMidi;
        if (originalMidi.readFrom(inputStream))
        {
            if (auto* firstTrack = originalMidi.getTrack(0))
            {
                for (int i = 0; i < firstTrack->getNumEvents(); ++i)
                {
                    const auto& event = firstTrack->getEventPointer(i)->message;
                    if (event.isTempoMetaEvent())
                    {
                        originalBPM = 60000000.0 / event.getTempoMetaEventTickLength(480);
                        break;
                    }
                }
            }
        }
    }
    
    DBG("Original BPM: " + juce::String(originalBPM, 2) + ", Current BPM: " + juce::String(currentBPM, 2));
    
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);

    juce::MidiMessageSequence track;
    
    // Apply tempo scaling
    double timeScale = originalBPM / currentBPM;
    
    for (int i = 0; i < part.sequence.getNumEvents(); ++i)
    {
        const auto* event = part.sequence.getEventPointer(i);
        if (event)
        {
            auto newEvent = event->message;
            newEvent.setTimeStamp(event->message.getTimeStamp() * timeScale);
            track.addEvent(newEvent);
        }
    }
    
    // Add tempo meta event at the beginning
    double microsecondsPerQuarterNote = 60000000.0 / currentBPM;
    auto tempoEvent = juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerQuarterNote));
    tempoEvent.setTimeStamp(0.0);
    track.addEvent(tempoEvent, 0.0);

    track.sort();
    track.updateMatchedPairs();
    midiFile.addTrack(track);

    // Write with scope to ensure stream closes
    {
        juce::FileOutputStream stream(tempFile);
        if (stream.openedOk())
        {
            midiFile.writeTo(stream);
            stream.flush();
        }
        else
        {
            DBG("ERROR: Stream failed!");
            return;
        }
    }
    
    // Verify
    if (tempFile.existsAsFile() && tempFile.getSize() > 0)
    {
        DBG("SUCCESS: File size " + juce::String(tempFile.getSize()) + " with BPM " + juce::String(currentBPM, 2));
    }
    else
    {
        DBG("ERROR: File not created!");
    }
}

void DrumPartsColumn::stopPlayback()
{
    processor.midiProcessor.stop();
}

// NEW: Handle right-click detection
void DrumPartsColumn::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) // Right-click
    {
        showContextMenu(row, e.getMouseDownPosition());
    }
}

// NEW: Show context menu for drum part export
void DrumPartsColumn::showContextMenu(int row, const juce::Point<int>& position)
{
    if (row < 0 || row >= drumParts.size())
        return;
    
    const auto& part = drumParts[row];
    
    juce::PopupMenu menu;
    menu.addItem(1, "Export to Desktop...");
    menu.addSeparator();
    menu.addItem(2, "Show Original File in Explorer");
    
    // Show menu at mouse position
    auto screenPos = localPointToGlobal(position);
    menu.showMenuAsync(juce::PopupMenu::Options()
                          .withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
        [this, part](int result)
        {
            if (result == 1) // Export to Desktop
            {
                exportPartToDesktop(part);
            }
            else if (result == 2) // Show in Explorer
            {
                if (originalMidiFile.existsAsFile())
                {
                    originalMidiFile.revealToUser();
                }
            }
        });
}

// NEW: Export drum part to Desktop
void DrumPartsColumn::exportPartToDesktop(const DrumPart& part)
{
    DBG("=== EXPORT DRUM PART TO DESKTOP WITH BPM ADJUSTMENT ===");
    DBG("Part: " + part.displayName);
    
    if (part.sequence.getNumEvents() == 0)
    {
        DBG("ERROR: Part has no MIDI events");
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Error",
            "This drum part contains no MIDI events.",
            "OK");
        return;
    }
    
    // Get current plugin BPM (header BPM)
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    double currentBPM = syncToHost ? processor.getHostBPM() 
                                   : processor.parameters.getRawParameterValue("manualBPM")->load();
    DBG("Plugin BPM: " + juce::String(currentBPM, 2));
    
    // Drum parts are typically at 120 BPM reference
    double originalBPM = 120.0;
    DBG("Part reference BPM: " + juce::String(originalBPM, 2));
    
    // Get Desktop directory
    auto desktopDir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    
    // Create a safe filename from the part name
    juce::String safeName = part.displayName;
    safeName = safeName.replaceCharacters("/\\:*?\"<>|", "_");
    
    // Add original file name context if available
    juce::String baseFileName = safeName;
    if (originalMidiFile.existsAsFile())
    {
        juce::String originalName = originalMidiFile.getFileNameWithoutExtension();
        baseFileName = originalName + "_" + safeName;
    }
    
    // Add BPM to filename for clarity
    baseFileName += "_" + juce::String(static_cast<int>(currentBPM)) + "bpm";
    
    // Ensure unique filename
    juce::File exportFile = desktopDir.getChildFile(baseFileName + ".mid");
    int counter = 1;
    while (exportFile.existsAsFile())
    {
        exportFile = desktopDir.getChildFile(baseFileName + "_" + juce::String(counter) + ".mid");
        counter++;
    }
    
    // Check if BPM adjustment is needed
    bool needsAdjustment = std::abs(currentBPM - originalBPM) > 0.1;
    
    // Create MIDI file
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);
    
    // Clone and adjust the sequence
    juce::MidiMessageSequence adjustedSequence;
    
    // Add tempo event
    int microsecondsPerQuarterNote = static_cast<int>(60000000.0 / currentBPM);
    adjustedSequence.addEvent(juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote), 0.0);
    
    if (needsAdjustment)
    {
        DBG("Applying BPM adjustment...");
        
        // ✅ CORRECTED: originalBPM / currentBPM (was backwards!)
        double timeStretchRatio = originalBPM / currentBPM;
        DBG("Time stretch ratio: " + juce::String(timeStretchRatio, 4));
        
        // Apply time stretch to all events
        for (int i = 0; i < part.sequence.getNumEvents(); ++i)
        {
            const auto* event = part.sequence.getEventPointer(i);
            if (!event) continue;
            
            auto message = event->message;
            
            // Skip tempo events
            if (message.isTempoMetaEvent())
                continue;
            
            // Apply time stretch to timestamp
            double originalTimestamp = message.getTimeStamp();
            double newTimestamp = originalTimestamp * timeStretchRatio;
            message.setTimeStamp(newTimestamp);
            
            adjustedSequence.addEvent(message, message.getTimeStamp());
        }
    }
    else
    {
        DBG("No BPM adjustment needed");
        
        // Copy events without time adjustment
        for (int i = 0; i < part.sequence.getNumEvents(); ++i)
        {
            const auto* event = part.sequence.getEventPointer(i);
            if (!event) continue;
            
            auto message = event->message;
            
            // Skip tempo events
            if (message.isTempoMetaEvent())
                continue;
            
            adjustedSequence.addEvent(message, message.getTimeStamp());
        }
    }
    
    adjustedSequence.updateMatchedPairs();
    midiFile.addTrack(adjustedSequence);
    
    // Write to file
    juce::FileOutputStream stream(exportFile);
    if (stream.openedOk())
    {
        midiFile.writeTo(stream);
        stream.flush();
        
        DBG("Successfully exported to: " + exportFile.getFullPathName());
        
        // Show success message with BPM info
        juce::String message = "Drum part exported to Desktop";
        if (needsAdjustment)
        {
            message += "\n\nBPM adjusted: " + juce::String(originalBPM, 1) + " → " + juce::String(currentBPM, 1);
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
        DBG("ERROR: Could not open file for writing");
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Error",
            "Could not write MIDI file to Desktop.\nPlease check permissions.",
            "OK");
    }
}

void DrumPartsColumn::startExternalDrag(int row)
{

    if (isExternalDragActive)
    {

        return;
    }
    
    isExternalDragActive = true;
    
    if (row < 0 || row >= drumParts.size())
    {
        isExternalDragActive = false;
        return;
    }
    
    const auto& part = drumParts[row];

    
    if (part.sequence.getNumEvents() == 0)
    {
        isExternalDragActive = false;
        return;
    }
    
    // Get drag container
    auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (!dragContainer)
    {
        isExternalDragActive = false;
        return;
    }
    
    
    // Create temp file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    // Create unique temp file with timestamp and random number
	juce::String uniqueName = "DrumGroovePro_drag_part_" + 
							juce::String(juce::Time::currentTimeMillis()) + "_" +
							juce::String(juce::Random::getSystemRandom().nextInt(10000));
	juce::File tempFile = tempDir.getChildFile(uniqueName + ".mid");
    
    
    // Get BPMs
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    double currentBPM = syncToHost ? processor.getHostBPM() : processor.parameters.getRawParameterValue("manualBPM")->load();
    
    double originalBPM = 120.0;
    juce::FileInputStream inputStream(originalMidiFile);
    if (inputStream.openedOk())
    {
        juce::MidiFile originalMidi;
        if (originalMidi.readFrom(inputStream))
        {
            if (auto* firstTrack = originalMidi.getTrack(0))
            {
                for (int i = 0; i < firstTrack->getNumEvents(); ++i)
                {
                    const auto& event = firstTrack->getEventPointer(i)->message;
                    if (event.isTempoMetaEvent())
                    {
                        originalBPM = 60000000.0 / event.getTempoMetaEventTickLength(480);
                        break;
                    }
                }
            }
        }
    }

    
    // Create MIDI file
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    juce::MidiMessageSequence track;
    
    double timeScale = originalBPM / currentBPM;
    
    for (int i = 0; i < part.sequence.getNumEvents(); ++i)
    {
        const auto* event = part.sequence.getEventPointer(i);
        if (event)
        {
            auto newEvent = event->message;
            newEvent.setTimeStamp(event->message.getTimeStamp() * timeScale);
            track.addEvent(newEvent);
        }
    }
    
    // Add tempo
    double microsecondsPerQuarterNote = 60000000.0 / currentBPM;
    auto tempoEvent = juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerQuarterNote));
    tempoEvent.setTimeStamp(0.0);
    track.addEvent(tempoEvent, 0.0);
    
    track.sort();
    track.updateMatchedPairs();
    midiFile.addTrack(track);

    
    // Write file
    {
        juce::FileOutputStream outputStream(tempFile);
        if (!outputStream.openedOk())
        {
            isExternalDragActive = false;
            return;
        }
        
        if (!midiFile.writeTo(outputStream))
        {
            isExternalDragActive = false;
            return;
        }
        
        outputStream.flush();
    }
    
    juce::Thread::sleep(50);
    
    if (!tempFile.existsAsFile())
    {
        isExternalDragActive = false;
        return;
    }
    
    juce::int64 fileSize = tempFile.getSize();
    if (fileSize == 0)
    {
        isExternalDragActive = false;
        return;
    }
    
    
    // Cleanup old file - try multiple times if locked
	if (lastTempFile.existsAsFile())
	{
		for (int i = 0; i < 3; ++i)
		{
			if (lastTempFile.deleteFile())
				break;
			juce::Thread::sleep(100);  // Wait if file is locked
		}
	}
	lastTempFile = tempFile;
    
    // Perform drag
    juce::StringArray files;
    files.add(tempFile.getFullPathName());
    
    
    dragContainer->performExternalDragDropOfFiles(files, true, this, [this, tempFile]()
	{
		isExternalDragActive = false;
		
		// Wait longer and try multiple times to delete
		juce::Timer::callAfterDelay(5000, [tempFile]()
		{
			if (tempFile.existsAsFile())
			{
				for (int i = 0; i < 5; ++i)
				{
					if (tempFile.deleteFile())
					{
						break;
					}
					juce::Thread::sleep(200);
				}
			}
		});
	});
    
}
