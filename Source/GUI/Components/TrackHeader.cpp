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

    // Note Division Label
    divisionLabel.setText("DIV", juce::dontSendNotification);
    divisionLabel.setFont(lnf.getSmallFont());
    divisionLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    divisionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(divisionLabel);

    // Note Division Combo
    setupDivisionCombo();
    addAndMakeVisible(divisionCombo);

    // BPM label
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setFont(lnf.getSmallFont());
    bpmLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    bpmLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmLabel);

    // BPM slider (more compact)
    bpmSlider.setRange(40.0, 400.0, 1.0);
    double currentHeaderBPM = processor.getCurrentEffectiveBPM();
    bpmSlider.setValue(currentHeaderBPM);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    bpmSlider.addListener(this);
    bpmSlider.setTooltip("Track BPM");
    addAndMakeVisible(bpmSlider);

    // Solo button (compact) - YELLOW when active
    soloButton.setButtonText("S");
    soloButton.setTooltip("Solo Track");
    soloButton.setClickingTogglesState(true);
    soloButton.addListener(this);
    // Set colors: Normal and Active (Yellow like playhead)
    soloButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    soloButton.setColour(juce::TextButton::buttonOnColourId, ColourPalette::yellowPlayhead);
    soloButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    soloButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    addAndMakeVisible(soloButton);

    // Mute button (compact) - RED when active
    muteButton.setButtonText("M");
    muteButton.setTooltip("Mute Track");
    muteButton.setClickingTogglesState(true);
    muteButton.addListener(this);
    // Set colors: Normal and Active (Red)
    muteButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    muteButton.setColour(juce::TextButton::buttonOnColourId, ColourPalette::errorRed);
    muteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    muteButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    addAndMakeVisible(muteButton);

    // Reset button (compact with dropdown)
    resetButton.setButtonText("R");
    resetButton.setTooltip("Reset Options");
    resetButton.addListener(this);
    addAndMakeVisible(resetButton);

    updateVisualState();

    // Initialize DIV field state based on current mode
    bool isBarMode = container.isBarMode();
    setDivisionEnabled(isBarMode);
    setBPMEnabled(!isBarMode);
}

TrackHeader::~TrackHeader() = default;

void TrackHeader::setupDivisionCombo()
{
    divisionCombo.addItem("1/4", static_cast<int>(NoteDivision::Quarter));
    divisionCombo.addItem("1/8", static_cast<int>(NoteDivision::Eighth));
    divisionCombo.addItem("1/16", static_cast<int>(NoteDivision::Sixteenth));
    divisionCombo.addItem("1/32", static_cast<int>(NoteDivision::ThirtySecond));
    divisionCombo.addItem("1/128", static_cast<int>(NoteDivision::OneTwentyEighth));

    divisionCombo.setSelectedId(static_cast<int>(NoteDivision::Sixteenth), juce::dontSendNotification);
    divisionCombo.addListener(this);
    divisionCombo.setTooltip("Note division for grid snapping");
}

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
        // Selected track gets blue background
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
        g.drawRect(getLocalBounds(), 3);

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

    // Row 1: Track name at top
    trackNameLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(2);

    // Row 2: DIV label + combo (shorter) + Mute button
    auto divRow = bounds.removeFromTop(22);
    divisionLabel.setBounds(divRow.removeFromLeft(30));

    // Reserve space for Mute button on the right (22px)
    auto muteArea = divRow.removeFromRight(22);
    divRow.removeFromRight(3); // spacing between combo and button

    // Division combo takes remaining space (shorter now)
    divisionCombo.setBounds(divRow.reduced(2, 0));

    // Mute button on the right
    muteButton.setBounds(muteArea);

    bounds.removeFromTop(2);

    // Row 3: BPM label + slider (shorter) + Solo button
    auto bpmRow = bounds.removeFromTop(22);
    bpmLabel.setBounds(bpmRow.removeFromLeft(30));

    // Reserve space for Solo button on the right (22px)
    auto soloArea = bpmRow.removeFromRight(22);
    bpmRow.removeFromRight(3); // spacing between slider and button

    // BPM slider takes remaining space (shorter now)
    bpmSlider.setBounds(bpmRow.reduced(2, 0));

    // Solo button on the right
    soloButton.setBounds(soloArea);

    // Hide reset button (not needed in this compact layout)
    resetButton.setBounds(0, 0, 0, 0);
}

void TrackHeader::buttonClicked(juce::Button* button)
{
    if (button == &soloButton)
    {
        bool newSoloState = soloButton.getToggleState();

        if (newSoloState)
        {
            container.handleSoloChange(trackNumber - 1);
        }

        updateVisualState();

        // CRITICAL: Repaint all track headers and tracks for immediate visual feedback
        for (int i = 0; i < container.getNumTracks(); ++i)
        {
            if (auto* header = container.getTrackHeader(i))
                header->repaint();
            if (auto* track = container.getTrack(i))
                track->repaint();
        }

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

        // CRITICAL: Repaint all track headers and tracks for immediate visual feedback
        for (int i = 0; i < container.getNumTracks(); ++i)
        {
            if (auto* header = container.getTrackHeader(i))
                header->repaint();
            if (auto* track = container.getTrack(i))
                track->repaint();
        }

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
    else if (button == &resetButton)
    {
        handleResetButtonClick();
    }
}


void TrackHeader::handleResetButtonClick()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Reset BPM to 120");
    menu.addItem(2, "Reset Division to 1/16");
    menu.addItem(3, "Clear All Clips");
    menu.addSeparator();
    menu.addItem(4, "Reset All Settings");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&resetButton),
                       [this](int result)
                       {
                           if (result == 0) return;

                           switch (result)
                           {
                               case 1: // Reset BPM
                                   setTrackBPM(120.0);
                                   break;
                               case 2: // Reset Division
                                   setNoteDivision(NoteDivision::Sixteenth);
                                   break;
                               case 3: // Clear clips
                                   if (auto* track = container.getTrack(trackNumber - 1))
                                   {
                                       track->clearAllClips();
                                   }
                                   break;
                               case 4: // Reset all
                                   setTrackBPM(120.0);
                                   setNoteDivision(NoteDivision::Sixteenth);
                                   setMuted(false);
                                   setSoloed(false);
                                   break;
                           }
                       });
}

void TrackHeader::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &bpmSlider)
    {
        updateBPMFromSlider();
    }
}

void TrackHeader::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &divisionCombo)
    {
        int selectedId = divisionCombo.getSelectedId();
        currentNoteDivision = static_cast<NoteDivision>(selectedId);
        DBG("Track " + juce::String(trackNumber) + " note division changed to " +
        divisionCombo.getText());
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

void TrackHeader::setNoteDivision(NoteDivision division)
{
    currentNoteDivision = division;
    divisionCombo.setSelectedId(static_cast<int>(division), juce::dontSendNotification);
}

void TrackHeader::setBPMEnabled(bool enabled)
{
    bpmSlider.setEnabled(enabled);
    bpmLabel.setEnabled(enabled);
    bpmSlider.setAlpha(enabled ? 1.0f : 0.4f);
    bpmLabel.setAlpha(enabled ? 1.0f : 0.4f);
}

void TrackHeader::setDivisionEnabled(bool enabled)
{
    divisionCombo.setEnabled(enabled);
    divisionLabel.setEnabled(enabled);

    // Visual feedback - gray out when disabled
    float alpha = enabled ? 1.0f : 0.4f;
    divisionCombo.setAlpha(alpha);
    divisionLabel.setAlpha(alpha);
}

void TrackHeader::showContextMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Rename Track");
    menu.addSeparator();
    menu.addItem(2, "Copy Track Settings");
    menu.addItem(3, "Paste Track Settings");
    menu.addSeparator();
    menu.addItem(4, "Clear All Clips", true, false, nullptr);
    menu.addItem(5, "Remove Track", container.getNumTracks() > 1);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [this](int result)
                       {
                           if (result == 0) return;

                           switch (result)
                           {
                               case 1: // Rename
                                   startNameEditing();
                                   break;
                               case 4: // Clear clips
                                   if (auto* track = container.getTrack(trackNumber - 1))
                                   {
                                       track->clearAllClips();
                                   }
                                   break;
                               case 5: // Remove track
                                   container.removeTrack(trackNumber - 1);
                                   break;
                           }
                       });
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
    if (!e.mods.isLeftButtonDown())
        return;

    // Let interactive controls handle their own clicks
    if (bpmSlider.getBounds().contains(e.getPosition()) ||
        divisionCombo.getBounds().contains(e.getPosition()) ||
        soloButton.getBounds().contains(e.getPosition()) ||
        muteButton.getBounds().contains(e.getPosition()) ||
        resetButton.getBounds().contains(e.getPosition()))
    {
        return;
    }

    bool multiSelect = e.mods.isShiftDown();
    bool toggleMode = selected && !multiSelect;

    container.selectTrack(trackNumber - 1, multiSelect, toggleMode);
}

void TrackHeader::updateBPMFromSlider()
{
    double bpm = bpmSlider.getValue();
    bpm = juce::jlimit(40.0, 400.0, bpm);
    syncBPMControls(bpm);

    container.invalidateBarWidthCache();
    container.onTrackBPMChanged();

    if (container.isPlaying())
    {
        processor.midiProcessor.updateTrackBPM(trackNumber, bpm);
    }
}

void TrackHeader::syncBPMControls(double bpm)
{
    bpm = juce::jlimit(40.0, 400.0, bpm);

    bpmSlider.setValue(bpm, juce::dontSendNotification);
}

void TrackHeader::startNameEditing()
{
    isEditingName = true;
    trackNameLabel.showEditor();
}

void TrackHeader::finishNameEditing()
{
    isEditingName = false;
}

void TrackHeader::updateVisualState()
{
    repaint();
}
