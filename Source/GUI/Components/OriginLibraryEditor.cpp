#include "OriginLibraryEditor.h"
#include "DrumGrooveLookAndFeel.h"
#include "GrooveBrowser.h"


OriginLibraryEditor::OriginLibraryEditor(DrumLibraryManager& manager)
: drumLibraryManager(manager)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    // Title - centered both horizontally and vertically
    titleLabel.setText("Origin MIDI Library Manager", juce::dontSendNotification);
    titleLabel.setFont(lnf.getBoldFont().withHeight(18.0f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(titleLabel);

    // Origin libraries section
    originLibrariesLabel.setText("Origin Libraries:", juce::dontSendNotification);
    originLibrariesLabel.setFont(lnf.getBoldFont().withHeight(14.0f));
    originLibrariesLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(originLibrariesLabel);

    originLibrariesList.setModel(this);
    originLibrariesList.setRowHeight(30);
    originLibrariesList.setColour(juce::ListBox::backgroundColourId, ColourPalette::secondaryBackground);
    addAndMakeVisible(originLibrariesList);

    addOriginButton.setButtonText("+ Add Origin Library");
    addOriginButton.addListener(this);
    addAndMakeVisible(addOriginButton);

    // Mappings section
    mappingsLabel.setText("Note Mappings (Origin -> GM):", juce::dontSendNotification);
    mappingsLabel.setFont(lnf.getBoldFont().withHeight(14.0f));
    mappingsLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(mappingsLabel);

    mappingsViewport.setViewedComponent(&mappingsContainer, false);
    mappingsViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(mappingsViewport);

    addMappingButton.setButtonText("+ Add Note Mapping");
    addMappingButton.addListener(this);
    addAndMakeVisible(addMappingButton);

    // Paste from clipboard button
    pasteFromClipboardButton.setButtonText("Paste from Clipboard");
    pasteFromClipboardButton.setTooltip("Paste mappings from Excel/spreadsheet (Origin Note, GM Note, Description)");
    pasteFromClipboardButton.addListener(this);
    addAndMakeVisible(pasteFromClipboardButton);

    // Bottom buttons
    cancelButton.setButtonText("Cancel");
    cancelButton.addListener(this);
    addAndMakeVisible(cancelButton);

    saveButton.setButtonText("Save & Close");
    saveButton.addListener(this);
    addAndMakeVisible(saveButton);

    // Load icons
    juce::File pluginFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File resourcesPath;

    #if JUCE_WINDOWS
    resourcesPath = pluginFile.getParentDirectory()
    .getParentDirectory()
    .getChildFile("Resources");
    #elif JUCE_MAC
    resourcesPath = pluginFile.getParentDirectory()
    .getParentDirectory()
    .getChildFile("Resources");
    #else
    resourcesPath = pluginFile.getParentDirectory()
    .getParentDirectory()
    .getChildFile("Resources");
    #endif

    juce::File deleteIconFile = resourcesPath.getChildFile("icons")
    .getChildFile("misc")
    .getChildFile("16x16")
    .getChildFile("cross.png");

    if (deleteIconFile.existsAsFile())
        deleteIcon = juce::ImageCache::getFromFile(deleteIconFile);

    juce::File arrowIconFile = resourcesPath.getChildFile("icons")
    .getChildFile("misc")
    .getChildFile("16x16")
    .getChildFile("arrow.png");

    if (arrowIconFile.existsAsFile())
        arrowIcon = juce::ImageCache::getFromFile(arrowIconFile);

    // Load working copy from DrumLibraryManager
    loadWorkingCopy();

    updateOriginLibrariesList();

    setSize(900, 600);
}

OriginLibraryEditor::~OriginLibraryEditor()
{
    addOriginButton.removeListener(this);
    addMappingButton.removeListener(this);
    pasteFromClipboardButton.removeListener(this);
    saveButton.removeListener(this);
    cancelButton.removeListener(this);
}

void OriginLibraryEditor::loadWorkingCopy()
{
    workingMappings.clear();
    pendingAddedLibraries.clear();
    pendingDeletedLibraries.clear();
    pendingAddAsTargetLibraries.clear();
    hasUnsavedChanges = false;

    // Load all configured origin libraries and their mappings
    auto configuredLibs = drumLibraryManager.getConfiguredOriginLibraries();

    for (const auto& libName : configuredLibs)
    {
        if (libName == "Unknown")
            continue;

        DrumLibrary lib = drumLibraryManager.getLibraryFromName(libName);
        if (lib == DrumLibrary::Unknown)
            continue;

        // Get current mappings for this library
        auto mappings = drumLibraryManager.getOriginLibraryMappings(lib);

        std::vector<PendingMapping> libMappings;
        for (const auto& [originNote, gmNote] : mappings)
        {
            PendingMapping pm;
            pm.originNote = originNote;
            pm.gmNote = gmNote;
            pm.description = drumLibraryManager.getCustomDrumName(lib, gmNote);
            if (pm.description.isEmpty())
                pm.description = DrumLibraryManager::getGMDrumName(gmNote);
            libMappings.push_back(pm);
        }

        workingMappings[libName] = libMappings;
    }

    DBG("Loaded working copy with " + juce::String(workingMappings.size()) + " origin libraries");
}

std::vector<OriginLibraryEditor::PendingMapping>& OriginLibraryEditor::getWorkingMappingsForLibrary(const juce::String& libraryName)
{
    return workingMappings[libraryName];
}

bool OriginLibraryEditor::isProtectedLibrary(const juce::String& libraryName) const
{
    // Only General MIDI has protected (read-only) mappings for origin libraries
    return libraryName == "General MIDI";
}

void OriginLibraryEditor::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);

    // Draw header background
    g.setColour(ColourPalette::primaryBlue);
    g.fillRect(0, 0, getWidth(), 50);
}

void OriginLibraryEditor::resized()
{
    auto bounds = getLocalBounds();

    // Title area - 50px high header, text centered vertically and horizontally
    auto headerArea = bounds.removeFromTop(50);
    titleLabel.setBounds(headerArea);

    bounds = bounds.reduced(20);
    bounds.removeFromTop(10);

    auto leftSection = bounds.removeFromLeft(300);
    bounds.removeFromLeft(20);

    originLibrariesLabel.setBounds(leftSection.removeFromTop(30));
    addOriginButton.setBounds(leftSection.removeFromBottom(30));
    leftSection.removeFromBottom(10);
    originLibrariesList.setBounds(leftSection);

    mappingsLabel.setBounds(bounds.removeFromTop(30));
    auto bottomButtons = bounds.removeFromBottom(40);
    bounds.removeFromBottom(10);

    auto buttonRow = bounds.removeFromBottom(30);
    addMappingButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);
    pasteFromClipboardButton.setBounds(buttonRow.removeFromLeft(140));

    bounds.removeFromBottom(10);
    mappingsViewport.setBounds(bounds);

    int totalHeight = mappingRows.size() * 35;

    mappingsContainer.setBounds(0, 0, bounds.getWidth() - 20, juce::jmax(totalHeight, bounds.getHeight()));

    int yPos = 0;
    for (auto* row : mappingRows)
    {
        row->setBounds(0, yPos, mappingsContainer.getWidth(), 35);
        yPos += 35;
    }

    saveButton.setBounds(bottomButtons.removeFromRight(120));
    bottomButtons.removeFromRight(10);
    cancelButton.setBounds(bottomButtons.removeFromRight(120));
}

int OriginLibraryEditor::getNumRows()
{
    return allOriginLibraries.size();
}

void OriginLibraryEditor::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= allOriginLibraries.size())
        return;

    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    if (rowIsSelected)
        g.fillAll(ColourPalette::primaryBlue.withAlpha(0.3f));
    else if (rowNumber % 2 == 0)
        g.fillAll(ColourPalette::secondaryBackground);
    else
        g.fillAll(ColourPalette::mainBackground.brighter(0.05f));

    g.setColour(ColourPalette::primaryText);
    g.setFont(lnf.getNormalFont().withHeight(13.0f));

    juce::String libraryName = allOriginLibraries[rowNumber];

    // Mark pending added libraries with asterisk
    juce::String displayName = libraryName;
    if (pendingAddedLibraries.count(libraryName.toStdString()) > 0)
        displayName += " *";

    g.drawText(displayName, 10, 0, width - 80, height, juce::Justification::centredLeft);

    // Draw delete icon for non-protected libraries
    if (!isProtectedLibrary(libraryName))
    {
        auto deleteArea = juce::Rectangle<int>(width - 70, height / 2 - 8, 16, 16);

        if (deleteIcon.isValid())
        {
            g.drawImage(deleteIcon, deleteArea.toFloat(),
                        juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            g.setColour(ColourPalette::errorRed);
            g.setFont(lnf.getBoldFont().withHeight(14.0f));
            g.drawText("X", deleteArea, juce::Justification::centred);
        }
    }
}

void OriginLibraryEditor::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row >= allOriginLibraries.size())
        return;

    int deleteAreaStart = originLibrariesList.getWidth() - 75;
    int deleteAreaEnd = originLibrariesList.getWidth() - 50;

    if (e.x >= deleteAreaStart && e.x <= deleteAreaEnd)
    {
        if (!isProtectedLibrary(allOriginLibraries[row]))
        {
            deleteOriginLibrary(row);
        }
    }
    else
    {
        selectedOriginIndex = row;
        updateMappingsForSelectedOrigin();
    }
}

void OriginLibraryEditor::buttonClicked(juce::Button* button)
{
    if (button == &addOriginButton)
    {
        showAddOriginLibraryDialog();
    }
    else if (button == &addMappingButton)
    {
        showAddNoteMappingDialog();
    }
    else if (button == &pasteFromClipboardButton)
    {
        pasteFromClipboard();
    }
    else if (button == &cancelButton)
    {
        // Cancel - just close the window without saving
        if (auto* parent = getParentComponent())
        {
            if (auto* dialogWindow = dynamic_cast<juce::DialogWindow*>(parent))
                dialogWindow->exitModalState(0);
        }
    }
    else if (button == &saveButton)
    {
        saveCustomMappings();

        // Call the callback first (for AddFolderDialog refresh)
        if (onLibrariesChanged)
            onLibrariesChanged();

        // Also directly refresh GrooveBrowser's target dropdown
        // This handles the case when OriginLibraryEditor is opened from AddFolderDialog
        // and a new library was added as target
        for (int i = juce::TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
        {
            if (auto* window = juce::TopLevelWindow::getTopLevelWindow(i))
            {
                std::function<GrooveBrowser*(juce::Component*)> findGrooveBrowser;
                findGrooveBrowser = [&findGrooveBrowser](juce::Component* comp) -> GrooveBrowser*
                {
                    if (comp == nullptr) return nullptr;
                    if (auto* gb = dynamic_cast<GrooveBrowser*>(comp))
                        return gb;
                    for (int j = 0; j < comp->getNumChildComponents(); ++j)
                    {
                        if (auto* found = findGrooveBrowser(comp->getChildComponent(j)))
                            return found;
                    }
                    return nullptr;
                };

                if (auto* grooveBrowser = findGrooveBrowser(window))
                {
                    grooveBrowser->refreshTargetLibraryCombo();
                    DBG("Refreshed GrooveBrowser target library dropdown after save");
                    break;
                }
            }
        }

        if (auto* parent = getParentComponent())
        {
            if (auto* dialogWindow = dynamic_cast<juce::DialogWindow*>(parent))
                dialogWindow->exitModalState(1);
        }
    }
}

void OriginLibraryEditor::updateOriginLibrariesList()
{
    allOriginLibraries.clear();

    // Add General MIDI first (protected)
    allOriginLibraries.add("General MIDI");

    // Add libraries from working copy (includes pending added libraries)
    for (const auto& [libName, mappings] : workingMappings)
    {
        if (libName != "General MIDI" && libName != "Unknown")
        {
            // Skip if marked for deletion
            if (pendingDeletedLibraries.count(libName.toStdString()) == 0)
            {
                if (!allOriginLibraries.contains(libName))
                    allOriginLibraries.add(libName);
            }
        }
    }

    // Add pending new libraries that aren't in working copy yet
    for (const auto& libName : pendingAddedLibraries)
    {
        juce::String jLibName(libName);
        if (!allOriginLibraries.contains(jLibName))
            allOriginLibraries.add(jLibName);
    }

    originLibrariesList.updateContent();
    originLibrariesList.repaint();
}

void OriginLibraryEditor::updateMappingsForSelectedOrigin()
{
    mappingRows.clear();
    mappingsContainer.removeAllChildren();

    if (selectedOriginIndex < 0 || selectedOriginIndex >= allOriginLibraries.size())
    {
        mappingsLabel.setText("Note Mappings (Origin -> GM):", juce::dontSendNotification);
        resized();
        return;
    }

    juce::String selectedOrigin = allOriginLibraries[selectedOriginIndex];
    mappingsLabel.setText("Note Mappings (" + selectedOrigin + " -> GM):", juce::dontSendNotification);

    bool isReadOnly = isProtectedLibrary(selectedOrigin);

    // Get mappings from working copy
    auto& mappings = getWorkingMappingsForLibrary(selectedOrigin);

    for (size_t i = 0; i < mappings.size(); ++i)
    {
        const auto& pm = mappings[i];

        auto* row = mappingRows.add(new MappingRow(pm.originNote, pm.gmNote, pm.description, isReadOnly));
        mappingsContainer.addAndMakeVisible(row);

        if (!isReadOnly)
        {
            size_t capturedIndex = i;

            row->onDelete = [this, capturedIndex, selectedOrigin]()
            {
                auto& libMappings = getWorkingMappingsForLibrary(selectedOrigin);
                if (capturedIndex < libMappings.size())
                {
                    libMappings.erase(libMappings.begin() + capturedIndex);
                    hasUnsavedChanges = true;
                    updateMappingsForSelectedOrigin();
                }
            };

            row->onEdit = [this, row]()
            {
                showEditMappingDialog(row);
            };
        }
    }

    resized();
}

void OriginLibraryEditor::showAddOriginLibraryDialog()
{
    auto* parentWindow = findParentComponentOfClass<juce::DialogWindow>();
    juce::Component* dialogParent = parentWindow ? static_cast<juce::Component*>(parentWindow) : this;

    auto* w = new juce::AlertWindow("Add Origin Library",
                                    "Enter the name of the origin MIDI library:",
                                    juce::AlertWindow::QuestionIcon,
                                    dialogParent);

    w->addTextEditor("name", "", "Library Name:");
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, w, dialogParent](int result)
        {
            if (result == 1)
            {
                juce::String libraryName = w->getTextEditorContents("name").trim();

                if (libraryName.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                           "Error", "Please enter a library name.");
                    return;
                }

                if (allOriginLibraries.contains(libraryName))
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                           "Already Exists", "This library already exists.");
                    return;
                }

                // Add to pending - DO NOT write to XML yet
                pendingAddedLibraries.insert(libraryName.toStdString());

                // Now show the second dialog asking about target library
                auto* targetDialog = new juce::AlertWindow(
                    "Add as Target Library?",
                    "Do you also want to add '" + libraryName + "' as a Target library?\n\n"
                    "This allows you to map FROM General MIDI TO this library,\n"
                    "and it will appear in the Target Drum Library dropdown.",
                    juce::AlertWindow::QuestionIcon,
                    dialogParent);

                targetDialog->addButton("Yes, add as Target", 1);
                targetDialog->addButton("No, Origin only", 0);

                targetDialog->enterModalState(true, juce::ModalCallbackFunction::create(
                    [this, libraryName](int targetResult)
                    {
                        if (targetResult == 1)
                        {
                            // Mark for adding as target on Save
                            pendingAddAsTargetLibraries.insert(libraryName.toStdString());
                            DBG("Will add '" + libraryName + "' as both Origin and Target library on Save");
                        }
                        else
                        {
                            DBG("Will add '" + libraryName + "' as Origin library only on Save");
                        }

                        // Initialize working copy with GM identity mapping
                        std::vector<PendingMapping> newMappings;
                        for (int note = 35; note <= 81; ++note)
                        {
                            PendingMapping pm;
                            pm.originNote = static_cast<uint8_t>(note);
                            pm.gmNote = static_cast<uint8_t>(note);
                            pm.description = DrumLibraryManager::getGMDrumName(static_cast<uint8_t>(note));
                            newMappings.push_back(pm);
                        }
                        workingMappings[libraryName] = newMappings;

                        hasUnsavedChanges = true;
                        updateOriginLibrariesList();

                        // Select the newly added library
                        int newIndex = allOriginLibraries.indexOf(libraryName);
                        if (newIndex >= 0)
                        {
                            originLibrariesList.selectRow(newIndex);
                            selectedOriginIndex = newIndex;
                            updateMappingsForSelectedOrigin();
                        }
                    }), true);
            }
        }), true);
}

void OriginLibraryEditor::showAddNoteMappingDialog()
{
    if (selectedOriginIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "No Library Selected", "Please select an origin library first.");
        return;
    }

    juce::String selectedOrigin = allOriginLibraries[selectedOriginIndex];

    if (isProtectedLibrary(selectedOrigin))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Protected Library",
                                               "'" + selectedOrigin + "' has fixed mappings and cannot be modified.");
        return;
    }

    juce::AlertWindow w("Add Note Mapping",
                        "Map a note from " + selectedOrigin + " to General MIDI:",
                        juce::AlertWindow::QuestionIcon);

    w.addTextEditor("originNote", "", "Origin Note (0-127):");
    w.addTextEditor("gmNote", "", "General MIDI Note (0-127):");
    w.addTextEditor("description", "", "Description (optional):");
    w.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (w.runModalLoop() == 1)
    {
        int originNote = w.getTextEditorContents("originNote").getIntValue();
        int gmNote = w.getTextEditorContents("gmNote").getIntValue();
        juce::String description = w.getTextEditorContents("description").trim();

        if (gmNote < 0 || gmNote > 127 || originNote < 0 || originNote > 127)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Invalid Note", "Notes must be between 0 and 127.");
            return;
        }

        // Add to working copy
        PendingMapping pm;
        pm.originNote = (uint8_t)originNote;
        pm.gmNote = (uint8_t)gmNote;
        pm.description = description.isEmpty() ? DrumLibraryManager::getGMDrumName((uint8_t)gmNote) : description;

        auto& libMappings = getWorkingMappingsForLibrary(selectedOrigin);

        // Check for duplicate origin note
        bool found = false;
        for (auto& existing : libMappings)
        {
            if (existing.originNote == pm.originNote)
            {
                // Update existing
                existing.gmNote = pm.gmNote;
                existing.description = pm.description;
                found = true;
                break;
            }
        }

        if (!found)
            libMappings.push_back(pm);

        hasUnsavedChanges = true;
        updateMappingsForSelectedOrigin();
    }
}

void OriginLibraryEditor::showEditMappingDialog(MappingRow* row)
{
    if (!row || selectedOriginIndex < 0)
        return;

    juce::String selectedOrigin = allOriginLibraries[selectedOriginIndex];

    juce::AlertWindow w("Edit Note Mapping",
                        "Edit the note mapping:",
                        juce::AlertWindow::QuestionIcon);

    w.addTextEditor("originNote", juce::String(row->getOriginNote()), "Origin Note (0-127):");
    w.addTextEditor("gmNote", juce::String(row->getGMNote()), "General MIDI Note (0-127):");
    w.addTextEditor("description", row->getDescription(), "Description:");
    w.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    uint8_t oldOriginNote = row->getOriginNote();

    if (w.runModalLoop() == 1)
    {
        int newOriginNote = w.getTextEditorContents("originNote").getIntValue();
        int newGmNote = w.getTextEditorContents("gmNote").getIntValue();
        juce::String newDescription = w.getTextEditorContents("description").trim();

        if (newGmNote < 0 || newGmNote > 127 || newOriginNote < 0 || newOriginNote > 127)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Invalid Note", "Notes must be between 0 and 127.");
            return;
        }

        auto& libMappings = getWorkingMappingsForLibrary(selectedOrigin);

        // Find and update the mapping
        for (auto& pm : libMappings)
        {
            if (pm.originNote == oldOriginNote)
            {
                pm.originNote = (uint8_t)newOriginNote;
                pm.gmNote = (uint8_t)newGmNote;
                pm.description = newDescription.isEmpty() ? DrumLibraryManager::getGMDrumName((uint8_t)newGmNote) : newDescription;
                break;
            }
        }

        hasUnsavedChanges = true;
        updateMappingsForSelectedOrigin();
    }
}

void OriginLibraryEditor::deleteOriginLibrary(int index)
{
    if (index < 0 || index >= allOriginLibraries.size())
        return;

    juce::String libraryName = allOriginLibraries[index];

    bool result = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                     "Delete Origin Library",
                                                     "Remove '" + libraryName + "' and all its mappings?\n\nThis will take effect when you click Save.",
                                                     "Delete", "Cancel");

    if (result)
    {
        // Mark for deletion
        pendingDeletedLibraries.insert(libraryName.toStdString());

        // Remove from working copy
        workingMappings.erase(libraryName);

        // If it was a pending add, remove from that too
        pendingAddedLibraries.erase(libraryName.toStdString());
        pendingAddAsTargetLibraries.erase(libraryName.toStdString());

        hasUnsavedChanges = true;

        // Clear selection if deleted library was selected
        if (selectedOriginIndex == index)
            selectedOriginIndex = -1;

        updateOriginLibrariesList();
        updateMappingsForSelectedOrigin();
    }
}

void OriginLibraryEditor::deleteMapping(uint8_t originNote)
{
    if (selectedOriginIndex < 0)
        return;

    juce::String selectedOrigin = allOriginLibraries[selectedOriginIndex];
    auto& libMappings = getWorkingMappingsForLibrary(selectedOrigin);

    libMappings.erase(
        std::remove_if(libMappings.begin(), libMappings.end(),
                       [originNote](const PendingMapping& pm) { return pm.originNote == originNote; }),
                      libMappings.end()
    );

    hasUnsavedChanges = true;
    updateMappingsForSelectedOrigin();
}

void OriginLibraryEditor::commitChanges()
{
    DBG("=== Committing Origin Library Changes ===");

    // 1. Delete libraries marked for deletion
    for (const auto& libName : pendingDeletedLibraries)
    {
        juce::String jLibName(libName);
        DBG("Deleting origin library: " + jLibName);
        drumLibraryManager.removeOriginLibrary(jLibName);
        DrumLibrary lib = drumLibraryManager.getLibraryFromName(jLibName);
        if (lib != DrumLibrary::Unknown)
            drumLibraryManager.removeLibraryFromMappings(lib);
    }

    // 2. Add new origin libraries
    for (const auto& libName : pendingAddedLibraries)
    {
        juce::String jLibName(libName);
        DBG("Adding origin library: " + jLibName);
        drumLibraryManager.addOriginLibrary(jLibName);
    }

    // 3. Add libraries that should also be target libraries
    for (const auto& libName : pendingAddAsTargetLibraries)
    {
        juce::String jLibName(libName);
        DBG("Adding as target library: " + jLibName);
        drumLibraryManager.addCustomLibrary(jLibName);

        // Initialize target mapping with GM identity
        DrumLibrary newLib = drumLibraryManager.getLibraryFromName(jLibName);
        if (newLib != DrumLibrary::Unknown)
        {
            drumLibraryManager.initializeTargetMappingWithGM(newLib);
        }
    }

    // 4. Update all mappings from working copy (skip protected libraries)
    for (const auto& [libName, mappings] : workingMappings)
    {
        if (isProtectedLibrary(libName))
            continue;

        DrumLibrary lib = drumLibraryManager.getLibraryFromName(libName);
        if (lib == DrumLibrary::Unknown)
            continue;

        DBG("Updating mappings for origin: " + libName + " (" + juce::String(mappings.size()) + " mappings)");

        for (const auto& pm : mappings)
        {
            drumLibraryManager.updateLibraryMapping(lib, pm.gmNote, pm.originNote);

            // Set custom name if different from default
            juce::String defaultName = DrumLibraryManager::getGMDrumName(pm.gmNote);
            if (pm.description != defaultName && pm.description.isNotEmpty())
                drumLibraryManager.setCustomDrumName(lib, pm.gmNote, pm.description);
            else
                drumLibraryManager.clearCustomDrumName(lib, pm.gmNote);
        }
    }

    // 5. Save to files
    drumLibraryManager.saveOriginMappings();
    drumLibraryManager.saveCustomMappings();

    hasUnsavedChanges = false;
    pendingAddedLibraries.clear();
    pendingDeletedLibraries.clear();
    pendingAddAsTargetLibraries.clear();

    if (onLibrariesChanged)
        onLibrariesChanged();

    DBG("=== Origin Library Changes Committed ===");
}

void OriginLibraryEditor::saveCustomMappings()
{
    commitChanges();
}

void OriginLibraryEditor::pasteFromClipboard()
{
    if (selectedOriginIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "No Library Selected", "Please select an origin library first.");
        return;
    }

    juce::String selectedOrigin = allOriginLibraries[selectedOriginIndex];

    if (isProtectedLibrary(selectedOrigin))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Protected Library",
                                               "'" + selectedOrigin + "' has fixed mappings and cannot be modified.");
        return;
    }

    juce::String clipboardText = juce::SystemClipboard::getTextFromClipboard();

    if (clipboardText.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Empty Clipboard", "Nothing to paste from clipboard.");
        return;
    }

    auto& libMappings = getWorkingMappingsForLibrary(selectedOrigin);

    juce::StringArray lines;
    lines.addLines(clipboardText);

    int addedCount = 0;
    int errorCount = 0;

    for (const auto& line : lines)
    {
        if (line.trim().isEmpty())
            continue;

        if (line.containsIgnoreCase("Origin") && line.containsIgnoreCase("GM"))
            continue;

        juce::StringArray columns;
        if (line.contains("\t"))
            columns.addTokens(line, "\t", "\"");
        else
            columns.addTokens(line, ",", "\"");

        if (columns.size() < 2)
        {
            errorCount++;
            continue;
        }

        int originNote = columns[0].trim().getIntValue();
        int gmNote = columns[1].trim().getIntValue();

        if (originNote < 0 || originNote > 127 || gmNote < 0 || gmNote > 127)
        {
            errorCount++;
            continue;
        }

        juce::String description;
        if (columns.size() >= 3)
            description = columns[2].trim();
        if (description.isEmpty())
            description = DrumLibraryManager::getGMDrumName((uint8_t)gmNote);

        PendingMapping pm;
        pm.originNote = (uint8_t)originNote;
        pm.gmNote = (uint8_t)gmNote;
        pm.description = description;

        // Check for duplicate
        bool found = false;
        for (auto& existing : libMappings)
        {
            if (existing.originNote == pm.originNote)
            {
                existing.gmNote = pm.gmNote;
                existing.description = pm.description;
                found = true;
                break;
            }
        }

        if (!found)
            libMappings.push_back(pm);

        addedCount++;
    }

    hasUnsavedChanges = true;
    updateMappingsForSelectedOrigin();

    juce::String message = juce::String(addedCount) + " mapping(s) imported successfully.";
    if (errorCount > 0)
        message += "\n" + juce::String(errorCount) + " line(s) skipped (invalid format).";

    juce::AlertWindow::showMessageBoxAsync(
        errorCount > 0 ? juce::AlertWindow::WarningIcon : juce::AlertWindow::InfoIcon,
        "Import Complete", message);
}

void OriginLibraryEditor::showEditor(DrumLibraryManager& manager, juce::Component* parent, std::function<void()> onLibrariesChanged)
{
    auto* editor = new OriginLibraryEditor(manager);
    editor->onLibrariesChanged = onLibrariesChanged;

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(editor);
    options.dialogTitle = "Origin MIDI Library Manager";
    options.dialogBackgroundColour = ColourPalette::mainBackground;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    auto* dialogWindow = options.launchAsync();

    if (parent)
        dialogWindow->centreAroundComponent(parent, editor->getWidth(), editor->getHeight());
}

// ============================================================================
// MappingRow Implementation
// ============================================================================

OriginLibraryEditor::MappingRow::MappingRow(uint8_t originNote, uint8_t gmNote,
                                            const juce::String& description, bool readOnly)
: currentOriginNote(originNote)
, currentGMNote(gmNote)
, currentDescription(description)
, isReadOnly(readOnly)
, deleteButton("Delete", juce::DrawableButton::ImageFitted)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    originNoteLabel.setText(juce::String(originNote), juce::dontSendNotification);
    originNoteLabel.setFont(lnf.getNormalFont().withHeight(12.0f));
    originNoteLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    originNoteLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    originNoteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(originNoteLabel);

    arrowLabel.setText("", juce::dontSendNotification);
    arrowLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(arrowLabel);

    gmNoteLabel.setText(juce::String(gmNote), juce::dontSendNotification);
    gmNoteLabel.setFont(lnf.getNormalFont().withHeight(12.0f));
    gmNoteLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    gmNoteLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    gmNoteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gmNoteLabel);

    drumNameLabel.setText(description, juce::dontSendNotification);
    drumNameLabel.setFont(lnf.getNormalFont().withHeight(12.0f));

    juce::String defaultName = DrumLibraryManager::getGMDrumName(gmNote);
    bool isCustom = (description != defaultName && description.isNotEmpty());
    drumNameLabel.setColour(juce::Label::textColourId,
                            isCustom ? ColourPalette::successGreen : ColourPalette::secondaryText);
    drumNameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    drumNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(drumNameLabel);

    editButton.setButtonText("Edit");
    editButton.setTooltip("Edit this mapping");
    editButton.addListener(this);
    editButton.setEnabled(!readOnly);
    addAndMakeVisible(editButton);

    // Load icons
    juce::File pluginFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File resourcesPath;

    #if JUCE_WINDOWS
    resourcesPath = pluginFile.getParentDirectory().getParentDirectory().getChildFile("Resources");
    #elif JUCE_MAC
    resourcesPath = pluginFile.getParentDirectory().getParentDirectory().getChildFile("Resources");
    #else
    resourcesPath = pluginFile.getParentDirectory().getParentDirectory().getChildFile("Resources");
    #endif

    juce::File deleteIconFile = resourcesPath.getChildFile("icons").getChildFile("misc")
    .getChildFile("16x16").getChildFile("cross.png");
    juce::File arrowIconFile = resourcesPath.getChildFile("icons").getChildFile("misc")
    .getChildFile("16x16").getChildFile("arrow.png");

    if (deleteIconFile.existsAsFile())
        deleteIcon = juce::ImageCache::getFromFile(deleteIconFile);
    if (arrowIconFile.existsAsFile())
        arrowIcon = juce::ImageCache::getFromFile(arrowIconFile);

    if (deleteIcon.isValid() && !readOnly)
    {
        deleteNormalImage = std::make_unique<juce::DrawableImage>();
        deleteNormalImage->setImage(deleteIcon);

        deleteOverImage = std::make_unique<juce::DrawableImage>();
        deleteOverImage->setImage(deleteIcon);
        deleteOverImage->setOverlayColour(ColourPalette::errorRed.withAlpha(0.3f));

        deleteButton.setImages(deleteNormalImage.get(), deleteOverImage.get(), deleteOverImage.get());
    }

    deleteButton.setTooltip("Remove this mapping");
    deleteButton.addListener(this);
    deleteButton.setVisible(!readOnly);
    addAndMakeVisible(deleteButton);

    setSize(600, 35);
}

OriginLibraryEditor::MappingRow::~MappingRow()
{
    editButton.removeListener(this);
    deleteButton.removeListener(this);
}

void OriginLibraryEditor::MappingRow::buttonClicked(juce::Button* button)
{
    if (button == &deleteButton)
    {
        if (onDelete)
            onDelete();
    }
    else if (button == &editButton)
    {
        if (onEdit)
            onEdit();
    }
}

void OriginLibraryEditor::MappingRow::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::secondaryBackground);

    if (arrowIcon.isValid())
    {
        auto arrowBounds = arrowLabel.getBounds();
        g.drawImage(arrowIcon, arrowBounds.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else
    {
        g.setColour(ColourPalette::primaryBlue);
        g.setFont(14.0f);
        g.drawText("->", arrowLabel.getBounds(), juce::Justification::centred);
    }
}

void OriginLibraryEditor::MappingRow::resized()
{
    auto bounds = getLocalBounds().reduced(5, 0);

    // Layout: Origin Note | Arrow | GM Note | Description | Edit | Delete
    originNoteLabel.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(5);
    arrowLabel.setBounds(bounds.removeFromLeft(30));
    bounds.removeFromLeft(5);
    gmNoteLabel.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(10);

    deleteButton.setBounds(bounds.removeFromRight(30));
    bounds.removeFromRight(5);
    editButton.setBounds(bounds.removeFromRight(40));
    bounds.removeFromRight(10);

    drumNameLabel.setBounds(bounds);
}
