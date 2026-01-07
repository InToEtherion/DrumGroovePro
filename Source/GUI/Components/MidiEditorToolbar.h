#pragma once

#include <JuceHeader.h>
#include "../EditorTools/EditorTool.h"

class MidiEditorToolbar : public juce::Component
{
public:
    MidiEditorToolbar();
    ~MidiEditorToolbar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Tool selection
    void setCurrentTool(EditorTool tool);
    EditorTool getCurrentTool() const { return currentTool; }

    // Grid settings
    void setGridResolution(double resolution);
    double getGridResolution() const { return gridResolution; }

    // Playback speed
    void setPlaybackSpeed(double speedPercent);
    double getPlaybackSpeed() const;

    // Callbacks
    std::function<void(EditorTool)> onToolChanged;
    std::function<void(double)> onGridResolutionChanged;
    std::function<void()> onPlayClicked;
    std::function<void()> onStopClicked;
    std::function<void()> onQuantizeClicked;
    std::function<void()> onUndoClicked;
    std::function<void()> onRedoClicked;
    std::function<void(double)> onPlaybackSpeedChanged;
    std::function<void(bool)> onVelocityToggled;
    std::function<void(bool)> onLoopToggled;
    std::function<void()> onZoomIn;
    std::function<void()> onZoomOut;
    std::function<void(float)> onZoomChanged;  // Slider callback
    std::function<void()> onSaveClicked;
    std::function<void()> onSaveAsClicked;
    std::function<void(bool)> onPreviewNotesToggled;

    void updateUndoRedoButtons(bool canUndo, bool canRedo);
    void updatePlayButton(bool isPlaying);
    void setLoopEnabled(bool enabled);
    void setReadOnly(bool readOnly);
    void updateLoopRegionDisplay(double startQN, double endQN);
    void setPreviewNotesEnabled(bool enabled);
    bool isPreviewNotesEnabled() const { return previewNotesButton.getToggleState(); }

private:
    EditorTool currentTool = EditorTool::Pencil;
    double gridResolution = 0.25; // 16th note

    juce::TextButton pencilButton;
    juce::TextButton eraserButton;
    juce::TextButton selectButton;

    juce::ComboBox gridResolutionCombo;
    juce::Label gridLabel;

    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton quantizeButton;

    juce::TextButton undoButton;
    juce::TextButton redoButton;

    juce::TextButton previewNotesButton;

    juce::TextButton saveButton;
    juce::TextButton saveAsButton;

    juce::TextButton velocityButton;

    // Loop controls
    juce::ToggleButton loopButton;
    juce::Label loopStartLabel;
    juce::Label loopEndLabel;
    juce::Label loopStartValue;
    juce::Label loopEndValue;

    // Zoom controls
    juce::TextButton zoomInButton;
    juce::TextButton zoomOutButton;
    juce::Label zoomLabel;
    juce::Slider zoomSlider;

    juce::Label speedLabel;
    juce::Slider speedSlider;

    void setupButton(juce::TextButton& button, const juce::String& text);
    void updateToolButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditorToolbar)
};
