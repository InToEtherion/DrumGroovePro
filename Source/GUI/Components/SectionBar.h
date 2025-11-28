#pragma once

#include <JuceHeader.h>
#include "SectionManager.h"

class DrumGrooveProcessor;

/**
 * UI component for displaying and editing musical sections
 * IMPROVED LAYOUT - Better proportions and cleaner appearance
 */
class SectionBar : public juce::Component,
                   public juce::ChangeBroadcaster,
                   private juce::ChangeListener
{
public:
    SectionBar(DrumGrooveProcessor& proc, SectionManager& sectionMgr);
    ~SectionBar() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Section selection
    void selectSection(int sectionIndex);
    int getSelectedSectionIndex() const { return selectedSectionIndex; }
	
	// Enable/disable section bar (for Time/Bar mode)
    void setSectionBarEnabled(bool enabled);
    
	// Check if Loop Section is enabled
	bool isLoopSectionEnabled() const { return loopSectionButton && loopSectionButton->getToggleState(); }

private:
    DrumGrooveProcessor& processor;
    SectionManager& sectionManager;
    
    int selectedSectionIndex = 0;
	bool isEnabled = true; // Track enabled state
    
    // UI Components - Reorganized for better layout
    std::unique_ptr<juce::TextButton> prevSectionButton;
    std::unique_ptr<juce::TextButton> nextSectionButton;
    std::unique_ptr<juce::Label> sectionInfoLabel;
    
    std::unique_ptr<juce::Label> timeSigLabel;
    std::unique_ptr<juce::ComboBox> timeSigCombo;
    
    std::unique_ptr<juce::Label> barsLabel;
    std::unique_ptr<juce::Label> barsValueLabel;  // ADDED - displays current bars value
    std::unique_ptr<juce::Slider> barsSlider;
    
    std::unique_ptr<juce::Label> bpmLabel;
    std::unique_ptr<juce::Label> bpmValueLabel;  // ADDED - displays current BPM value
    std::unique_ptr<juce::Slider> bpmSlider;
    std::unique_ptr<juce::ToggleButton> useGlobalBPMButton;
	
    
    std::unique_ptr<juce::Label> nameLabel;
    std::unique_ptr<juce::Label> sectionNameLabel;  // ADDED - "NAME:" label
    std::unique_ptr<juce::TextEditor> sectionNameEditor;
    
    std::unique_ptr<juce::TextButton> addSectionButton;
    std::unique_ptr<juce::TextButton> removeSectionButton;
    
	std::unique_ptr<juce::Label> playbackBPMLabel;
	std::unique_ptr<juce::Label> playbackBPMValueLabel;
	std::unique_ptr<juce::Slider> playbackBPMSlider;
	std::unique_ptr<juce::ToggleButton> useGlobalPlaybackBPMButton;
	std::unique_ptr<juce::ToggleButton> loopSectionButton;

    void setupComponents();
    void updateComponentsForSelectedSection();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    void addSection();
    void removeSection();
    void updateSelectedSection();
    void navigateToPrevSection();
    void navigateToNextSection();
	
	void enableSectionLoop();
	void disableSectionLoop();
    
    // Common time signatures
    struct TimeSig
    {
        int numerator;
        int denominator;
        juce::String display;
    };
    
    static const std::vector<TimeSig>& getCommonTimeSignatures();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SectionBar)
};