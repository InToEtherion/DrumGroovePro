#include "HeaderSection.h"
#include "../../PluginProcessor.h"
#include "../../PluginEditor.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"

HeaderSection::HeaderSection(DrumGrooveProcessor& p)
: processor(p)
{
    setupComponents();
}

HeaderSection::~HeaderSection()
{
    syncAttachment.reset();
    bpmAttachment.reset();
}

void HeaderSection::setupComponents()
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    // Create parameter attachments with safety checks
    if (processor.parameters.getParameter("syncToHost") != nullptr)
    {
        syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "syncToHost", syncToHostButton);
    }
    else
    {
        DBG("ERROR: syncToHost parameter not found!");
    }

    if (processor.parameters.getParameter("manualBPM") != nullptr)
    {
        bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, "manualBPM", manualBPMSlider);
    }
    else
    {
        DBG("ERROR: manualBPM parameter not found - using manual callback");
        // Fallback: manually sync slider changes
        manualBPMSlider.onValueChange = [this]() {
            // Direct sync without attachment
        };
    }

    // BPM Controls - WIDER SYNC BUTTON for full text
    syncToHostButton.setButtonText("Sync to Host");
    syncToHostButton.setToggleState(true, juce::dontSendNotification);
    syncToHostButton.addListener(this);
    syncToHostButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    syncToHostButton.setColour(juce::TextButton::textColourOnId, ColourPalette::primaryText);
    syncToHostButton.setTooltip("Sync to Host BPM");
    addAndMakeVisible(syncToHostButton);

    // Bypass Track BPM Sync checkbox
    bypassTrackBPMSyncButton.setButtonText("Bypass Track BPM Sync");
    bypassTrackBPMSyncButton.setToggleState(false, juce::dontSendNotification);
    bypassTrackBPMSyncButton.addListener(this);
    bypassTrackBPMSyncButton.setTooltip("When enabled, tracks default to 120 BPM and don't sync with Header BPM changes");
    addAndMakeVisible(bypassTrackBPMSyncButton);

    manualBPMLabel.setText("Manual BPM:", juce::dontSendNotification);
    manualBPMLabel.setFont(lnf.getSmallFont());
    addAndMakeVisible(manualBPMLabel);

    manualBPMSlider.setRange(60.0, 300.0, 1.0);
    manualBPMSlider.setValue(120.0);
    manualBPMSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    manualBPMSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 18);
    manualBPMSlider.setTextValueSuffix(" BPM");
    manualBPMSlider.addListener(this);
    addAndMakeVisible(manualBPMSlider);

    currentBPMLabel.setText("Current: Host 120 BPM", juce::dontSendNotification);
    currentBPMLabel.setFont(lnf.getSmallFont());
    currentBPMLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(currentBPMLabel);

    createNewGrooveButton.setButtonText("Create New");
    createNewGrooveButton.onClick = [this]()
    {
        auto* editor = findParentComponentOfClass<DrumGrooveEditor>();  // <-- CORRECT!
        if (editor)
        {
            editor->createNewMidiGroove();
        }
    };
    addAndMakeVisible(createNewGrooveButton);

    // Batch Remap button
    batchRemapButton.setButtonText("Batch Remap");
    batchRemapButton.setTooltip("Batch remap MIDI files from one drum library to another");
    batchRemapButton.onClick = [this]() {
        auto* dialog = new BatchRemapDialog(processor.drumLibraryManager);
        dialog->onDialogClosed = [dialog]() {
            delete dialog;
        };
    };
    addAndMakeVisible(batchRemapButton);

    updateBPMControlsVisibility();
}

void HeaderSection::paint(juce::Graphics& g)
{
    // Make header section transparent - don't fill with any color
    // Background image from MainComponent will show through;
}

void HeaderSection::resized()
{
    auto bounds = getLocalBounds().reduced(10, 5);

    // Sync to Host button
    syncToHostButton.setBounds(bounds.removeFromLeft(75).withHeight(18));
    bounds.removeFromLeft(8);

    // Bypass checkbox - right after Sync to Host, smaller width
    bypassTrackBPMSyncButton.setBounds(bounds.removeFromLeft(115).withHeight(18));
    bounds.removeFromLeft(10);

    // Manual BPM controls
    manualBPMLabel.setBounds(bounds.removeFromLeft(70).withHeight(25));
    manualBPMSlider.setBounds(bounds.removeFromLeft(100).withHeight(30));
    bounds.removeFromLeft(10);

    // Current BPM label
    currentBPMLabel.setBounds(bounds.removeFromLeft(150).withHeight(25));

    // Buttons
    bounds.removeFromLeft(20);
    createNewGrooveButton.setBounds(bounds.removeFromLeft(100).withHeight(25));
    bounds.removeFromLeft(10);
    batchRemapButton.setBounds(bounds.removeFromLeft(120).withHeight(25));
}

void HeaderSection::buttonClicked(juce::Button* button)
{
    if (button == &syncToHostButton || button == &bypassTrackBPMSyncButton)
    {
        if (button == &syncToHostButton)
        {
            updateBPMControlsVisibility();

            // Calculate new BPM
            bool syncToHost = syncToHostButton.getToggleState();
            double newBPM = syncToHost ? processor.getHostBPM() : manualBPMSlider.getValue();

            // ALWAYS notify about BPM change (whether playing or not)
            if (onBPMChanged)
                onBPMChanged(newBPM);

            // Update MIDI processor only during playback
            if (processor.midiProcessor.isPlaying())
            {
                processor.midiProcessor.updateTrackBPM(0, newBPM);
            }
        }

        // Update track BPM control states whenever either button changes
        updateTrackBPMControlsState();
    }
}

void HeaderSection::comboBoxChanged(juce::ComboBox* comboBox)
{
    // No combo box in HeaderSection anymore
}

void HeaderSection::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &manualBPMSlider)
    {
        updateBPMDisplay();

        // Calculate new BPM
        double newBPM = manualBPMSlider.getValue();

        // ALWAYS notify about BPM change (whether playing or not)
        if (onBPMChanged)
            onBPMChanged(newBPM);

        // Update MIDI processor only during playback
        if (processor.midiProcessor.isPlaying())
        {
            processor.midiProcessor.updateTrackBPM(0, newBPM);
        }
    }
}

void HeaderSection::updateBPMControlsVisibility()
{
    bool syncToHost = syncToHostButton.getToggleState();
    manualBPMLabel.setVisible(!syncToHost);
    manualBPMSlider.setVisible(!syncToHost);

    updateBPMDisplay();
}

void HeaderSection::updateBPMDisplay()
{
    bool syncToHost = syncToHostButton.getToggleState();

    if (syncToHost)
    {
        double hostBPM = processor.getHostBPM();
        currentBPMLabel.setText("Current: Host " + juce::String(hostBPM, 1) + " BPM",
                                juce::dontSendNotification);
    }
    else
    {
        double manualBPM = manualBPMSlider.getValue();
        currentBPMLabel.setText("Current: Manual " + juce::String(manualBPM, 1) + " BPM",
                                juce::dontSendNotification);
    }
}

void HeaderSection::updateTrackBPMControlsState()
{
    // Logic: Track BPM controls should be enabled UNLESS syncToHost is checked AND bypass is unchecked
    // In other words:
    // - If syncToHost = false (unchecked) → Enable track BPM (regardless of bypass)
    // - If syncToHost = true AND bypass = false → Disable track BPM (grayed out)
    // - If syncToHost = true AND bypass = true → Enable track BPM

    bool syncToHost = syncToHostButton.getToggleState();
    bool bypassEnabled = bypassTrackBPMSyncButton.getToggleState();

    bool shouldEnableTrackBPM = !syncToHost || bypassEnabled;

    // Notify via callback
    if (onTrackBPMControlStateChanged)
    {
        onTrackBPMControlStateChanged(shouldEnableTrackBPM);
    }
}
