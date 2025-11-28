#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Forward declarations
class DrumGrooveProcessor;
class MultiTrackContainer;

/**
 * Note division options for track grid snapping
 */
enum class NoteDivision
{
    Quarter = 4,      // 1/4 notes
    Eighth = 8,       // 1/8 notes
    Sixteenth = 16,   // 1/16 notes
    ThirtySecond = 32, // 1/32 notes
    OneTwentyEighth = 128 // 1/128 notes
};

class TrackHeader : public juce::Component,
                    public juce::Button::Listener,
                    public juce::Slider::Listener,
                    public juce::Label::Listener,
                    public juce::ComboBox::Listener
{
public:
    TrackHeader(DrumGrooveProcessor& processor, MultiTrackContainer& container, int trackNumber);
    ~TrackHeader() override;
	
    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void labelTextChanged(juce::Label* label) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    // Track state
    bool isMuted() const { return muteButton.getToggleState(); }
    bool isSoloed() const { return soloButton.getToggleState(); }
    double getTrackBPM() const { return bpmSlider.getValue(); }
    juce::String getTrackName() const { return trackNameLabel.getText(); }
    bool isSelected() const { return selected; }
    NoteDivision getNoteDivision() const { return currentNoteDivision; }

    void setMuted(bool muted);
    void setSoloed(bool soloed);
    void setTrackBPM(double bpm);
    void setTrackName(const juce::String& name);
    void setSelected(bool shouldBeSelected);
    void setNoteDivision(NoteDivision division);

    // Visual scaling factor for timeline
    double getVisualScaleFactor() const { return 120.0 / getTrackBPM(); }

    // Enable/disable BPM controls (for Bar mode)
    void setBPMEnabled(bool enabled); 

    // Enable/disable DIV controls (for Bar mode) - ADD THIS
    void setDivisionEnabled(bool enabled);

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

    // Note division selector
    juce::Label divisionLabel;
    juce::ComboBox divisionCombo;
    NoteDivision currentNoteDivision = NoteDivision::Quarter; // Default 1/4

    // BPM controls (more compact)
    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    // Solo/Mute/Reset buttons (compact layout)
    juce::TextButton soloButton;
    juce::TextButton muteButton;
    juce::TextButton resetButton;  // With dropdown for reset options

    // Visual feedback
    juce::Colour normalBackgroundColour;
    juce::Colour mutedBackgroundColour;
    
    // Selection state
    bool selected = false;

    void updateBPMFromSlider();
    void syncBPMControls(double bpm);
    void startNameEditing();
    void finishNameEditing();
    void updateVisualState();
    void setupDivisionCombo();
    void handleResetButtonClick();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};
