#include "SectionBar.h"
#include "../PluginProcessor.h"
#include "MultiTrackContainer.h"
#include "../MainComponent.h"
#include "LookAndFeel/ColourPalette.h"

const std::vector<SectionBar::TimeSig>& SectionBar::getCommonTimeSignatures()
{
    static const std::vector<TimeSig> timeSignatures = {
        { 2, 4, "2/4" },
        { 3, 4, "3/4" },
        { 4, 4, "4/4" },
        { 5, 4, "5/4" },
        { 6, 8, "6/8" },
        { 7, 8, "7/8" },
        { 9, 8, "9/8" },
        { 12, 8, "12/8" }
    };
    return timeSignatures;
}

SectionBar::SectionBar(DrumGrooveProcessor& proc, SectionManager& sectionMgr)
    : processor(proc)
    , sectionManager(sectionMgr)
{
    setupComponents();
    sectionManager.addChangeListener(this);
    updateComponentsForSelectedSection();
	setSectionBarEnabled(false);
}

SectionBar::~SectionBar()
{
    sectionManager.removeChangeListener(this);
}

void SectionBar::setupComponents()
{
    // Navigation
    prevSectionButton = std::make_unique<juce::TextButton>("<");
    prevSectionButton->setTooltip("Previous Section");
    prevSectionButton->onClick = [this] { navigateToPrevSection(); };
    addAndMakeVisible(prevSectionButton.get());

    nextSectionButton = std::make_unique<juce::TextButton>(">");
    nextSectionButton->setTooltip("Next Section");
    nextSectionButton->onClick = [this] { navigateToNextSection(); };
    addAndMakeVisible(nextSectionButton.get());

    sectionInfoLabel = std::make_unique<juce::Label>("", "Section 1/1");
    sectionInfoLabel->setFont(juce::Font(13.0f, juce::Font::bold));
    sectionInfoLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sectionInfoLabel.get());

    // Time signature
    timeSigLabel = std::make_unique<juce::Label>("", "TIME SIG:");
    timeSigLabel->setFont(juce::Font(11.0f, juce::Font::bold));
    timeSigLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(timeSigLabel.get());

    timeSigCombo = std::make_unique<juce::ComboBox>();
    const auto& timeSigs = getCommonTimeSignatures();
    for (int i = 0; i < static_cast<int>(timeSigs.size()); ++i)
        timeSigCombo->addItem(timeSigs[i].display, i + 1);
    timeSigCombo->onChange = [this] { /* Only affects next +ADD */ };
    addAndMakeVisible(timeSigCombo.get());

    // ── BARS ──────────────────────────────────────────────────────────────────
    barsLabel = std::make_unique<juce::Label>("", "BARS:");
    barsLabel->setFont(juce::Font(11.0f, juce::Font::bold));
    barsLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(barsLabel.get());

    barsValueLabel = std::make_unique<juce::Label>("", "4");
    barsValueLabel->setFont(juce::Font(14.0f));
    barsValueLabel->setJustificationType(juce::Justification::centred);
    barsValueLabel->setTooltip("Double-click to type a value (1 – 32)");
    // Make the label editable on double-click, in-place (no popup window)
    barsValueLabel->setEditable(false, true, false);
    barsValueLabel->onTextChange = [this]
    {
        int newVal = juce::jlimit(1, 32, barsValueLabel->getText().getIntValue());
        // Keep label showing the clamped value
        barsValueLabel->setText(juce::String(newVal), juce::dontSendNotification);
        // Drive the slider so updateSelectedSection() sees the change
        barsSlider->setValue(static_cast<double>(newVal), juce::dontSendNotification);
        updateSelectedSection();
    };
    addAndMakeVisible(barsValueLabel.get());

    barsSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    barsSlider->setRange(1.0, 32.0, 1.0);
    barsSlider->setValue(4.0);
    barsSlider->onValueChange = [this]
    {
        int val = static_cast<int>(barsSlider->getValue());
        barsValueLabel->setText(juce::String(val), juce::dontSendNotification);
        updateSelectedSection();
    };
    addAndMakeVisible(barsSlider.get());

    // ── GRID BPM ──────────────────────────────────────────────────────────────
    bpmLabel = std::make_unique<juce::Label>("", "GRID BPM:");
    bpmLabel->setFont(juce::Font(11.0f, juce::Font::bold));
    bpmLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bpmLabel.get());

    bpmValueLabel = std::make_unique<juce::Label>("", "120");
    bpmValueLabel->setFont(juce::Font(14.0f));
    bpmValueLabel->setJustificationType(juce::Justification::centred);
    bpmValueLabel->setTooltip("Double-click to type a value (40 – 300). Disabled when 'Global Grid' is active.");
    bpmValueLabel->setEditable(false, true, false);
    bpmValueLabel->onTextChange = [this]
    {
        // Only apply when the slider is actually active (not using global BPM)
        if (!bpmSlider->isEnabled())
        {
            // Restore the displayed value so the label doesn't show garbage
            bpmValueLabel->setText(juce::String(static_cast<int>(bpmSlider->getValue())),
                                   juce::dontSendNotification);
            return;
        }
        int newVal = juce::jlimit(40, 300, bpmValueLabel->getText().getIntValue());
        bpmValueLabel->setText(juce::String(newVal), juce::dontSendNotification);
        bpmSlider->setValue(static_cast<double>(newVal), juce::dontSendNotification);
        updateSelectedSection();
    };
    addAndMakeVisible(bpmValueLabel.get());

    bpmSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    bpmSlider->setRange(40.0, 300.0, 1.0);
    bpmSlider->setValue(120.0);
    bpmSlider->onValueChange = [this]
    {
        int val = static_cast<int>(bpmSlider->getValue());
        bpmValueLabel->setText(juce::String(val), juce::dontSendNotification);
        updateSelectedSection();
    };
    addAndMakeVisible(bpmSlider.get());

    useGlobalBPMButton = std::make_unique<juce::ToggleButton>("Global Grid");
    useGlobalBPMButton->setToggleState(true, juce::dontSendNotification);
    useGlobalBPMButton->onClick = [this]
    {
        bool usingGlobal = useGlobalBPMButton->getToggleState();
        bpmSlider->setEnabled(!usingGlobal);
        // Value label should look editable only when slider is active
        bpmValueLabel->setEnabled(!usingGlobal);
        updateSelectedSection();
    };
    addAndMakeVisible(useGlobalBPMButton.get());

    // ── SPEED BPM ─────────────────────────────────────────────────────────────
    playbackBPMLabel = std::make_unique<juce::Label>("", "SPEED BPM:");
    playbackBPMLabel->setFont(juce::Font(11.0f, juce::Font::bold));
    playbackBPMLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(playbackBPMLabel.get());

    playbackBPMValueLabel = std::make_unique<juce::Label>("", "120");
    playbackBPMValueLabel->setFont(juce::Font(14.0f));
    playbackBPMValueLabel->setJustificationType(juce::Justification::centred);
    playbackBPMValueLabel->setTooltip("Double-click to type a value (40 – 300). Disabled when 'Global Speed' is active.");
    playbackBPMValueLabel->setEditable(false, true, false);
    playbackBPMValueLabel->onTextChange = [this]
    {
        if (!playbackBPMSlider->isEnabled())
        {
            playbackBPMValueLabel->setText(
                juce::String(static_cast<int>(playbackBPMSlider->getValue())),
                                           juce::dontSendNotification);
            return;
        }
        int newVal = juce::jlimit(40, 300, playbackBPMValueLabel->getText().getIntValue());
        playbackBPMValueLabel->setText(juce::String(newVal), juce::dontSendNotification);
        playbackBPMSlider->setValue(static_cast<double>(newVal), juce::dontSendNotification);
        updateSelectedSection();
    };
    addAndMakeVisible(playbackBPMValueLabel.get());

    playbackBPMSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    playbackBPMSlider->setRange(40.0, 300.0, 1.0);
    playbackBPMSlider->setValue(120.0);
    playbackBPMSlider->onValueChange = [this]
    {
        int val = static_cast<int>(playbackBPMSlider->getValue());
        playbackBPMValueLabel->setText(juce::String(val), juce::dontSendNotification);
        updateSelectedSection();
    };
    addAndMakeVisible(playbackBPMSlider.get());

    useGlobalPlaybackBPMButton = std::make_unique<juce::ToggleButton>("Global Speed");
    useGlobalPlaybackBPMButton->setToggleState(true, juce::dontSendNotification);
    useGlobalPlaybackBPMButton->onClick = [this]
    {
        bool usingGlobal = useGlobalPlaybackBPMButton->getToggleState();
        playbackBPMSlider->setEnabled(!usingGlobal);
        playbackBPMValueLabel->setEnabled(!usingGlobal);
        updateSelectedSection();
    };
    addAndMakeVisible(useGlobalPlaybackBPMButton.get());

    // ── SECTION NAME ──────────────────────────────────────────────────────────
    sectionNameLabel = std::make_unique<juce::Label>("", "NAME:");
    sectionNameLabel->setFont(juce::Font(11.0f, juce::Font::bold));
    sectionNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sectionNameLabel.get());

    sectionNameEditor = std::make_unique<juce::TextEditor>();
    sectionNameEditor->setMultiLine(false);
    sectionNameEditor->setReturnKeyStartsNewLine(false);
    sectionNameEditor->onTextChange = [this] { updateSelectedSection(); };
    addAndMakeVisible(sectionNameEditor.get());

    // Loop section button
    loopSectionButton = std::make_unique<juce::ToggleButton>("Loop Section");
    loopSectionButton->onClick = [this]
    {
        if (loopSectionButton->getToggleState())
            enableSectionLoop();
        else
            disableSectionLoop();
    };
    addAndMakeVisible(loopSectionButton.get());

    // Add/Remove buttons
    addSectionButton = std::make_unique<juce::TextButton>("+ ADD");
    addSectionButton->onClick = [this] { addSection(); };
    addAndMakeVisible(addSectionButton.get());

    removeSectionButton = std::make_unique<juce::TextButton>("x REMOVE");
    removeSectionButton->onClick = [this] { removeSection(); };
    addAndMakeVisible(removeSectionButton.get());
}

void SectionBar::paint(juce::Graphics& g)
{
    // Background with semi-transparent dark color
    g.fillAll(ColourPalette::mainBackground.withAlpha(0.9f));
    
    // Border at bottom
    g.setColour(ColourPalette::borderColour);
    g.drawLine(0.0f, static_cast<float>(getHeight()), 
               static_cast<float>(getWidth()), static_cast<float>(getHeight()), 1.0f);
    
    // NEW: Draw grayed-out overlay when disabled (TIME mode)
    if (!isEnabled)
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillAll();
        
    }
}

void SectionBar::resized()
{
    auto bounds = getLocalBounds().reduced(8, 4);
    int spacing = 8;
    
    // Buttons first - 150px
    addSectionButton->setBounds(bounds.removeFromLeft(70));
    bounds.removeFromLeft(5);
    removeSectionButton->setBounds(bounds.removeFromLeft(75));
    bounds.removeFromLeft(spacing + 5);
    
    // Navigation - 110px
    auto navBounds = bounds.removeFromLeft(110);
    prevSectionButton->setBounds(navBounds.removeFromLeft(28).reduced(2));
    nextSectionButton->setBounds(navBounds.removeFromRight(28).reduced(2));
    sectionInfoLabel->setBounds(navBounds);
    bounds.removeFromLeft(spacing);
    
    // Time signature - 130px
    auto timeSigSection = bounds.removeFromLeft(130);
    timeSigLabel->setBounds(timeSigSection.removeFromLeft(60));
    timeSigCombo->setBounds(timeSigSection);
    bounds.removeFromLeft(spacing);
    
    // Bars - 140px
    auto barsSection = bounds.removeFromLeft(140);
    barsLabel->setBounds(barsSection.removeFromLeft(40));
    barsValueLabel->setBounds(barsSection.removeFromLeft(25));
    barsSlider->setBounds(barsSection);
    bounds.removeFromLeft(spacing);
    
    // Grid BPM - 230px
    auto gridSection = bounds.removeFromLeft(230);
    bpmLabel->setBounds(gridSection.removeFromLeft(70));
    bpmValueLabel->setBounds(gridSection.removeFromLeft(35));
    bpmSlider->setBounds(gridSection.removeFromLeft(65));
    useGlobalBPMButton->setBounds(gridSection);
    bounds.removeFromLeft(spacing);
    
    // Speed BPM - 240px
    auto speedSection = bounds.removeFromLeft(240);
    playbackBPMLabel->setBounds(speedSection.removeFromLeft(80));
    playbackBPMValueLabel->setBounds(speedSection.removeFromLeft(35));
    playbackBPMSlider->setBounds(speedSection.removeFromLeft(65));
    useGlobalPlaybackBPMButton->setBounds(speedSection);
    bounds.removeFromLeft(spacing);
    
    // Loop - 90px
    loopSectionButton->setBounds(bounds.removeFromLeft(90));
    bounds.removeFromLeft(spacing);
    
    // Name - remaining
    auto nameSection = bounds;
    sectionNameLabel->setBounds(nameSection.removeFromLeft(45));
    sectionNameEditor->setBounds(nameSection);
}

void SectionBar::selectSection(int sectionIndex)
{
    int numSections = sectionManager.getNumSections();
    if (sectionIndex < 0 || sectionIndex >= numSections)
        return;
    
    selectedSectionIndex = sectionIndex;
    updateComponentsForSelectedSection();
    sendChangeMessage();
}

void SectionBar::updateComponentsForSelectedSection()
{
    auto* section = sectionManager.getSection(selectedSectionIndex);
    if (!section)
        return;
    
    int numSections = sectionManager.getNumSections();
    sectionInfoLabel->setText("Section " + juce::String(selectedSectionIndex + 1) + "/" + juce::String(numSections),
                              juce::dontSendNotification);
    
    // Time signature
    const auto& timeSigs = getCommonTimeSignatures();
    int timeSigIndex = 0;
    for (int i = 0; i < static_cast<int>(timeSigs.size()); ++i)
    {
        if (timeSigs[i].numerator == section->numerator &&
            timeSigs[i].denominator == section->denominator)
        {
            timeSigIndex = i;
            break;
        }
    }
    timeSigCombo->setSelectedItemIndex(timeSigIndex, juce::dontSendNotification);
    
    // Bars
    barsSlider->setValue(section->numBars, juce::dontSendNotification);
    barsValueLabel->setText(juce::String(section->numBars), juce::dontSendNotification);
    
    // Grid BPM
    bool useGlobalGrid = (section->bpm <= 0.0);
    useGlobalBPMButton->setToggleState(useGlobalGrid, juce::dontSendNotification);
    
    if (!useGlobalGrid)
    {
        bpmSlider->setValue(section->bpm, juce::dontSendNotification);
        bpmValueLabel->setText(juce::String(static_cast<int>(section->bpm)), juce::dontSendNotification);
        // Only enable if section bar is enabled (BAR mode)
        bpmSlider->setEnabled(isEnabled);
    }
    else
    {
        double globalBPM = sectionManager.getGlobalBPM();
        bpmSlider->setValue(globalBPM, juce::dontSendNotification);
        bpmValueLabel->setText(juce::String(static_cast<int>(globalBPM)), juce::dontSendNotification);
        bpmSlider->setEnabled(false);
    }
    
    // Playback BPM
    bool useGlobalPlayback = (section->playbackBPM <= 0.0);
    useGlobalPlaybackBPMButton->setToggleState(useGlobalPlayback, juce::dontSendNotification);
    
    if (!useGlobalPlayback)
    {
        playbackBPMSlider->setValue(section->playbackBPM, juce::dontSendNotification);
        playbackBPMValueLabel->setText(juce::String(static_cast<int>(section->playbackBPM)), juce::dontSendNotification);
        // Only enable if section bar is enabled (BAR mode)
        playbackBPMSlider->setEnabled(isEnabled);
    }
    else
    {
        double globalBPM = sectionManager.getGlobalBPM();
        playbackBPMSlider->setValue(globalBPM, juce::dontSendNotification);
        playbackBPMValueLabel->setText(juce::String(static_cast<int>(globalBPM)), juce::dontSendNotification);
        playbackBPMSlider->setEnabled(false);
    }
    
    sectionNameEditor->setText(section->name, juce::dontSendNotification);
    
    // Only enable navigation and remove if section bar is enabled (BAR mode)
    if (isEnabled)
    {
        prevSectionButton->setEnabled(selectedSectionIndex > 0);
        nextSectionButton->setEnabled(selectedSectionIndex < numSections - 1);
        removeSectionButton->setEnabled(numSections > 1);
    }
    else
    {
        prevSectionButton->setEnabled(false);
        nextSectionButton->setEnabled(false);
        removeSectionButton->setEnabled(false);
    }
}

void SectionBar::enableSectionLoop()
{
    auto* section = sectionManager.getSection(selectedSectionIndex);
    if (!section)
        return;
    
    // Calculate section time boundaries
    double sectionStartTime = sectionManager.getSectionStartTime(selectedSectionIndex);
    double sectionBPM = sectionManager.getEffectiveBPM(selectedSectionIndex);
    double secondsPerBeat = 60.0 / sectionBPM;
    double secondsPerBar = secondsPerBeat * section->numerator;
    double sectionEndTime = sectionStartTime + (section->numBars * secondsPerBar);
    
    // Get MultiTrackContainer through processor
    if (auto* editor = processor.getActiveEditor())
    {
        if (auto* mainComp = dynamic_cast<MainComponent*>(editor))
        {
            if (auto* container = mainComp->getMultiTrackContainer())
            {
                // Set selection range to section boundaries
                container->setSelectionStart(sectionStartTime);
                container->setSelectionEnd(sectionEndTime);
                container->setLoopStart(sectionStartTime);
                container->setLoopEnd(sectionEndTime);
                
                // Enable loop if not already enabled
                if (!container->isLoopEnabled())
                    container->toggleLoop();
                
                // If currently playing, restart from section start
                if (container->isPlaying())
                {
                    container->setPlayheadPosition(sectionStartTime);
                }
            }
        }
    }
}

void SectionBar::disableSectionLoop()
{
    if (auto* editor = processor.getActiveEditor())
    {
        if (auto* mainComp = dynamic_cast<MainComponent*>(editor))
        {
            if (auto* container = mainComp->getMultiTrackContainer())
            {
                if (container->isLoopEnabled())
                    container->toggleLoop();
            }
        }
    }
}


void SectionBar::updateSelectedSection()
{
    auto* section = sectionManager.getSection(selectedSectionIndex);
    if (!section)
        return;
    
    int newBars = static_cast<int>(barsSlider->getValue());
    double newGridBPM = useGlobalBPMButton->getToggleState() ? 0.0 : bpmSlider->getValue();
    double newPlaybackBPM = useGlobalPlaybackBPMButton->getToggleState() ? 0.0 : playbackBPMSlider->getValue();
    juce::String newName = sectionNameEditor->getText();
    
    bool changed = false;
    
    if (section->numBars != newBars)
    {
        section->numBars = newBars;
        changed = true;
    }
    
    if (std::abs(section->bpm - newGridBPM) > 0.01)
    {
        section->bpm = newGridBPM;
        changed = true;
    }
    
    if (std::abs(section->playbackBPM - newPlaybackBPM) > 0.01)
    {
        section->playbackBPM = newPlaybackBPM;
        changed = true;
    }
    
    if (section->name != newName)
    {
        section->name = newName;
        changed = true;
    }
    
    if (changed)
        sectionManager.sendChangeMessage();
}

void SectionBar::addSection()
{
    int timeSigIndex = timeSigCombo->getSelectedItemIndex();
    const auto& timeSigs = getCommonTimeSignatures();
    
    if (timeSigIndex < 0 || timeSigIndex >= static_cast<int>(timeSigs.size()))
        return;
    
    const auto& timeSig = timeSigs[timeSigIndex];
    int numBars = static_cast<int>(barsSlider->getValue());
    double bpm = useGlobalBPMButton->getToggleState() ? 0.0 : bpmSlider->getValue();
    
    // Create new section
    Section newSection;
    newSection.numerator = timeSig.numerator;
    newSection.denominator = timeSig.denominator;
    newSection.numBars = numBars;
    newSection.bpm = bpm;
    newSection.name = sectionNameEditor->getText();
    
    sectionManager.addSection(newSection);
    
    selectedSectionIndex = sectionManager.getNumSections() - 1;
    updateComponentsForSelectedSection();
    sendChangeMessage();
}



void SectionBar::removeSection()
{
    if (sectionManager.getNumSections() <= 1)
        return;
    
    int newSelection = selectedSectionIndex;
    if (newSelection >= sectionManager.getNumSections() - 1)
        newSelection = sectionManager.getNumSections() - 2;
    
    sectionManager.removeSection(selectedSectionIndex);
    selectSection(newSelection);
}

void SectionBar::navigateToPrevSection()
{
    if (selectedSectionIndex > 0)
        selectSection(selectedSectionIndex - 1);
}

void SectionBar::navigateToNextSection()
{
    if (selectedSectionIndex < sectionManager.getNumSections() - 1)
        selectSection(selectedSectionIndex + 1);
}

void SectionBar::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &sectionManager)
    {
        // Section manager changed, update UI
        updateComponentsForSelectedSection();
    }
}

void SectionBar::setSectionBarEnabled(bool enabled)
{
    isEnabled = enabled;
    
    // =========================================================================
    // BAR-RELATED CONTROLS - Always disable in TIME mode, conditionally enable in BAR mode
    // =========================================================================
    
    // Add/Remove buttons - always controlled by enabled state
    if (addSectionButton)
        addSectionButton->setEnabled(enabled);
    
    if (removeSectionButton)
        removeSectionButton->setEnabled(enabled && sectionManager.getNumSections() > 1);
    
    // Global Speed checkbox - always controlled by enabled state
    if (useGlobalPlaybackBPMButton)
        useGlobalPlaybackBPMButton->setEnabled(enabled);
    
    // Loop Section checkbox - always controlled by enabled state
    if (loopSectionButton)
        loopSectionButton->setEnabled(enabled);
    
    // =========================================================================
    // OTHER CONTROLS - Conditionally enable based on their internal state
    // =========================================================================
    
    if (enabled)
    {
        // BAR MODE - Enable all controls appropriately
        
        // Navigation buttons - always enable, will be updated by updateComponentsForSelectedSection
        if (prevSectionButton)
            prevSectionButton->setEnabled(true);
        
        if (nextSectionButton)
            nextSectionButton->setEnabled(true);
        
        // Time signature combo - always enabled in BAR mode
        if (timeSigCombo)
            timeSigCombo->setEnabled(true);
        
        // Bars slider - always enabled in BAR mode
        if (barsSlider)
            barsSlider->setEnabled(true);
        
        // Grid BPM - always enabled in BAR mode
        if (useGlobalBPMButton)
            useGlobalBPMButton->setEnabled(true);
        
        // Section name editor - always enabled in BAR mode
        if (sectionNameEditor)
            sectionNameEditor->setEnabled(true);
        
        // BPM sliders - need to check current section state
        auto* section = sectionManager.getSection(selectedSectionIndex);
        if (section)
        {
            // Grid BPM slider - conditional on "Use Global" state
            bool useGlobalGrid = (section->bpm <= 0.0);
            if (bpmSlider)
                bpmSlider->setEnabled(!useGlobalGrid);
            
            // Playback BPM slider - conditional on "Global Speed" state
            bool useGlobalPlayback = (section->playbackBPM <= 0.0);
            if (playbackBPMSlider)
                playbackBPMSlider->setEnabled(!useGlobalPlayback);
        }
        else
        {
            // If no section available, enable sliders by default
            if (bpmSlider)
                bpmSlider->setEnabled(true);
            if (playbackBPMSlider)
                playbackBPMSlider->setEnabled(true);
        }
        
        // Let updateComponentsForSelectedSection fine-tune the enabled states
        // based on section count and position
    }
    else
    {
        // TIME MODE - Disable everything
        if (prevSectionButton)
            prevSectionButton->setEnabled(false);
        
        if (nextSectionButton)
            nextSectionButton->setEnabled(false);
        
        if (timeSigCombo)
            timeSigCombo->setEnabled(false);
        
        if (barsSlider)
            barsSlider->setEnabled(false);
        
        if (bpmSlider)
            bpmSlider->setEnabled(false);
        
        if (useGlobalBPMButton)
            useGlobalBPMButton->setEnabled(false);
        
        if (playbackBPMSlider)
            playbackBPMSlider->setEnabled(false);
        
        if (sectionNameEditor)
            sectionNameEditor->setEnabled(false);
    }
    
    // Set alpha for all labels to show disabled state
    float alpha = enabled ? 1.0f : 0.4f;
    
    if (sectionInfoLabel)
        sectionInfoLabel->setAlpha(alpha);
    
    if (timeSigLabel)
        timeSigLabel->setAlpha(alpha);
    
    if (barsLabel)
        barsLabel->setAlpha(alpha);
    
    if (barsValueLabel)
        barsValueLabel->setAlpha(alpha);
    
    if (bpmLabel)
        bpmLabel->setAlpha(alpha);
    
    if (bpmValueLabel)
        bpmValueLabel->setAlpha(alpha);
    
    if (playbackBPMLabel)
        playbackBPMLabel->setAlpha(alpha);
    
    if (playbackBPMValueLabel)
        playbackBPMValueLabel->setAlpha(alpha);
    
    if (sectionNameLabel)
        sectionNameLabel->setAlpha(alpha);
    
    // Force repaint to show the overlay
    repaint();
}
