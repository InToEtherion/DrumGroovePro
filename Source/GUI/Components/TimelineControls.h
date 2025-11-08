#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DrumGrooveProcessor;
class MultiTrackContainer;
class TimelineManager;

class TimelineControls : public juce::Component,
                         public juce::Button::Listener,
                         public juce::Slider::Listener,
                         public juce::ChangeListener,
                         private juce::Timer
{
public:
    TimelineControls(DrumGrooveProcessor& processor, MultiTrackContainer& container);
    ~TimelineControls() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    void setLoopStartTime(double timeInSeconds);
    void setLoopEndTime(double timeInSeconds);

private:
    DrumGrooveProcessor& processor;
    MultiTrackContainer& container;
    std::unique_ptr<TimelineManager> timelineManager;

    juce::TextButton fileButton;
    juce::TextButton addButton;
    juce::TextButton removeButton;

    juce::TextButton playButton, pauseButton, stopButton;
    juce::TextButton loopButton;

    juce::Label timeDisplay;

    juce::Label loopStartLabel, loopEndLabel;
    juce::TextEditor loopStartField, loopEndField;

    juce::Label speedLabel;
    juce::Slider speedSlider;

    juce::TextButton zoomInButton, zoomOutButton, fitButton;
    juce::Slider zoomSlider;

    static constexpr int FILE_BUTTONS_WIDTH = 180;
    static constexpr int SEPARATOR_WIDTH = 2;
    static constexpr int LEFT_MARGIN = 10;

    void timerCallback() override;
    
    void updateTimeDisplay();
    void updateZoomDisplay();
    void updateTransportButtons();
    void updateLoopButton();
    void updateLoopTimeFields();
    void handleLoopStartChange();
    void handleLoopEndChange();

    void showFileMenu();
    void showExportMenu();
    void onAddFile();
    void onRemoveFile();

    static juce::String formatTime(double seconds);
    static double parseTime(const juce::String& timeStr);
    static bool isValidTimeFormat(const juce::String& timeStr);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineControls)
};