#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/MainComponent.h"
#include "GUI/Components/MultiTrackContainer.h"

DrumGrooveProcessor::DrumGrooveProcessor()
: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
),
parameters(*this, nullptr, juce::Identifier("DrumGrooveProParams"), createParameterLayout()),
midiProcessor(drumLibraryManager)
{
    drumLibraryManager.loadConfiguration();

    // Initialize GUI state tree with default values
    guiStateTree.setProperty("currentBrowserFolder", "", nullptr);
    guiStateTree.setProperty("selectedFile", "", nullptr);
    guiStateTree.setProperty("editorWidth", 1300, nullptr);
    guiStateTree.setProperty("editorHeight", 900, nullptr);
    guiStateTree.setProperty("editorX", -1, nullptr);
    guiStateTree.setProperty("editorY", -1, nullptr);

    // Add GUI state to main parameters tree so it gets saved/loaded automatically
    parameters.state.addChild(guiStateTree, -1, nullptr);

    // Listen for changes to the ValueTree
    parameters.state.addListener(this);
}

DrumGrooveProcessor::~DrumGrooveProcessor()
{
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
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

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
		auto xmlState = getXmlFromBinary(data, sizeInBytes);

		if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
		{
			auto newState = juce::ValueTree::fromXml(*xmlState);
			parameters.replaceState(newState);

			// Find and update GUI state tree
			auto guiChild = newState.getChildWithName("GuiState");
			if (guiChild.isValid())
			{
				guiStateTree = guiChild;
            
				// Restore complete GUI state to active editor
				restoreCompleteGuiState();
			}
        
			// CRITICAL FIX: Force parameter notification to update GUI controls
			// This ensures ComboBoxes sync with loaded parameter values in VST3
			juce::MessageManager::callAsync([this]()
			{
				// Get the actual parameter value that was just loaded
				float targetLibValue = parameters.getRawParameterValue("targetLibrary")->load();
            
				DBG("=== VST3 State Loaded ===");
				DBG("Target Library parameter value: " + juce::String(targetLibValue));
            
				// Notify the parameter listener explicitly
				// This triggers GrooveBrowser::parameterChanged which syncs the ComboBox
				auto* targetLibParam = parameters.getParameter("targetLibrary");
				if (targetLibParam)
				{
					// Force notification by temporarily changing and restoring value
					// (this is a workaround for VST3 state restoration timing issues)
					targetLibParam->beginChangeGesture();
					targetLibParam->setValueNotifyingHost(targetLibValue);
					targetLibParam->endChangeGesture();
                
					DBG("Forced targetLibrary parameter notification");
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

    // Target Library parameter - UPDATED with all new libraries
    juce::StringArray libraryChoices;
    libraryChoices.add("Addictive Drums 2");           // 0
    libraryChoices.add("Battery 4");                   // 1
    libraryChoices.add("BFD3");                        // 2
    libraryChoices.add("Bypass (No Remapping)");       // 3
    libraryChoices.add("Damage 2");                    // 4
    libraryChoices.add("DrumGizmo");                   // 5
    libraryChoices.add("EZdrummer");                   // 6
    libraryChoices.add("General MIDI");                // 7
    libraryChoices.add("GetGood Drums");               // 8
    libraryChoices.add("Krimh Drums");                 // 9
    libraryChoices.add("MT Power Drum Kit 2");         // 10
    libraryChoices.add("Shreddage Drums");             // 11
    libraryChoices.add("Sitala");                      // 12
    libraryChoices.add("Steven Slate Drums");          // 13
    libraryChoices.add("Superior Drummer 3");          // 14
    libraryChoices.add("The Monarch Kit");             // 15
    libraryChoices.add("Ugritone");                    // 16

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "targetLibrary",
        "Target Library",
        libraryChoices,
        7)); // Default to "General MIDI" (index 7)

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

    return layout;
}

// GUI State Management Implementation
DrumGrooveProcessor::GuiState DrumGrooveProcessor::getGuiState() const
{
    GuiState state;

    state.currentBrowserFolder = guiStateTree.getProperty("currentBrowserFolder", "");
    state.selectedFile = guiStateTree.getProperty("selectedFile", "");
    state.editorWidth = guiStateTree.getProperty("editorWidth", 1300);
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
    // Get the complete state from the active editor
    if (auto* editor = dynamic_cast<DrumGrooveEditor*>(getActiveEditor()))
    {
        // Get MainComponent from editor
        if (auto* mainComp = dynamic_cast<MainComponent*>(editor->getChildComponent(0)))
        {
            // Get MultiTrackContainer from MainComponent
            if (auto* container = mainComp->getMultiTrackContainer())
            {
                saveCompleteGuiState(container);
            }
        }
    }
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
    // Restore the complete state to the active editor
    if (auto* editor = dynamic_cast<DrumGrooveEditor*>(getActiveEditor()))
    {
        // Get MainComponent from editor
        if (auto* mainComp = dynamic_cast<MainComponent*>(editor->getChildComponent(0)))
        {
            // Get MultiTrackContainer from MainComponent
            if (auto* container = mainComp->getMultiTrackContainer())
            {
                // Restore the complete state tree to MultiTrackContainer
                container->restoreGuiState(guiStateTree);
            }
        }
    }
}

