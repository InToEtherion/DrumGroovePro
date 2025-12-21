#include "BatchRemapDialog.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../../Core/DrumLibraryManager.h"

BatchRemapDialog::BatchRemapDialog(DrumLibraryManager& libManager)
    : DocumentWindow("Batch Remap MIDI Files", juce::Colours::darkgrey, DocumentWindow::allButtons),
      drumLibraryManager(libManager)
{
    setUsingNativeTitleBar(true);
    setResizable(false, false);

    content = std::make_unique<DialogContent>(drumLibraryManager);
    setContentOwned(content.get(), true);

    centreWithSize(700, 500);
    setVisible(true);
}

BatchRemapDialog::~BatchRemapDialog()
{
}

void BatchRemapDialog::closeButtonPressed()
{
    setVisible(false);
    if (onDialogClosed)
        onDialogClosed();
}

// ============================================================================
// DialogContent Implementation
// ============================================================================

BatchRemapDialog::DialogContent::DialogContent(DrumLibraryManager& libManager)
    : drumLibraryManager(libManager), progressBar(progress)
{
    // Source library
    addAndMakeVisible(sourceLibraryLabel);
    sourceLibraryLabel.setText("Source Library (Origin):", juce::dontSendNotification);
    sourceLibraryLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(sourceLibraryCombo);

    // Target library
    addAndMakeVisible(targetLibraryLabel);
    targetLibraryLabel.setText("Target Library:", juce::dontSendNotification);
    targetLibraryLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(targetLibraryCombo);

    // Load available mappings
    loadAvailableMappings();

    // Source path
    addAndMakeVisible(sourcePathLabel);
    sourcePathLabel.setText("Source MIDI File/Folder:", juce::dontSendNotification);
    sourcePathLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(singleFileRadio);
    singleFileRadio.setButtonText("Single MIDI File");
    singleFileRadio.setRadioGroupId(1);
    singleFileRadio.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(folderRadio);
    folderRadio.setButtonText("Folder (with subfolders)");
    folderRadio.setRadioGroupId(1);

    addAndMakeVisible(sourcePathDisplay);
    sourcePathDisplay.setText("[No file/folder selected]", juce::dontSendNotification);
    sourcePathDisplay.setJustificationType(juce::Justification::centredLeft);
    sourcePathDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF2A2A2A));
    sourcePathDisplay.setColour(juce::Label::outlineColourId, juce::Colour(0xFF404040));

    addAndMakeVisible(browseSourceButton);
    browseSourceButton.setButtonText("Browse...");
    browseSourceButton.onClick = [this]() { browseSource(); };

    // Destination path
    addAndMakeVisible(destinationPathLabel);
    destinationPathLabel.setText("Destination Folder:", juce::dontSendNotification);
    destinationPathLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(destinationPathDisplay);
    destinationPathDisplay.setText("[No folder selected]", juce::dontSendNotification);
    destinationPathDisplay.setJustificationType(juce::Justification::centredLeft);
    destinationPathDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF2A2A2A));
    destinationPathDisplay.setColour(juce::Label::outlineColourId, juce::Colour(0xFF404040));

    addAndMakeVisible(browseDestinationButton);
    browseDestinationButton.setButtonText("Browse...");
    browseDestinationButton.onClick = [this]() { browseDestination(); };

    // Progress
    addAndMakeVisible(progressLabel);
    progressLabel.setText("Ready to convert", juce::dontSendNotification);
    progressLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(progressBar);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xFF2A2A2A));
    progressBar.setColour(juce::ProgressBar::foregroundColourId, ColourPalette::primaryBlue);

    // Buttons
    addAndMakeVisible(convertButton);
    convertButton.setButtonText("Convert");
    convertButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2E7D32)); // Green
    convertButton.onClick = [this]() { startConversion(); };

    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.onClick = [this]() {
        if (auto* window = findParentComponentOfClass<BatchRemapDialog>())
            window->closeButtonPressed();
    };
}

BatchRemapDialog::DialogContent::~DialogContent()
{
}

void BatchRemapDialog::DialogContent::loadAvailableMappings()
{
    // Load origin libraries (from OriginLibraryMappings.xml)
    juce::StringArray originLibs = drumLibraryManager.getConfiguredOriginLibraries();
    
    sourceLibraryCombo.clear();
    int id = 1;
    for (const auto& libName : originLibs)
    {
        sourceLibraryCombo.addItem(libName, id++);
    }
    
    if (sourceLibraryCombo.getNumItems() > 0)
        sourceLibraryCombo.setSelectedId(1, juce::dontSendNotification);

    // Load target libraries (from TargetDrumMapping.xml)
    juce::StringArray targetLibs = drumLibraryManager.getAllLibraryNames();
    
    targetLibraryCombo.clear();
    id = 1;
    for (const auto& libName : targetLibs)
    {
        targetLibraryCombo.addItem(libName, id++);
    }
    
    if (targetLibraryCombo.getNumItems() > 0)
        targetLibraryCombo.setSelectedId(1, juce::dontSendNotification);
}

void BatchRemapDialog::DialogContent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A1A));
}

void BatchRemapDialog::DialogContent::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Source library
    auto row = bounds.removeFromTop(30);
    sourceLibraryLabel.setBounds(row.removeFromLeft(180));
    row.removeFromLeft(10);
    sourceLibraryCombo.setBounds(row);
    bounds.removeFromTop(15);

    // Target library
    row = bounds.removeFromTop(30);
    targetLibraryLabel.setBounds(row.removeFromLeft(180));
    row.removeFromLeft(10);
    targetLibraryCombo.setBounds(row);
    bounds.removeFromTop(25);

    // Source type selection
    row = bounds.removeFromTop(30);
    singleFileRadio.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(10);
    folderRadio.setBounds(row.removeFromLeft(200));
    bounds.removeFromTop(10);

    // Source path
    sourcePathLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);
    row = bounds.removeFromTop(30);
    sourcePathDisplay.setBounds(row.removeFromLeft(row.getWidth() - 110));
    row.removeFromLeft(10);
    browseSourceButton.setBounds(row);
    bounds.removeFromTop(20);

    // Destination path
    destinationPathLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);
    row = bounds.removeFromTop(30);
    destinationPathDisplay.setBounds(row.removeFromLeft(row.getWidth() - 110));
    row.removeFromLeft(10);
    browseDestinationButton.setBounds(row);
    bounds.removeFromTop(30);

    // Progress
    progressLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);
    progressBar.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(30);

    // Buttons
    auto buttonArea = bounds.removeFromBottom(40);
    cancelButton.setBounds(buttonArea.removeFromRight(120));
    buttonArea.removeFromRight(10);
    convertButton.setBounds(buttonArea.removeFromRight(120));
}

void BatchRemapDialog::DialogContent::browseSource()
{
    if (singleFileRadio.getToggleState())
    {
        // Browse for single MIDI file
        juce::FileChooser chooser("Select MIDI File", juce::File(), "*.mid;*.midi");

        if (chooser.browseForFileToOpen())
        {
            sourceFile = chooser.getResult();

            if (sourceFile.existsAsFile() && sourceFile.hasFileExtension(".mid;.midi"))
            {
                sourcePathDisplay.setText(sourceFile.getFullPathName(), juce::dontSendNotification);
            }
            else
            {
                sourceFile = juce::File();
                sourcePathDisplay.setText("[No file selected]", juce::dontSendNotification);
            }
        }
    }
    else
    {
        // Browse for folder
        juce::FileChooser chooser("Select Folder Containing MIDI Files");

        if (chooser.browseForDirectory())
        {
            sourceFile = chooser.getResult();
            sourcePathDisplay.setText(sourceFile.getFullPathName(), juce::dontSendNotification);
        }
    }
}

void BatchRemapDialog::DialogContent::browseDestination()
{
    juce::FileChooser chooser("Select Destination Folder");

    if (chooser.browseForDirectory())
    {
        destinationFolder = chooser.getResult();
        
        // Check if folder is read-only
        if (isDestinationReadOnly(destinationFolder))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Read-Only Folder",
                "The selected folder is marked as read-only in your configuration.\n"
                "Please choose a different destination folder.",
                "OK");
            destinationFolder = juce::File();
            destinationPathDisplay.setText("[No folder selected]", juce::dontSendNotification);
        }
        else
        {
            destinationPathDisplay.setText(destinationFolder.getFullPathName(), juce::dontSendNotification);
        }
    }
}

void BatchRemapDialog::DialogContent::startConversion()
{
    // Validation
    if (!sourceFile.exists())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Source Selected",
            "Please select a MIDI file or folder to convert.",
            "OK");
        return;
    }

    if (!destinationFolder.exists())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Destination Selected",
            "Please select a destination folder.",
            "OK");
        return;
    }

    if (sourceLibraryCombo.getSelectedItemIndex() < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Source Library Selected",
            "Please select a source library.",
            "OK");
        return;
    }

    if (targetLibraryCombo.getSelectedItemIndex() < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Target Library Selected",
            "Please select a target library.",
            "OK");
        return;
    }

    isProcessing = true;
    convertButton.setEnabled(false);
    browseSourceButton.setEnabled(false);
    browseDestinationButton.setEnabled(false);
    sourceLibraryCombo.setEnabled(false);
    targetLibraryCombo.setEnabled(false);

    processFiles();

    isProcessing = false;
    convertButton.setEnabled(true);
    browseSourceButton.setEnabled(true);
    browseDestinationButton.setEnabled(true);
    sourceLibraryCombo.setEnabled(true);
    targetLibraryCombo.setEnabled(true);
}

void BatchRemapDialog::DialogContent::processFiles()
{
    juce::String sourceName = sourceLibraryCombo.getText();
    juce::String targetName = targetLibraryCombo.getText();

    DrumLibrary sourceLib = drumLibraryManager.getLibraryFromName(sourceName);
    DrumLibrary targetLib = drumLibraryManager.getLibraryFromName(targetName);

    int filesProcessed = 0;
    int filesSkipped = 0;

    if (sourceFile.existsAsFile())
    {
        // Single file
        updateProgress(0.0, "Processing: " + sourceFile.getFileName());
        processSingleFile(sourceFile, destinationFolder, "");
        filesProcessed = 1;
        updateProgress(1.0, "Completed!");
    }
    else if (sourceFile.isDirectory())
    {
        // Folder - count files first
        juce::Array<juce::File> allMidiFiles;
        sourceFile.findChildFiles(allMidiFiles, juce::File::findFiles, true, "*.mid;*.midi");
        int totalFiles = allMidiFiles.size();

        if (totalFiles == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No MIDI Files Found",
                "No MIDI files were found in the selected folder.",
                "OK");
            updateProgress(0.0, "Ready to convert");
            return;
        }

        // Process all MIDI files
        for (int i = 0; i < allMidiFiles.size(); ++i)
        {
            auto& file = allMidiFiles.getReference(i);
            
            // Calculate relative path from source folder
            juce::String relativePath = file.getParentDirectory().getFullPathName()
                                       .replace(sourceFile.getFullPathName(), "");
            if (relativePath.startsWithChar(juce::File::getSeparatorChar()))
                relativePath = relativePath.substring(1);

            updateProgress((double)i / totalFiles, 
                         "Processing (" + juce::String(i + 1) + "/" + juce::String(totalFiles) + "): " + file.getFileName());

            processSingleFile(file, destinationFolder, relativePath);
            filesProcessed++;
        }

        updateProgress(1.0, "Completed!");
    }

    showCompletionDialog(filesProcessed, filesSkipped);
}

void BatchRemapDialog::DialogContent::processSingleFile(const juce::File& inputFile, 
                                                        const juce::File& outputFolder,
                                                        const juce::String& relativePath)
{
    juce::String sourceName = sourceLibraryCombo.getText();
    juce::String targetName = targetLibraryCombo.getText();

    DrumLibrary sourceLib = drumLibraryManager.getLibraryFromName(sourceName);
    DrumLibrary targetLib = drumLibraryManager.getLibraryFromName(targetName);

    // Read MIDI file
    juce::FileInputStream inputStream(inputFile);
    if (!inputStream.openedOk())
        return;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(inputStream))
        return;

    // Create output folder structure
    juce::String sanitizedSource = sanitizeFileName(sourceName);
    juce::String sanitizedTarget = sanitizeFileName(targetName);
    juce::String folderSuffix = "_" + sanitizedSource + "_to_" + sanitizedTarget;

    juce::File outputSubFolder = outputFolder;
    
    // Create subfolder structure if there's a relative path
    if (relativePath.isNotEmpty())
    {
        juce::File relativeFolder = outputFolder.getChildFile(relativePath);
        juce::String folderName = relativeFolder.getFileNameWithoutExtension() + folderSuffix;
        outputSubFolder = relativeFolder.getParentDirectory().getChildFile(folderName);
    }
    else if (sourceFile.isDirectory())
    {
        // Top level folder
        juce::String folderName = sourceFile.getFileNameWithoutExtension() + folderSuffix;
        outputSubFolder = outputFolder.getChildFile(folderName);
    }
    else
    {
        // Single file - create folder with file name + suffix
        juce::String folderName = inputFile.getFileNameWithoutExtension() + folderSuffix;
        outputSubFolder = outputFolder.getChildFile(folderName);
    }

    if (!outputSubFolder.exists())
        outputSubFolder.createDirectory();

    // Create output filename
    juce::String outputFileName = inputFile.getFileNameWithoutExtension() + folderSuffix + ".mid";
    juce::File outputFile = outputSubFolder.getChildFile(outputFileName);

    // Remap notes
    juce::MidiFile remappedMidi;
    remappedMidi.setTicksPerQuarterNote(midiFile.getTimeFormat());

    for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
    {
        const juce::MidiMessageSequence* track = midiFile.getTrack(trackIndex);
        juce::MidiMessageSequence newTrack;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            juce::MidiMessage message = track->getEventPointer(i)->message;

            // Remap note on/off messages
            if (message.isNoteOnOrOff())
            {
                uint8_t originalNote = static_cast<uint8_t>(message.getNoteNumber());
                uint8_t remappedNote = drumLibraryManager.mapNoteToLibrary(originalNote, sourceLib, targetLib);
                message.setNoteNumber(remappedNote);
            }

            newTrack.addEvent(message, message.getTimeStamp());
        }

        newTrack.updateMatchedPairs();
        remappedMidi.addTrack(newTrack);
    }

    // Write output file
    juce::FileOutputStream outputStream(outputFile);
    if (outputStream.openedOk())
    {
        remappedMidi.writeTo(outputStream);
        outputStream.flush();
    }
}

void BatchRemapDialog::DialogContent::processFolder(const juce::File& inputFolder,
                                                     const juce::File& outputFolder,
                                                     const juce::String& relativePath)
{
    // This is handled in processFiles() by recursively finding all MIDI files
}

bool BatchRemapDialog::DialogContent::isDestinationReadOnly(const juce::File& folder)
{
    // Load config.xml from AppData
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                     .getChildFile("DrumGroovePro");
    auto configFile = appDataDir.getChildFile("config.xml");

    if (!configFile.existsAsFile())
        return false;

    auto xml = juce::parseXML(configFile);
    if (!xml)
        return false;

    // Check RootFolders element
    auto* foldersElement = xml->getChildByName("RootFolders");
    if (!foldersElement)
        return false;

    juce::String folderPath = folder.getFullPathName();

    for (auto* folderElement : foldersElement->getChildIterator())
    {
        if (folderElement->hasTagName("Folder"))
        {
            juce::String configPath = folderElement->getStringAttribute("path");
            bool isWritable = folderElement->getBoolAttribute("isWritable", true);

            // Check if destination is within a read-only folder
            if (folderPath.startsWith(configPath) && !isWritable)
            {
                return true; // Folder is read-only
            }
        }
    }

    return false; // Not in any protected folder
}

juce::String BatchRemapDialog::DialogContent::sanitizeFileName(const juce::String& name)
{
    juce::String sanitized = name;
    
    // Replace invalid characters with underscores
    sanitized = sanitized.replaceCharacters("\\/:*?\"<>|", "_________");
    
    // Remove any spaces
    sanitized = sanitized.replace(" ", "_");
    
    return sanitized;
}

void BatchRemapDialog::DialogContent::updateProgress(double newProgress, const juce::String& message)
{
    progress = newProgress;
    progressLabel.setText(message, juce::dontSendNotification);
    repaint();
}

void BatchRemapDialog::DialogContent::showCompletionDialog(int filesProcessed, int filesSkipped)
{
    juce::String message = "Batch conversion completed!\n\n";
    message += "Files processed: " + juce::String(filesProcessed) + "\n";
    
    if (filesSkipped > 0)
        message += "Files skipped: " + juce::String(filesSkipped) + "\n";

    message += "\nOutput location: " + destinationFolder.getFullPathName();

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Conversion Complete",
        message,
        "OK");

    updateProgress(0.0, "Ready to convert");
}
