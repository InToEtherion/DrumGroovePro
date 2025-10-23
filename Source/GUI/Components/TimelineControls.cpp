#include "TimelineControls.h"
#include "MultiTrackContainer.h"
#include "TimelineManager.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../../PluginProcessor.h"

TimelineControls::TimelineControls(DrumGrooveProcessor& p, MultiTrackContainer& c)
: processor(p), container(c)
{
    // Create TimelineManager
    timelineManager = std::make_unique<TimelineManager>(&container);
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    // File management buttons
    fileButton.setButtonText("File");
    fileButton.addListener(this);
    fileButton.setTooltip("File operations");
    addAndMakeVisible(fileButton);

    addButton.setButtonText("+");
    addButton.addListener(this);
    addButton.setTooltip("Add new track");
    addButton.setClickingTogglesState(false);  // Don't toggle, just click
    addAndMakeVisible(addButton);

    removeButton.setButtonText("-");
    removeButton.addListener(this);
    removeButton.setTooltip("Remove selected track or clear clips");
    removeButton.setClickingTogglesState(false);  // Don't toggle, just click
    addAndMakeVisible(removeButton);

    // Transport buttons
    playButton.setButtonText("PLAY");
    playButton.addListener(this);
    playButton.setVisible(true);
    addAndMakeVisible(playButton);

    pauseButton.setButtonText("PAUSE");
    pauseButton.addListener(this);
    pauseButton.setVisible(false);
    addAndMakeVisible(pauseButton);

    stopButton.setButtonText("STOP");
    stopButton.addListener(this);
    addAndMakeVisible(stopButton);

    // Loop button
    loopButton.setButtonText("LOOP");
    loopButton.addListener(this);
    loopButton.setTooltip("Click and drag on ruler to set selection range");
    addAndMakeVisible(loopButton);

    // Time display
    timeDisplay.setText("00:00:00:000", juce::dontSendNotification);
    timeDisplay.setFont(lnf.getMonospaceFont().withHeight(15.0f));
    timeDisplay.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    timeDisplay.setColour(juce::Label::backgroundColourId, ColourPalette::secondaryBackground);
    timeDisplay.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeDisplay);

    // Loop/Selection time fields with EXACT original setup
    loopStartLabel.setText("Start:", juce::dontSendNotification);
    loopStartLabel.setFont(lnf.getSmallFont());
    loopStartLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(loopStartLabel);

    loopStartField.setText("00:00:00:000");
    loopStartField.setFont(lnf.getMonospaceFont().withHeight(11.0f));
    loopStartField.setColour(juce::TextEditor::backgroundColourId, ColourPalette::inputBackground);
    loopStartField.setColour(juce::TextEditor::textColourId, ColourPalette::primaryText);
    loopStartField.setColour(juce::TextEditor::outlineColourId, ColourPalette::borderColour);
    loopStartField.setJustification(juce::Justification::centred);
    loopStartField.setInputRestrictions(12, "0123456789:");  // Input restrictions
    loopStartField.onTextChange = [this]() { handleLoopStartChange(); };  // onTextChange
    loopStartField.onFocusLost = [this]() { updateLoopTimeFields(); };
    addAndMakeVisible(loopStartField);

    loopEndLabel.setText("End:", juce::dontSendNotification);
    loopEndLabel.setFont(lnf.getSmallFont());
    loopEndLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(loopEndLabel);

    loopEndField.setText("00:00:00:000");
    loopEndField.setFont(lnf.getMonospaceFont().withHeight(11.0f));
    loopEndField.setColour(juce::TextEditor::backgroundColourId, ColourPalette::inputBackground);
    loopEndField.setColour(juce::TextEditor::textColourId, ColourPalette::primaryText);
    loopEndField.setColour(juce::TextEditor::outlineColourId, ColourPalette::borderColour);
    loopEndField.setJustification(juce::Justification::centred);
    loopEndField.setInputRestrictions(12, "0123456789:");  // Input restrictions
    loopEndField.onTextChange = [this]() { handleLoopEndChange(); };  // onTextChange
    loopEndField.onFocusLost = [this]() { updateLoopTimeFields(); };
    addAndMakeVisible(loopEndField);

    // Zoom controls (KEPT!)
    zoomOutButton.setButtonText("-");
    zoomOutButton.addListener(this);
    addAndMakeVisible(zoomOutButton);

    zoomSlider.setRange(10.0, 500.0, 1.0);
    zoomSlider.setValue(100.0);
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    zoomSlider.setTextValueSuffix("%");
    zoomSlider.addListener(this);
    addAndMakeVisible(zoomSlider);

    zoomInButton.setButtonText("+");
    zoomInButton.addListener(this);
    addAndMakeVisible(zoomInButton);

    fitButton.setButtonText("Fit");
    fitButton.addListener(this);
    addAndMakeVisible(fitButton);

    // Add container as change listener
    container.addChangeListener(this);

    // Initialize states
    updateTransportButtons();
    updateLoopButton();
    updateLoopTimeFields();

    // Start timer
    startTimer(50);
}

TimelineControls::~TimelineControls()
{
    stopTimer();
    container.removeChangeListener(this);
}

void TimelineControls::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);

    // Draw top border (optional, was in original)
    // g.setColour(ColourPalette::borderColour);
    // g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);

    // Draw separator between file buttons and transport buttons
    float separatorX = static_cast<float>(FILE_BUTTONS_WIDTH + LEFT_MARGIN - 5);
    g.setColour(ColourPalette::separator);
    g.fillRect(separatorX, 5.0f, static_cast<float>(SEPARATOR_WIDTH), static_cast<float>(getHeight() - 10));
}

void TimelineControls::resized()
{
    auto bounds = getLocalBounds().reduced(5);

    // File management buttons on the left
    auto fileButtonsArea = bounds.removeFromLeft(FILE_BUTTONS_WIDTH);
    fileButton.setBounds(fileButtonsArea.removeFromLeft(80).reduced(2));
    fileButtonsArea.removeFromLeft(3);
    addButton.setBounds(fileButtonsArea.removeFromLeft(40).reduced(2));
    fileButtonsArea.removeFromLeft(3);
    removeButton.setBounds(fileButtonsArea.removeFromLeft(40).reduced(2));

    // Add space for separator + alignment with ruler
    bounds.removeFromLeft(LEFT_MARGIN);

    // Transport controls (PLAY, PAUSE, STOP, LOOP)
    // But now starting from after the File buttons
    playButton.setBounds(bounds.removeFromLeft(60));
    pauseButton.setBounds(playButton.getBounds());
    bounds.removeFromLeft(5);
    stopButton.setBounds(bounds.removeFromLeft(60));
    bounds.removeFromLeft(5);
    loopButton.setBounds(bounds.removeFromLeft(60));

    // Time display
    bounds.removeFromLeft(10);
    timeDisplay.setBounds(bounds.removeFromLeft(110));

    // Selection time fields (Start and End)
    bounds.removeFromLeft(15);
    auto loopFieldArea = bounds.removeFromLeft(280);
    loopStartLabel.setBounds(loopFieldArea.removeFromLeft(35));
    loopStartField.setBounds(loopFieldArea.removeFromLeft(95).withHeight(22));
    loopFieldArea.removeFromLeft(15);
    loopEndLabel.setBounds(loopFieldArea.removeFromLeft(30));
    loopEndField.setBounds(loopFieldArea.removeFromLeft(95).withHeight(22));

    // Zoom controls on far right (KEPT!)
    auto zoomArea = bounds.removeFromRight(230);
    fitButton.setBounds(zoomArea.removeFromRight(40));
    zoomArea.removeFromRight(5);
    zoomInButton.setBounds(zoomArea.removeFromRight(30));
    zoomSlider.setBounds(zoomArea.removeFromRight(120));
    zoomOutButton.setBounds(zoomArea.removeFromRight(30));
}

void TimelineControls::buttonClicked(juce::Button* button)
{
    // File button handlers
    if (button == &fileButton)
    {
        showFileMenu();
    }
    else if (button == &addButton)
    {
        onAddFile();
    }
    else if (button == &removeButton)
    {
        onRemoveFile();
    }
    // Transport button handlers
    else if (button == &playButton)
    {
        container.play();
        updateTransportButtons();
    }
    else if (button == &pauseButton)
    {
        container.pause();
        updateTransportButtons();
    }
    else if (button == &stopButton)
    {
        container.stop();
        updateTransportButtons();
    }
    else if (button == &loopButton)
    {
        container.toggleLoop();
        updateLoopButton();
    }
    // Zoom button handlers (KEPT!)
    else if (button == &zoomInButton)
    {
        zoomSlider.setValue(zoomSlider.getValue() * 1.2);
    }
    else if (button == &zoomOutButton)
    {
        zoomSlider.setValue(zoomSlider.getValue() / 1.2);
    }
    else if (button == &fitButton)
    {
        container.fitToContent();
        zoomSlider.setValue(container.getZoom());
    }
}

void TimelineControls::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &zoomSlider)
    {
        container.setZoom(static_cast<float>(slider->getValue()));
        updateZoomDisplay();
    }
}

void TimelineControls::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &container)
    {
        updateLoopTimeFields();
    }
}

// Timer callback (KEPT!)
void TimelineControls::timerCallback()
{
    updateTimeDisplay();
    updateLoopButton();
    updateLoopTimeFields();
}

// Update methods (ALL KEPT!)
void TimelineControls::updateTimeDisplay()
{
    double time = container.getPlayheadPosition();
    timeDisplay.setText(formatTime(time), juce::dontSendNotification);
}

void TimelineControls::updateZoomDisplay()
{
    // Zoom display is handled by the slider's text box
}

void TimelineControls::updateTransportButtons()
{
    bool playing = container.isPlaying();
    playButton.setVisible(!playing);
    pauseButton.setVisible(playing);
}

void TimelineControls::updateLoopButton()
{
    bool loopEnabled = container.isLoopEnabled();

    // Color change behavior (KEPT!)
    if (loopEnabled)
    {
        loopButton.setColour(juce::TextButton::buttonColourId, ColourPalette::primaryBlue);
        loopButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    }
    else
    {
        loopButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
        loopButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    }
}

void TimelineControls::updateLoopTimeFields()
{
    // Update fields if they don't have focus (KEPT!)
    if (!loopStartField.hasKeyboardFocus(true))
    {
        if (container.hasSelection())
        {
            double selectionStart = container.getSelectionStart();
            loopStartField.setText(formatTime(selectionStart), false);
        }
        else
        {
            loopStartField.setText("00:00:00:000", false);
        }
    }

    if (!loopEndField.hasKeyboardFocus(true))
    {
        if (container.hasSelection())
        {
            double selectionEnd = container.getSelectionEnd();
            loopEndField.setText(formatTime(selectionEnd), false);
        }
        else
        {
            loopEndField.setText("00:00:00:000", false);
        }
    }
}

void TimelineControls::setLoopStartTime(double timeInSeconds)
{
    // TODO: Implement loop start time setting
    // This will update the loop start field and container selection
    // when user creates selection on timeline ruler
    
    // For now, just update the field text
    loopStartField.setText(formatTime(timeInSeconds), juce::dontSendNotification);
}

void TimelineControls::setLoopEndTime(double timeInSeconds)
{
    // TODO: Implement loop end time setting
    // This will update the loop end field and container selection
    // when user creates selection on timeline ruler
    
    // For now, just update the field text
    loopEndField.setText(formatTime(timeInSeconds), juce::dontSendNotification);
}

void TimelineControls::handleLoopStartChange()
{
    auto timeStr = loopStartField.getText();
    if (isValidTimeFormat(timeStr))
    {
        double time = parseTime(timeStr);
        if (time >= 0.0)
        {
            container.setSelectionStart(time);
        }
    }
}

void TimelineControls::handleLoopEndChange()
{
    auto timeStr = loopEndField.getText();
    if (isValidTimeFormat(timeStr))
    {
        double time = parseTime(timeStr);
        if (time > container.getSelectionStart())
        {
            container.setSelectionEnd(time);
        }
    }
}

// Export submenu
void TimelineControls::showExportMenu()
{
    juce::PopupMenu menu;
    
    menu.addItem(1, "Export as Single MIDI File");
    menu.addItem(2, "Export as Separate MIDI Files (One per Track)");
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&fileButton),
        [this](int result) {
            if (!timelineManager) return;
            
            switch (result)
            {
                case 1:
                    timelineManager->exportTimelineAsMidi();
                    break;
                case 2:
                    timelineManager->exportTimelineAsSeparateMidis();
                    break;
            }
        });
}

// File button handlers
void TimelineControls::showFileMenu()
{
    juce::PopupMenu menu;

    menu.addItem(1, "Save Timeline State");
    menu.addItem(2, "Load Timeline State");
    menu.addSeparator();
    menu.addItem(3, "Clear All Tracks");  // Changed from 4 to 3

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&fileButton),
        [this](int result) {
            switch (result)
            {
                case 1:
                    if (timelineManager)
                        timelineManager->saveTimelineState();
                    break;
                case 2:
                    if (timelineManager)
                        timelineManager->loadTimelineState();
                    break;
                case 3:  // Changed from case 4 to case 3
                    container.clearAllTracks();
                    break;
            }
        });
}

void TimelineControls::onAddFile()
{
    // Add a new track to the timeline
    container.addTrack();
}

void TimelineControls::onRemoveFile()
{
    // Get the selected track index
    int selectedTrack = container.getSelectedTrackIndex();
    
    if (selectedTrack >= 0)
    {
        // Remove the selected track (or clear it if it's one of the first 3)
        container.removeTrack(selectedTrack);
    }
    else
    {
        // No track selected, delete selected clips instead
        container.deleteSelectedClips();
    }
}

// Time formatting utilities (KEPT EXACTLY!)
juce::String TimelineControls::formatTime(double seconds)
{
    int hours = static_cast<int>(seconds) / 3600;
    int mins = (static_cast<int>(seconds) % 3600) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int millis = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);

    return juce::String::formatted("%02d:%02d:%02d:%03d", hours, mins, secs, millis);
}

double TimelineControls::parseTime(const juce::String& timeStr)
{
    auto parts = juce::StringArray::fromTokens(timeStr, ":", "");
    if (parts.size() >= 4)
    {
        int hours = parts[0].getIntValue();
        int mins = parts[1].getIntValue();
        int secs = parts[2].getIntValue();
        int millis = parts[3].getIntValue();

        return hours * 3600.0 + mins * 60.0 + secs + millis / 1000.0;
    }
    return 0.0;
}

bool TimelineControls::isValidTimeFormat(const juce::String& timeStr)
{
    auto parts = juce::StringArray::fromTokens(timeStr, ":", "");
    return parts.size() == 4 &&
           parts[0].containsOnly("0123456789") &&
           parts[1].containsOnly("0123456789") &&
           parts[2].containsOnly("0123456789") &&
           parts[3].containsOnly("0123456789");
}