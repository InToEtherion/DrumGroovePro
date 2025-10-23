#include "TrackHeader.h"
#include "MultiTrackContainer.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../../PluginProcessor.h"

TrackHeader::TrackHeader(DrumGrooveProcessor& p, MultiTrackContainer& c, int trackNum)
    : processor(p), container(c), trackNumber(trackNum)
{
    normalBackgroundColour = ColourPalette::panelBackground;
    mutedBackgroundColour = ColourPalette::secondaryBackground.darker(0.3f);

    // Track name label
    trackNameLabel.setText("Track " + juce::String(trackNumber), juce::dontSendNotification);
    trackNameLabel.setEditable(false, true, false);
    trackNameLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    trackNameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    trackNameLabel.setJustificationType(juce::Justification::centred);
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    trackNameLabel.setFont(lnf.getNormalFont().withHeight(13.0f).boldened());
    trackNameLabel.onEditorShow = [this]() { startNameEditing(); };
    trackNameLabel.onEditorHide = [this]() { finishNameEditing(); };
    trackNameLabel.addListener(this);
    addAndMakeVisible(trackNameLabel);

    // BPM label
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setFont(lnf.getSmallFont());
    bpmLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    bpmLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmLabel);

    // BPM slider
    bpmSlider.setRange(40.0, 400.0, 1.0);
    bpmSlider.setValue(120.0);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bpmSlider.addListener(this);
    bpmSlider.setTooltip("Track BPM");
    addAndMakeVisible(bpmSlider);

    // BPM text box
    bpmTextBox.setText("120", juce::dontSendNotification);
    bpmTextBox.setColour(juce::TextEditor::backgroundColourId, ColourPalette::inputBackground);
    bpmTextBox.setColour(juce::TextEditor::textColourId, ColourPalette::primaryText);
    bpmTextBox.setColour(juce::TextEditor::outlineColourId, ColourPalette::borderColour);
    bpmTextBox.setJustification(juce::Justification::centred);
    bpmTextBox.setInputRestrictions(3, "0123456789");
    bpmTextBox.onTextChange = [this]() { updateBPMFromTextBox(); };
    bpmTextBox.onFocusLost = [this]() { syncBPMControls(bpmSlider.getValue()); };
    addAndMakeVisible(bpmTextBox);

    // Solo button
    soloButton.setButtonText("SOLO");
    soloButton.setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    soloButton.setColour(juce::ToggleButton::tickColourId, ColourPalette::warningOrange);
    soloButton.addListener(this);
    soloButton.setTooltip("Solo Track");
    addAndMakeVisible(soloButton);

    // Mute button
    muteButton.setButtonText("MUTE");
    muteButton.setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    muteButton.setColour(juce::ToggleButton::tickColourId, ColourPalette::errorRed);
    muteButton.addListener(this);
    muteButton.setTooltip("Mute Track");
    addAndMakeVisible(muteButton);

    updateVisualState();
}

TrackHeader::~TrackHeader() = default;

void TrackHeader::paint(juce::Graphics& g)
{
    // Check if this track is inactive due to solo on another track
    bool isInactiveDueToSolo = false;
    if (!isSoloed())
    {
        // Check if any other track is soloed
        for (int i = 0; i < container.getNumTracks(); ++i)
        {
            if (i != (trackNumber - 1) && container.isTrackSoloed(i))
            {
                isInactiveDueToSolo = true;
                break;
            }
        }
    }
    
    // Draw background based on selection, mute, and solo state
    if (isInactiveDueToSolo)
    {
        // Gray out inactive tracks when another track is soloed
        g.fillAll(ColourPalette::secondaryBackground.darker(0.5f));
    }
    else if (selected)
    {
        // Selected track gets blue background - more opaque for better visibility
        g.fillAll(ColourPalette::primaryBlue.withAlpha(0.4f));
    }
    else if (isMuted())
    {
        g.fillAll(mutedBackgroundColour);
    }
    else
    {
        g.fillAll(normalBackgroundColour);
    }

    // Draw border - thicker for selected tracks
    if (selected)
    {
        g.setColour(ColourPalette::primaryBlue);
        g.drawRect(getLocalBounds(), 3);  // Even thicker border for selected (3px)
        
        // Add inner highlight for better visibility
        auto innerBounds = getLocalBounds().reduced(3);
        g.setColour(ColourPalette::primaryBlue.withAlpha(0.6f));
        g.drawRect(innerBounds, 1);
    }
    else
    {
        g.setColour(ColourPalette::borderColour);
        g.drawRect(getLocalBounds(), 1);
    }

    // Draw right separator line
    g.setColour(ColourPalette::separator);
    g.drawLine(static_cast<float>(getWidth() - 1), 0.0f,
               static_cast<float>(getWidth() - 1), static_cast<float>(getHeight()), 2.0f);
}
void TrackHeader::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    
    trackNameLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(3);

    auto bpmRow = bounds.removeFromTop(22);
    bpmLabel.setBounds(bpmRow.removeFromLeft(30));
    bpmTextBox.setBounds(bpmRow.removeFromRight(40));
    bpmSlider.setBounds(bpmRow.reduced(3, 0));

    bounds.removeFromTop(5);

    auto buttonArea = bounds.removeFromTop(25);
    int totalButtonWidth = buttonArea.getWidth() - 5;
    int buttonWidth = totalButtonWidth / 2 - 2;

    auto soloArea = buttonArea.removeFromLeft(buttonWidth);
    buttonArea.removeFromLeft(5);
    auto muteArea = buttonArea.removeFromLeft(buttonWidth);

    soloButton.setBounds(soloArea);
    muteButton.setBounds(muteArea);
}

void TrackHeader::buttonClicked(juce::Button* button)
{
    if (button == &soloButton)
    {
        // Use the value directly instead of storing in unused variable
        bool newSoloState = soloButton.getToggleState();
        
        // If soloing this track, unsolo all other tracks
        if (newSoloState)
        {
            container.handleSoloChange(trackNumber - 1);
        }
        
        updateVisualState();
        
        if (container.isPlaying())
        {
            double currentPosition = container.getPlayheadPosition();
            container.stop();
            container.setPlayheadPosition(currentPosition);
            container.play();
        }
        
        DBG("Track " + juce::String(trackNumber) + " solo " +
            (newSoloState ? "enabled" : "disabled"));
       }
    else if (button == &muteButton)
    {
        updateVisualState();
        
        if (container.isPlaying())
        {
            double currentPosition = container.getPlayheadPosition();
            container.stop();
            container.setPlayheadPosition(currentPosition);
            container.play();
        }
        
        DBG("Track " + juce::String(trackNumber) + " mute " + 
            (muteButton.getToggleState() ? "enabled" : "disabled"));
    }
   }
    
    void TrackHeader::sliderValueChanged(juce::Slider* slider)
    {
        if (slider == &bpmSlider)
        {
            updateBPMFromSlider();
        }
    }

void TrackHeader::labelTextChanged(juce::Label* label)
{
    if (label == &trackNameLabel)
    {
        finishNameEditing();
    }
}

void TrackHeader::setMuted(bool muted)
{
    muteButton.setToggleState(muted, juce::dontSendNotification);
    updateVisualState();
}

void TrackHeader::setSoloed(bool soloed)
{
    soloButton.setToggleState(soloed, juce::dontSendNotification);
    updateVisualState();
}

void TrackHeader::setTrackBPM(double bpm)
{
    bpm = juce::jlimit(40.0, 400.0, bpm);
    syncBPMControls(bpm);
    
    // ✅ CRITICAL FIX: Update MidiProcessor in real-time if playing
    if (container.isPlaying())
    {
        processor.midiProcessor.updateTrackBPM(trackNumber, bpm);
        DBG("Updated track BPM in MidiProcessor: Track " + juce::String(trackNumber) + 
            " = " + juce::String(bpm, 2) + " BPM");
    }
}

void TrackHeader::setTrackName(const juce::String& name)
{
    trackNameLabel.setText(name, juce::dontSendNotification);
}

void TrackHeader::showContextMenu()
{
    juce::PopupMenu menu;
    
    menu.addItem(1, "Reset BPM to 120");
    menu.addItem(2, "Rename Track");
    menu.addSeparator();
    menu.addItem(3, "Clear All Clips");
    
    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result) {
            switch (result)
            {
                case 1:
                    setTrackBPM(120.0);
                    break;
                case 2:
                    trackNameLabel.showEditor();
                    break;
                case 3:
                    // Clear clips on this track
                    break;
            }
        });
}

void TrackHeader::updateBPMFromSlider()
{
    double bpm = bpmSlider.getValue();
    bpmTextBox.setText(juce::String(static_cast<int>(bpm)), juce::dontSendNotification);
    container.onTrackBPMChanged();
    
    if (container.isPlaying())
    {
        processor.midiProcessor.updateTrackBPM(trackNumber, bpm);
    }
}

void TrackHeader::updateBPMFromTextBox()
{
    int bpm = bpmTextBox.getText().getIntValue();
    if (bpm >= 40 && bpm <= 400)
    {
        bpmSlider.setValue(static_cast<double>(bpm), juce::dontSendNotification);
        container.onTrackBPMChanged();
        
        if (container.isPlaying())
        {
            processor.midiProcessor.updateTrackBPM(trackNumber, static_cast<double>(bpm));
        }
    }
}

void TrackHeader::syncBPMControls(double bpm)
{
    bpmSlider.setValue(bpm, juce::dontSendNotification);
    bpmTextBox.setText(juce::String(static_cast<int>(bpm)), juce::dontSendNotification);
    container.onTrackBPMChanged();
}

void TrackHeader::startNameEditing()
{
    isEditingName = true;
}

void TrackHeader::finishNameEditing()
{
    isEditingName = false;
    // Notify container of name change if needed
}

void TrackHeader::updateVisualState()
{
    // Just repaint - the paint() method will use the correct colors
    repaint();
}

void TrackHeader::setSelected(bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void TrackHeader::mouseDown(const juce::MouseEvent& e)
{
    DBG("TrackHeader::mouseDown called for track " + juce::String(trackNumber) + 
        " at position (" + juce::String(e.x) + ", " + juce::String(e.y) + ")");
    
    // Only handle left clicks
    if (!e.mods.isLeftButtonDown())
    {
        DBG("Not a left click, ignoring");
        return;
    }
    
    // Check if click is on an interactive control (BPM slider/textbox, solo, mute buttons)
    // Note: We allow clicking on track name label for selection
    if (bpmSlider.getBounds().contains(e.getPosition()))
    {
        DBG("Click on BPM slider, letting control handle it");
        return;
    }
    if (bpmTextBox.getBounds().contains(e.getPosition()))
    {
        DBG("Click on BPM textbox, letting control handle it");
        return;
    }
    if (soloButton.getBounds().contains(e.getPosition()))
    {
        DBG("Click on solo button, letting control handle it");
        return;
    }
    if (muteButton.getBounds().contains(e.getPosition()))
    {
        DBG("Click on mute button, letting control handle it");
        return;
    }
    
    // Determine selection mode based on modifiers and current state
    bool multiSelect = e.mods.isShiftDown();
    bool toggleMode = selected && !multiSelect;  // Toggle if clicking on already selected track (without Shift)
    
    DBG("Click in selectable area, selecting track " + juce::String(trackNumber) + 
        ", multiSelect: " + (multiSelect ? "true" : "false") +
        ", toggleMode: " + (toggleMode ? "true" : "false") +
        ", currently selected: " + (selected ? "true" : "false"));
    
    // Select this track through the container with appropriate mode
    // This includes clicks on: track name, empty space, BPM label, or any non-interactive area
    container.selectTrack(trackNumber - 1, multiSelect, toggleMode);  // trackNumber is 1-based, index is 0-based
}
