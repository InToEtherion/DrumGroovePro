#include "MidiEditorToolbar.h"

MidiEditorToolbar::MidiEditorToolbar()
{
    // Setup tool buttons
    setupButton(pencilButton, "Pencil (P)");
    setupButton(eraserButton, "Eraser (E)");
    setupButton(selectButton, "Select (S)");

    pencilButton.onClick = [this]() { setCurrentTool(EditorTool::Pencil); };
    eraserButton.onClick = [this]() { setCurrentTool(EditorTool::Eraser); };
    selectButton.onClick = [this]() { setCurrentTool(EditorTool::Select); };

    // Grid resolution combo
    addAndMakeVisible(gridLabel);
    gridLabel.setText("Grid:", juce::dontSendNotification);
    gridLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(gridResolutionCombo);
    gridResolutionCombo.addItem("1/4 (Quarter)", 1);
    gridResolutionCombo.addItem("1/8 (Eighth)", 2);
    gridResolutionCombo.addItem("1/16 (Sixteenth)", 3);
    gridResolutionCombo.addItem("1/32 (Thirty-second)", 4);
    gridResolutionCombo.addItem("1/64 (Sixty-fourth)", 5);
    gridResolutionCombo.addItem("1/128 (Hundred-twenty-eighth)", 6);
    gridResolutionCombo.addItem("1/4T (Quarter Triplet)", 7);
    gridResolutionCombo.addItem("1/8T (Eighth Triplet)", 8);
    gridResolutionCombo.setSelectedId(3, juce::dontSendNotification); // Default: 16th

    gridResolutionCombo.onChange = [this]()
    {
        int id = gridResolutionCombo.getSelectedId();
        double resolution = 0.25; // Default
        switch (id)
        {
            case 1: resolution = 1.0; break;
            case 2: resolution = 0.5; break;
            case 3: resolution = 0.25; break;
            case 4: resolution = 0.125; break;
            case 5: resolution = 0.0625; break;
            case 6: resolution = 0.03125; break;
            case 7: resolution = 1.0 / 3.0; break;
            case 8: resolution = 0.5 / 3.0; break;
        }
        gridResolution = resolution;
        if (onGridResolutionChanged)
            onGridResolutionChanged(resolution);
    };

    // Playback buttons
    setupButton(playButton, "Play");
    setupButton(stopButton, "Stop");
    playButton.onClick = [this]() { if (onPlayClicked) onPlayClicked(); };
    stopButton.onClick = [this]() { if (onStopClicked) onStopClicked(); };

    // Speed slider
    addAndMakeVisible(speedLabel);
    speedLabel.setText("Speed:", juce::dontSendNotification);
    speedLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(speedSlider);
    speedSlider.setRange(25.0, 200.0, 1.0);
    speedSlider.setValue(100.0);
    speedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    speedSlider.setTextValueSuffix("%");
    speedSlider.setTooltip("Playback speed: 100% = normal");
    speedSlider.onValueChange = [this]()
    {
        if (onPlaybackSpeedChanged)
            onPlaybackSpeedChanged(speedSlider.getValue() / 100.0);
    };

    // Quantize button
    setupButton(quantizeButton, "Quantize");
    quantizeButton.onClick = [this]() { if (onQuantizeClicked) onQuantizeClicked(); };

    // Velocity button
    addAndMakeVisible(velocityButton);
    velocityButton.setButtonText("Velocity");
    velocityButton.setClickingTogglesState(true);
    velocityButton.setToggleState(false, juce::dontSendNotification);
    velocityButton.onClick = [this]()
    {
        if (onVelocityToggled)
            onVelocityToggled(velocityButton.getToggleState());
    };

    // Preview Notes button
    addAndMakeVisible(previewNotesButton);
    previewNotesButton.setButtonText("Preview");
    previewNotesButton.setClickingTogglesState(true);
    previewNotesButton.setToggleState(false, juce::dontSendNotification);
    previewNotesButton.setTooltip("Play notes when clicking or adding them");
    previewNotesButton.onClick = [this]()
    {
        if (onPreviewNotesToggled)
            onPreviewNotesToggled(previewNotesButton.getToggleState());
    };

    // Loop controls
    addAndMakeVisible(loopButton);
    loopButton.setButtonText("Loop");
    loopButton.setToggleState(false, juce::dontSendNotification);
    loopButton.onClick = [this]()
    {
        if (onLoopToggled)
            onLoopToggled(loopButton.getToggleState());
    };

    addAndMakeVisible(loopStartLabel);
    loopStartLabel.setText("Start:", juce::dontSendNotification);
    loopStartLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(loopEndLabel);
    loopEndLabel.setText("End:", juce::dontSendNotification);
    loopEndLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(loopStartValue);
    loopStartValue.setText("0.00", juce::dontSendNotification);
    loopStartValue.setJustificationType(juce::Justification::centredLeft);
    loopStartValue.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF2A2A2A));
    loopStartValue.setColour(juce::Label::outlineColourId, juce::Colour(0xFF404040));

    addAndMakeVisible(loopEndValue);
    loopEndValue.setText("4.00", juce::dontSendNotification);
    loopEndValue.setJustificationType(juce::Justification::centredLeft);
    loopEndValue.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF2A2A2A));
    loopEndValue.setColour(juce::Label::outlineColourId, juce::Colour(0xFF404040));

    // Initially hide loop region displays
    loopStartLabel.setVisible(false);
    loopEndLabel.setVisible(false);
    loopStartValue.setVisible(false);
    loopEndValue.setVisible(false);

    // Zoom controls
    setupButton(zoomInButton, "+");
    setupButton(zoomOutButton, "-");
    zoomInButton.onClick = [this]() { if (onZoomIn) onZoomIn(); };
    zoomOutButton.onClick = [this]() { if (onZoomOut) onZoomOut(); };

    addAndMakeVisible(zoomLabel);
    zoomLabel.setText("Zoom:", juce::dontSendNotification);
    zoomLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(zoomSlider);
    zoomSlider.setRange(0.1, 4.0, 0.1);
    zoomSlider.setValue(1.0);
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.onValueChange = [this]()
    {
        if (onZoomChanged)
            onZoomChanged((float)zoomSlider.getValue());
    };

    // Undo/Redo buttons
    setupButton(undoButton, "Undo");
    setupButton(redoButton, "Redo");
    undoButton.onClick = [this]() { if (onUndoClicked) onUndoClicked(); };
    redoButton.onClick = [this]() { if (onRedoClicked) onRedoClicked(); };

    // Save/Save As buttons
    setupButton(saveButton, "Save");
    setupButton(saveAsButton, "Save As");
    saveButton.onClick = [this]() { if (onSaveClicked) onSaveClicked(); };
    saveAsButton.onClick = [this]() { if (onSaveAsClicked) onSaveAsClicked(); };
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2E7D32)); // Green
    saveAsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1565C0)); // Blue

    updateToolButtons();
    updateUndoRedoButtons(false, false);
}

MidiEditorToolbar::~MidiEditorToolbar()
{
}

void MidiEditorToolbar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF2A2A2A));
    g.setColour(juce::Colour(0xFF404040));
    g.drawLine(0, getHeight() - 1, getWidth(), getHeight() - 1, 1.0f);

    // Draw separator between rows
    g.drawLine(0, getHeight() / 2, getWidth(), getHeight() / 2, 1.0f);
}

void MidiEditorToolbar::resized()
{
    auto bounds = getLocalBounds().reduced(8, 5);
    int rowHeight = bounds.getHeight() / 2;

    // ROW 1: Tools, Grid, Playback, Speed
    auto row1 = bounds.removeFromTop(rowHeight);

    // Tools section
    int buttonWidth = 85;
    pencilButton.setBounds(row1.removeFromLeft(buttonWidth));
    row1.removeFromLeft(8);
    eraserButton.setBounds(row1.removeFromLeft(buttonWidth));
    row1.removeFromLeft(8);
    selectButton.setBounds(row1.removeFromLeft(buttonWidth));
    row1.removeFromLeft(25);

    // Grid section
    gridLabel.setBounds(row1.removeFromLeft(45));
    row1.removeFromLeft(5);
    gridResolutionCombo.setBounds(row1.removeFromLeft(200));
    row1.removeFromLeft(25);

    // Playback section
    playButton.setBounds(row1.removeFromLeft(70));
    row1.removeFromLeft(8);
    stopButton.setBounds(row1.removeFromLeft(70));
    row1.removeFromLeft(20);

    // Speed section
    speedLabel.setBounds(row1.removeFromLeft(55));
    row1.removeFromLeft(5);
    speedSlider.setBounds(row1.removeFromLeft(160));

    // Right side of row 1: Save, Save As, Undo, Redo
    auto row1Right = getLocalBounds().reduced(8, 5).removeFromTop(rowHeight);
    redoButton.setBounds(row1Right.removeFromRight(70));
    row1Right.removeFromRight(8);
    undoButton.setBounds(row1Right.removeFromRight(70));
    row1Right.removeFromRight(15);
    saveAsButton.setBounds(row1Right.removeFromRight(85));
    row1Right.removeFromRight(8);
    saveButton.setBounds(row1Right.removeFromRight(75));

    // ROW 2: Loop, Zoom, Quantize, Velocity
    auto row2 = bounds;

    // Loop section
    loopButton.setBounds(row2.removeFromLeft(70));
    row2.removeFromLeft(12);

    loopStartLabel.setBounds(row2.removeFromLeft(45));
    row2.removeFromLeft(5);
    loopStartValue.setBounds(row2.removeFromLeft(65));
    row2.removeFromLeft(12);

    loopEndLabel.setBounds(row2.removeFromLeft(40));
    row2.removeFromLeft(5);
    loopEndValue.setBounds(row2.removeFromLeft(65));
    row2.removeFromLeft(25);

    // Zoom section
    zoomLabel.setBounds(row2.removeFromLeft(50));
    row2.removeFromLeft(5);
    zoomInButton.setBounds(row2.removeFromLeft(35));
    row2.removeFromLeft(5);
    zoomOutButton.setBounds(row2.removeFromLeft(35));
    row2.removeFromLeft(5);
    zoomSlider.setBounds(row2.removeFromLeft(120));
    row2.removeFromLeft(25);

    // Action buttons section
    quantizeButton.setBounds(row2.removeFromLeft(90));
    velocityButton.setBounds(row2.removeFromLeft(85));
    row2.removeFromLeft(12);
    previewNotesButton.setBounds(row2.removeFromLeft(85));
}

void MidiEditorToolbar::setCurrentTool(EditorTool tool)
{
    currentTool = tool;
    updateToolButtons();
    if (onToolChanged)
        onToolChanged(tool);
}

void MidiEditorToolbar::setGridResolution(double resolution)
{
    gridResolution = resolution;

    int id = 3; // Default 16th
    if (std::abs(resolution - 1.0) < 0.01) id = 1;
    else if (std::abs(resolution - 0.5) < 0.01) id = 2;
    else if (std::abs(resolution - 0.25) < 0.01) id = 3;
    else if (std::abs(resolution - 0.125) < 0.01) id = 4;
    else if (std::abs(resolution - 0.0625) < 0.01) id = 5;
    else if (std::abs(resolution - 0.03125) < 0.01) id = 6;
    else if (std::abs(resolution - 1.0/3.0) < 0.01) id = 7;
    else if (std::abs(resolution - 0.5/3.0) < 0.01) id = 8;

    gridResolutionCombo.setSelectedId(id, juce::dontSendNotification);
}

void MidiEditorToolbar::setPlaybackSpeed(double speedPercent)
{
    speedSlider.setValue(speedPercent, juce::dontSendNotification);
}

double MidiEditorToolbar::getPlaybackSpeed() const
{
    return speedSlider.getValue() / 100.0;
}

void MidiEditorToolbar::updatePlayButton(bool isPlaying)
{
    if (isPlaying)
    {
        playButton.setButtonText("Stop");
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFCC0000));
    }
    else
    {
        playButton.setButtonText("Play");
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00AA00));
    }
}

void MidiEditorToolbar::setLoopEnabled(bool enabled)
{
    loopButton.setToggleState(enabled, juce::dontSendNotification);
    loopStartLabel.setVisible(enabled);
    loopEndLabel.setVisible(enabled);
    loopStartValue.setVisible(enabled);
    loopEndValue.setVisible(enabled);
}

void MidiEditorToolbar::setReadOnly(bool readOnly)
{
    // Disable ALL controls except Save As
    pencilButton.setEnabled(!readOnly);
    eraserButton.setEnabled(!readOnly);
    selectButton.setEnabled(!readOnly);
    gridResolutionCombo.setEnabled(!readOnly);
    playButton.setEnabled(!readOnly);
    stopButton.setEnabled(!readOnly);
    speedSlider.setEnabled(!readOnly);
    quantizeButton.setEnabled(!readOnly);
    undoButton.setEnabled(!readOnly);
    redoButton.setEnabled(!readOnly);
    velocityButton.setEnabled(!readOnly);
    loopButton.setEnabled(!readOnly);
    zoomInButton.setEnabled(!readOnly);
    zoomOutButton.setEnabled(!readOnly);
    zoomSlider.setEnabled(!readOnly);

    // Save button disabled for read-only
    saveButton.setEnabled(!readOnly);

    // Save As always available
    saveAsButton.setEnabled(true);
}

void MidiEditorToolbar::updateUndoRedoButtons(bool canUndo, bool canRedo)
{
    undoButton.setEnabled(canUndo);
    redoButton.setEnabled(canRedo);
}

void MidiEditorToolbar::setupButton(juce::TextButton& button, const juce::String& text)
{
    addAndMakeVisible(button);
    button.setButtonText(text);
    button.setClickingTogglesState(false);
}

void MidiEditorToolbar::updateToolButtons()
{
    pencilButton.setToggleState(currentTool == EditorTool::Pencil, juce::dontSendNotification);
    eraserButton.setToggleState(currentTool == EditorTool::Eraser, juce::dontSendNotification);
    selectButton.setToggleState(currentTool == EditorTool::Select, juce::dontSendNotification);

    auto updateButtonColor = [](juce::TextButton& btn, bool isSelected)
    {
        if (isSelected)
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00A8E8));
        else
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF404040));
    };

    updateButtonColor(pencilButton, currentTool == EditorTool::Pencil);
    updateButtonColor(eraserButton, currentTool == EditorTool::Eraser);
    updateButtonColor(selectButton, currentTool == EditorTool::Select);

    if (velocityButton.getToggleState())
        velocityButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00A8E8));
    else
        velocityButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF404040));

    if (previewNotesButton.getToggleState())
        previewNotesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00A8E8));
    else
        previewNotesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF404040));
}

void MidiEditorToolbar::updateLoopRegionDisplay(double startQN, double endQN)
{
    loopStartValue.setText(juce::String(startQN, 2), juce::dontSendNotification);
    loopEndValue.setText(juce::String(endQN, 2), juce::dontSendNotification);
}

void MidiEditorToolbar::setPreviewNotesEnabled(bool enabled)
{
    previewNotesButton.setToggleState(enabled, juce::dontSendNotification);
}
