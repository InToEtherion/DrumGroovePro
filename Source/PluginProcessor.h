#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MidiProcessor.h"
#include "Core/DrumLibraryManager.h"
#include "Core/FavoritesManager.h"

// Forward declaration
class MultiTrackContainer;

class DrumGrooveProcessor : public juce::AudioProcessor,
public juce::ValueTree::Listener
{
public:
    DrumGrooveProcessor();
    ~DrumGrooveProcessor() override;
	
	FavoritesManager favoritesManager;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Public access to core components
    juce::AudioProcessorValueTreeState parameters;
    DrumLibraryManager drumLibraryManager;
    MidiProcessor midiProcessor;

    // BPM access methods
    double getHostBPM() const
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
                if (position->getBpm().hasValue())
                    return *position->getBpm();
        }
        return 120.0;
    }

    bool isHostPlaying() const
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
                return position->getIsPlaying();
        }
        return false;
    }

    // Parameter access methods
    bool isTrackMuted() const
    {
        return parameters.getRawParameterValue("trackMute")->load() > 0.5f;
    }

    void setTrackMuted(bool muted)
    {
        if (auto* param = parameters.getRawParameterValue("trackMute"))
        {
            *param = muted ? 1.0f : 0.0f;
        }
    }

    bool isTrackSoloed() const
    {
        return parameters.getRawParameterValue("trackSolo")->load() > 0.5f;
    }

    void setTrackSoloed(bool soloed)
    {
        if (auto* param = parameters.getRawParameterValue("trackSolo"))
        {
            *param = soloed ? 1.0f : 0.0f;
        }
    }

    // Target library access
    DrumLibrary getTargetLibrary() const
    {
        int libraryIndex = static_cast<int>(parameters.getRawParameterValue("targetLibrary")->load());
        return static_cast<DrumLibrary>(libraryIndex + 1); // +1 because enum starts at 1
    }

    void setTargetLibrary(DrumLibrary library)
    {
        if (auto* param = parameters.getRawParameterValue("targetLibrary"))
        {
            *param = static_cast<float>(static_cast<int>(library) - 1); // -1 because enum starts at 1
        }
    }

    // Sync settings access
    bool isSyncToHost() const
    {
        return parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
    }

    void setSyncToHost(bool sync)
    {
        if (auto* param = parameters.getRawParameterValue("syncToHost"))
        {
            *param = sync ? 1.0f : 0.0f;
        }
    }

    // GUI State Management - NEW APPROACH
    struct GuiState
    {
        juce::String currentBrowserFolder;
        juce::StringArray browserNavigationPath;
        juce::String selectedFile;
        int editorWidth = 1300;
        int editorHeight = 900;
        int editorX = -1;
        int editorY = -1;
    };

    // GUI state access methods
    GuiState getGuiState() const;
    void setGuiState(const GuiState& state);
    
    // Complete GUI state persistence - production requirement
    void saveCompleteGuiState();
    void saveCompleteGuiState(class MultiTrackContainer* container);
    void restoreCompleteGuiState();
    juce::ValueTree getCompleteGuiStateTree() const { return guiStateTree; }
    void setCompleteGuiStateTree(const juce::ValueTree& state) { guiStateTree = state; }

    // ValueTree::Listener override for state changes
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                  const juce::Identifier& property) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Internal GUI state storage in ValueTree
    juce::ValueTree guiStateTree { "GuiState" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGrooveProcessor)
};
