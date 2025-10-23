#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Forward declarations
class DrumGrooveProcessor;
class MultiTrackContainer;
class TimelineManager;

class TimelineControls : public juce::Component,
                         public juce::Button::Listener,
                         public juce::Slider::Listener,
                         public juce::ChangeListener,
                         private juce::Timer  // Timer for updates
{
public:
    TimelineControls(DrumGrooveProcessor& processor, MultiTrackContainer& container);
    ~TimelineControls() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    // Loop time setters (public interface)
    void setLoopStartTime(double timeInSeconds);
    void setLoopEndTime(double timeInSeconds);

private:
    DrumGrooveProcessor& processor;
    MultiTrackContainer& container;
    std::unique_ptr<TimelineManager> timelineManager;

    // File management buttons (left side)
    juce::TextButton fileButton;
    juce::TextButton addButton;
    juce::TextButton removeButton;

    // Transport controls
    juce::TextButton playButton, pauseButton, stopButton;
    juce::TextButton loopButton;

    // Time display
    juce::Label timeDisplay;

    // Loop time fields
    juce::Label loopStartLabel, loopEndLabel;
    juce::TextEditor loopStartField, loopEndField;

    // Zoom controls (KEPT!)
    juce::TextButton zoomInButton, zoomOutButton, fitButton;
    juce::Slider zoomSlider;

    // Visual constants for new layout
    static constexpr int FILE_BUTTONS_WIDTH = 180;
    static constexpr int SEPARATOR_WIDTH = 2;
    static constexpr int LEFT_MARGIN = 10;

    // Timer callback (KEPT!)
    void timerCallback() override;
    
    // Update methods (ALL KEPT!)
    void updateTimeDisplay();
    void updateZoomDisplay();
    void updateTransportButtons();
    void updateLoopButton();
    void updateLoopTimeFields();
    void handleLoopStartChange();
    void handleLoopEndChange();

    // File button handlers
    void showFileMenu();
    void showExportMenu();
    void onAddFile();
    void onRemoveFile();

    // Time formatting utilities (KEPT!)
    static juce::String formatTime(double seconds);
    static double parseTime(const juce::String& timeStr);
    static bool isValidTimeFormat(const juce::String& timeStr);  // KEPT!

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineControls)
};