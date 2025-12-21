#include "MidiEditorComponent.h"
#include "PluginProcessor.h"

static juce::String getGridResolutionName(double resolution)
{
    if (std::abs(resolution - 1.0) < 0.01) return "1/4";
    if (std::abs(resolution - 0.5) < 0.01) return "1/8";
    if (std::abs(resolution - 0.25) < 0.01) return "1/16";
    if (std::abs(resolution - 0.125) < 0.01) return "1/32";
    if (std::abs(resolution - 0.0625) < 0.01) return "1/64";
    if (std::abs(resolution - 0.03125) < 0.01) return "1/128";
    if (std::abs(resolution - 1.0/3.0) < 0.01) return "1/4T";
    if (std::abs(resolution - 0.5/3.0) < 0.01) return "1/8T";
    return "custom";
}

MidiEditorComponent::EditorContent::EditorContent(EditableMidiClip& midiClip, DrumLibraryManager& libManager, MidiProcessor& midiProc, DrumGrooveProcessor& proc)
: toolbar(), gridView(midiClip, libManager), velocityLane(midiClip, gridView), clip(midiClip), midiProcessor(midiProc), processor(proc)
{
    setWantsKeyboardFocus(true);

    addAndMakeVisible(toolbar);
    addAndMakeVisible(gridView);
    addAndMakeVisible(velocityLane);
    velocityLane.setVisible(false);

    // Tool and grid callbacks
    toolbar.onToolChanged = [this](EditorTool tool) { gridView.setCurrentTool(tool); };
    toolbar.onGridResolutionChanged = [this](double resolution) { gridView.setGridResolution(resolution); };
    toolbar.onPlayClicked = [this]() { isPlaying ? stopPlayback() : startPlayback(); };
    toolbar.onStopClicked = [this]() { stopPlayback(); };
    toolbar.onPlaybackSpeedChanged = [this](double speed) { midiProcessor.setPlaybackSpeed(speed); };
    toolbar.onQuantizeClicked = [this]() { quantizeNotes(); };
    toolbar.onUndoClicked = [this]() {
        if (undoManager.undo())
        {
            setUnsavedChanges(true);
            gridView.repaint();
            velocityLane.refresh();
            toolbar.updateUndoRedoButtons(undoManager.canUndo(), undoManager.canRedo());
        }
    };

    toolbar.onRedoClicked = [this]() {
        if (undoManager.redo())
        {
            setUnsavedChanges(true);
            gridView.repaint();
            velocityLane.refresh();
            toolbar.updateUndoRedoButtons(undoManager.canUndo(), undoManager.canRedo());
        }
    };

    // Velocity lane toggle
    toolbar.onVelocityToggled = [this](bool visible) { setVelocityLaneVisible(visible); };

    // Preview notes toggle
    toolbar.onPreviewNotesToggled = [this](bool enabled) {
        previewNotesEnabled = enabled;
        gridView.setPreviewNotesEnabled(enabled);
    };

    // Loop toggle
    toolbar.onLoopToggled = [this](bool enabled) { setLoopEnabled(enabled); };

    // Loop region changed callback - update toolbar display
    gridView.onLoopRegionChanged = [this](double startQN, double endQN) {
        loopStartQN = startQN;
        loopEndQN = endQN;
        toolbar.updateLoopRegionDisplay(startQN, endQN);
    };

    // Zoom controls
    toolbar.onZoomIn = [this]()
    {
        gridView.zoomIn();
        if (velocityLaneVisible)
            velocityLane.setZoomFactor(gridView.getZoomLevel());
    };

    toolbar.onZoomOut = [this]()
    {
        gridView.zoomOut();
        if (velocityLaneVisible)
            velocityLane.setZoomFactor(gridView.getZoomLevel());
    };

    // Zoom slider
    toolbar.onZoomChanged = [this](float zoom)
    {
        gridView.setZoomLevel(zoom);
        if (velocityLaneVisible)
            velocityLane.setZoomFactor(zoom);
    };


    // Save/Save As buttons (forward to parent MidiEditorComponent)
    // Will be connected from parent in setupCallbacks()

    // Clip callbacks
    gridView.onClipModified = [this]()
    {
        setUnsavedChanges(true);
        velocityLane.refresh();
    };

    // Connect preview note callback
    gridView.onPlayPreviewNote = [this](int noteNumber, int velocity) {
        playPreviewNote(noteNumber, velocity);
    };

    gridView.setPreviewNotesEnabled(false);

    velocityLane.onVelocityChanged = [this]()
    {
        setUnsavedChanges(true);
        gridView.repaint();
    };

    velocityLane.onNeedsRefresh = [this]()
    {
        gridView.repaint();
    };

    // Set initial playback speed
    midiProcessor.setPlaybackSpeed(toolbar.getPlaybackSpeed());

    // Initialize loop region
    loopEndQN = clip.getDurationInQuarterNotes();

    // Initialize loop display
    toolbar.updateLoopRegionDisplay(loopStartQN, loopEndQN);

    // Set undo manager for grid view
    gridView.setUndoManager(&undoManager);

    startTimerHz(30);
}

bool MidiEditorComponent::EditorContent::keyPressed(const juce::KeyPress& key)
{

    // Ctrl+Z = Undo
    if (key.getKeyCode() == 'z' && key.getModifiers().isCtrlDown() && !key.getModifiers().isShiftDown())
    {
        if (undoManager.undo())
        {
            setUnsavedChanges(true);
            gridView.repaint();
            velocityLane.refresh();
            toolbar.updateUndoRedoButtons(undoManager.canUndo(), undoManager.canRedo());
        }
        return true;
    }

    // Ctrl+Shift+Z or Ctrl+Y = Redo
    if ((key.getKeyCode() == 'z' && key.getModifiers().isCtrlDown() && key.getModifiers().isShiftDown()) ||
        (key.getKeyCode() == 'y' && key.getModifiers().isCtrlDown()))
    {
        if (undoManager.redo())
        {
            setUnsavedChanges(true);
            gridView.repaint();
            velocityLane.refresh();
            toolbar.updateUndoRedoButtons(undoManager.canUndo(), undoManager.canRedo());
        }
        return true;
    }

    return Component::keyPressed(key);
}

MidiEditorComponent::EditorContent::~EditorContent()
{
    stopTimer();
    stopPlayback();
}

void MidiEditorComponent::EditorContent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A1A));
}

void MidiEditorComponent::EditorContent::resized()
{
    auto bounds = getLocalBounds();

    // Toolbar (2 rows = 100px)
    toolbar.setBounds(bounds.removeFromTop(100));

    // Velocity lane at bottom if visible
    if (velocityLaneVisible)
    {
        auto velBounds = bounds.removeFromBottom(VelocityLaneComponent::LANE_HEIGHT);
        // Don't remove LABEL_WIDTH - let velocity lane span full width
        velocityLane.setBounds(velBounds);
    }

    // Grid view
    gridView.setBounds(bounds);
}

void MidiEditorComponent::EditorContent::timerCallback()
{
    if (isPlaying)
    {
        // Use visual playhead position (includes latency compensation)
        double positionInSeconds = midiProcessor.getVisualPlayheadPosition();
        double bpm = clip.getBPM();

        // Convert seconds to quarter notes
        double positionInQuarterNotes = positionInSeconds * (bpm / 60.0);

        gridView.setPlayheadPosition(positionInQuarterNotes);
    }

    // Repaint velocity lane to stay in sync with grid scroll
    if (velocityLaneVisible)
        velocityLane.repaint();

    // Update undo/redo button states
    toolbar.updateUndoRedoButtons(undoManager.canUndo(), undoManager.canRedo());
}

void MidiEditorComponent::EditorContent::setVelocityLaneVisible(bool visible)
{
    velocityLaneVisible = visible;
    velocityLane.setVisible(visible);

    if (visible)
    {
        velocityLane.setZoomFactor(gridView.getZoomLevel());
        velocityLane.refresh();
    }

    resized();
}

void MidiEditorComponent::EditorContent::setReadOnly(bool readOnly)
{
    // Disable grid view editing
    gridView.setReadOnly(readOnly);

    // Disable velocity lane editing
    velocityLane.setEnabled(!readOnly);
}

void MidiEditorComponent::EditorContent::playPreviewNote(int noteNumber, int velocity)
{
    if (!previewNotesEnabled)
        return;

    // Get target library for remapping
    DrumLibrary targetLib = clip.getSourceLibrary();

    // Create a short MIDI note
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, noteNumber, (juce::uint8)velocity);
    juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, noteNumber);

    // Send to audio processor
    midiProcessor.addPreviewNote(noteOn, noteOff);
}

void MidiEditorComponent::EditorContent::setLoopEnabled(bool enabled)
{
    loopEnabled = enabled;
    gridView.setLoopRegionDragEnabled(enabled);
    toolbar.setLoopEnabled(enabled);

    if (enabled)
        gridView.setLoopRegion(loopStartQN, loopEndQN);
    else
        gridView.setLoopRegion(0.0, 0.0);

    repaint();
}

void MidiEditorComponent::EditorContent::setLoopRegion(double startQN, double endQN)
{
    loopStartQN = startQN;
    loopEndQN = endQN;

    if (loopEnabled)
        gridView.setLoopRegion(startQN, endQN);
}

void MidiEditorComponent::EditorContent::startPlayback()
{
    // Stop any existing playback and clear clips
    midiProcessor.stop();
    midiProcessor.clearAllClips();

    // Create temporary MIDI file with unique name
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto tempFile = tempDir.getChildFile("DrumGroovePro_editor_preview_" +
    juce::String(juce::Random::getSystemRandom().nextInt()) + ".mid");

    if (!clip.saveToMidiFile(tempFile))
    {
        DBG("Failed to save temp MIDI file for playback");
        return;
    }

    if (tempFile.existsAsFile())
    {
        // Calculate actual duration from last note end time
        double maxEndTimeQN = 0.0;
        const auto& notes = clip.getNotes();
        for (const auto& note : notes)
        {
            double noteEndQN = note.startTime + note.duration;
            maxEndTimeQN = juce::jmax(maxEndTimeQN, noteEndQN);
        }

        // Add buffer for last note to ring out
        maxEndTimeQN += 0.5; // Half quarter note buffer

        double bpm = clip.getBPM();
        double duration = maxEndTimeQN * (60.0 / bpm); // Convert QN to seconds

        DrumLibrary sourceLib = clip.getSourceLibrary();

        // Add clip to processor with explicit duration
        midiProcessor.addMidiClip(
            tempFile,
            0.0,  // startTime
            sourceLib,
            bpm,  // referenceBPM
            bpm,  // targetBPM
            0,    // trackNum
            duration,  // explicit duration from actual notes
            "editor_preview_" + juce::Uuid().toString()
        );

        // Set playhead to start
        midiProcessor.setPlayheadPosition(0.0);

        // Start playback
        midiProcessor.play();

        isPlaying = true;
        toolbar.updatePlayButton(true);
        startTimerHz(30);

        DBG("Playing MIDI editor clip: " + juce::String(clip.getNoteCount()) +
        " notes, duration: " + juce::String(duration, 3) + "s (maxEndQN: " +
        juce::String(maxEndTimeQN, 3) + ")");
    }
}

void MidiEditorComponent::EditorContent::stopPlayback()
{
    midiProcessor.stop();
    midiProcessor.clearAllClips();
    isPlaying = false;
    toolbar.updatePlayButton(false);
    stopTimer();
}

void MidiEditorComponent::EditorContent::quantizeNotes()
{
    double gridRes = toolbar.getGridResolution();
    auto selectedNotes = clip.getSelectedNotes();

    // Build batch move command
    std::vector<BatchMoveCommand::NoteMove> moves;

    if (selectedNotes.isEmpty())
    {
        // Quantize all notes
        const auto& allNotes = clip.getNotes();
        for (const auto& note : allNotes)
        {
            double quantized = std::round(note.startTime / gridRes) * gridRes;
            if (std::abs(quantized - note.startTime) > 0.001) // Only if position changes
            {
                moves.push_back({note.id, note.startTime, note.noteNumber, quantized, note.noteNumber});
            }
        }
    }
    else
    {
        // Quantize only selected notes
        for (auto* note : selectedNotes)
        {
            if (note)
            {
                double quantized = std::round(note->startTime / gridRes) * gridRes;
                if (std::abs(quantized - note->startTime) > 0.001)
                {
                    moves.push_back({note->id, note->startTime, note->noteNumber, quantized, note->noteNumber});
                }
            }
        }
    }

    if (!moves.empty())
    {
        undoManager.perform(std::make_unique<BatchMoveCommand>(clip, moves));
        setUnsavedChanges(true);
        gridView.repaint();
        velocityLane.refresh();
        toolbar.updateUndoRedoButtons(undoManager.canUndo(), undoManager.canRedo());
    }
}

// MidiEditorComponent Implementation
MidiEditorComponent::MidiEditorComponent(DrumLibraryManager& libManager, MidiProcessor& midiProc, DrumGrooveProcessor& proc)
: DocumentWindow("MIDI Editor", juce::Colours::darkgrey, DocumentWindow::allButtons),
drumLibraryManager(libManager), midiProcessor(midiProc), processor(proc)
{
    setUsingNativeTitleBar(true);
    setResizable(true, false);
}

MidiEditorComponent::~MidiEditorComponent()
{
}

bool MidiEditorComponent::openMidiFile(const juce::File& file, DrumLibrary sourceLib)
{
    clip = std::make_unique<EditableMidiClip>();

    if (!clip->loadFromMidiFile(file, sourceLib))
        return false;

    content = std::make_unique<EditorContent>(*clip, drumLibraryManager, midiProcessor, processor);
    setContentOwned(content.get(), true);
    content->setWantsKeyboardFocus(true);

    currentFile = file;
    originalFile = file;

    // Check if file is from read-only folder
    isReadOnly = isReadOnlyFile(file);

    if (isReadOnly)
    {
        // Disable all controls except Save As
        content->setReadOnly(true);
        content->toolbar.setReadOnly(true);

        // Show warning
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Read-Only File",
            "This MIDI file is in a protected folder.\nAll editing controls are disabled.\nUse 'Save As' to create an editable copy.",
            "OK");
    }
    else
    {
        content->toolbar.setReadOnly(false);
    }

    setupCallbacks();
    updateTitle();

    centreWithSize(1400, 800);
    setVisible(true);
    content->grabKeyboardFocus();

    return true;
}

void MidiEditorComponent::createNewClip(DrumLibrary sourceLib, double bpm, int bars)
{
    clip = std::make_unique<EditableMidiClip>();
    clip->setSourceLibrary(sourceLib);
    clip->setBPM(bpm);
    clip->setLengthInBars(bars);

    content = std::make_unique<EditorContent>(*clip, drumLibraryManager, midiProcessor, processor);
    setContentOwned(content.get(), true);

    currentFile = juce::File();
    originalFile = juce::File();
    isReadOnly = false;

    content->toolbar.setReadOnly(false);
    setupCallbacks();
    updateTitle();

    centreWithSize(1400, 800);
    setVisible(true);
    content->grabKeyboardFocus();
}

void MidiEditorComponent::setupCallbacks()
{
    // Connect toolbar Save/Save As to MidiEditorComponent methods
    content->toolbar.onSaveClicked = [this]() { saveClip(); };
    content->toolbar.onSaveAsClicked = [this]() { saveClipAs(); };
}

void MidiEditorComponent::closeButtonPressed()
{
    if (promptToSaveChanges())
    {
        setVisible(false);
        if (onEditorClosed)
            onEditorClosed();
    }
}

bool MidiEditorComponent::saveClip()
{
    // Block direct save for read-only files
    if (isReadOnly)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Save",
            "This file is in a protected folder.\nPlease use 'Save As' to save to a different location.",
            "OK");
        return false;
    }

    // If no current file, use Save As dialog
    if (!currentFile.existsAsFile())
        return saveClipAs();

    // Save to existing file
    if (clip->saveToMidiFile(currentFile))
    {
        content->setUnsavedChanges(false);
        originalFile = currentFile;
        updateTitle();

        if (onClipSaved)
            onClipSaved(currentFile);

        return true;
    }

    return false;
}

bool MidiEditorComponent::saveClipAs()
{
    // Keep prompting until user selects a valid folder or cancels
    while (true)
    {
        juce::FileChooser chooser("Save MIDI File", juce::File(), "*.mid");

        if (!chooser.browseForFileToSave(true))
        {
            // User cancelled
            return false;
        }

        auto selectedFile = chooser.getResult();

        // Check if the target folder is protected (read-only)
        if (isReadOnlyFile(selectedFile))
        {
            // Show error and loop back to folder selection
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Cannot Save Here",
                "The selected folder is marked as read-only/protected in your configuration.\n"
                "Please choose a different location to save your file.",
                "OK");

            // Wait a bit for the message to be acknowledged, then continue loop
            juce::Thread::sleep(100);
            continue;
        }

        // Valid location - proceed with save
        currentFile = selectedFile;
        isReadOnly = false;
        content->toolbar.setReadOnly(false);

        // Save directly to the file
        if (clip->saveToMidiFile(currentFile))
        {
            content->setUnsavedChanges(false);
            originalFile = currentFile;
            updateTitle();

            if (onClipSaved)
                onClipSaved(currentFile);

            return true;
        }

        return false;
    }
}

void MidiEditorComponent::updateTitle()
{
    juce::String title = "MIDI Editor";

    if (currentFile.existsAsFile())
    {
        title += " - " + currentFile.getFileName();
        if (content && content->hasUnsavedChanges())
            title += " *";
    }
    else
    {
        title += " - [New Clip]";
        if (content && content->hasUnsavedChanges())
            title += " *";
    }

    setName(title);
}

bool MidiEditorComponent::promptToSaveChanges()
{
    if (!content || !content->hasUnsavedChanges())
        return true;

    int result = juce::AlertWindow::showYesNoCancelBox(
        juce::AlertWindow::QuestionIcon,
        "Unsaved Changes",
        "Do you want to save changes before closing?",
        "Save", "Don't Save", "Cancel");

    if (result == 0) return false; // Cancel
    if (result == 1) return saveClip(); // Save
    return true; // Don't save
}

bool MidiEditorComponent::isLibraryFile(const juce::File& file) const
{
    // Check if file is in library folders
    return false; // TODO: Implement
}

bool MidiEditorComponent::isReadOnlyFile(const juce::File& file) const
{
    // Load config.xml from AppData
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
    .getChildFile("DrumGroovePro");
    auto configFile = appDataDir.getChildFile("config.xml");

    if (!configFile.existsAsFile())
        return false;

    auto xml = juce::parseXML(configFile);
    if (!xml)
        return false;

    // Check RootFolders element
    auto* foldersElement = xml->getChildByName("RootFolders");
    if (!foldersElement)
        return false;

    juce::String filePath = file.getFullPathName();

    for (auto* folderElement : foldersElement->getChildIterator())
    {
        if (folderElement->hasTagName("Folder"))
        {
            juce::String folderPath = folderElement->getStringAttribute("path");
            bool isWritable = folderElement->getBoolAttribute("isWritable", true);

            // Check if file is within this folder
            if (filePath.startsWith(folderPath) && !isWritable)
            {
                return true; // File is in read-only folder
            }
        }
    }

    return false;
}
