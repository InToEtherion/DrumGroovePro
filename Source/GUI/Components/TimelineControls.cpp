#include "TimelineControls.h"
#include "MultiTrackContainer.h"
#include "TimelineManager.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../../PluginProcessor.h"
#include <cmath>  // for std::abs

TimelineControls::TimelineControls(DrumGrooveProcessor& p, MultiTrackContainer& c)
: processor(p), container(c)
{
    timelineManager = std::make_unique<TimelineManager>(&container, processor);
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    fileButton.setButtonText("File");
    fileButton.addListener(this);
    fileButton.setTooltip("File operations");
    addAndMakeVisible(fileButton);

    addButton.setButtonText("+");
    addButton.addListener(this);
    addButton.setTooltip("Add new track");
    addButton.setClickingTogglesState(false);
    addAndMakeVisible(addButton);

    removeButton.setButtonText("-");
    removeButton.addListener(this);
    removeButton.setTooltip("Remove selected track or clear clips");
    removeButton.setClickingTogglesState(false);
    addAndMakeVisible(removeButton);

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

    loopButton.setButtonText("LOOP");
    loopButton.addListener(this);
    loopButton.setTooltip("Click and drag on ruler to set selection range");
    addAndMakeVisible(loopButton);

    timeDisplay.setText("00:00:00:000", juce::dontSendNotification);
    timeDisplay.setFont(lnf.getMonospaceFont().withHeight(15.0f));
    timeDisplay.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    timeDisplay.setColour(juce::Label::backgroundColourId, ColourPalette::secondaryBackground);
    timeDisplay.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeDisplay);

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
    loopStartField.setInputRestrictions(12, "0123456789:");
    loopStartField.onTextChange = [this]() { handleLoopStartChange(); };
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
    loopEndField.setInputRestrictions(12, "0123456789:");
    loopEndField.onTextChange = [this]() { handleLoopEndChange(); };
    loopEndField.onFocusLost = [this]() { updateLoopTimeFields(); };
    addAndMakeVisible(loopEndField);

    speedLabel.setText("Speed:", juce::dontSendNotification);
    speedLabel.setFont(lnf.getSmallFont());
    speedLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(speedLabel);

    speedSlider.setRange(25.0, 200.0, 1.0);
    speedSlider.setValue(100.0);
    speedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    speedSlider.setTextValueSuffix("%");
    speedSlider.setTooltip("Playback speed: 100% = normal, lower = slower, higher = faster");
    speedSlider.addListener(this);
    addAndMakeVisible(speedSlider);

    latencyLabel.setText("Latency:", juce::dontSendNotification);
    latencyLabel.setFont(lnf.getSmallFont());
    latencyLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(latencyLabel);

    // Text-only latency offset control (type value directly)
    latencyOffsetField.setText("-20", juce::dontSendNotification);
    latencyOffsetField.setFont(lnf.getMonospaceFont().withHeight(12.0f));
    latencyOffsetField.setColour(juce::TextEditor::backgroundColourId, ColourPalette::inputBackground);
    latencyOffsetField.setColour(juce::TextEditor::textColourId, ColourPalette::primaryText);
    latencyOffsetField.setColour(juce::TextEditor::outlineColourId, ColourPalette::borderColour);
    latencyOffsetField.setJustification(juce::Justification::centred);
    latencyOffsetField.setInputRestrictions(4, "-0123456789");
    latencyOffsetField.setTooltip("Visual latency offset in ms: Negative = visual lags behind audio (default: -20ms, range: -200 to 200)");
    latencyOffsetField.onReturnKey = [this]() {
        handleLatencyOffsetChange();
        latencyOffsetField.giveAwayKeyboardFocus();
    };
    latencyOffsetField.onEscapeKey = [this]() {
        updateLatencyOffsetField();
        latencyOffsetField.giveAwayKeyboardFocus();
    };
    latencyOffsetField.onFocusLost = [this]() {
        handleLatencyOffsetChange();
    };
    addAndMakeVisible(latencyOffsetField);

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

    container.addChangeListener(this);

    updateTransportButtons();
    updateLoopButton();
    updateLoopTimeFields();

    // Initialize latency field with current parameter value
    updateLatencyOffsetField();

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

    float separatorX = static_cast<float>(FILE_BUTTONS_WIDTH + LEFT_MARGIN - 5);
    g.setColour(ColourPalette::separator);
    g.fillRect(separatorX, 5.0f, static_cast<float>(SEPARATOR_WIDTH), static_cast<float>(getHeight() - 10));
}

void TimelineControls::resized()
{
    auto bounds = getLocalBounds().reduced(5);

    // === FILE OPERATIONS (keep same size) ===
    auto fileButtonsArea = bounds.removeFromLeft(FILE_BUTTONS_WIDTH);
    fileButton.setBounds(fileButtonsArea.removeFromLeft(80).reduced(2));
    fileButtonsArea.removeFromLeft(3);
    addButton.setBounds(fileButtonsArea.removeFromLeft(40).reduced(2));
    fileButtonsArea.removeFromLeft(3);
    removeButton.setBounds(fileButtonsArea.removeFromLeft(40).reduced(2));

    // Space after file buttons
    bounds.removeFromLeft(20);

    // === TRANSPORT CONTROLS ===
    playButton.setBounds(bounds.removeFromLeft(60));
    pauseButton.setBounds(playButton.getBounds());
    bounds.removeFromLeft(5);
    stopButton.setBounds(bounds.removeFromLeft(60));
    bounds.removeFromLeft(5);
    loopButton.setBounds(bounds.removeFromLeft(60));

    // Space after transport
    bounds.removeFromLeft(20);

    // === TIME DISPLAY ===
    timeDisplay.setBounds(bounds.removeFromLeft(110));

    // Space after time
    bounds.removeFromLeft(25);

    // === LOOP SELECTION ===
    auto loopFieldArea = bounds.removeFromLeft(280);
    loopStartLabel.setBounds(loopFieldArea.removeFromLeft(35));
    loopStartField.setBounds(loopFieldArea.removeFromLeft(95).withHeight(22));
    loopFieldArea.removeFromLeft(15);
    loopEndLabel.setBounds(loopFieldArea.removeFromLeft(30));
    loopEndField.setBounds(loopFieldArea.removeFromLeft(95).withHeight(22));

    // Space after loop fields
    bounds.removeFromLeft(25);

    // === SPEED CONTROL ===
    auto speedArea = bounds.removeFromLeft(160);
    speedLabel.setBounds(speedArea.removeFromLeft(45));
    speedSlider.setBounds(speedArea);

    // Space after speed
    bounds.removeFromLeft(25);

    // === LATENCY CONTROL ===
    auto latencyArea = bounds.removeFromLeft(100);
    latencyLabel.setBounds(latencyArea.removeFromLeft(55));
    latencyOffsetField.setBounds(latencyArea.withHeight(22));

    // Space before zoom controls
    bounds.removeFromLeft(20);

    // === ZOOM CONTROLS ===
    auto zoomArea = bounds.removeFromRight(215);

    zoomOutButton.setBounds(zoomArea.removeFromLeft(30));
    zoomArea.removeFromLeft(5);
    zoomSlider.setBounds(zoomArea.removeFromLeft(100));
    zoomArea.removeFromLeft(5);
    zoomInButton.setBounds(zoomArea.removeFromLeft(30));
    zoomArea.removeFromLeft(5);
    fitButton.setBounds(zoomArea.removeFromLeft(40));
}

void TimelineControls::buttonClicked(juce::Button* button)
{
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
    else if (slider == &speedSlider)
    {
        double speedPercent = slider->getValue();
        double speedMultiplier = speedPercent / 100.0;
        container.setPlaybackSpeed(speedMultiplier);
    }
}

void TimelineControls::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &container)
    {
        updateLoopTimeFields();
    }
}

void TimelineControls::timerCallback()
{
    updateTimeDisplay();
    updateLoopButton();
    updateLoopTimeFields();
    updateTransportButtons();
    updateZoomDisplay();  // Keep zoom slider in sync with actual zoom level
}

void TimelineControls::updateTimeDisplay()
{
    double time = container.getPlayheadPosition();
    float latencyMs = processor.getVisualLatencyOffset();

    // Show latency value in display
    juce::String timeText = formatTime(time) + " (" + juce::String(static_cast<int>(latencyMs)) + "ms)";
    timeDisplay.setText(timeText, juce::dontSendNotification);
}

void TimelineControls::updateZoomDisplay()
{
    // Get current zoom level from container and update slider
    float currentZoom = container.getZoom();
    
    // Only update if the value is different to avoid triggering unnecessary callbacks
    if (std::abs(zoomSlider.getValue() - currentZoom) > 0.1)
    {
        zoomSlider.setValue(currentZoom, juce::dontSendNotification);
    }
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
    loopStartField.setText(formatTime(timeInSeconds), juce::dontSendNotification);
}

void TimelineControls::setLoopEndTime(double timeInSeconds)
{
    loopEndField.setText(formatTime(timeInSeconds), juce::dontSendNotification);
}

void TimelineControls::handleLoopStartChange()
{
    juce::String text = loopStartField.getText();

    if (!isValidTimeFormat(text))
        return;

    double time = parseTime(text);

    if (time >= 0.0)
    {
        container.setSelectionStart(time);
    }
}

void TimelineControls::handleLoopEndChange()
{
    juce::String text = loopEndField.getText();

    if (!isValidTimeFormat(text))
        return;

    double time = parseTime(text);

    if (time >= 0.0)
    {
        container.setSelectionEnd(time);
    }
}

void TimelineControls::showFileMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save Timeline State");
    menu.addItem(2, "Load Timeline State");
    menu.addSeparator();
    menu.addItem(3, "Export as MIDI");
    menu.addItem(4, "Export Tracks as Separate MIDIs");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&fileButton),
                       [this](int result)
                       {
                           if (result == 1)
                               timelineManager->saveTimelineState();
                           else if (result == 2)
                               timelineManager->loadTimelineState();
                           else if (result == 3)
                               showExportMenu();
                           else if (result == 4)
                               timelineManager->exportTimelineAsSeparateMidis();
                       });
}

void TimelineControls::showExportMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Export Full Timeline");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&fileButton),
                       [this](int result)
                       {
                           if (result == 1)
                               timelineManager->exportTimelineAsMidi();
                       });
}

void TimelineControls::onAddFile()
{
    container.addTrack();
}

void TimelineControls::onRemoveFile()
{
    int selectedTrack = container.getSelectedTrackIndex();

    if (selectedTrack >= 0)
    {
        container.removeTrack(selectedTrack);
    }
}

juce::String TimelineControls::formatTime(double seconds)
{
    int totalMilliseconds = static_cast<int>(seconds * 1000.0);
    int hours = totalMilliseconds / 3600000;
    int minutes = (totalMilliseconds % 3600000) / 60000;
    int secs = (totalMilliseconds % 60000) / 1000;
    int millis = totalMilliseconds % 1000;

    return juce::String::formatted("%02d:%02d:%02d:%03d", hours, minutes, secs, millis);
}

double TimelineControls::parseTime(const juce::String& timeStr)
{
    juce::StringArray parts;
    parts.addTokens(timeStr, ":", "");

    if (parts.size() != 4)
        return 0.0;

    int hours = parts[0].getIntValue();
    int minutes = parts[1].getIntValue();
    int seconds = parts[2].getIntValue();
    int milliseconds = parts[3].getIntValue();

    return hours * 3600.0 + minutes * 60.0 + seconds + milliseconds / 1000.0;
}

bool TimelineControls::isValidTimeFormat(const juce::String& timeStr)
{
    juce::StringArray parts;
    parts.addTokens(timeStr, ":", "");

    if (parts.size() != 4)
        return false;

    for (const auto& part : parts)
    {
        if (!part.containsOnly("0123456789"))
            return false;
    }

    return true;
}

void TimelineControls::handleLatencyOffsetChange()
{
    juce::String text = latencyOffsetField.getText().trim();

    if (text.isEmpty() || text == "-")
        return;

    int latencyMs = text.getIntValue();

    // Force negative values only
    if (latencyMs > 0)
        latencyMs = -latencyMs;

    // Clamp to valid range (-200 to 0)
    latencyMs = juce::jlimit(-200, 0, latencyMs);

    processor.setVisualLatencyOffset(static_cast<float>(latencyMs));
    latencyOffsetField.setText(juce::String(latencyMs), juce::dontSendNotification);
}

void TimelineControls::updateLatencyOffsetField()
{
    // Only update if field doesn't have focus (don't interfere while typing)
    if (!latencyOffsetField.hasKeyboardFocus(true))
    {
        float currentValue = processor.getVisualLatencyOffset();
        int roundedValue = juce::roundToInt(currentValue);
        latencyOffsetField.setText(juce::String(roundedValue), juce::dontSendNotification);
    }
}