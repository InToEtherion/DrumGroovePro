#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "GUI/MainComponent.h"

// Forward declaration
class MidiEditorComponent;

class DrumGrooveEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer,
                         private juce::Timer
	{
    public:
        DrumGrooveEditor(DrumGrooveProcessor&);
        ~DrumGrooveEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

        // Timer callback for BPM updates
        void timerCallback() override;

        // Component visibility override for state persistence
        void setVisible(bool shouldBeVisible) override;
        
        // Override focus methods to prevent playback stopping when focus is lost
        void focusLost(FocusChangeType cause) override;
        void focusGained(FocusChangeType cause) override;
        
        // Override visibility to prevent playback stopping when minimized
        void visibilityChanged() override;
		
		MainComponent* getMainComponent() const { return mainComponent.get(); }

		void openMidiEditor(const juce::File& midiFile, DrumLibrary sourceLib);
        void createNewMidiGroove();
	

    private:
        // Core components
        DrumGrooveProcessor& processor;
        std::unique_ptr<MainComponent> mainComponent;
		
        // Simplified state management
        struct EditorState
        {
            int x = -1, y = -1;
            int width = 1300, height = 900; 
               float zoomLevel = 1.0f;
               int horizontalScrollPos = 0;
               int verticalScrollPos = 0;
               juce::XmlElement* createXml() const;
               void restoreFromXml(const juce::XmlElement& xml);
            MainComponent::GuiState guiState;
        } currentEditorState;

        // Simple flags
        bool isResizing = false;

        // Simplified state management
        void saveStateToProcessor();
        void saveGuiState();
        void restoreGuiState();

        // Window management helpers
        bool isPositionOnScreen(int x, int y) const;
        juce::Rectangle<int> getValidWindowBounds(int x, int y, int width, int height) const;

        juce::OwnedArray<MidiEditorComponent> activeEditors;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGrooveEditor)
    };
