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
    setRowHeight(50);  // Restore original size

    setColour(juce::ListBox::backgroundColourId, ColourPalette::mainBackground.withAlpha(0.8f));
    setColour(juce::ListBox::outlineColourId, ColourPalette::borderColour);
    setMultipleSelectionEnabled(false);

    // Configure scrollbar - make it always visible and prominent
    auto& scrollbar = getVerticalScrollBar();
    scrollbar.setAutoHide(false);
    scrollbar.setColour(juce::ScrollBar::thumbColourId, juce::Colours::grey);
    scrollbar.setColour(juce::ScrollBar::trackColourId, juce::Colours::darkgrey);
    scrollbar.setColour(juce::ScrollBar::backgroundColourId, ColourPalette::secondaryBackground);

    startTimerHz(10);
}

DrumPartsColumn::~DrumPartsColumn()
{
    if (lastTempFile.existsAsFile())
    {
        lastTempFile.deleteFile();
    }
    stopTimer();
}

void DrumPartsColumn::resized()
{
    // Call base class resized first
    juce::ListBox::resized();

    int totalContentHeight = getNumRows() * getRowHeight();
    int availableHeight = getHeight();

    DBG("DrumPartsColumn::resized - Rows: " + juce::String(getNumRows()) +
    ", RowHeight: " + juce::String(getRowHeight()) +
    ", TotalContent: " + juce::String(totalContentHeight) +
    ", Available: " + juce::String(availableHeight));

    // Update content to ensure proper scrollbar calculation
    updateContent();
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

        // Draw playback progress indicator
        if (isPreviewPlaying && playbackProgress > 0.0f)
        {
            float progressWidth = bounds.getWidth() * playbackProgress;
            g.setColour(part.colour.withAlpha(0.6f).darker(0.4f)); // Darker version
            g.fillRect(0.0f, 0.0f, progressWidth, static_cast<float>(bounds.getHeight()));
        }

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

    // Show remapped notes only if there are actual remappings
    if (!part.remappedNotes.isEmpty())
    {
        mappingText += " -> ";

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
        // CRITICAL: Select this row before playing so progress shows correctly
        selectRow(row);
        selectedRow = row;

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
            juce::String dragDescription = part.name + "|PART|" +
            originalMidiFile.getFullPathName() + "|" +
            juce::String(static_cast<int>(sourceLibrary));
            return juce::var(dragDescription);
        }
    }
    return {};
}

void DrumPartsColumn::setDrumParts(const juce::Array<DrumPart>& parts, const juce::File& sourceFile,
                                   DrumLibrary srcLib)
{
    drumParts = parts;
    originalMidiFile = sourceFile;
    sourceLibrary = srcLib;
    selectedRow = -1;

    deselectAllRows();
    updateContent();
    resized();
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

        // ====================================================================
        // FIX: Get current BPM from header (respects Sync to Host setting)
        // ====================================================================
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

        // ====================================================================
        // FIX: Calculate actual duration from the MIDI file
        // ====================================================================
        double actualDuration = 1.0; // Default fallback for drum parts

        juce::MidiFile midiFile;
        juce::FileInputStream stream(tempFile);
        if (midiFile.readFrom(stream))
        {
            double ticksPerQuarterNote = midiFile.getTimeFormat();
            if (ticksPerQuarterNote <= 0)
                ticksPerQuarterNote = 480.0;

            // Read BPM and time signature from file
            double fileBPM = 120.0;
            int timeSignatureNumerator = 4;
            int timeSignatureDenominator = 4;

            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                auto* track = const_cast<juce::MidiMessageSequence*>(midiFile.getTrack(t));
                if (!track) continue;

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
                        event->message.getTimeSignatureInfo(timeSignatureNumerator,
                                                            timeSignatureDenominator);
                    }
                }
            }

            // Find max time across all tracks
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

                    // Check note-off times
                    auto* eventHolder = track->getEventPointer(e);
                    if (eventHolder && eventHolder->noteOffObject != nullptr)
                    {
                        double noteOffTime = eventHolder->noteOffObject->message.getTimeStamp();
                        maxTimeInTicks = juce::jmax(maxTimeInTicks, noteOffTime);
                    }
                }
            }

            // Round up to complete bars
            double ticksPerBar = ticksPerQuarterNote * (4.0 / timeSignatureDenominator) * timeSignatureNumerator;
            double numBars = std::ceil(maxTimeInTicks / ticksPerBar);
            double roundedTicks = numBars * ticksPerBar;

            // CRITICAL: Calculate duration at CURRENT BPM (from header), not file BPM
            currentPartReferenceBPM = currentBPM;  // Store as reference
            actualDuration = (roundedTicks / ticksPerQuarterNote) * (60.0 / currentPartReferenceBPM);
            actualDuration = juce::jmax(0.1, actualDuration);

            // Store for loop recalculation
            currentPlaybackMidiTicks = roundedTicks;
            currentPlaybackTPQN = ticksPerQuarterNote;

            actualDuration = juce::jmax(0.5, actualDuration); // Minimum 0.5 seconds

            DBG("Part duration calculated: " + juce::String(actualDuration, 3) +
            "s at " + juce::String(currentBPM, 2) + " BPM" +
            " (ticks: " + juce::String(roundedTicks, 0) +
            ", PPQN: " + juce::String(ticksPerQuarterNote, 0) + ")");
        }

        // Add clip with CALCULATED duration
        DrumLibrary targetLib = processor.getTargetLibrary();
        processor.midiProcessor.addMidiClip(tempFile, 0.0, targetLib, currentPartReferenceBPM, currentBPM, 0,
                                            actualDuration,
                                            "drumpart_preview_" + juce::Uuid().toString());

        // Enable looping for preview
        processor.midiProcessor.setLoopRange(0.0, actualDuration);
        processor.midiProcessor.setLoopEnabled(true);

        processor.midiProcessor.setPlayheadPosition(0.0);


        // Start BPM monitoring timer
        startTimerHz(10);

        processor.midiProcessor.play();

        DBG("Playing drum part: " + part.displayName + " with " +
        juce::String(part.eventCount) + " events at " + juce::String(currentBPM, 2) +
        " BPM, duration: " + juce::String(actualDuration, 3) + "s");
    }
    // After starting playback:
    isPreviewPlaying = true;
    currentPreviewDuration = actualDuration;
    currentPreviewBPM = currentBPM;
    playbackProgress = 0.0f;
    repaint();

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

    // CRITICAL: Get original file's TPQN and BPM for proper tick scaling
    int originalTPQN = 480;  // Default
    double originalBPM = 120.0;
    juce::FileInputStream inputStream(originalMidiFile);
    if (inputStream.openedOk())
    {
        juce::MidiFile originalMidi;
        if (originalMidi.readFrom(inputStream))
        {
            // Get TPQN
            originalTPQN = originalMidi.getTimeFormat();
            DBG("Preview: Original MIDI file TPQN: " + juce::String(originalTPQN));

            if (auto* firstTrack = originalMidi.getTrack(0))
            {
                for (int i = 0; i < firstTrack->getNumEvents(); ++i)
                {
                    const auto& event = firstTrack->getEventPointer(i)->message;
                    if (event.isTempoMetaEvent())
                    {
                        originalBPM = 60000000.0 / event.getTempoMetaEventTickLength(originalTPQN);
                        break;
                    }
                }
            }
        }
    }

    DBG("Preview: Original BPM: " + juce::String(originalBPM, 2) + ", Current BPM: " + juce::String(currentBPM, 2));

    juce::MidiFile midiFile;
    int newTPQN = 480;
    midiFile.setTicksPerQuarterNote(newTPQN);

    juce::MidiMessageSequence track;

    // CRITICAL FIX: Apply both BPM scaling AND TPQN scaling
    // BPM scaling: adjusts playback speed to match current BPM
    // TPQN scaling: converts tick resolution from original to new file
    double bpmScale = originalBPM / currentBPM;
    double tpqnScale = static_cast<double>(newTPQN) / static_cast<double>(originalTPQN);
    double combinedScale = bpmScale * tpqnScale;

    DBG("Preview: BPM scale=" + juce::String(bpmScale, 4) +
    ", TPQN scale=" + juce::String(tpqnScale, 4) +
    ", Combined=" + juce::String(combinedScale, 4));

    for (int i = 0; i < part.sequence.getNumEvents(); ++i)
    {
        const auto* event = part.sequence.getEventPointer(i);
        if (event)
        {
            auto newEvent = event->message;
            // Apply combined scaling: BPM adjustment AND TPQN conversion
            newEvent.setTimeStamp(event->message.getTimeStamp() * combinedScale);
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
    isPreviewPlaying = false;
    currentPreviewDuration = 0.0;
    currentPreviewBPM = 120.0;
    playbackProgress = 0.0f;
    stopTimer();
}

void DrumPartsColumn::updatePreviewForBPMChange()
{
    if (!isPreviewPlaying)
        return;

    // Only update if we have valid MIDI timing data
    if (currentPlaybackMidiTicks <= 0.0 || currentPlaybackTPQN <= 0.0)
        return;

    // Get NEW target BPM
    bool syncToHost = processor.parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    double newTargetBPM = syncToHost ? processor.getHostBPM() :
    processor.parameters.getRawParameterValue("manualBPM")->load();

    if (std::abs(newTargetBPM - currentPreviewBPM) < 0.01)
        return;

    // CRITICAL FIX: Calculate ACTUAL playback duration at NEW target BPM
    double actualPlaybackDuration = (currentPlaybackMidiTicks / currentPlaybackTPQN) * (60.0 / newTargetBPM);
    actualPlaybackDuration = juce::jmax(0.1, actualPlaybackDuration);

    // Update MidiProcessor clip to play at new target BPM
    processor.midiProcessor.updateTrackBPM(0, newTargetBPM);

    // Update loop range to actual playback duration
    processor.midiProcessor.setLoopRange(0.0, actualPlaybackDuration);

    currentPreviewBPM = newTargetBPM;
    currentPreviewDuration = actualPlaybackDuration;

    DBG("Preview BPM changed - Target: " + juce::String(newTargetBPM, 2) +
    ", Actual duration: " + juce::String(actualPlaybackDuration, 3) + "s");
}


// Handle right-click detection
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

        // âœ… CORRECTED: originalBPM / currentBPM (was backwards!)
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
            message += "\n\nBPM adjusted: " + juce::String(originalBPM, 1) + " -> " + juce::String(currentBPM, 1);
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

void DrumPartsColumn::timerCallback()
{
    // Update playback progress for visual indicator
    if (isPreviewPlaying)
    {
        double position = processor.midiProcessor.getPlayheadPosition();
        double duration = currentPreviewDuration;

        if (duration > 0.0)
        {
            playbackProgress = static_cast<float>(position / duration);
            playbackProgress = juce::jlimit(0.0f, 1.0f, playbackProgress);
            repaint();
        }
    }

    // Monitor BPM changes during playback
    if (isPreviewPlaying && processor.midiProcessor.isPlaying())
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

        // If BPM changed significantly
        if (std::abs(currentBPM - lastKnownBPM) > 0.01)
        {
            processor.midiProcessor.updateTrackBPM(0, currentBPM);
            lastKnownBPM = currentBPM;

            // Update loop duration
            updateLoopDurationForBPMChange();

            DBG("DrumParts: BPM changed to " + juce::String(currentBPM, 2) + " BPM");
        }
    }
}

void DrumPartsColumn::updateLoopDurationForBPMChange()
{
    if (!isPreviewPlaying || currentPlaybackMidiTicks == 0.0)
        return;

    // Get current BPM
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

    // Recalculate duration using stored MIDI ticks and current BPM
    double newDurationInSeconds = (currentPlaybackMidiTicks / currentPlaybackTPQN) * (60.0 / headerBPM);
    newDurationInSeconds = juce::jmax(0.1, newDurationInSeconds);

    // Update loop range with new duration
    processor.midiProcessor.setLoopRange(0.0, newDurationInSeconds);

    DBG("DrumParts: Loop duration updated: " + juce::String(newDurationInSeconds, 3) + "s at " + juce::String(headerBPM, 2) + " BPM");
}
