#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/MainComponent.h"
#include "GUI/Components/MultiTrackContainer.h"
#include "GUI/Components/GrooveBrowser.h"
#include "GUI/Components/AudioTrack.h"

DrumGrooveProcessor::DrumGrooveProcessor()
: AudioProcessor(BusesProperties()
.withInput("Input", juce::AudioChannelSet::stereo(), true)
.withOutput("Output", juce::AudioChannelSet::stereo(), true)
),
parameters(*this, nullptr, juce::Identifier("DrumGrooveProParams"), createParameterLayout()),
midiProcessor(drumLibraryManager)
{
    // CRITICAL: Conditional loading to handle both Linux scanning and Windows runtime
    // Linux Reaper scanner doesn't run message loop, so we need sync loading
    // Windows DAWs need async to prevent "Not Responding" freeze
    #if JUCE_LINUX
    // On Linux, load synchronously for Reaper validator compatibility
    drumLibraryManager.loadConfiguration();
    #else
    // On Windows/Mac, load asynchronously to prevent freezing
    juce::MessageManager::callAsync([this]()
    {
        drumLibraryManager.loadConfiguration();
    });
    #endif

    // Initialize GUI state tree with default values
    guiStateTree.setProperty("currentBrowserFolder", "", nullptr);
    guiStateTree.setProperty("selectedFile", "", nullptr);
    guiStateTree.setProperty("editorWidth", 1500, nullptr);
    guiStateTree.setProperty("editorHeight", 900, nullptr);
    guiStateTree.setProperty("editorX", -1, nullptr);
    guiStateTree.setProperty("editorY", -1, nullptr);

    // Add GUI state to main parameters tree so it gets saved/loaded automatically
    parameters.state.addChild(guiStateTree, -1, nullptr);

    // Listen for changes to the ValueTree
    parameters.state.addListener(this);

    // Listen for parameter changes (including when state is restored)
    parameters.addParameterListener("visualLatencyOffset", this);

    // Load global settings (includes visual latency offset) from persistent storage
    // This runs BEFORE any DAW project state is restored
    loadGlobalSettings();

    // Initialize Global BPM with current effective BPM (for BAR mode)
    sectionManager.setGlobalBPM(getCurrentEffectiveBPM());

    cachedLibraryNames.clear();
    libraryNamesNeedUpdate.store(true);
}

DrumGrooveProcessor::~DrumGrooveProcessor()
{
    isBeingDeleted.store(true);
    parameters.removeParameterListener("visualLatencyOffset", this);
    parameters.state.removeListener(this);
    drumLibraryManager.saveConfiguration();
}

void DrumGrooveProcessor::updateLibraryNamesCache()
{
    cachedLibraryNames = drumLibraryManager.getLoadedLibraryNames();
    libraryNamesNeedUpdate.store(false);
}

const juce::String DrumGrooveProcessor::getName() const
{
    return JucePlugin_Name;
}


bool DrumGrooveProcessor::acceptsMidi() const
{
    return true;
}

bool DrumGrooveProcessor::producesMidi() const
{
    return true;
}

bool DrumGrooveProcessor::isMidiEffect() const
{
    return true;
}

double DrumGrooveProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DrumGrooveProcessor::getNumPrograms()
{
    return 1;
}

int DrumGrooveProcessor::getCurrentProgram()
{
    return 0;
}

void DrumGrooveProcessor::setCurrentProgram(int /*index*/)
{
    // No programs to set
}

const juce::String DrumGrooveProcessor::getProgramName(int /*index*/)
{
    return {};
}

void DrumGrooveProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
    // No programs to rename
}

void DrumGrooveProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    midiProcessor.prepareToPlay(sampleRate, samplesPerBlock);
    drumMixer.prepareToPlay(sampleRate, samplesPerBlock);
    sampleEngine.prepareToPlay(sampleRate, samplesPerBlock);

    // CRITICAL FIX: Pre-allocate buffers with double size for safety
    // Never call setSize() in processBlock() - causes audio glitches
    int maxBufferSize = samplesPerBlock * 2;
    audioBuffer.setSize(2, maxBufferSize);
    audioTrackBuffer.setSize(2, maxBufferSize);

    // Initialize per-part buffers
    for (auto& buffer : partBuffers)
    {
        buffer.setSize(2, maxBufferSize);
    }

    // Prepare registered audio tracks
    for (auto* track : registeredAudioTracks)
    {
        if (track)
            track->prepareToPlay(sampleRate);
    }
}

void DrumGrooveProcessor::releaseResources()
{
    midiProcessor.releaseResources();
    drumMixer.reset();
    sampleEngine.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DrumGrooveProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // For a MIDI effect, we need to support various audio configurations
    // Accept mono, stereo, or disabled audio buses

    // Get input and output channel sets
    auto mainOutput = layouts.getMainOutputChannelSet();
    auto mainInput = layouts.getMainInputChannelSet();

    // Accept disabled buses (no audio)
    if (mainOutput.isDisabled() && mainInput.isDisabled())
        return true;

    // Accept mono or stereo for output
    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo()
        && !mainOutput.isDisabled())
        return false;

    // Accept mono or stereo for input
    if (mainInput != juce::AudioChannelSet::mono()
        && mainInput != juce::AudioChannelSet::stereo()
        && !mainInput.isDisabled())
        return false;

    // Input and output should match (for pass-through)
    // OR one can be disabled
    if (!mainInput.isDisabled() && !mainOutput.isDisabled())
    {
        if (mainInput != mainOutput)
            return false;
    }

    return true;
}
#endif


juce::AudioProcessorEditor* DrumGrooveProcessor::createEditor()
{
    return new DrumGrooveEditor(*this);
}

void DrumGrooveProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    saveCompleteGuiState();
    auto state = parameters.copyState();

    if (guiStateTree.isValid())
        state.appendChild(guiStateTree.createCopy(), nullptr);

    // Save section state
    auto sectionState = sectionManager.saveState();
    if (sectionState.isValid())
        state.appendChild(sectionState, nullptr);

    // Save DrumMixer state for persistence
    auto mixerState = drumMixer.saveState();
    if (mixerState.isValid())
        state.appendChild(mixerState, nullptr);

    auto xml = state.createXml();
    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}


void DrumGrooveProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // SAFETY: Validate input data
    if (data == nullptr || sizeInBytes <= 0)
        return;

    auto xmlState = getXmlFromBinary(data, sizeInBytes);

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml(*xmlState);

        // SAFETY: Verify the new state is valid before replacing
        if (!newState.isValid())
            return;

        // Replace parameter state
        parameters.replaceState(newState);

        // SAFETY: Restore GUI state tree carefully
        auto newGuiState = newState.getChildWithName("GuiState");
        if (newGuiState.isValid())
        {
            // Clear existing GUI state
            guiStateTree.removeAllChildren(nullptr);
            guiStateTree.removeAllProperties(nullptr);

            // Copy properties from new state
            for (int i = 0; i < newGuiState.getNumProperties(); ++i)
            {
                auto propName = newGuiState.getPropertyName(i);
                guiStateTree.setProperty(propName, newGuiState.getProperty(propName), nullptr);
            }

            // Copy children from new state
            for (int i = 0; i < newGuiState.getNumChildren(); ++i)
            {
                guiStateTree.appendChild(newGuiState.getChild(i).createCopy(), nullptr);
            }
        }

        // Restore section manager state
        auto sectionState = newState.getChildWithName("Sections");
        if (sectionState.isValid())
        {
            sectionManager.restoreState(sectionState);
        }

        // Restore DrumMixer state for persistence
        auto mixerState = newState.getChildWithName("DrumMixer");
        if (mixerState.isValid())
        {
            drumMixer.restoreState(mixerState);
        }

        // IMPORTANT: Explicitly restore visual latency offset to MidiProcessor
        // Parameter listeners may not fire reliably during replaceState()
        if (auto* latencyParam = parameters.getRawParameterValue("visualLatencyOffset"))
        {
            float restoredLatency = latencyParam->load();
            midiProcessor.setVisualLatencyOffset(restoredLatency);
            DBG("Restored visual latency offset: " + juce::String(restoredLatency) + " ms");
        }

        // IMPORTANT: Sync Global BPM with current effective BPM after restore
        // This ensures BAR mode uses the correct BPM even in old project files
        sectionManager.setGlobalBPM(getCurrentEffectiveBPM());

        // SAFETY: Defer GUI restoration to ensure all components are initialized
        juce::MessageManager::callAsync([this]()
        {
            restoreCompleteGuiState();
        });
    }
}


juce::AudioProcessorValueTreeState::ParameterLayout DrumGrooveProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Manual BPM parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "manualBPM",
        "Manual BPM",
        juce::NormalisableRange<float>(20.0f, 300.0f, 0.1f),
                                                           120.0f));

    // Sync to host parameter
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "syncToHost",
        "Sync to Host",
        true));

    // Target Library parameter - FIXED: Use alphabetically sorted list matching GUI
    juce::StringArray libraryChoices;

    // Get the sorted library list from DrumLibraryManager
    // This matches exactly what GrooveBrowser uses
    DrumLibraryManager tempManager;
    libraryChoices = tempManager.getLoadedLibraryNames();

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "targetLibrary",
        "Target Library",
        libraryChoices,
        libraryChoices.indexOf("General MIDI"))); // Default to General MIDI

    // Track Solo parameter
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "trackSolo",
        "Track Solo",
        false));

    // Track Mute parameter
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "trackMute",
        "Track Mute",
        false));

    // Visual Latency Offset parameter (in milliseconds)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "visualLatencyOffset",
        "Visual Latency Offset (ms)",
                                                           juce::NormalisableRange<float>(-200.0f, 0.0f, 1.0f),
                                                           -20.0f)); // Default: -20ms

    // Audio mode toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "audioMode",
        "Audio Mode",
        false));  // Default: MIDI mode
    //layout.add(std::make_unique<juce::AudioParameterBool>("audioMode", "Audio Mode", false));

    // Master EQ enable
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "masterEQEnabled",
        "Master EQ Enabled",
        false));

    return layout;
}

// GUI State Management Implementation
DrumGrooveProcessor::GuiState DrumGrooveProcessor::getGuiState() const
{
    GuiState state;

    state.currentBrowserFolder = guiStateTree.getProperty("currentBrowserFolder", "");
    state.selectedFile = guiStateTree.getProperty("selectedFile", "");
    state.editorWidth = guiStateTree.getProperty("editorWidth", 1500);
    state.editorHeight = guiStateTree.getProperty("editorHeight", 900);
    state.editorX = guiStateTree.getProperty("editorX", -1);
    state.editorY = guiStateTree.getProperty("editorY", -1);

    // Load navigation path
    auto pathString = guiStateTree.getProperty("browserNavigationPath", "").toString();
    if (pathString.isNotEmpty())
    {
        state.browserNavigationPath.addTokens(pathString, "|", "");
    }

    return state;
}

void DrumGrooveProcessor::setGuiState(const GuiState& state)
{
    guiStateTree.setProperty("currentBrowserFolder", state.currentBrowserFolder, nullptr);
    guiStateTree.setProperty("selectedFile", state.selectedFile, nullptr);
    guiStateTree.setProperty("editorWidth", state.editorWidth, nullptr);
    guiStateTree.setProperty("editorHeight", state.editorHeight, nullptr);
    guiStateTree.setProperty("editorX", state.editorX, nullptr);
    guiStateTree.setProperty("editorY", state.editorY, nullptr);

    // Save navigation path as delimited string
    juce::String pathString;
    for (int i = 0; i < state.browserNavigationPath.size(); ++i)
    {
        if (i > 0) pathString += "|";
        pathString += state.browserNavigationPath[i];
    }
    guiStateTree.setProperty("browserNavigationPath", pathString, nullptr);
}

void DrumGrooveProcessor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                                   const juce::Identifier& /*property*/)
{
    // Handle any parameter changes that might affect the GUI
    // This allows the GUI to update when parameters change from automation or preset loading
    if (treeWhosePropertyHasChanged == guiStateTree)
    {
        // GUI state changed - editors will be notified through other mechanisms
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumGrooveProcessor();
}


//==========================================================================
// COMPLETE GUI STATE PERSISTENCE
//==========================================================================

void DrumGrooveProcessor::saveCompleteGuiState()
{
    // SAFETY: Comprehensive nullptr checking to prevent crashes during state save

    // Step 1: Check if there's an active editor
    auto* editor = getActiveEditor();
    if (!editor)
        return; // No editor open, nothing to save

        // Step 2: Try to cast to our specific editor type
        auto* drumEditor = dynamic_cast<DrumGrooveEditor*>(editor);
    if (!drumEditor)
        return; // Not our editor type (shouldn't happen, but be safe)

        // Step 3: Check if editor has children before accessing child 0
        if (drumEditor->getNumChildComponents() == 0)
            return; // Editor not fully initialized yet

            // Step 4: Get the first child component
            auto* firstChild = drumEditor->getChildComponent(0);
        if (!firstChild)
            return; // Child component doesn't exist

            // Step 5: Try to cast to MainComponent
            auto* mainComp = dynamic_cast<MainComponent*>(firstChild);
        if (!mainComp)
            return; // First child is not MainComponent (shouldn't happen, but be safe)

            // Step 6: Get MultiTrackContainer from MainComponent
            auto* container = mainComp->getMultiTrackContainer();
        if (!container)
            return; // MultiTrackContainer not initialized yet

            // All checks passed - safe to save state
            saveCompleteGuiState(container);
}

void DrumGrooveProcessor::saveCompleteGuiState(MultiTrackContainer* container)
{
    if (!container) return;

    // Save the complete state tree from MultiTrackContainer
    juce::ValueTree completeState = container->saveGuiState();

    // Merge with existing guiStateTree (preserve browser state)
    guiStateTree.removeAllChildren(nullptr);
    for (int i = 0; i < completeState.getNumChildren(); ++i)
    {
        guiStateTree.appendChild(completeState.getChild(i).createCopy(), nullptr);
    }

    // Copy all properties
    for (int i = 0; i < completeState.getNumProperties(); ++i)
    {
        auto propName = completeState.getPropertyName(i);
        guiStateTree.setProperty(propName, completeState.getProperty(propName), nullptr);
    }
}

void DrumGrooveProcessor::restoreCompleteGuiState()
{
    // SAFETY: Comprehensive nullptr checking to prevent crashes during state restore

    // Step 1: Check if there's an active editor
    auto* editor = getActiveEditor();
    if (!editor)
        return; // No editor open, nothing to restore

        // Step 2: Try to cast to our specific editor type
        auto* drumEditor = dynamic_cast<DrumGrooveEditor*>(editor);
    if (!drumEditor)
        return; // Not our editor type (shouldn't happen, but be safe)

        // Step 3: Check if editor has children before accessing child 0
        if (drumEditor->getNumChildComponents() == 0)
            return; // Editor not fully initialized yet

            // Step 4: Get the first child component
            auto* firstChild = drumEditor->getChildComponent(0);
        if (!firstChild)
            return; // Child component doesn't exist

            // Step 5: Try to cast to MainComponent
            auto* mainComp = dynamic_cast<MainComponent*>(firstChild);
        if (!mainComp)
            return; // First child is not MainComponent (shouldn't happen, but be safe)

            // Step 6: Get MultiTrackContainer from MainComponent
            auto* container = mainComp->getMultiTrackContainer();
        if (!container)
            return; // MultiTrackContainer not initialized yet

            // Step 7: Verify we have valid state to restore
            if (!guiStateTree.isValid())
                return; // No valid state to restore

                // All checks passed - safe to restore state
                container->restoreGuiState(guiStateTree);
}

void DrumGrooveProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "visualLatencyOffset")
    {
        midiProcessor.setVisualLatencyOffset(newValue);
    }
}

GrooveBrowser* DrumGrooveProcessor::getGrooveBrowser() const
{
    // Try to get GrooveBrowser through the editor chain
    auto* editor = getActiveEditor();
    if (!editor)
        return nullptr;

    auto* drumEditor = dynamic_cast<DrumGrooveEditor*>(editor);
    if (!drumEditor)
        return nullptr;

    auto* mainComp = drumEditor->getMainComponent();
    if (!mainComp)
        return nullptr;

    return mainComp->getGrooveBrowser();
}

void DrumGrooveProcessor::saveBrowserState(const juce::File& currentFolder,
                                           const juce::Array<juce::File>& navPath,
                                           const juce::File& selectedFile)
{
    guiStateTree.setProperty("browserCurrentFolder", currentFolder.getFullPathName(), nullptr);
    guiStateTree.setProperty("browserSelectedFile", selectedFile.getFullPathName(), nullptr);

    juce::String navPathStr;
    for (int i = 0; i < navPath.size(); ++i)
    {
        if (i > 0) navPathStr += "|";
        navPathStr += navPath[i].getFullPathName();
    }
    guiStateTree.setProperty("browserNavPath", navPathStr, nullptr);
}

void DrumGrooveProcessor::restoreBrowserState(juce::File& currentFolder,
                                              juce::Array<juce::File>& navPath,
                                              juce::File& selectedFile) const
                                              {
                                                  currentFolder = juce::File(guiStateTree.getProperty("browserCurrentFolder", "").toString());
                                                  selectedFile = juce::File(guiStateTree.getProperty("browserSelectedFile", "").toString());

                                                  navPath.clear();
                                                  juce::String navPathStr = guiStateTree.getProperty("browserNavPath", "").toString();
                                                  if (navPathStr.isNotEmpty())
                                                  {
                                                      juce::StringArray paths = juce::StringArray::fromTokens(navPathStr, "|", "");
                                                      for (const auto& path : paths)
                                                          navPath.add(juce::File(path));
                                                  }
                                              }

void DrumGrooveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Merge preview notes for audition/preview
    if (!previewMidiBuffer.isEmpty())
    {
        midiMessages.addEvents(previewMidiBuffer, 0, -1, 0);
        previewMidiBuffer.clear();
    }

    // Get current BPM
    double currentBPM = 120.0;
    bool syncToHost = parameters.getRawParameterValue("syncToHost")->load() > 0.5f;

    if (syncToHost)
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                if (position->getBpm().hasValue())
                    currentBPM = *position->getBpm();
            }
        }
    }
    else
    {
        currentBPM = parameters.getRawParameterValue("manualBPM")->load();
    }

    // Get target library
    int libraryIndex = static_cast<int>(parameters.getRawParameterValue("targetLibrary")->load());

    if (libraryNamesNeedUpdate.load())
        updateLibraryNamesCache();
    const auto& sortedNames = cachedLibraryNames;

    DrumLibrary targetLibrary = DrumLibrary::GeneralMIDI;
    if (libraryIndex >= 0 && libraryIndex < sortedNames.size())
    {
        juce::String libraryName = sortedNames[libraryIndex];
        targetLibrary = drumLibraryManager.getLibraryFromName(libraryName);
    }

    // Get section-aware playback BPM
    double playheadPos = midiProcessor.getPlayheadPosition();
    const Section* currentSection = sectionManager.getSectionAtTime(playheadPos, currentBPM);
    double playbackBPM = currentBPM;

    if (currentSection && currentSection->playbackBPM > 0.0)
        playbackBPM = currentSection->playbackBPM;

    // CRITICAL FIX: Always process MIDI first to advance playhead and generate MIDI
    midiProcessor.processBlock(midiMessages, playbackBPM, targetLibrary);

    // Check audio mode
    bool audioMode = isAudioMode();

    if (audioMode && sampleEngine.isLoaded())
    {
        // AUDIO MODE: Route MIDI through sample engine with per-part processing

        const int numSamples = buffer.getNumSamples();

        // Ensure per-part buffers are correct size
        for (auto& partBuffer : partBuffers)
        {
            if (partBuffer.getNumSamples() != numSamples)
            {
                partBuffer.setSize(2, numSamples, false, false, true);
            }
        }

        // Ensure output buffer is correct size
        if (audioBuffer.getNumSamples() != numSamples)
        {
            audioBuffer.setSize(2, numSamples, false, false, true);
        }

        audioBuffer.clear();

        // Process MIDI through sample engine to per-part buffers
        sampleEngine.processBlockToPartBuffers(partBuffers, midiMessages);

        // Process per-part buffers through drum mixer (applies EQ, volume, pan, reverb per part)
        drumMixer.processPerPartBuffers(partBuffers, audioBuffer);

        // Copy to output
        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), audioBuffer.getNumChannels()); ++ch)
        {
            buffer.copyFrom(ch, 0, audioBuffer, ch, 0, numSamples);
        }

        // Mix in audio tracks if any are registered and playing
        if (midiProcessor.isPlaying())
        {
            juce::SpinLock::ScopedTryLockType lock(audioTrackLock);
            if (!lock.isLocked())
            {
                // CRITICAL FIX: Log lock contention for debugging
                static int lockFailCount = 0;
                if (++lockFailCount % 100 == 0)  // Log every 100 failures
                {
                    DBG("WARNING: Audio track lock failed " + juce::String(lockFailCount) + " times - possible contention");
                }
            }
            else if (!registeredAudioTracks.empty())
            {
                // Ensure audio track buffer is correct size
                if (audioTrackBuffer.getNumSamples() != numSamples)
                {
                    audioTrackBuffer.setSize(2, numSamples, false, false, true);
                }

                double playheadSecs = midiProcessor.getPlayheadPosition();

                for (auto* track : registeredAudioTracks)
                {
                    if (track && track->isLoaded() && !track->isMuted())
                    {
                        audioTrackBuffer.clear();
                        juce::AudioSourceChannelInfo info(&audioTrackBuffer, 0, numSamples);
                        track->getNextAudioBlock(info, playheadSecs, 1.0);

                        // Add to main buffer
                        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), audioTrackBuffer.getNumChannels()); ++ch)
                        {
                            buffer.addFrom(ch, 0, audioTrackBuffer, ch, 0, numSamples);
                        }
                    }
                }
            }
        }

        // Clear MIDI output in audio mode
        midiMessages.clear();
    }
    else
    {
        // MIDI MODE: Output MIDI directly, mix in audio tracks only
        buffer.clear();

        // Mix in audio tracks if any are registered and playing
        if (!registeredAudioTracks.empty() && midiProcessor.isPlaying())
        {
            const int numSamples = buffer.getNumSamples();

            // Ensure audio track buffer is correct size
            if (audioTrackBuffer.getNumSamples() != numSamples)
            {
                audioTrackBuffer.setSize(2, numSamples, false, false, true);
            }

            double playheadSecs = midiProcessor.getPlayheadPosition();

            for (auto* track : registeredAudioTracks)
            {
                if (track && track->isLoaded() && !track->isMuted())
                {
                    audioTrackBuffer.clear();
                    juce::AudioSourceChannelInfo info(&audioTrackBuffer, 0, numSamples);
                    track->getNextAudioBlock(info, playheadSecs, 1.0);

                    // Add to main buffer
                    for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), audioTrackBuffer.getNumChannels()); ++ch)
                    {
                        buffer.addFrom(ch, 0, audioTrackBuffer, ch, 0, numSamples);
                    }
                }
            }
        }
    }
}

void DrumGrooveProcessor::setAudioMode(bool enabled)
{
    if (auto* param = parameters.getRawParameterValue("audioMode"))
        *param = enabled ? 1.0f : 0.0f;
}

bool DrumGrooveProcessor::loadDrumSamples()
{
    juce::File baseDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("DrumGroovePro").getChildFile("Samples");

    // Try to find any drum kit in the Samples directory (SFZ format only)
    juce::File samplesDir;

    // Check for salamanderDrumkit first (preferred SFZ format)
    auto salamanderDir = baseDir.getChildFile("salamanderDrumkit");
    if (salamanderDir.exists() && salamanderDir.isDirectory())
    {
        auto sfzFile = salamanderDir.getChildFile("ALL.sfz");
        if (sfzFile.existsAsFile())
        {
            samplesDir = salamanderDir;
            DBG("Found salamanderDrumkit (SFZ format)");
        }
    }

    // If not found, search for any SFZ kit in the base Samples directory
    if (!samplesDir.exists())
    {
        auto subdirs = baseDir.findChildFiles(juce::File::findDirectories, false);
        for (const auto& subdir : subdirs)
        {
            // Check for SFZ format (ALL.sfz file)
            auto sfzFile = subdir.getChildFile("ALL.sfz");
            if (sfzFile.existsAsFile())
            {
                samplesDir = subdir;
                DBG("Found SFZ kit: " + subdir.getFileName());
                break;
            }
        }
    }

    if (!samplesDir.exists())
    {
        DBG("No SFZ drum kit samples found in: " + baseDir.getFullPathName());
        return false;
    }

    DBG("Loading SFZ format from: " + samplesDir.getFullPathName());
    return sampleEngine.loadSamplesFromDirectory(samplesDir);
}

bool DrumGrooveProcessor::hasAudioTracks() const
{
    juce::SpinLock::ScopedLockType lock(audioTrackLock);
    return !registeredAudioTracks.empty();
}

void DrumGrooveProcessor::registerAudioTrack(AudioTrack* track)
{
    if (track == nullptr)
        return;

    juce::SpinLock::ScopedLockType lock(audioTrackLock);

    // Check if already registered
    auto it = std::find(registeredAudioTracks.begin(), registeredAudioTracks.end(), track);
    if (it == registeredAudioTracks.end())
    {
        registeredAudioTracks.push_back(track);
        track->prepareToPlay(currentSampleRate);
    }
}

void DrumGrooveProcessor::unregisterAudioTrack(AudioTrack* track)
{
    juce::SpinLock::ScopedLockType lock(audioTrackLock);

    auto it = std::find(registeredAudioTracks.begin(), registeredAudioTracks.end(), track);
    if (it != registeredAudioTracks.end())
    {
        registeredAudioTracks.erase(it);
    }
}

void DrumGrooveProcessor::clearAudioTracks()
{
    juce::SpinLock::ScopedLockType lock(audioTrackLock);
    registeredAudioTracks.clear();
}

// =============================================================================
// Global Settings Persistence
// These settings persist across plugin sessions (even without a DAW project)
// =============================================================================

juce::File DrumGrooveProcessor::getGlobalSettingsFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
    .getChildFile("DrumGroovePro")
    .getChildFile("settings.xml");
}

void DrumGrooveProcessor::loadGlobalSettings()
{
    juce::File settingsFile = getGlobalSettingsFile();

    if (!settingsFile.existsAsFile())
    {
        // No settings file - use defaults
        DBG("No global settings file found, using defaults");
        midiProcessor.setVisualLatencyOffset(-20.0);  // Default -20ms
        return;
    }

    auto xml = juce::XmlDocument::parse(settingsFile);
    if (xml == nullptr || !xml->hasTagName("DrumGrooveProSettings"))
    {
        DBG("Invalid global settings file, using defaults");
        midiProcessor.setVisualLatencyOffset(-20.0);
        return;
    }

    // Load visual latency offset
    float latencyMs = static_cast<float>(xml->getDoubleAttribute("visualLatencyOffset", -20.0));
    latencyMs = juce::jlimit(-200.0f, 0.0f, latencyMs);

    // Apply to parameter and MidiProcessor
    if (auto* param = parameters.getRawParameterValue("visualLatencyOffset"))
    {
        param->store(latencyMs);
    }
    midiProcessor.setVisualLatencyOffset(latencyMs);

    DBG("Loaded global settings: visualLatencyOffset = " + juce::String(latencyMs) + " ms");
}

void DrumGrooveProcessor::saveGlobalSettings()
{
    juce::File settingsFile = getGlobalSettingsFile();

    // Ensure directory exists
    settingsFile.getParentDirectory().createDirectory();

    // Create XML document
    juce::XmlElement settings("DrumGrooveProSettings");

    // Save visual latency offset
    float latencyMs = getVisualLatencyOffset();
    settings.setAttribute("visualLatencyOffset", latencyMs);

    // Write to file
    if (!settings.writeTo(settingsFile))
    {
        DBG("ERROR: Failed to save global settings to: " + settingsFile.getFullPathName());
    }
    else
    {
        DBG("Saved global settings: visualLatencyOffset = " + juce::String(latencyMs) + " ms");
    }
}

void DrumGrooveProcessor::triggerPreviewNote(int midiNoteNumber, int velocity)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    DBG("Triggering preview note: " + juce::String(midiNoteNumber));

    // Create MIDI note-on message
    auto noteOn = juce::MidiMessage::noteOn(1, midiNoteNumber, (juce::uint8)velocity);

    // Inject into the preview buffer (will be merged in processBlock)
    previewMidiBuffer.addEvent(noteOn, 0);

    // CRITICAL FIX: Capture processor pointer and check deletion flag
    auto* processor = this;

    // Schedule note-off after 300ms
    juce::Timer::callAfterDelay(300, [processor, midiNoteNumber]()
    {
        // Check if processor still exists before accessing
        if (!processor->isBeingDeleted.load())
        {
            auto noteOff = juce::MidiMessage::noteOff(1, midiNoteNumber);
            processor->previewMidiBuffer.addEvent(noteOff, 0);
        }
    });
}
