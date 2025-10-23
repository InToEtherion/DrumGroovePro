#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "GUI/MainComponent.h"

class DrumGrooveEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer,
                         public juce::OpenGLRenderer,
                         private juce::Timer
	{
    public:
        DrumGrooveEditor(DrumGrooveProcessor&);
        ~DrumGrooveEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

        // OpenGL callbacks for hardware acceleration
        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

        // Timer callback for BPM updates
        void timerCallback() override;

        // Component visibility override for state persistence
        void setVisible(bool shouldBeVisible) override;

    private:
        // Core components
        DrumGrooveProcessor& processor;
        juce::OpenGLContext openGLContext;
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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGrooveEditor)
    };
