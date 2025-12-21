#include "SamplesManagerWindow.h"
#include "DrumMixer.h"
#include "DrumMixerChannel.h"
#include "SimpleEQ.h"
#include "DrumCompressor.h"
#include "ReverbProcessor.h"
#include "ColourPalette.h"
#include "EQPresetManager.h"
#include "../../PluginProcessor.h"

//==============================================================================
// MixerChannelComponent Implementation
//==============================================================================

MixerChannelComponent::MixerChannelComponent(const juce::String& name, DrumMixerChannel& channel, bool isKick1)
: channelName(name), mixerChannel(channel), isKick1Channel(isKick1)
{
    // Name label
    nameLabel.setText(channelName, juce::dontSendNotification);
    nameLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setColour(juce::Label::textColourId, ColourPalette::cyanAccent);
    addAndMakeVisible(nameLabel);

    // Kick alternation toggle (only for Kick1)
    if (isKick1Channel)
    {
        alternToggle.setButtonText("Altern");
        alternToggle.setColour(juce::ToggleButton::textColourId, ColourPalette::warningOrange);
        alternToggle.setColour(juce::ToggleButton::tickColourId, ColourPalette::warningOrange);
        alternToggle.onStateChange = [this]() {
            if (onKickAlternationChanged)
                onKickAlternationChanged(alternToggle.getToggleState());
        };
        addAndMakeVisible(alternToggle);
    }

    // Volume - horizontal
    setupHorizontalSlider(volumeSlider, volumeLabel, "Vol", 0.0, 1.0, 0.8);
    volumeSlider.onValueChange = [this]() {
        mixerChannel.setVolume(static_cast<float>(volumeSlider.getValue()));
    };

    // Reverb Send - horizontal
    setupHorizontalSlider(reverbSendSlider, reverbSendLabel, "Rev", 0.0, 1.0, 0.0);
    reverbSendSlider.onValueChange = [this]() {
        mixerChannel.setReverbSend(static_cast<float>(reverbSendSlider.getValue()));
    };

    // 3-band EQ Group with VERTICAL sliders
    eqGroup.setText("EQ");
    eqGroup.setTextLabelPosition(juce::Justification::centredTop);
    eqGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    eqGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(eqGroup);

    // Low EQ - vertical
    setupVerticalSlider(lowGainSlider, lowGainLabel, "Lo", -12.0, 12.0, 0.0);
    lowGainSlider.onValueChange = [this]() {
        mixerChannel.getEQ().setGain(SimpleEQ::Low, static_cast<float>(lowGainSlider.getValue()));
    };

    // Mid EQ - vertical
    setupVerticalSlider(midGainSlider, midGainLabel, "Mid", -12.0, 12.0, 0.0);
    midGainSlider.onValueChange = [this]() {
        mixerChannel.getEQ().setGain(SimpleEQ::Mid, static_cast<float>(midGainSlider.getValue()));
    };

    // High EQ - vertical
    setupVerticalSlider(highGainSlider, highGainLabel, "Hi", -12.0, 12.0, 0.0);
    highGainSlider.onValueChange = [this]() {
        mixerChannel.getEQ().setGain(SimpleEQ::High, static_cast<float>(highGainSlider.getValue()));
    };

    // Compressor Group with HORIZONTAL sliders
    compGroup.setText("Comp");
    compGroup.setTextLabelPosition(juce::Justification::centredTop);
    compGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    compGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(compGroup);

    // Compressor Enable
    compEnableToggle.setButtonText("On");
    compEnableToggle.setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    compEnableToggle.setColour(juce::ToggleButton::tickColourId, ColourPalette::successGreen);
    compEnableToggle.onStateChange = [this]() {
        mixerChannel.getCompressor().setEnabled(compEnableToggle.getToggleState());
    };
    addAndMakeVisible(compEnableToggle);

    // Threshold - compact horizontal
    setupCompactSlider(thresholdSlider, -60.0, 0.0, -20.0);
    thresholdSlider.onValueChange = [this]() {
        mixerChannel.getCompressor().setThreshold(static_cast<float>(thresholdSlider.getValue()));
    };
    thresholdLabel.setText("Thr", juce::dontSendNotification);
    thresholdLabel.setFont(juce::Font(9.0f));
    thresholdLabel.setJustificationType(juce::Justification::centredLeft);
    thresholdLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(thresholdLabel);

    // Ratio - compact horizontal
    setupCompactSlider(ratioSlider, 1.0, 20.0, 4.0);
    ratioSlider.setSkewFactorFromMidPoint(4.0);
    ratioSlider.onValueChange = [this]() {
        mixerChannel.getCompressor().setRatio(static_cast<float>(ratioSlider.getValue()));
    };
    ratioLabel.setText("Rat", juce::dontSendNotification);
    ratioLabel.setFont(juce::Font(9.0f));
    ratioLabel.setJustificationType(juce::Justification::centredLeft);
    ratioLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(ratioLabel);

    // Attack - compact horizontal
    setupCompactSlider(attackSlider, 0.1, 100.0, 10.0);
    attackSlider.setSkewFactorFromMidPoint(10.0);
    attackSlider.onValueChange = [this]() {
        mixerChannel.getCompressor().setAttack(static_cast<float>(attackSlider.getValue()));
    };
    attackLabel.setText("Att", juce::dontSendNotification);
    attackLabel.setFont(juce::Font(9.0f));
    attackLabel.setJustificationType(juce::Justification::centredLeft);
    attackLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(attackLabel);

    // Release - compact horizontal
    setupCompactSlider(releaseSlider, 10.0, 500.0, 100.0);
    releaseSlider.setSkewFactorFromMidPoint(100.0);
    releaseSlider.onValueChange = [this]() {
        mixerChannel.getCompressor().setRelease(static_cast<float>(releaseSlider.getValue()));
    };
    releaseLabel.setText("Rel", juce::dontSendNotification);
    releaseLabel.setFont(juce::Font(9.0f));
    releaseLabel.setJustificationType(juce::Justification::centredLeft);
    releaseLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(releaseLabel);

    // Makeup Gain - compact horizontal
    setupCompactSlider(makeupSlider, 0.0, 24.0, 0.0);
    makeupSlider.onValueChange = [this]() {
        mixerChannel.getCompressor().setMakeupGain(static_cast<float>(makeupSlider.getValue()));
    };
    makeupLabel.setText("Gain", juce::dontSendNotification);
    makeupLabel.setFont(juce::Font(9.0f));
    makeupLabel.setJustificationType(juce::Justification::centredLeft);
    makeupLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(makeupLabel);

    // Solo/Mute buttons
    setupButton(soloButton, "S");
    soloButton.setClickingTogglesState(true);
    soloButton.onClick = [this]() {
        mixerChannel.setSolo(soloButton.getToggleState());
    };

    setupButton(muteButton, "M");
    muteButton.setClickingTogglesState(true);
    muteButton.onClick = [this]() {
        mixerChannel.setMute(muteButton.getToggleState());
    };
}

void MixerChannelComponent::paint(juce::Graphics& g)
{
    // Semi-transparent background (0.85 opacity) to show background image through
    g.setColour(ColourPalette::panelBackground.withAlpha(0.85f));
    g.fillRect(getLocalBounds());

    g.setColour(ColourPalette::borderColour);
    g.drawRect(getLocalBounds(), 1);
}

void MixerChannelComponent::resized()
{
    auto area = getLocalBounds().reduced(5);

    // Name at top (24px)
    auto nameRow = area.removeFromTop(24);

    if (isKick1Channel)
    {
        // Split: name on left, Altern toggle on right
        nameLabel.setBounds(nameRow.removeFromLeft(nameRow.getWidth() / 2));
        alternToggle.setBounds(nameRow);
    }
    else
    {
        nameLabel.setBounds(nameRow);
    }

    area.removeFromTop(4);

    // Volume and Reverb side by side (44px total)
    auto volRevRow = area.removeFromTop(44);
    int halfWidth = volRevRow.getWidth() / 2;

    auto volArea = volRevRow.removeFromLeft(halfWidth);
    volumeLabel.setBounds(volArea.removeFromTop(14));
    volumeSlider.setBounds(volArea);

    reverbSendLabel.setBounds(volRevRow.removeFromTop(14));
    reverbSendSlider.setBounds(volRevRow);

    area.removeFromTop(4);

    // 3-band EQ with VERTICAL sliders (160px)
    auto eqArea = area.removeFromTop(160);
    eqGroup.setBounds(eqArea);
    auto eqContent = eqArea.reduced(5, 16);
    int eqBandWidth = eqContent.getWidth() / 3;

    auto lowArea = eqContent.removeFromLeft(eqBandWidth);
    lowGainLabel.setBounds(lowArea.removeFromBottom(14));
    lowGainSlider.setBounds(lowArea.reduced(6, 0));

    auto midArea = eqContent.removeFromLeft(eqBandWidth);
    midGainLabel.setBounds(midArea.removeFromBottom(14));
    midGainSlider.setBounds(midArea.reduced(6, 0));

    highGainLabel.setBounds(eqContent.removeFromBottom(14));
    highGainSlider.setBounds(eqContent.reduced(6, 0));

    area.removeFromTop(4);

    // Compressor with HORIZONTAL sliders (120px)
    auto compArea = area.removeFromTop(120);
    compGroup.setBounds(compArea);
    auto compContent = compArea.reduced(5, 16);

    // Enable toggle (16px)
    compEnableToggle.setBounds(compContent.removeFromTop(16));
    compContent.removeFromTop(3);

    // 5 rows of label + slider (each ~17px)
    int rowHeight = 17;
    int labelWidth = 30;

    // Threshold row
    auto thrRow = compContent.removeFromTop(rowHeight);
    thresholdLabel.setBounds(thrRow.removeFromLeft(labelWidth));
    thresholdSlider.setBounds(thrRow);

    // Ratio row
    auto ratRow = compContent.removeFromTop(rowHeight);
    ratioLabel.setBounds(ratRow.removeFromLeft(labelWidth));
    ratioSlider.setBounds(ratRow);

    // Attack row
    auto attRow = compContent.removeFromTop(rowHeight);
    attackLabel.setBounds(attRow.removeFromLeft(labelWidth));
    attackSlider.setBounds(attRow);

    // Release row
    auto relRow = compContent.removeFromTop(rowHeight);
    releaseLabel.setBounds(relRow.removeFromLeft(labelWidth));
    releaseSlider.setBounds(relRow);

    // Makeup row
    auto mkRow = compContent.removeFromTop(rowHeight);
    makeupLabel.setBounds(mkRow.removeFromLeft(labelWidth));
    makeupSlider.setBounds(mkRow);

    area.removeFromTop(4);

    // Solo/Mute buttons at bottom (remaining ~30px)
    auto buttonRow = area;
    soloButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(3, 0));
    muteButton.setBounds(buttonRow.reduced(3, 0));
}

void MixerChannelComponent::updateFromChannel()
{
    volumeSlider.setValue(mixerChannel.getVolume(), juce::dontSendNotification);
    reverbSendSlider.setValue(mixerChannel.getReverbSend(), juce::dontSendNotification);
    soloButton.setToggleState(mixerChannel.isSoloed(), juce::dontSendNotification);
    muteButton.setToggleState(mixerChannel.isMuted(), juce::dontSendNotification);

    // Update 3-band EQ values
    auto& eq = mixerChannel.getEQ();
    lowGainSlider.setValue(eq.getGain(SimpleEQ::Low), juce::dontSendNotification);
    midGainSlider.setValue(eq.getGain(SimpleEQ::Mid), juce::dontSendNotification);
    highGainSlider.setValue(eq.getGain(SimpleEQ::High), juce::dontSendNotification);

    // Update Compressor values
    auto& comp = mixerChannel.getCompressor();
    compEnableToggle.setToggleState(comp.isEnabled(), juce::dontSendNotification);
    thresholdSlider.setValue(comp.getThreshold(), juce::dontSendNotification);
    ratioSlider.setValue(comp.getRatio(), juce::dontSendNotification);
    attackSlider.setValue(comp.getAttack(), juce::dontSendNotification);
    releaseSlider.setValue(comp.getRelease(), juce::dontSendNotification);
    makeupSlider.setValue(comp.getMakeupGain(), juce::dontSendNotification);
}

void MixerChannelComponent::setKickAlternationEnabled(bool enabled)
{
    if (isKick1Channel)
    {
        alternToggle.setToggleState(enabled, juce::dontSendNotification);
    }
}

void MixerChannelComponent::setupHorizontalSlider(juce::Slider& slider, juce::Label& label,
                                                  const juce::String& text, double min, double max,
                                                  double defaultValue)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 32, 12);
    slider.setRange(min, max, 0.01);
    slider.setValue(defaultValue);
    slider.setColour(juce::Slider::thumbColourId, ColourPalette::primaryBlue);
    slider.setColour(juce::Slider::trackColourId, ColourPalette::borderColour);
    slider.setColour(juce::Slider::backgroundColourId, ColourPalette::inputBackground);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(9.0f));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(label);
}

void MixerChannelComponent::setupVerticalSlider(juce::Slider& slider, juce::Label& label,
                                                const juce::String& text, double min, double max,
                                                double defaultValue)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(min, max, 0.1);
    slider.setValue(defaultValue);
    slider.setColour(juce::Slider::thumbColourId, ColourPalette::primaryBlue);
    slider.setColour(juce::Slider::trackColourId, ColourPalette::borderColour);
    slider.setColour(juce::Slider::backgroundColourId, ColourPalette::inputBackground);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(8.0f));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(label);
}

void MixerChannelComponent::setupCompactSlider(juce::Slider& slider, double min, double max, double defaultValue)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(min, max, 0.1);
    slider.setValue(defaultValue);
    slider.setColour(juce::Slider::thumbColourId, ColourPalette::primaryBlue);
    slider.setColour(juce::Slider::trackColourId, ColourPalette::borderColour);
    slider.setColour(juce::Slider::backgroundColourId, ColourPalette::inputBackground);
    addAndMakeVisible(slider);
}

void MixerChannelComponent::setupButton(juce::TextButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    button.setColour(juce::TextButton::buttonOnColourId, ColourPalette::warningOrange);
    button.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(button);
}

//==============================================================================
// SamplesManagerWindow Implementation
//==============================================================================

SamplesManagerWindow::SamplesManagerWindow(DrumMixer& mixer, DrumLibraryManager& libraryManager, DrumGrooveProcessor& proc)
: drumMixer(mixer), libManager(libraryManager), processor(proc),
downloadProgress(currentProgress)
{
    setSize(1500, 930);

    // Load background image from VST3 bundle Resources folder
    // Use currentExecutableFile and search multiple paths (like MainComponent does)
    juce::Array<juce::File> searchPaths;
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);

    // Path 1: VST3 bundle structure - executable is in Contents/x86_64-win/ or Contents/x86_64-linux/
    // Resources are at Contents/Resources/
    searchPaths.add(executableFile.getParentDirectory()  // x86_64-win or x86_64-linux
    .getParentDirectory()                 // Contents
    .getChildFile("Resources")
    .getChildFile("background")
    .getChildFile("sample.png"));

    // Path 2: Alternative structure (one level up)
    searchPaths.add(executableFile.getParentDirectory()
    .getParentDirectory()
    .getParentDirectory()
    .getChildFile("Resources")
    .getChildFile("background")
    .getChildFile("sample.png"));

    // Path 3: Next to executable (for development)
    searchPaths.add(executableFile.getParentDirectory()
    .getChildFile("Resources")
    .getChildFile("background")
    .getChildFile("sample.png"));

    // Path 4: Fallback to user documents
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("DrumGroovePro")
    .getChildFile("Resources")
    .getChildFile("background")
    .getChildFile("sample.png"));

    #if JUCE_LINUX
    // Linux additional path: ~/.vst3/DrumGroovePro.vst3/Contents/Resources/
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
    .getChildFile(".vst3")
    .getChildFile("DrumGroovePro.vst3")
    .getChildFile("Contents")
    .getChildFile("Resources")
    .getChildFile("background")
    .getChildFile("sample.png"));
    #endif

    bool imageLoaded = false;
    for (const auto& path : searchPaths)
    {
        DBG("Trying sample.png path: " + path.getFullPathName());
        if (path.existsAsFile())
        {
            backgroundImage = juce::ImageCache::getFromFile(path);
            if (backgroundImage.isValid())
            {
                DBG("Sample background loaded: " + path.getFullPathName() +
                " (" + juce::String(backgroundImage.getWidth()) + "x" +
                juce::String(backgroundImage.getHeight()) +
                ", hasAlpha=" + juce::String(backgroundImage.hasAlphaChannel() ? "true" : "false") + ")");
                imageLoaded = true;
                break;
            }
        }
    }

    if (!imageLoaded)
    {
        DBG("Sample background image not found in any search path");
        DBG("Executable location: " + executableFile.getFullPathName());
    }

    // Title - CENTERED and CYAN (large font for 100px area)
    titleLabel.setText("Samples Manager", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(32.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, ColourPalette::cyanAccent);
    addAndMakeVisible(titleLabel);

    // Close button
    closeButton.setButtonText("X");
    closeButton.setColour(juce::TextButton::buttonColourId, ColourPalette::errorRed);
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeButton.onClick = [this]() {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton);

    // Initialize preset manager
    presetManager = std::make_unique<EQPresetManager>();

    setupAudioModeSection();
    setupLibrarySection();
    setupMixerSection();
    setupMasterEQSection();
    setupPresetSection();
    setupDownloadSection();
    refreshLibraryList();
    updateSamplesStatus();
    setupHumanizationSection();

    refreshPresetList();
    updateSamplesStatus();
    updateFromMixer();
}

SamplesManagerWindow::~SamplesManagerWindow()
{
    if (downloader)
    {
        downloader->cancelDownload();
    }
}

void SamplesManagerWindow::paint(juce::Graphics& g)
{
    // Fill with pure black background first (visible when window is resized larger than image)
    g.fillAll(juce::Colours::black);

    // Draw background image centered at original size (not scaled)
    if (backgroundImage.isValid())
    {
        int imgWidth = backgroundImage.getWidth();
        int imgHeight = backgroundImage.getHeight();
        int x = (getWidth() - imgWidth) / 2;
        int y = (getHeight() - imgHeight) / 2;

        g.drawImageAt(backgroundImage, x, y);
    }

    // Draw semi-transparent overlay (0.7 opacity) on top
    g.setColour(ColourPalette::mainBackground.withAlpha(0.7f));
    g.fillAll();
}

void SamplesManagerWindow::resized()
{
    auto area = getLocalBounds().reduced(6);

    // 1. TITLE ROW (100px) - centered
    auto titleRow = area.removeFromTop(100);
    closeButton.setBounds(titleRow.removeFromRight(30).reduced(0, 35));
    titleLabel.setBounds(titleRow);

    area.removeFromTop(4);

    // 2. AUDIO PLAYBACK + DRUM LIBRARY + HUMANIZATION ROW (100px)
    auto topRow = area.removeFromTop(100);

    // Audio Playback section (left ~28%)
    auto audioModeArea = topRow.removeFromLeft(static_cast<int>(topRow.getWidth() * 0.28f));
    audioModeGroup.setBounds(audioModeArea);

    // Center the controls within audio mode section
    auto audioContent = audioModeArea.reduced(10, 20);
    int audioControlsWidth = 340;
    int audioLeftPadding = (audioContent.getWidth() - audioControlsWidth) / 2;
    if (audioLeftPadding > 0) audioContent.removeFromLeft(audioLeftPadding);

    // First row: Samples label, Loaded checkbox, Load Samples button, Audio Out button (all on same row)
    auto audioRow1 = audioContent.removeFromTop(26);
    audioModeLabel.setBounds(audioRow1.removeFromLeft(58));
    audioModeToggle.setBounds(audioRow1.removeFromLeft(78));
    audioRow1.removeFromLeft(5);
    loadSamplesButton.setBounds(audioRow1.removeFromLeft(105));
    audioRow1.removeFromLeft(5);
    midiModeButton.setBounds(audioRow1.removeFromLeft(85));  // Audio Out / MIDI Out button next to Load Samples

    audioContent.removeFromTop(6);
    samplesStatusLabel.setBounds(audioContent.removeFromTop(18));

    topRow.removeFromLeft(6);

    // Drum Library section (middle ~42%)
    auto libraryArea = topRow.removeFromLeft(static_cast<int>(topRow.getWidth() * 0.60f));
    libraryGroup.setBounds(libraryArea);

    // Center the controls within library section
    auto libContent = libraryArea.reduced(10, 20);
    int libControlsWidth = 480;
    int libLeftPadding = (libContent.getWidth() - libControlsWidth) / 2;
    if (libLeftPadding > 0) libContent.removeFromLeft(libLeftPadding);

    // First row: Library selector
    auto libRow = libContent.removeFromTop(26);
    libraryLabel.setBounds(libRow.removeFromLeft(50));
    libraryCombo.setBounds(libRow.removeFromLeft(140));
    libRow.removeFromLeft(10);

    // Download library selector and buttons
    downloadLibLabel.setBounds(libRow.removeFromLeft(30));
    downloadLibCombo.setBounds(libRow.removeFromLeft(100));
    libRow.removeFromLeft(6);
    downloadButton.setBounds(libRow.removeFromLeft(70));
    libRow.removeFromLeft(4);
    deleteButton.setBounds(libRow.removeFromLeft(60));

    // Download section appears below buttons
    if (downloadSectionVisible)
    {
        libContent.removeFromTop(6);
        auto downloadArea = libContent.removeFromTop(60);  // Allocate 60px height for download section
        downloadSection.setBounds(downloadArea);
        auto dlArea = downloadArea.reduced(2);
        downloadStatusLabel.setBounds(dlArea.removeFromTop(18));
        downloadProgress.setBounds(dlArea.removeFromTop(20));
        dlArea.removeFromTop(2);
        cancelDownloadButton.setBounds(dlArea.removeFromTop(18).removeFromRight(75));
    }

    topRow.removeFromLeft(6);

    // Humanization section (right ~30%)
    humanizationGroup.setBounds(topRow);

    auto humanContent = topRow.reduced(8, 18);
    int sliderHeight = 22;
    int labelWidth = 70;
    int spacing = 4;

    // Velocity row
    auto velRow = humanContent.removeFromTop(sliderHeight);
    velocityHumanLabel.setBounds(velRow.removeFromLeft(labelWidth));
    velocityHumanSlider.setBounds(velRow);

    humanContent.removeFromTop(spacing);

    // Timing row
    auto timRow = humanContent.removeFromTop(sliderHeight);
    timingHumanLabel.setBounds(timRow.removeFromLeft(labelWidth));
    timingHumanSlider.setBounds(timRow);

    humanContent.removeFromTop(spacing);

    // Round Robin row
    auto rrRow = humanContent.removeFromTop(sliderHeight);
    roundRobinLabel.setBounds(rrRow.removeFromLeft(labelWidth));
    roundRobinSlider.setBounds(rrRow);

    area.removeFromTop(4);

    // 3. DRUM PARTS ROW (400px)
    auto mixerRow = area.removeFromTop(400);

    // 7 channels evenly distributed
    int totalSpacing = 6 * 4;  // 6 gaps of 4px
    int channelWidth = (mixerRow.getWidth() - totalSpacing) / 7;

    for (size_t i = 0; i < 7; ++i)
    {
        if (mixerChannels[i])
        {
            mixerChannels[i]->setBounds(mixerRow.removeFromLeft(channelWidth));
        }
        mixerRow.removeFromLeft(4);
    }

    area.removeFromTop(4);

    // 4. BOTTOM ROW (300px) - Presets + 8-band EQ + Master Volume
    auto bottomRow = area;

    // Preset section (left, 160px)
    auto presetArea = bottomRow.removeFromLeft(160);
    presetGroup.setBounds(presetArea);
    auto presetContent = presetArea.reduced(8, 22);
    presetCombo.setBounds(presetContent.removeFromTop(28));
    presetContent.removeFromTop(12);
    auto presetButtons = presetContent.removeFromTop(30);
    savePresetButton.setBounds(presetButtons.removeFromLeft(presetButtons.getWidth() / 2).reduced(2, 0));
    deletePresetButton.setBounds(presetButtons.reduced(2, 0));

    bottomRow.removeFromLeft(8);

    // Master Volume (vertical slider, 80px wide) - on RIGHT
    auto masterVolArea = bottomRow.removeFromRight(80);
    masterGroup.setBounds(masterVolArea);
    auto masterContent = masterVolArea.reduced(8, 22);
    masterVolumeLabel.setBounds(masterContent.removeFromTop(18));
    masterContent.removeFromTop(6);
    masterVolumeSlider.setBounds(masterContent.reduced(14, 0));

    bottomRow.removeFromRight(8);

    // 8-band EQ (remaining center space)
    masterEQGroup.setBounds(bottomRow);
    auto eqContent = bottomRow.reduced(10, 22);
    masterEQEnableToggle.setBounds(eqContent.removeFromTop(24).removeFromLeft(120));
    eqContent.removeFromTop(8);

    int bandWidth = eqContent.getWidth() / 8;
    for (size_t i = 0; i < 8; ++i)
    {
        auto bandArea = eqContent.removeFromLeft(bandWidth);
        eqBands[i].freqLabel.setBounds(bandArea.removeFromTop(18));
        eqBands[i].gainSlider.setBounds(bandArea.reduced(14, 0));
    }
}

void SamplesManagerWindow::setupAudioModeSection()
{
    audioModeGroup.setText("Audio Playback");
    audioModeGroup.setTextLabelPosition(juce::Justification::centredTop);
    audioModeGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    audioModeGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(audioModeGroup);

    audioModeLabel.setText("Samples:", juce::dontSendNotification);
    audioModeLabel.setFont(juce::Font(11.0f));
    audioModeLabel.setJustificationType(juce::Justification::centredLeft);
    audioModeLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(audioModeLabel);

    audioModeToggle.setButtonText("Loaded");
    audioModeToggle.setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    audioModeToggle.setColour(juce::ToggleButton::tickColourId, ColourPalette::successGreen);
    audioModeToggle.setEnabled(false);  // Read-only indicator
    addAndMakeVisible(audioModeToggle);

    samplesStatusLabel.setText("No samples loaded", juce::dontSendNotification);
    samplesStatusLabel.setFont(juce::Font(10.0f));
    samplesStatusLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(samplesStatusLabel);

    loadSamplesButton.setButtonText("Load Samples");
    loadSamplesButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    loadSamplesButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadSamplesButton.onClick = [this]() {
        int selectedIndex = libraryCombo.getSelectedId();
        if (selectedIndex > 0)
        {
            // Show loading indicator
            loadSamplesButton.setButtonText("Loading...");
            loadSamplesButton.setEnabled(false);
            repaint();

            // Use Timer to allow UI to update before loading
            juce::Timer::callAfterDelay(50, [this]() {
                handleLibraryChange();
                loadSamplesButton.setButtonText("Load Samples");
                loadSamplesButton.setEnabled(true);
            });
        }
    };
    addAndMakeVisible(loadSamplesButton);

    // MIDI/Audio Mode toggle button
    midiModeButton.setClickingTogglesState(true);
    midiModeButton.setColour(juce::TextButton::buttonColourId, ColourPalette::primaryBlue);
    midiModeButton.setColour(juce::TextButton::buttonOnColourId, ColourPalette::warningOrange);
    midiModeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    midiModeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    midiModeButton.onClick = [this]() { handleAudioModeToggle(); };
    addAndMakeVisible(midiModeButton);

    // Initialize button state
    updateAudioModeButton();
}

void SamplesManagerWindow::setupLibrarySection()
{
    libraryGroup.setText("Drum Library");
    libraryGroup.setTextLabelPosition(juce::Justification::centredTop);
    libraryGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    libraryGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(libraryGroup);

    libraryLabel.setText("Library:", juce::dontSendNotification);
    libraryLabel.setFont(juce::Font(11.0f));
    libraryLabel.setJustificationType(juce::Justification::centredLeft);
    libraryLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(libraryLabel);

    libraryCombo.setColour(juce::ComboBox::backgroundColourId, ColourPalette::inputBackground);
    libraryCombo.setColour(juce::ComboBox::textColourId, ColourPalette::primaryText);
    // DON'T set onChange - samples should only load when clicking "Load Samples" button
    // libraryCombo.onChange = [this]() { handleLibraryChange(); };
    addAndMakeVisible(libraryCombo);

    // NOW populate library list after setup is complete
    refreshLibraryList();

    // Download library selector - populated from SampleDownloader::getAvailableLibraries()
    downloadLibLabel.setText("Get:", juce::dontSendNotification);
    downloadLibLabel.setFont(juce::Font(11.0f));
    downloadLibLabel.setJustificationType(juce::Justification::centredLeft);
    downloadLibLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(downloadLibLabel);

    downloadLibCombo.setColour(juce::ComboBox::backgroundColourId, ColourPalette::inputBackground);
    downloadLibCombo.setColour(juce::ComboBox::textColourId, ColourPalette::primaryText);

    // Populate with available libraries from SampleDownloader (with file sizes)
    auto availableLibs = SampleDownloader::getAvailableLibraries();
    int itemId = 1;
    for (const auto& lib : availableLibs)
    {
        juce::String displayName = lib.name + " (" + juce::String(lib.expectedSizeMB) + " MB)";
        downloadLibCombo.addItem(displayName, itemId++);
    }
    if (downloadLibCombo.getNumItems() > 0)
        downloadLibCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(downloadLibCombo);

    downloadButton.setButtonText("Download");
    downloadButton.setColour(juce::TextButton::buttonColourId, ColourPalette::primaryBlue);
    downloadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    downloadButton.onClick = [this]() { handleDownloadClick(); };
    addAndMakeVisible(downloadButton);

    deleteButton.setButtonText("Delete");
    deleteButton.setColour(juce::TextButton::buttonColourId, ColourPalette::errorRed);
    deleteButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    deleteButton.onClick = [this]() { handleDeleteClick(); };
    addAndMakeVisible(deleteButton);
}

void SamplesManagerWindow::setupHumanizationSection()
{
    // Humanization group box
    humanizationGroup.setText("Humanization");
    humanizationGroup.setTextLabelPosition(juce::Justification::centredTop);
    humanizationGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    humanizationGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(humanizationGroup);

    // Velocity Humanization slider
    velocityHumanLabel.setText("Velocity", juce::dontSendNotification);
    velocityHumanLabel.setFont(juce::Font(10.0f));
    velocityHumanLabel.setJustificationType(juce::Justification::centred);
    velocityHumanLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(velocityHumanLabel);

    velocityHumanSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velocityHumanSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    velocityHumanSlider.setRange(0.0, 100.0, 1.0);
    velocityHumanSlider.setValue(0.0, juce::dontSendNotification);
    velocityHumanSlider.setTextValueSuffix("%");
    velocityHumanSlider.setColour(juce::Slider::thumbColourId, ColourPalette::cyanAccent);
    velocityHumanSlider.setColour(juce::Slider::trackColourId, ColourPalette::inputBackground);
    velocityHumanSlider.setColour(juce::Slider::textBoxTextColourId, ColourPalette::primaryText);
    velocityHumanSlider.setColour(juce::Slider::textBoxBackgroundColourId, ColourPalette::inputBackground);
    velocityHumanSlider.setColour(juce::Slider::textBoxOutlineColourId, ColourPalette::borderColour);
    velocityHumanSlider.onValueChange = [this]() { handleVelocityHumanizationChanged(); };
    addAndMakeVisible(velocityHumanSlider);

    // Timing Humanization slider
    timingHumanLabel.setText("Timing", juce::dontSendNotification);
    timingHumanLabel.setFont(juce::Font(10.0f));
    timingHumanLabel.setJustificationType(juce::Justification::centred);
    timingHumanLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(timingHumanLabel);

    timingHumanSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timingHumanSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    timingHumanSlider.setRange(0.0, 100.0, 1.0);
    timingHumanSlider.setValue(0.0, juce::dontSendNotification);
    timingHumanSlider.setTextValueSuffix("%");
    timingHumanSlider.setColour(juce::Slider::thumbColourId, ColourPalette::warningOrange);
    timingHumanSlider.setColour(juce::Slider::trackColourId, ColourPalette::inputBackground);
    timingHumanSlider.setColour(juce::Slider::textBoxTextColourId, ColourPalette::primaryText);
    timingHumanSlider.setColour(juce::Slider::textBoxBackgroundColourId, ColourPalette::inputBackground);
    timingHumanSlider.setColour(juce::Slider::textBoxOutlineColourId, ColourPalette::borderColour);
    timingHumanSlider.onValueChange = [this]() { handleTimingHumanizationChanged(); };
    addAndMakeVisible(timingHumanSlider);

    // Round Robin slider
    roundRobinLabel.setText("Round Robin", juce::dontSendNotification);
    roundRobinLabel.setFont(juce::Font(10.0f));
    roundRobinLabel.setJustificationType(juce::Justification::centred);
    roundRobinLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(roundRobinLabel);

    roundRobinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    roundRobinSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    roundRobinSlider.setRange(0.0, 100.0, 1.0);
    roundRobinSlider.setValue(100.0, juce::dontSendNotification);  // Default to 100%
    roundRobinSlider.setTextValueSuffix("%");
    roundRobinSlider.setColour(juce::Slider::thumbColourId, ColourPalette::successGreen);
    roundRobinSlider.setColour(juce::Slider::trackColourId, ColourPalette::inputBackground);
    roundRobinSlider.setColour(juce::Slider::textBoxTextColourId, ColourPalette::primaryText);
    roundRobinSlider.setColour(juce::Slider::textBoxBackgroundColourId, ColourPalette::inputBackground);
    roundRobinSlider.setColour(juce::Slider::textBoxOutlineColourId, ColourPalette::borderColour);
    roundRobinSlider.onValueChange = [this]() { handleRoundRobinChanged(); };
    addAndMakeVisible(roundRobinSlider);

    // Initialize from SampleEngine current values
    auto& sampleEngine = processor.getSampleEngine();
    velocityHumanSlider.setValue(sampleEngine.getVelocityHumanization(), juce::dontSendNotification);
    timingHumanSlider.setValue(sampleEngine.getTimingHumanization(), juce::dontSendNotification);
    roundRobinSlider.setValue(sampleEngine.getRoundRobinAmount(), juce::dontSendNotification);
}

void SamplesManagerWindow::setupMixerSection()
{
    // 7 channels: Kick1 (with Altern), Kick2, Snare, Hi-Hat, Toms, Crash, Rides
    const juce::StringArray channelNames = { "Kick 1", "Kick 2", "Snare", "Hi-Hat", "Toms", "Crash", "Rides" };

    for (int i = 0; i < 7; ++i)
    {
        bool isKick1 = (i == 0);  // First channel is Kick1 with Altern toggle
        mixerChannels[static_cast<size_t>(i)] = std::make_unique<MixerChannelComponent>(
            channelNames[i],
            drumMixer.getChannel(static_cast<DrumMixer::DrumPart>(i)),
                                                                                        isKick1
        );

        // Set up kick alternation callback for Kick1
        if (isKick1)
        {
            mixerChannels[0]->onKickAlternationChanged = [this](bool enabled) {
                handleKickAlternationChanged(enabled);
            };
        }

        addAndMakeVisible(mixerChannels[static_cast<size_t>(i)].get());
    }

    // Master volume
    masterGroup.setText("Master");
    masterGroup.setTextLabelPosition(juce::Justification::centredTop);
    masterGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    masterGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(masterGroup);

    masterVolumeLabel.setText("Vol", juce::dontSendNotification);
    masterVolumeLabel.setFont(juce::Font(11.0f));
    masterVolumeLabel.setJustificationType(juce::Justification::centred);
    masterVolumeLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(masterVolumeLabel);

    masterVolumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 16);
    masterVolumeSlider.setRange(0.0, 2.0, 0.01);
    masterVolumeSlider.setValue(1.0);
    masterVolumeSlider.setColour(juce::Slider::thumbColourId, ColourPalette::primaryBlue);
    masterVolumeSlider.setColour(juce::Slider::trackColourId, ColourPalette::borderColour);
    masterVolumeSlider.setColour(juce::Slider::backgroundColourId, ColourPalette::inputBackground);
    masterVolumeSlider.onValueChange = [this]() {
        processor.getSampleEngine().setMasterGain(static_cast<float>(masterVolumeSlider.getValue()));
    };
    addAndMakeVisible(masterVolumeSlider);
}

void SamplesManagerWindow::setupMasterEQSection()
{
    masterEQGroup.setText("8-Band Master EQ");
    masterEQGroup.setTextLabelPosition(juce::Justification::centredTop);
    masterEQGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    masterEQGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(masterEQGroup);

    masterEQEnableToggle.setButtonText("Enable EQ");
    masterEQEnableToggle.setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    masterEQEnableToggle.setColour(juce::ToggleButton::tickColourId, ColourPalette::successGreen);
    masterEQEnableToggle.onStateChange = [this]() {
        drumMixer.setMasterEQEnabled(masterEQEnableToggle.getToggleState());
    };
    addAndMakeVisible(masterEQEnableToggle);

    const juce::StringArray freqLabels = { "60", "150", "400", "1k", "2.5k", "5k", "10k", "15k" };

    for (int i = 0; i < 8; ++i)
    {
        auto& band = eqBands[static_cast<size_t>(i)];

        band.freqLabel.setText(freqLabels[i], juce::dontSendNotification);
        band.freqLabel.setFont(juce::Font(10.0f));
        band.freqLabel.setJustificationType(juce::Justification::centred);
        band.freqLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
        addAndMakeVisible(band.freqLabel);

        band.gainSlider.setSliderStyle(juce::Slider::LinearVertical);
        band.gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        band.gainSlider.setRange(-12.0, 12.0, 0.1);
        band.gainSlider.setValue(0.0);
        band.gainSlider.setColour(juce::Slider::thumbColourId, ColourPalette::primaryBlue);
        band.gainSlider.setColour(juce::Slider::trackColourId, ColourPalette::borderColour);
        band.gainSlider.setColour(juce::Slider::backgroundColourId, ColourPalette::inputBackground);

        const int bandIndex = i;
        band.gainSlider.onValueChange = [this, bandIndex]() {
            drumMixer.setMasterEQGain(bandIndex, static_cast<float>(eqBands[static_cast<size_t>(bandIndex)].gainSlider.getValue()));
        };

        addAndMakeVisible(band.gainSlider);
    }
}

void SamplesManagerWindow::setupPresetSection()
{
    presetGroup.setText("Presets");
    presetGroup.setTextLabelPosition(juce::Justification::centredTop);
    presetGroup.setColour(juce::GroupComponent::outlineColourId, ColourPalette::borderColour);
    presetGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(presetGroup);

    presetCombo.setColour(juce::ComboBox::backgroundColourId, ColourPalette::inputBackground);
    presetCombo.setColour(juce::ComboBox::textColourId, ColourPalette::primaryText);
    presetCombo.onChange = [this]() { loadSelectedPreset(); };
    addAndMakeVisible(presetCombo);

    savePresetButton.setButtonText("Save");
    savePresetButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    savePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    savePresetButton.onClick = [this]() { saveCurrentAsPreset(); };
    addAndMakeVisible(savePresetButton);

    deletePresetButton.setButtonText("Delete");
    deletePresetButton.setColour(juce::TextButton::buttonColourId, ColourPalette::errorRed);
    deletePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    deletePresetButton.onClick = [this]() { deleteSelectedPreset(); };
    addAndMakeVisible(deletePresetButton);
}

void SamplesManagerWindow::setupDownloadSection()
{
    downloadSection.setOpaque(false);
    addChildComponent(downloadSection);

    downloadStatusLabel.setText("Ready", juce::dontSendNotification);
    downloadStatusLabel.setFont(juce::Font(11.0f));  // Changed from 9.0f to 11.0f - more visible
    downloadStatusLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);  // Changed from secondaryText
    downloadStatusLabel.setJustificationType(juce::Justification::centredLeft);  // ADD THIS
    downloadSection.addAndMakeVisible(downloadStatusLabel);

    downloadProgress.setColour(juce::ProgressBar::backgroundColourId, ColourPalette::inputBackground);
    downloadProgress.setColour(juce::ProgressBar::foregroundColourId, ColourPalette::successGreen);
    downloadProgress.setTextToDisplay("0%");  // ADD THIS - shows percentage
    downloadSection.addAndMakeVisible(downloadProgress);

    cancelDownloadButton.setButtonText("Cancel");
    cancelDownloadButton.setColour(juce::TextButton::buttonColourId, ColourPalette::errorRed);
    cancelDownloadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    cancelDownloadButton.onClick = [this]() { handleCancelDownload(); };
    downloadSection.addAndMakeVisible(cancelDownloadButton);
}

void SamplesManagerWindow::showDownloadSection(bool show)
{
    downloadSectionVisible = show;
    downloadSection.setVisible(show);

    if (show)
    {
        downloadSection.repaint();
        repaint();
    }

    resized();
}

void SamplesManagerWindow::updateFromMixer()
{
    for (auto& channel : mixerChannels)
    {
        if (channel)
            channel->updateFromChannel();
    }

    // Update kick alternation state
    bool kickAltEnabled = processor.getSampleEngine().isKickAlternationEnabled();
    if (mixerChannels[0])
        mixerChannels[0]->setKickAlternationEnabled(kickAltEnabled);

    float masterGain = processor.getSampleEngine().getMasterGain();
    masterVolumeSlider.setValue(masterGain, juce::dontSendNotification);

    for (int i = 0; i < 8; ++i)
    {
        eqBands[static_cast<size_t>(i)].gainSlider.setValue(drumMixer.getMasterEQGain(i), juce::dontSendNotification);
    }
    masterEQEnableToggle.setToggleState(drumMixer.isMasterEQEnabled(), juce::dontSendNotification);

    // Update audio mode button
    updateAudioModeButton();
}

void SamplesManagerWindow::handleKickAlternationChanged(bool enabled)
{
    processor.getSampleEngine().setKickAlternationEnabled(enabled);
    DBG("Kick Alternation: " + juce::String(enabled ? "ON" : "OFF"));
}

void SamplesManagerWindow::handleAudioModeToggle()
{
    bool isAudioMode = midiModeButton.getToggleState();
    processor.setAudioMode(isAudioMode);
    updateAudioModeButton();
    DBG("Audio Mode: " + juce::String(isAudioMode ? "AUDIO" : "MIDI"));
}

void SamplesManagerWindow::updateAudioModeButton()
{
    bool isAudioMode = processor.isAudioMode();
    midiModeButton.setToggleState(isAudioMode, juce::dontSendNotification);

    if (isAudioMode)
    {
        midiModeButton.setButtonText("Audio Out");
    }
    else
    {
        midiModeButton.setButtonText("MIDI Out");
    }
}

void SamplesManagerWindow::refreshLibraryList()
{
    libraryCombo.clear();

    juce::File baseDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("DrumGroovePro").getChildFile("Samples");

    if (!baseDir.exists())
    {
        libraryCombo.addItem("No libraries found", 1);
        return;
    }

    juce::StringArray addedLibraries;
    int itemId = 1;

    // Scan for subdirectories
    juce::Array<juce::File> subDirs;
    baseDir.findChildFiles(subDirs, juce::File::findDirectories, false);

    for (const auto& dir : subDirs)
    {
        juce::String libName = dir.getFileName();

        // Check for SFZ format (has ALL.sfz file)
        juce::File sfzFile = dir.getChildFile("ALL.sfz");
        bool hasSFZ = sfzFile.existsAsFile();

        // Check for DrumGizmo format (has Midimap.xml and at least one kit .xml)
        juce::File midimapXml = dir.getChildFile("Midimap.xml");
        bool hasMidimap = midimapXml.existsAsFile();

        bool hasDrumGizmo = false;
        if (hasMidimap)
        {
            // Look for any .xml file that's not Midimap.xml (that's the kit XML)
            juce::Array<juce::File> xmlFiles;
            dir.findChildFiles(xmlFiles, juce::File::findFiles, false, "*.xml");
            for (const auto& xmlFile : xmlFiles)
            {
                if (xmlFile.getFileName() != "Midimap.xml")
                {
                    hasDrumGizmo = true;
                    break;
                }
            }
        }

        if ((hasSFZ || hasDrumGizmo) && !addedLibraries.contains(libName))
        {
            addedLibraries.add(libName);
            libraryCombo.addItem(libName, itemId++);
        }
    }

    if (libraryCombo.getNumItems() == 0)
    {
        libraryCombo.addItem("No libraries found", 1);
        return;
    }

    if (libraryCombo.getNumItems() > 0)
    {
        libraryCombo.setSelectedId(1, juce::dontSendNotification);
    }
}

void SamplesManagerWindow::handleLibraryChange()
{
    juce::String selectedLib = libraryCombo.getText();
    if (selectedLib.isEmpty() || selectedLib == "No libraries found")
        return;

    if (processor.getSampleEngine().loadLibraryByName(selectedLib))
    {
        samplesStatusLabel.setText("Loaded: " + selectedLib, juce::dontSendNotification);
        audioModeToggle.setToggleState(true, juce::dontSendNotification);
    }
    else
    {
        samplesStatusLabel.setText("Failed to load " + selectedLib, juce::dontSendNotification);
        audioModeToggle.setToggleState(false, juce::dontSendNotification);
    }
}

void SamplesManagerWindow::handleDownloadClick()
{
    // Get selected library from download combo (format: "Name (XXX MB)")
    juce::String selectedLibDisplay = downloadLibCombo.getText();
    if (selectedLibDisplay.isEmpty())
    {
        samplesStatusLabel.setText("Please select a library to download", juce::dontSendNotification);
        return;
    }

    // Extract library name by removing the size suffix
    juce::String selectedLibName = selectedLibDisplay.upToFirstOccurrenceOf(" (", false, false);

    // Find the library info
    auto availableLibs = SampleDownloader::getAvailableLibraries();
    juce::String libraryFolderName;

    for (const auto& lib : availableLibs)
    {
        if (lib.name == selectedLibName)
        {
            libraryFolderName = lib.folderName;
            break;
        }
    }

    if (libraryFolderName.isEmpty())
    {
        samplesStatusLabel.setText("Unknown library selected", juce::dontSendNotification);
        return;
    }

    showDownloadSection(true);
    downloadStatusLabel.setText("Starting download of " + selectedLibName + "...", juce::dontSendNotification);
    currentProgress = 0.0;

    // Show downloading indicator
    downloadButton.setButtonText("Downloading...");
    downloadButton.setEnabled(false);

    downloader = std::make_unique<SampleDownloader>();
    downloader->startDownload(libraryFolderName,
                              [this](double p, juce::String s) { onDownloadProgress(p, s); },
                              [this](bool success, juce::String msg) { onDownloadComplete(success, msg); }
    );
}

void SamplesManagerWindow::handleDeleteClick()
{
    juce::String selectedLib = libraryCombo.getText();
    if (selectedLib.isEmpty() || selectedLib == "No libraries found")
        return;

    auto result = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Delete Library",
        "Are you sure you want to delete '" + selectedLib + "'?\nThis cannot be undone.",
        "Delete",
        "Cancel"
    );

    if (result)
    {
        juce::File libDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("DrumGroovePro").getChildFile("Samples").getChildFile(selectedLib);

        if (libDir.deleteRecursively())
        {
            refreshLibraryList();
            updateSamplesStatus();
        }
    }
}

void SamplesManagerWindow::handleCancelDownload()
{
    if (downloader)
    {
        downloader->cancelDownload();
        downloader.reset();
    }
    showDownloadSection(false);
    downloadStatusLabel.setText("Download cancelled", juce::dontSendNotification);

    // Restore download button
    downloadButton.setButtonText("Download");
    downloadButton.setEnabled(true);
}

void SamplesManagerWindow::onDownloadProgress(double progress, juce::String status)
{
    currentProgress = progress;
    downloadStatusLabel.setText(status, juce::dontSendNotification);

    // Update progress bar text to show percentage
    int percentage = static_cast<int>(progress * 100);
    downloadProgress.setTextToDisplay(juce::String(percentage) + "%");

    downloadProgress.repaint();
    downloadSection.repaint();
    repaint();
}

void SamplesManagerWindow::onDownloadComplete(bool success, juce::String message)
{
    showDownloadSection(false);

    // Restore download button
    downloadButton.setButtonText("Download");
    downloadButton.setEnabled(true);

    if (success)
    {
        refreshLibraryList();
        updateSamplesStatus();
        samplesStatusLabel.setText(message, juce::dontSendNotification);
    }
    else
    {
        samplesStatusLabel.setText("Download failed: " + message, juce::dontSendNotification);
    }

    downloader.reset();
}

void SamplesManagerWindow::updateSamplesStatus()
{
    bool samplesLoaded = processor.getSampleEngine().isLoaded();
    audioModeToggle.setToggleState(samplesLoaded, juce::dontSendNotification);

    if (samplesLoaded)
    {
        int sampleCount = processor.getSampleEngine().getLoadedSampleCount();
        juce::String libName = processor.getSampleEngine().getCurrentLibraryName();
        if (sampleCount > 0)
        {
            samplesStatusLabel.setText(juce::String(sampleCount) + " samples (" + libName + ")", juce::dontSendNotification);
        }
        else
        {
            samplesStatusLabel.setText("Samples loaded", juce::dontSendNotification);
        }
    }
    else
    {
        samplesStatusLabel.setText("No samples loaded (MIDI only)", juce::dontSendNotification);
    }
}

void SamplesManagerWindow::refreshPresetList()
{
    presetCombo.clear();
    presetCombo.addItem("-- Default --", 1);

    auto presets = presetManager->getPresetList();
    int id = 2;
    for (const auto& name : presets)
    {
        presetCombo.addItem(name, id++);
    }

    // Restore the current preset selection from DrumMixer
    juce::String currentPreset = drumMixer.getCurrentPresetName();
    if (currentPreset.isEmpty())
    {
        presetCombo.setSelectedId(1, juce::dontSendNotification);  // Default
    }
    else
    {
        // Find and select the current preset
        bool found = false;
        for (int i = 0; i < presetCombo.getNumItems(); ++i)
        {
            if (presetCombo.getItemText(i) == currentPreset)
            {
                presetCombo.setSelectedId(presetCombo.getItemId(i), juce::dontSendNotification);
                found = true;
                break;
            }
        }
        if (!found)
        {
            presetCombo.setSelectedId(1, juce::dontSendNotification);  // Default if not found
        }
    }
}

void SamplesManagerWindow::saveCurrentAsPreset()
{
    auto preset = captureCurrentSettings();

    // Get current preset name (if not "-- Default --")
    juce::String currentName;
    int selectedId = presetCombo.getSelectedId();
    if (selectedId > 1)  // Not default
    {
        currentName = presetCombo.getText();
    }

    juce::AlertWindow dialog("Save Preset", "Enter a name for this preset:", juce::AlertWindow::QuestionIcon);
    dialog.addTextEditor("name", currentName, "Preset Name:");
    dialog.addButton("Save", 1);
    dialog.addButton("Cancel", 0);

    if (dialog.runModalLoop() == 1)
    {
        juce::String name = dialog.getTextEditorContents("name").trim();
        if (name.isNotEmpty())
        {
            // Check if preset already exists and warn user
            if (presetManager->presetExists(name))
            {
                auto confirmResult = juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Overwrite Preset",
                    "A preset named '" + name + "' already exists.\n\nDo you want to overwrite it?",
                    "Overwrite",
                    "Cancel"
                );

                if (!confirmResult)
                {
                    // User cancelled - don't save
                    return;
                }
            }

            preset.name = name;
            presetManager->savePreset(preset);
            drumMixer.setCurrentPresetName(name);  // Store current preset name
            refreshPresetList();

            // Select the saved preset in the combo
            for (int i = 0; i < presetCombo.getNumItems(); ++i)
            {
                if (presetCombo.getItemText(i) == name)
                {
                    presetCombo.setSelectedId(presetCombo.getItemId(i), juce::dontSendNotification);
                    break;
                }
            }
        }
    }
}

void SamplesManagerWindow::loadSelectedPreset()
{
    int selectedId = presetCombo.getSelectedId();
    if (selectedId == 1)
    {
        resetToDefaultSettings();
        drumMixer.setCurrentPresetName("");  // Clear preset name for default
        return;
    }

    juce::String presetName = presetCombo.getText();
    auto preset = presetManager->loadPreset(presetName);

    if (preset.name.isNotEmpty())
    {
        applyPresetSettings(preset);
        drumMixer.setCurrentPresetName(presetName);  // Store current preset name
    }
}

void SamplesManagerWindow::deleteSelectedPreset()
{
    int selectedId = presetCombo.getSelectedId();
    if (selectedId <= 1)
        return;

    juce::String presetName = presetCombo.getText();

    auto result = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Delete Preset",
        "Delete preset '" + presetName + "'?",
        "Delete",
        "Cancel"
    );

    if (result)
    {
        presetManager->deletePreset(presetName);
        refreshPresetList();
    }
}

EQPresetManager::EQPreset SamplesManagerWindow::captureCurrentSettings()
{
    EQPresetManager::EQPreset preset;

    preset.masterVolume = static_cast<float>(masterVolumeSlider.getValue());
    preset.masterEQEnabled = masterEQEnableToggle.getToggleState();

    for (int i = 0; i < 8; ++i)
    {
        preset.masterEQGains[static_cast<size_t>(i)] = static_cast<float>(eqBands[static_cast<size_t>(i)].gainSlider.getValue());
    }

    auto& reverb = drumMixer.getReverb();
    preset.reverbRoomSize = reverb.getRoomSize();
    preset.reverbDamping = reverb.getDamping();
    preset.reverbWetLevel = reverb.getWetLevel();

    // Capture all 7 channels
    for (int i = 0; i < 7; ++i)
    {
        auto& channel = drumMixer.getChannel(i);
        auto& chPreset = preset.channels[static_cast<size_t>(i)];

        chPreset.volume = channel.getVolume();
        chPreset.pan = channel.getPan();
        chPreset.reverbSend = channel.getReverbSend();

        auto& eq = channel.getEQ();
        chPreset.eqLowGain = eq.getGain(SimpleEQ::Low);
        chPreset.eqMidGain = eq.getGain(SimpleEQ::Mid);
        chPreset.eqHighGain = eq.getGain(SimpleEQ::High);

        auto& comp = channel.getCompressor();
        chPreset.compEnabled = comp.isEnabled();
        chPreset.compThreshold = comp.getThreshold();
        chPreset.compRatio = comp.getRatio();
        chPreset.compAttack = comp.getAttack();
        chPreset.compRelease = comp.getRelease();
        chPreset.compMakeup = comp.getMakeupGain();
    }

    return preset;
}

void SamplesManagerWindow::applyPresetSettings(const EQPresetManager::EQPreset& preset)
{
    masterVolumeSlider.setValue(preset.masterVolume, juce::sendNotification);
    masterEQEnableToggle.setToggleState(preset.masterEQEnabled, juce::sendNotification);

    // CRITICAL FIX: Update DrumMixer 8-band EQ DIRECTLY, not just via sliders
    // The slider's sendNotification only triggers callback if value actually changes
    for (int i = 0; i < 8; ++i)
    {
        // First update the DrumMixer directly
        drumMixer.setMasterEQGain(i, preset.masterEQGains[static_cast<size_t>(i)]);
        // Then update the slider visually (without re-triggering callback)
        eqBands[static_cast<size_t>(i)].gainSlider.setValue(
            preset.masterEQGains[static_cast<size_t>(i)], juce::dontSendNotification);
    }

    // Also update the master EQ enabled state directly
    drumMixer.setMasterEQEnabled(preset.masterEQEnabled);

    auto& reverb = drumMixer.getReverb();
    reverb.setRoomSize(preset.reverbRoomSize);
    reverb.setDamping(preset.reverbDamping);
    reverb.setWetLevel(preset.reverbWetLevel);

    // Apply to all 7 channels
    for (int i = 0; i < 7; ++i)
    {
        auto& channel = drumMixer.getChannel(i);
        const auto& chPreset = preset.channels[static_cast<size_t>(i)];

        channel.setVolume(chPreset.volume);
        channel.setPan(chPreset.pan);
        channel.setReverbSend(chPreset.reverbSend);

        auto& eq = channel.getEQ();
        eq.setGain(SimpleEQ::Low, chPreset.eqLowGain);
        eq.setGain(SimpleEQ::Mid, chPreset.eqMidGain);
        eq.setGain(SimpleEQ::High, chPreset.eqHighGain);

        auto& comp = channel.getCompressor();
        comp.setEnabled(chPreset.compEnabled);
        comp.setThreshold(chPreset.compThreshold);
        comp.setRatio(chPreset.compRatio);
        comp.setAttack(chPreset.compAttack);
        comp.setRelease(chPreset.compRelease);
        comp.setMakeupGain(chPreset.compMakeup);
    }

    updateFromMixer();
}

void SamplesManagerWindow::resetToDefaultSettings()
{
    EQPresetManager::EQPreset defaultPreset;
    defaultPreset.masterVolume = 1.0f;
    defaultPreset.masterEQEnabled = false;

    for (size_t i = 0; i < 8; ++i)
        defaultPreset.masterEQGains[i] = 0.0f;

    defaultPreset.reverbRoomSize = 0.5f;
    defaultPreset.reverbDamping = 0.5f;
    defaultPreset.reverbWetLevel = 0.33f;

    // Default for all 7 channels
    for (size_t i = 0; i < 7; ++i)
    {
        auto& ch = defaultPreset.channels[i];
        ch.volume = 0.8f;
        ch.pan = 0.0f;
        ch.reverbSend = 0.0f;
        ch.eqLowGain = 0.0f;
        ch.eqMidGain = 0.0f;
        ch.eqHighGain = 0.0f;
        ch.compEnabled = false;
        ch.compThreshold = -20.0f;
        ch.compRatio = 4.0f;
        ch.compAttack = 10.0f;
        ch.compRelease = 100.0f;
        ch.compMakeup = 0.0f;
    }

    applyPresetSettings(defaultPreset);
}

void SamplesManagerWindow::handleVelocityHumanizationChanged()
{
    float value = static_cast<float>(velocityHumanSlider.getValue());
    processor.getSampleEngine().setVelocityHumanization(value);
    DBG("Velocity Humanization set to: " + juce::String(value) + "%");
}

void SamplesManagerWindow::handleTimingHumanizationChanged()
{
    float value = static_cast<float>(timingHumanSlider.getValue());
    processor.getSampleEngine().setTimingHumanization(value);
    DBG("Timing Humanization set to: " + juce::String(value) + "%");
}

void SamplesManagerWindow::handleRoundRobinChanged()
{
    float value = static_cast<float>(roundRobinSlider.getValue());
    processor.getSampleEngine().setRoundRobinAmount(value);
    DBG("Round Robin set to: " + juce::String(value) + "%");
}
