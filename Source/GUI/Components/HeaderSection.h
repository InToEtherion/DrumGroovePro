#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Forward declarations
class DrumGrooveProcessor;

class HeaderSection : public juce::Component,
                     public juce::Button::Listener,
                     public juce::ComboBox::Listener,
                     public juce::Slider::Listener
{
public:
    explicit HeaderSection(DrumGrooveProcessor& processor);
    ~HeaderSection() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void sliderValueChanged(juce::Slider* slider) override;
    
    void updateBPMDisplay();
    
private:
    DrumGrooveProcessor& processor;
    
    // BPM Controls
    juce::ToggleButton syncToHostButton;
    juce::Label manualBPMLabel;
    juce::Slider manualBPMSlider;
    juce::Label currentBPMLabel;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
    
    void setupComponents();
    void updateBPMControlsVisibility();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderSection)
};