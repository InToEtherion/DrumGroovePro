#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/MainComponent.h"
#include "GUI/Components/MultiTrackContainer.h"

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
    guiStateTree.setProperty("editorWidth", 1400, nullptr);
    guiStateTree.setProperty("editorHeight", 900, nullptr);
    guiStateTree.setProperty("editorX", -1, nullptr);
    guiStateTree.setProperty("editorY", -1, nullptr);

    // Add GUI state to main parameters tree so it gets saved/loaded automatically
    parameters.state.addChild(guiStateTree, -1, nullptr);

    // Listen for changes to the ValueTree
    parameters.state.addListener(this);

    // Listen for parameter changes (including when state is restored)
    parameters.addParameterListener("visualLatencyOffset", this);

    // Initialize visual latency offset in MidiProcessor from parameter
    midiProcessor.setVisualLatencyOffset(getVisualLatencyOffset());
}

DrumGrooveProcessor::~DrumGrooveProcessor()
{
    parameters.removeParameterListener("visualLatencyOffset", this);
    parameters.state.removeListener(this);
    drumLibraryManager.saveConfiguration();
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
    midiProcessor.prepareToPlay(sampleRate, samplesPerBlock);
}

void DrumGrooveProcessor::releaseResources()
{
    midiProcessor.releaseResources();
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

void DrumGrooveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

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
    DrumLibrary targetLibrary = static_cast<DrumLibrary>(libraryIndex + 1);

    // Process MIDI with correct parameters
    midiProcessor.processBlock(midiMessages, currentBPM, targetLibrary);
}

juce::AudioProcessorEditor* DrumGrooveProcessor::createEditor()
{
    return new DrumGrooveEditor(*this);
}

void DrumGrooveProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save complete GUI state before serializing
    saveCompleteGuiState();

    auto state = parameters.copyState();

    // Append GUI state tree to the main state
    if (guiStateTree.isValid())
    {
        state.appendChild(guiStateTree.createCopy(), nullptr);
    }

    auto xml = state.createXml();
    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
    }
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
        
        parameters.replaceState(newState);

        // Find and store GUI state tree IN MEMORY (no restoration yet)
        auto guiChild = newState.getChildWithName("GuiState");
        if (guiChild.isValid())
        {
            guiStateTree = guiChild;
        }

        // CRITICAL FIX: Force parameter notification to update GUI controls
        // NOTE: Capturing 'this' is safe here because AudioProcessor deletion happens on message thread
        juce::MessageManager::callAsync([this]()
        {
            // SAFETY: Check if parameter exists before accessing
            auto* targetLibValuePtr = parameters.getRawParameterValue("targetLibrary");
            if (!targetLibValuePtr)
                return;
            
            float targetLibValue = targetLibValuePtr->load();

            DBG("=== VST3 State Loaded ===");
            DBG("Target Library parameter value: " + juce::String(targetLibValue));

            auto* targetLibParam = parameters.getParameter("targetLibrary");
            if (targetLibParam)
            {
                targetLibParam->setValue(targetLibValue);
                targetLibParam->sendValueChangedMessageToListeners(targetLibValue);
            }
        });
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumGrooveProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Sync to Host parameter
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "syncToHost",
        "Sync to Host",
        true));

    // Manual BPM parameter (used when not syncing to host)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "manualBPM",
        "Manual BPM",
        juce::NormalisableRange<float>(20.0f, 300.0f, 0.1f),
                                                           120.0f));

    // Target Library parameter - CRITICAL: Order MUST match enum order (excluding Unknown=0)
    // This ensures parameter index + 1 = enum value
    juce::StringArray libraryChoices;
    libraryChoices.add("Bypass (No Remapping)");       // 0 → Bypass = 1
    libraryChoices.add("General MIDI");                // 1 → GeneralMIDI = 2
    libraryChoices.add("Superior Drummer 3");          // 2 → SuperiorDrummer3 = 3
    libraryChoices.add("Addictive Drums 2");           // 3 → AddictiveDrums2 = 4
    libraryChoices.add("Battery 4");                   // 4 → Battery4 = 5
    libraryChoices.add("EZdrummer");                   // 5 → EZdrummer = 6
    libraryChoices.add("GetGood Drums");               // 6 → GetGoodDrums = 7
    libraryChoices.add("Steven Slate Drums");          // 7 → StevenSlateDrums = 8
    libraryChoices.add("Ugritone");                    // 8 → Ugritone = 9
    libraryChoices.add("BFD3");                        // 9 → BFD3 = 10
    libraryChoices.add("MT Power Drum Kit 2");         // 10 → MTPowerDrumKit2 = 11
    libraryChoices.add("DrumGizmo");                   // 11 → DrumGizmo = 12
    libraryChoices.add("Sitala");                      // 12 → Sitala = 13
    libraryChoices.add("Krimh Drums");                 // 13 → KrimhDrums = 14
    libraryChoices.add("The Monarch Kit");             // 14 → TheMonarchKit = 15
    libraryChoices.add("Shreddage Drums");             // 15 → ShreddageDrums = 16
    libraryChoices.add("Damage 2");                    // 16 → Damage2 = 17
    libraryChoices.add("Triaz");                       // 17 → Triaz = 18
    libraryChoices.add("MODO Drum");                   // 18 → MODODrum = 19
    libraryChoices.add("Drum Locker");                 // 19 → DrumLocker = 20

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "targetLibrary",
        "Target Library",
        libraryChoices,
        1)); // Default to "General MIDI" (index 1 → enum 2)

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

        return layout;
}

// GUI State Management Implementation
DrumGrooveProcessor::GuiState DrumGrooveProcessor::getGuiState() const
{
    GuiState state;

    state.currentBrowserFolder = guiStateTree.getProperty("currentBrowserFolder", "");
    state.selectedFile = guiStateTree.getProperty("selectedFile", "");
    state.editorWidth = guiStateTree.getProperty("editorWidth", 1400);
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
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
        .getChildFile("visual_playhead.log");
        logFile.appendText("\nCALLBACK: " + juce::String(newValue) + "ms\n");

        midiProcessor.setVisualLatencyOffset(newValue);
    }
}