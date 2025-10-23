#include "HeaderSection.h"
#include "../../PluginProcessor.h"
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

    // Create parameter attachments
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "syncToHost", syncToHostButton);
    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "manualBPM", manualBPMSlider);

    // BPM Controls - WIDER SYNC BUTTON for full text
    syncToHostButton.setButtonText("Sync to Host");
    syncToHostButton.setToggleState(true, juce::dontSendNotification);
    syncToHostButton.addListener(this);
    syncToHostButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    syncToHostButton.setColour(juce::TextButton::textColourOnId, ColourPalette::primaryText);
    syncToHostButton.setTooltip("Sync to Host BPM");
    addAndMakeVisible(syncToHostButton);

    manualBPMLabel.setText("Manual BPM:", juce::dontSendNotification);
    manualBPMLabel.setFont(lnf.getSmallFont());
    addAndMakeVisible(manualBPMLabel);

    manualBPMSlider.setRange(60.0, 400.0, 1.0);
    manualBPMSlider.setValue(120.0);
    manualBPMSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    manualBPMSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 18);
    manualBPMSlider.setTextValueSuffix(" BPM");
    addAndMakeVisible(manualBPMSlider);

    currentBPMLabel.setText("Current: Host 120 BPM", juce::dontSendNotification);
    currentBPMLabel.setFont(lnf.getSmallFont());
    currentBPMLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(currentBPMLabel);

    updateBPMControlsVisibility();
}

void HeaderSection::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);
}

void HeaderSection::resized()
{
    auto bounds = getLocalBounds().reduced(10, 5);

    // SMALLER sync button - reduced from 90px to 75px width and from 20px to 18px height
    syncToHostButton.setBounds(bounds.removeFromLeft(75).withHeight(18));
    bounds.removeFromLeft(8);  // Slightly more spacing

    manualBPMLabel.setBounds(bounds.removeFromLeft(70).withHeight(25));
    manualBPMSlider.setBounds(bounds.removeFromLeft(100).withHeight(30));
    bounds.removeFromLeft(10);
    currentBPMLabel.setBounds(bounds.removeFromLeft(150).withHeight(25));
}

void HeaderSection::buttonClicked(juce::Button* button)
{
    if (button == &syncToHostButton)
    {
        updateBPMControlsVisibility();
        
        if (processor.midiProcessor.isPlaying())
        {
            bool syncToHost = syncToHostButton.getToggleState();
            double newBPM = syncToHost ? processor.getHostBPM() : manualBPMSlider.getValue();
            processor.midiProcessor.updateTrackBPM(0, newBPM);
        }
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
        
        if (processor.midiProcessor.isPlaying())
        {
            double newBPM = manualBPMSlider.getValue();
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
