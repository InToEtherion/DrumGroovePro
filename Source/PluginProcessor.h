#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include "Core/MidiProcessor.h"
#include "Core/DrumLibraryManager.h"
#include "Core/FavoritesManager.h"
#include "SectionManager.h"
#include "DrumMixer.h"
#include "SampleEngine.h"

// Forward declarations
class MultiTrackContainer;
class SamplesManagerWindow;
class AudioTrack;

class DrumGrooveProcessor : public juce::AudioProcessor,
public juce::ValueTree::Listener,
    public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        DrumGrooveProcessor();
        ~DrumGrooveProcessor() override;

        FavoritesManager favoritesManager;
        SectionManager sectionManager;

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

        // Audio engine components
        DrumMixer drumMixer;
        SampleEngine sampleEngine;

        // Access to components
        DrumMixer& getDrumMixer() { return drumMixer; }
        SampleEngine& getSampleEngine() { return sampleEngine; }

        // Audio mode control
        bool isAudioMode() const { return parameters.getRawParameterValue("audioMode")->load() > 0.5f; }
        void setAudioMode(bool enabled);
        bool areSamplesLoaded() const { return sampleEngine.isLoaded(); }

        // Sample loading
        bool loadDrumSamples();
        juce::String getSamplesStatus() const { return sampleEngine.getStatusText(); }

        // Trigger a preview note (will be routed through normal audio/MIDI path)
        void triggerPreviewNote(int midiNoteNumber, int velocity = 100);

        // Get current effective BPM (either host or manual based on syncToHost)
        double getCurrentEffectiveBPM() const
        {
            bool syncToHost = parameters.getRawParameterValue("syncToHost")->load() > 0.5f;
            if (syncToHost)
            {
                return getHostBPM();
            }
            else
            {
                auto* param = parameters.getRawParameterValue("manualBPM");
                return param ? param->load() : 120.0;
            }
        }

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
            return static_cast<DrumLibrary>(libraryIndex + 1);
        }

        void setTargetLibrary(DrumLibrary library)
        {
            if (auto* param = parameters.getRawParameterValue("targetLibrary"))
            {
                *param = static_cast<float>(static_cast<int>(library) - 1);
            }
        }

        // Access to GrooveBrowser through editor
        class GrooveBrowser* getGrooveBrowser() const;

        // Visual latency offset access (in milliseconds)
        float getVisualLatencyOffset() const
        {
            return static_cast<float>(midiProcessor.getVisualLatencyOffsetMs());
        }

        void setVisualLatencyOffset(float milliseconds)
        {
            milliseconds = juce::jlimit(-200.0f, 0.0f, milliseconds);

            if (auto* param = parameters.getRawParameterValue("visualLatencyOffset"))
            {
                param->store(milliseconds);
            }

            midiProcessor.setVisualLatencyOffset(milliseconds);

            // Save to global settings so it persists across sessions
            saveGlobalSettings();
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

        // Audio track playback support
        void registerAudioTrack(class AudioTrack* track);
        void unregisterAudioTrack(class AudioTrack* track);
        void clearAudioTracks();
        bool hasAudioTracks() const;

        // Get current playhead position for audio tracks
        double getInternalPlayheadPosition() const { return midiProcessor.getPlayheadPosition(); }
        double getCurrentSampleRate() const { return currentSampleRate; }

        // GUI State Management
        struct GuiState
        {
            juce::String currentBrowserFolder;
            juce::StringArray browserNavigationPath;
            juce::String selectedFile;
            int editorWidth = 1500;
            int editorHeight = 900;
            int editorX = -1;
            int editorY = -1;
        };

        GuiState getGuiState() const;
        void setGuiState(const GuiState& state);

        void saveCompleteGuiState();
        void saveCompleteGuiState(class MultiTrackContainer* container);
        void restoreCompleteGuiState();
        void updateLibraryNamesCache();
        juce::ValueTree getCompleteGuiStateTree() const { return guiStateTree; }
        void setCompleteGuiStateTree(const juce::ValueTree& state) { guiStateTree = state; }

        void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                      const juce::Identifier& property) override;

                                      juce::ValueTree& getGuiStateTree() { return guiStateTree; }
                                      const juce::ValueTree& getGuiStateTree() const { return guiStateTree; }

        // Browser state persistence
        void saveBrowserState(const juce::File& currentFolder,
                              const juce::Array<juce::File>& navPath,
                              const juce::File& selectedFile);
        void restoreBrowserState(juce::File& currentFolder,
                                 juce::Array<juce::File>& navPath,
                                 juce::File& selectedFile) const;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
        std::atomic<bool> isBeingDeleted{false};
        void parameterChanged(const juce::String& parameterID, float newValue) override;

        // Global settings persistence (persists across sessions, not just DAW projects)
        juce::File getGlobalSettingsFile() const;
        void loadGlobalSettings();
        void saveGlobalSettings();

        juce::ValueTree guiStateTree { "GuiState" };

        juce::StringArray cachedLibraryNames;
        std::atomic<bool> libraryNamesNeedUpdate{true};

        // Audio buffer for routing
        juce::AudioBuffer<float> audioBuffer;

        // Per-part buffers for proper drum part mixing (7 parts: Kick1, Kick2, Snare, Hi-Hat, Toms, Crash, Rides)
        std::array<juce::AudioBuffer<float>, 7> partBuffers;

        // Audio track playback - THREAD SAFE
        juce::SpinLock audioTrackLock;
        std::vector<AudioTrack*> registeredAudioTracks;
        juce::AudioBuffer<float> audioTrackBuffer;
        double currentSampleRate { 44100.0 };

        juce::MidiBuffer previewMidiBuffer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGrooveProcessor)
    };
