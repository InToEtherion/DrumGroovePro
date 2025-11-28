#pragma once

#include <JuceHeader.h>

class AudioTrack;

class AudioTrackHeader : public juce::Component,
public juce::Slider::Listener
{
public:
    explicit AudioTrackHeader(AudioTrack& track);
    ~AudioTrackHeader() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Slider listener
    void sliderValueChanged(juce::Slider* slider) override;

    std::function<void()> onRemoveTrack;

private:
    AudioTrack& audioTrack;

    juce::Label nameLabel;
    juce::Label audioLabel;
    juce::Label volLabel;
    juce::Slider volumeSlider;
    juce::TextButton muteButton;
    juce::TextButton removeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrackHeader)
};
