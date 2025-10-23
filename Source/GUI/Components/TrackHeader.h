#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Forward declarations
class DrumGrooveProcessor;
class MultiTrackContainer;

class TrackHeader : public juce::Component,
                    public juce::Button::Listener,
                    public juce::Slider::Listener,
                    public juce::Label::Listener
{
public:
    TrackHeader(DrumGrooveProcessor& processor, MultiTrackContainer& container, int trackNumber);
    ~TrackHeader() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void labelTextChanged(juce::Label* label) override;

    // Track state
    bool isMuted() const { return muteButton.getToggleState(); }
    bool isSoloed() const { return soloButton.getToggleState(); }
    double getTrackBPM() const { return bpmSlider.getValue(); }
    juce::String getTrackName() const { return trackNameLabel.getText(); }
    bool isSelected() const { return selected; }

    void setMuted(bool muted);
    void setSoloed(bool soloed);
    void setTrackBPM(double bpm);
    void setTrackName(const juce::String& name);
    void setSelected(bool shouldBeSelected);

    // Visual scaling factor for timeline
    double getVisualScaleFactor() const { return 120.0 / getTrackBPM(); }

    // Context menu
    void showContextMenu();
    
    // Mouse handling for selection
    void mouseDown(const juce::MouseEvent& e) override;

private:
    DrumGrooveProcessor& processor;
    MultiTrackContainer& container;
    int trackNumber;  // Store track number for reference

    // Track name
    juce::Label trackNameLabel;
    bool isEditingName = false;

    // BPM controls
    juce::Label bpmLabel;
    juce::Slider bpmSlider;
    juce::TextEditor bpmTextBox;

    // Solo/Mute buttons
    juce::ToggleButton soloButton;
    juce::ToggleButton muteButton;

    // Visual feedback
    juce::Colour normalBackgroundColour;
    juce::Colour mutedBackgroundColour;
    
    // Selection state
    bool selected = false;

    void updateBPMFromSlider();
    void updateBPMFromTextBox();
    void syncBPMControls(double bpm);
    void startNameEditing();
    void finishNameEditing();
    void updateVisualState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};