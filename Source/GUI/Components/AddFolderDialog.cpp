#include "AddFolderDialog.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../../PluginProcessor.h"

AddFolderDialog::AddFolderDialog(DrumGrooveProcessor& p)
    : DialogWindow("Add MIDI Folder to Library", ColourPalette::panelBackground, true),
      processor(p)
{
    component = std::make_unique<AddFolderComponent>(processor);

    // Connect button listeners
    component->browseButton.addListener(this);
    component->addButton.addListener(this);
    component->cancelButton.addListener(this);
    component->sourceLibraryCombo.addListener(this);

    setContentOwned(component.release(), false);

    setSize(500, 480);
    setResizable(false, false);
    setUsingNativeTitleBar(false);
    setAlwaysOnTop(true);
    centreWithSize(getWidth(), getHeight());
}

AddFolderDialog::~AddFolderDialog()
{
    stopTimer();
}

void AddFolderDialog::closeButtonPressed()
{
    if (!isProcessing)
    {
        processingCancelled = true;
        stopTimer();
        setVisible(false);
    }
}

void AddFolderDialog::buttonClicked(juce::Button* button)
{
    auto* comp = static_cast<AddFolderComponent*>(getContentComponent());

    if (button == &comp->browseButton)
    {
        // Use Windows native file chooser
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select MIDI Folder",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "",
            true  // Use native dialog on Windows
        );

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectDirectories,
            [this, comp, chooser](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.exists() && result.isDirectory())
                {
                    selectedFolder = result;
                    comp->folderPathEditor.setText(selectedFolder.getFullPathName());

                    if (comp->libraryNameEditor.getText().isEmpty())
                    {
                        comp->libraryNameEditor.setText(selectedFolder.getFileName());
                    }

                    updateAddButtonState();
                }
            }
        );
    }
    else if (button == &comp->addButton)
    {
        if (selectedFolder.exists())
        {
			auto selectedText = comp->sourceLibraryCombo.getText();
			auto sourceLib = DrumLibraryManager::getLibraryFromName(selectedText);
            selectedSourceLibrary = static_cast<int>(sourceLib);
            libraryName = comp->libraryNameEditor.getText();
            startProcessing();
        }
    }
    else if (button == &comp->cancelButton)
    {
        if (isProcessing)
        {
            processingCancelled = true;
            stopTimer();
            setProcessingState(false);
            comp->statusLabel.setText("Operation cancelled", juce::dontSendNotification);
        }
        else
        {
            closeButtonPressed();
        }
    }
}

void AddFolderDialog::comboBoxChanged(juce::ComboBox* comboBox)
{
    updateAddButtonState();
}

void AddFolderDialog::timerCallback()
{
    processNextChunk();
}

void AddFolderDialog::startProcessing()
{
    auto* comp = static_cast<AddFolderComponent*>(getContentComponent());

    isProcessing = true;
    processingCancelled = false;
    currentChunkIndex = 0;
    comp->progress = 0.0;

    setProcessingState(true);
    comp->statusLabel.setText("Scanning for MIDI files...", juce::dontSendNotification);

    midiFiles.clear();
    selectedFolder.findChildFiles(midiFiles, juce::File::findFiles, true, "*.mid;*.midi;*.MID;*.MIDI");

    if (midiFiles.isEmpty())
    {
        setProcessingState(false);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No MIDI Files Found",
            "The selected folder doesn't contain any MIDI files.");
        return;
    }

    comp->statusLabel.setText("Found " + juce::String(midiFiles.size()) + " MIDI files. Processing...",
                              juce::dontSendNotification);

    startTimer(20);
}

void AddFolderDialog::processNextChunk()
{
    if (processingCancelled)
    {
        stopTimer();
        setProcessingState(false);
        return;
    }

    auto* comp = static_cast<AddFolderComponent*>(getContentComponent());

    int startIdx = currentChunkIndex * CHUNK_SIZE;
    int endIdx = juce::jmin(startIdx + CHUNK_SIZE, midiFiles.size());

    if (startIdx >= midiFiles.size())
    {
        finishProcessing();
        return;
    }

    comp->progress = static_cast<double>(startIdx) / static_cast<double>(midiFiles.size());
    comp->progressBar.repaint();

    comp->statusLabel.setText("Processing files " + juce::String(startIdx + 1) +
                              "-" + juce::String(endIdx) +
                              " of " + juce::String(midiFiles.size()) + "...",
                              juce::dontSendNotification);

    for (int i = startIdx; i < endIdx && !processingCancelled; ++i)
    {
        // Processing simulation - actual processing would happen here
    }

    currentChunkIndex++;
}

void AddFolderDialog::finishProcessing()
{
    stopTimer();

    auto* comp = static_cast<AddFolderComponent*>(getContentComponent());

    comp->progress = 1.0;
    comp->progressBar.repaint();
    comp->statusLabel.setText("Finalizing library update...", juce::dontSendNotification);

	processor.drumLibraryManager.addRootFolder(selectedFolder, static_cast<DrumLibrary>(selectedSourceLibrary));

    comp->statusLabel.setText("Library updated successfully!", juce::dontSendNotification);

    juce::Timer::callAfterDelay(500, [this]()
    {
        if (onFolderAdded)
            onFolderAdded();

        setVisible(false);
    });
}

void AddFolderDialog::setProcessingState(bool processing)
{
    auto* comp = static_cast<AddFolderComponent*>(getContentComponent());

    comp->browseButton.setEnabled(!processing);
    comp->addButton.setEnabled(!processing);
    comp->sourceLibraryCombo.setEnabled(!processing);
    comp->libraryNameEditor.setEnabled(!processing);

    comp->progressBar.setVisible(processing);
    comp->statusLabel.setVisible(processing);

    if (processing)
    {
        comp->addButton.setButtonText("PROCESSING...");
    }
    else
    {
        comp->addButton.setButtonText("ADD TO LIBRARY");
        comp->progress = 0.0;
    }
}

void AddFolderDialog::updateAddButtonState()
{
    auto* comp = static_cast<AddFolderComponent*>(getContentComponent());

    bool canAdd = selectedFolder.exists() &&
                  comp->sourceLibraryCombo.getSelectedId() > 0;

    comp->addButton.setEnabled(canAdd);
}

//==============================================================================
AddFolderDialog::AddFolderComponent::AddFolderComponent(DrumGrooveProcessor& p)
    : processor(p), progressBar(progress)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    folderPathLabel.setText("Selected Folder:", juce::dontSendNotification);
    folderPathLabel.setFont(lnf.getNormalFont().boldened());
    addAndMakeVisible(folderPathLabel);

    folderPathEditor.setReadOnly(true);
    folderPathEditor.setText("Click Browse to select a MIDI folder");
    folderPathEditor.setColour(juce::TextEditor::backgroundColourId, ColourPalette::inputBackground);
    addAndMakeVisible(folderPathEditor);

    browseButton.setButtonText("BROWSE");
    addAndMakeVisible(browseButton);

    sourceLibraryLabel.setText("Source Drum Product:", juce::dontSendNotification);
    sourceLibraryLabel.setFont(lnf.getNormalFont().boldened());
    addAndMakeVisible(sourceLibraryLabel);

    // Populate with all source libraries (includes Unknown)
    auto sourceLibNames = DrumLibraryManager::getAllSourceLibraryNames();
    for (int i = 0; i < sourceLibNames.size(); ++i)
    {
        sourceLibraryCombo.addItem(sourceLibNames[i], i + 1);
    }
    sourceLibraryCombo.setSelectedId(1);
    addAndMakeVisible(sourceLibraryCombo);

    sourceHelpLabel.setText("What drum library were these MIDI files created for?",
                            juce::dontSendNotification);
    sourceHelpLabel.setFont(lnf.getSmallFont());
    sourceHelpLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(sourceHelpLabel);

    libraryNameLabel.setText("Library Name (optional):", juce::dontSendNotification);
    libraryNameLabel.setFont(lnf.getNormalFont().boldened());
    addAndMakeVisible(libraryNameLabel);

    libraryNameEditor.setText("");
    libraryNameEditor.setColour(juce::TextEditor::backgroundColourId, ColourPalette::inputBackground);
    addAndMakeVisible(libraryNameEditor);

    progressBar.setPercentageDisplay(true);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, ColourPalette::inputBackground);
    progressBar.setColour(juce::ProgressBar::foregroundColourId, ColourPalette::successGreen);
    progressBar.setVisible(false);
    addAndMakeVisible(progressBar);

    statusLabel.setFont(lnf.getNormalFont());
    statusLabel.setColour(juce::Label::textColourId, ColourPalette::successGreen);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setVisible(false);
    addAndMakeVisible(statusLabel);

    addButton.setButtonText("ADD TO LIBRARY");
    addButton.setEnabled(false);
    addButton.setColour(juce::TextButton::buttonColourId, ColourPalette::successGreen);
    addAndMakeVisible(addButton);

    cancelButton.setButtonText("CANCEL");
    addAndMakeVisible(cancelButton);
}

AddFolderDialog::AddFolderComponent::~AddFolderComponent() = default;

void AddFolderDialog::AddFolderComponent::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);

    g.setColour(ColourPalette::separator);
    g.drawLine(20.0f, 120.0f, getWidth() - 20.0f, 120.0f);
    g.drawLine(20.0f, 250.0f, getWidth() - 20.0f, 250.0f);
}

void AddFolderDialog::AddFolderComponent::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    folderPathLabel.setBounds(bounds.removeFromTop(25));
    auto pathRow = bounds.removeFromTop(30);
    browseButton.setBounds(pathRow.removeFromRight(120));
    pathRow.removeFromRight(10);
    folderPathEditor.setBounds(pathRow);

    bounds.removeFromTop(25);

    sourceLibraryLabel.setBounds(bounds.removeFromTop(25));
    sourceLibraryCombo.setBounds(bounds.removeFromTop(30));
    sourceHelpLabel.setBounds(bounds.removeFromTop(20));

    bounds.removeFromTop(25);

    libraryNameLabel.setBounds(bounds.removeFromTop(25));
    libraryNameEditor.setBounds(bounds.removeFromTop(30));

    bounds.removeFromTop(30);

    progressBar.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(10);
    statusLabel.setBounds(bounds.removeFromTop(30));

    bounds.removeFromBottom(20);
    auto buttonRow = bounds.removeFromBottom(35);
    cancelButton.setBounds(buttonRow.removeFromRight(100));
    buttonRow.removeFromRight(10);
    addButton.setBounds(buttonRow.removeFromRight(150));
}