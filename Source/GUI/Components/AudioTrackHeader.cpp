#include "AudioTrackHeader.h"
#include "AudioTrack.h"
#include "../LookAndFeel/ColourPalette.h"

AudioTrackHeader::AudioTrackHeader(AudioTrack& track)
: audioTrack(track)
{
    // File name label
    nameLabel.setText(audioTrack.getFileName(), juce::dontSendNotification);
    nameLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameLabel);

    // "AUDIO" label (replaces DIV in MIDI)
    audioLabel.setText("AUDIO", juce::dontSendNotification);
    audioLabel.setFont(juce::Font(10.0f));
    audioLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    audioLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(audioLabel);

    // "VOL" label (replaces BPM in MIDI)
    volLabel.setText("VOL", juce::dontSendNotification);
    volLabel.setFont(juce::Font(10.0f));
    volLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    volLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(volLabel);

    // Volume slider (matches BPM slider position)
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(audioTrack.getVolume(), juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    volumeSlider.setColour(juce::Slider::trackColourId, ColourPalette::panelBackground.brighter(0.2f));
    volumeSlider.setColour(juce::Slider::thumbColourId, ColourPalette::primaryBlue);
    volumeSlider.setColour(juce::Slider::textBoxTextColourId, ColourPalette::primaryText);
    volumeSlider.setColour(juce::Slider::textBoxBackgroundColourId, ColourPalette::inputBackground);
    volumeSlider.setColour(juce::Slider::textBoxOutlineColourId, ColourPalette::borderColour);
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    // Mute button (matches M button position in MIDI header)
    muteButton.setButtonText("M");
    muteButton.setClickingTogglesState(true);
    muteButton.setToggleState(audioTrack.isMuted(), juce::dontSendNotification);
    muteButton.setColour(juce::TextButton::buttonColourId, ColourPalette::panelBackground.brighter(0.1f));
    muteButton.setColour(juce::TextButton::buttonOnColourId, ColourPalette::errorRed);
    muteButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    muteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    muteButton.onClick = [this]() {
        bool isMuted = muteButton.getToggleState();
        audioTrack.setMuted(isMuted);
    };
    addAndMakeVisible(muteButton);

    // Remove track button (matches S button position in MIDI header)
    removeButton.setButtonText("R");
    removeButton.setColour(juce::TextButton::buttonColourId, ColourPalette::errorRed);
    removeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.onClick = [this]() {
        if (onRemoveTrack)
            onRemoveTrack();
    };
    addAndMakeVisible(removeButton);
}

AudioTrackHeader::~AudioTrackHeader()
{
    volumeSlider.removeListener(this);
}

void AudioTrackHeader::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider)
    {
        float newVolume = static_cast<float>(volumeSlider.getValue());
        audioTrack.setVolume(newVolume);
    }
}

void AudioTrackHeader::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);

    // Draw border
    g.setColour(ColourPalette::borderColour);
    g.drawRect(getLocalBounds(), 1);
}

void AudioTrackHeader::resized()
{
    auto area = getLocalBounds().reduced(5);

    // Track name label at top
    auto nameArea = area.removeFromTop(20);
    nameLabel.setBounds(nameArea);

    area.removeFromTop(3); // Spacing

    // First row: "AUDIO" label and M button (matches DIV row in MIDI)
    auto firstRow = area.removeFromTop(22);

    // "AUDIO" label on the left
    auto audioLabelArea = firstRow.removeFromLeft(45);
    audioLabel.setBounds(audioLabelArea);

    // Spacing
    firstRow.removeFromLeft(3);

    // M button on the right
    auto mButtonArea = firstRow.removeFromRight(20);
    muteButton.setBounds(mButtonArea);

    area.removeFromTop(3); // Spacing

    // Second row: "VOL" label, volume slider, and Remove button (matches BPM row in MIDI)
    auto secondRow = area.removeFromTop(22);

    // "VOL" label on the left
    auto volLabelArea = secondRow.removeFromLeft(30);
    volLabel.setBounds(volLabelArea);

    // Spacing
    secondRow.removeFromLeft(3);

    // Remove button on the right (matches S button position)
    auto removeButtonArea = secondRow.removeFromRight(20);
    removeButton.setBounds(removeButtonArea);

    // Spacing between slider and button
    secondRow.removeFromRight(3);

    // Volume slider takes remaining space
    volumeSlider.setBounds(secondRow);
}
