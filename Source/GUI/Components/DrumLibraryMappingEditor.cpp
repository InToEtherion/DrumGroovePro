#include "DrumLibraryMappingEditor.h"
#include "DrumGrooveLookAndFeel.h"
#include "PluginProcessor.h"

DrumLibraryMappingEditor::DrumLibraryMappingEditor(DrumLibraryManager& manager, DrumGrooveProcessor* proc)
: drumLibraryManager(manager), processor(proc)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    // Title - centered both horizontally and vertically
    titleLabel.setText("Target Drum Library Manager", juce::dontSendNotification);
    titleLabel.setFont(lnf.getBoldFont().withHeight(18.0f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(titleLabel);

    // Products section
    productsLabel.setText("Target Libraries:", juce::dontSendNotification);
    productsLabel.setFont(lnf.getBoldFont().withHeight(14.0f));
    productsLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(productsLabel);

    productsList.setModel(this);
    productsList.setRowHeight(30);
    productsList.setColour(juce::ListBox::backgroundColourId, ColourPalette::secondaryBackground);
    addAndMakeVisible(productsList);

    addProductButton.setButtonText("+ Add Target Library");
    addProductButton.addListener(this);
    addAndMakeVisible(addProductButton);

    // Mappings section
    mappingsLabel.setText("Note Mappings (GM -> Target):", juce::dontSendNotification);
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
    pasteFromClipboardButton.setTooltip("Paste mappings from Excel/spreadsheet (GM Note, Target Note, Description)");
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
    {
        crossIcon = juce::ImageCache::getFromFile(deleteIconFile);
        productDeleteIcon = crossIcon;
    }

    juce::File arrowIconFile = resourcesPath.getChildFile("icons")
    .getChildFile("misc")
    .getChildFile("16x16")
    .getChildFile("arrow.png");

    if (arrowIconFile.existsAsFile())
        arrowIcon = juce::ImageCache::getFromFile(arrowIconFile);

    // Load working copy from DrumLibraryManager
    loadWorkingCopy();

    updateProductsList();

    setSize(900, 600);
}

DrumLibraryMappingEditor::~DrumLibraryMappingEditor()
{
    addProductButton.removeListener(this);
    addMappingButton.removeListener(this);
    pasteFromClipboardButton.removeListener(this);
    saveButton.removeListener(this);
    cancelButton.removeListener(this);
}

void DrumLibraryMappingEditor::loadWorkingCopy()
{
    workingMappings.clear();
    pendingAddedLibraries.clear();
    pendingDeletedLibraries.clear();
    hasUnsavedChanges = false;

    // Load all products and their mappings using getAllLibraryNames()
    auto allLibraries = drumLibraryManager.getAllLibraryNames();

    for (const auto& libName : allLibraries)
    {
        // Skip Unknown but keep Bypass
        if (libName == "Unknown")
            continue;

        DrumLibrary lib = drumLibraryManager.getLibraryFromName(libName);
        if (lib == DrumLibrary::Unknown)
            continue;

        // Get current mappings for this library
        auto explicitMappings = drumLibraryManager.getExplicitMappingsForLibrary(lib);

        std::vector<PendingMapping> libMappings;
        for (const auto& [gmNote, targetNote] : explicitMappings)
        {
            PendingMapping pm;
            pm.gmNote = gmNote;
            pm.targetNote = targetNote;
            pm.description = drumLibraryManager.getCustomDrumName(lib, gmNote);
            if (pm.description.isEmpty())
                pm.description = DrumLibraryManager::getGMDrumName(gmNote);
            libMappings.push_back(pm);
        }

        workingMappings[libName] = libMappings;
    }

    DBG("Loaded working copy with " + juce::String(workingMappings.size()) + " target libraries");
}

std::vector<DrumLibraryMappingEditor::PendingMapping>& DrumLibraryMappingEditor::getWorkingMappingsForLibrary(const juce::String& libraryName)
{
    return workingMappings[libraryName];
}

bool DrumLibraryMappingEditor::isProtectedLibrary(const juce::String& libraryName) const
{
    // ONLY these 4 libraries are fully protected:
    // - No delete icon at library level
    // - No Edit/Delete buttons at row level
    return libraryName == "General MIDI" ||
    libraryName == "Bypass (No Remapping)" ||
    libraryName == "Salamander Drumkit" ||
    libraryName == "MuldjordKit" ||
    libraryName == "The Aasimonster";
}

void DrumLibraryMappingEditor::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);

    // Draw header background
    g.setColour(ColourPalette::primaryBlue);
    g.fillRect(0, 0, getWidth(), 50);
}

void DrumLibraryMappingEditor::resized()
{
    auto bounds = getLocalBounds();

    // Title area - 50px high header, text centered vertically and horizontally
    auto headerArea = bounds.removeFromTop(50);
    titleLabel.setBounds(headerArea);

    bounds = bounds.reduced(20);
    bounds.removeFromTop(10);

    auto leftSection = bounds.removeFromLeft(300);
    bounds.removeFromLeft(20);

    productsLabel.setBounds(leftSection.removeFromTop(30));
    addProductButton.setBounds(leftSection.removeFromBottom(30));
    leftSection.removeFromBottom(10);
    productsList.setBounds(leftSection);

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

int DrumLibraryMappingEditor::getNumRows()
{
    return allProducts.size();
}

void DrumLibraryMappingEditor::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= allProducts.size())
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

    juce::String productName = allProducts[rowNumber];

    // Mark pending added libraries with asterisk
    juce::String displayName = productName;
    if (pendingAddedLibraries.count(productName.toStdString()) > 0)
        displayName += " *";

    g.drawText(displayName, 10, 0, width - 80, height, juce::Justification::centredLeft);

    // Draw delete icon for non-protected libraries
    if (!isProtectedLibrary(productName))
    {
        auto deleteArea = juce::Rectangle<int>(width - 70, height / 2 - 8, 16, 16);

        if (productDeleteIcon.isValid())
        {
            g.drawImage(productDeleteIcon, deleteArea.toFloat(),
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

void DrumLibraryMappingEditor::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row >= allProducts.size())
        return;

    juce::String productName = allProducts[row];

    int deleteAreaStart = productsList.getWidth() - 75;
    int deleteAreaEnd = productsList.getWidth() - 50;

    if (e.x >= deleteAreaStart && e.x <= deleteAreaEnd)
    {
        // Can delete any library that is NOT protected
        if (!isProtectedLibrary(productName))
        {
            deleteProduct(row);
        }
    }
    else
    {
        selectedProductIndex = row;
        updateMappingsForSelectedProduct();
    }
}

void DrumLibraryMappingEditor::buttonClicked(juce::Button* button)
{
    if (button == &addProductButton)
    {
        showAddProductDialog();
    }
    else if (button == &addMappingButton)
    {
        showAddNoteMappingDialog();
    }
    else if (button == &pasteFromClipboardButton)
    {
        pasteFromClipboard();
    }
    else if (button == &saveButton)
    {
        commitChanges();

        if (auto* parent = getParentComponent())
        {
            if (auto* dialogWindow = dynamic_cast<juce::DialogWindow*>(parent))
                dialogWindow->exitModalState(1);
        }
    }
    else if (button == &cancelButton)
    {
        // Discard changes - just close without committing
        if (auto* parent = getParentComponent())
        {
            if (auto* dialogWindow = dynamic_cast<juce::DialogWindow*>(parent))
                dialogWindow->exitModalState(0);
        }
    }
}

void DrumLibraryMappingEditor::updateProductsList()
{
    allProducts.clear();

    // Add Bypass first (special - pass-through, no remapping)
    allProducts.add("Bypass (No Remapping)");

    // Add General MIDI (protected)
    allProducts.add("General MIDI");

    // Add built-in libraries from working copy
    for (const auto& [libName, mappings] : workingMappings)
    {
        if (libName != "General MIDI" &&
            libName != "Unknown" &&
            libName != "Bypass (No Remapping)")
        {
            if (pendingDeletedLibraries.count(libName.toStdString()) == 0)
            {
                if (!allProducts.contains(libName))
                    allProducts.add(libName);
            }
        }
    }

    // Add pending new libraries
    for (const auto& libName : pendingAddedLibraries)
    {
        juce::String jLibName(libName);
        if (!allProducts.contains(jLibName))
            allProducts.add(jLibName);
    }

    productsList.updateContent();
    productsList.repaint();
}

void DrumLibraryMappingEditor::updateMappingsForSelectedProduct()
{
    mappingRows.clear();
    mappingsContainer.removeAllChildren();

    if (selectedProductIndex < 0 || selectedProductIndex >= allProducts.size())
    {
        mappingsLabel.setText("Note Mappings (GM -> Target):", juce::dontSendNotification);
        resized();
        return;
    }

    juce::String selectedProduct = allProducts[selectedProductIndex];
    mappingsLabel.setText("Note Mappings (GM -> " + selectedProduct + "):", juce::dontSendNotification);

    // Check if this library has protected mappings (read-only rows)
    bool isReadOnly = isProtectedLibrary(selectedProduct);

    // Get mappings from working copy
    auto& mappings = getWorkingMappingsForLibrary(selectedProduct);

    for (size_t i = 0; i < mappings.size(); ++i)
    {
        const auto& pm = mappings[i];

        auto* row = mappingRows.add(new NoteMappingRow(pm.gmNote, pm.targetNote, pm.description, isReadOnly));
        mappingsContainer.addAndMakeVisible(row);

        if (!isReadOnly)
        {
            size_t capturedIndex = i;

            row->onDelete = [this, capturedIndex, selectedProduct]()
            {
                auto& libMappings = getWorkingMappingsForLibrary(selectedProduct);
                if (capturedIndex < libMappings.size())
                {
                    libMappings.erase(libMappings.begin() + capturedIndex);
                    hasUnsavedChanges = true;
                    updateMappingsForSelectedProduct();
                }
            };

            row->onEdit = [this, row]()
            {
                showEditMappingDialog(row);
            };
        }

        // ADD PLAY CALLBACK (for ALL rows, even read-only)
        row->onPlay = [this, targetNote = pm.targetNote]()
        {
            if (processor)
            {
                // Play target note directly (no remapping)
                processor->triggerPreviewNote(targetNote, 100);
            }
        };
    }

    resized();
}

void DrumLibraryMappingEditor::showAddProductDialog()
{
    juce::AlertWindow w("Add Target Library",
                        "Enter the name of the target drum library:",
                        juce::AlertWindow::QuestionIcon);

    w.addTextEditor("name", "", "Library Name:");
    w.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (w.runModalLoop() == 1)
    {
        juce::String libraryName = w.getTextEditorContents("name").trim();

        if (libraryName.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Invalid Name", "Library name cannot be empty.");
            return;
        }

        if (allProducts.contains(libraryName))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Duplicate", "A target library with that name already exists.");
            return;
        }

        // Add to pending and working copy
        pendingAddedLibraries.insert(libraryName.toStdString());
        workingMappings[libraryName] = std::vector<PendingMapping>();
        hasUnsavedChanges = true;

        updateProductsList();
    }
}

void DrumLibraryMappingEditor::showAddNoteMappingDialog()
{
    if (selectedProductIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "No Library Selected", "Please select a target library first.");
        return;
    }

    juce::String selectedProduct = allProducts[selectedProductIndex];

    if (isProtectedLibrary(selectedProduct))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Protected Library",
                                               "'" + selectedProduct + "' has fixed mappings and cannot be modified.");
        return;
    }

    juce::AlertWindow w("Add Note Mapping",
                        "Map a GM note to " + selectedProduct + ":",
                        juce::AlertWindow::QuestionIcon);

    w.addTextEditor("gmNote", "", "GM Note (0-127):");
    w.addTextEditor("targetNote", "", "Target Note (0-127):");
    w.addTextEditor("description", "", "Description (optional):");
    w.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (w.runModalLoop() == 1)
    {
        int gmNote = w.getTextEditorContents("gmNote").getIntValue();
        int targetNote = w.getTextEditorContents("targetNote").getIntValue();
        juce::String description = w.getTextEditorContents("description").trim();

        if (gmNote < 0 || gmNote > 127 || targetNote < 0 || targetNote > 127)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Invalid Note", "Notes must be between 0 and 127.");
            return;
        }

        PendingMapping pm;
        pm.gmNote = (uint8_t)gmNote;
        pm.targetNote = (uint8_t)targetNote;
        pm.description = description.isEmpty() ? DrumLibraryManager::getGMDrumName((uint8_t)gmNote) : description;

        auto& libMappings = getWorkingMappingsForLibrary(selectedProduct);

        // Check for duplicate GM note
        bool found = false;
        for (auto& existing : libMappings)
        {
            if (existing.gmNote == pm.gmNote)
            {
                existing.targetNote = pm.targetNote;
                existing.description = pm.description;
                found = true;
                break;
            }
        }

        if (!found)
            libMappings.push_back(pm);

        hasUnsavedChanges = true;
        updateMappingsForSelectedProduct();
    }
}

void DrumLibraryMappingEditor::showEditMappingDialog(NoteMappingRow* row)
{
    if (!row || selectedProductIndex < 0)
        return;

    juce::String selectedProduct = allProducts[selectedProductIndex];

    juce::AlertWindow w("Edit Note Mapping",
                        "Edit the note mapping:",
                        juce::AlertWindow::QuestionIcon);

    w.addTextEditor("gmNote", juce::String(row->getGMNote()), "GM Note (0-127):");
    w.addTextEditor("targetNote", juce::String(row->getTargetNote()), "Target Note (0-127):");
    w.addTextEditor("description", row->getDescription(), "Description:");
    w.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    uint8_t oldGmNote = row->getGMNote();

    if (w.runModalLoop() == 1)
    {
        int newGmNote = w.getTextEditorContents("gmNote").getIntValue();
        int newTargetNote = w.getTextEditorContents("targetNote").getIntValue();
        juce::String newDescription = w.getTextEditorContents("description").trim();

        if (newGmNote < 0 || newGmNote > 127 || newTargetNote < 0 || newTargetNote > 127)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Invalid Note", "Notes must be between 0 and 127.");
            return;
        }

        auto& libMappings = getWorkingMappingsForLibrary(selectedProduct);

        for (auto& pm : libMappings)
        {
            if (pm.gmNote == oldGmNote)
            {
                pm.gmNote = (uint8_t)newGmNote;
                pm.targetNote = (uint8_t)newTargetNote;
                pm.description = newDescription.isEmpty() ? DrumLibraryManager::getGMDrumName((uint8_t)newGmNote) : newDescription;
                break;
            }
        }

        hasUnsavedChanges = true;
        updateMappingsForSelectedProduct();
    }
}

void DrumLibraryMappingEditor::deleteProduct(int productIndex)
{
    if (productIndex < 0 || productIndex >= allProducts.size())
        return;

    juce::String productName = allProducts[productIndex];

    // Double-check protection
    if (isProtectedLibrary(productName))
        return;

    bool result = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                     "Delete Target Library",
                                                     "Remove '" + productName + "' and all its mappings?\n\nThis will take effect when you click Save.",
                                                     "Delete", "Cancel");

    if (result)
    {
        pendingDeletedLibraries.insert(productName.toStdString());
        workingMappings.erase(productName);
        pendingAddedLibraries.erase(productName.toStdString());

        hasUnsavedChanges = true;

        if (selectedProductIndex == productIndex)
            selectedProductIndex = -1;

        updateProductsList();
        updateMappingsForSelectedProduct();
    }
}

void DrumLibraryMappingEditor::commitChanges()
{
    DBG("=== Committing Target Library Changes ===");

    // 1. Delete libraries marked for deletion (only non-protected)
    for (const auto& libName : pendingDeletedLibraries)
    {
        juce::String jLibName(libName);

        // Skip protected libraries
        if (isProtectedLibrary(jLibName))
            continue;

        DBG("Deleting target library: " + jLibName);
        DrumLibrary lib = drumLibraryManager.getLibraryFromName(jLibName);

        // For custom libraries (enum >= 1000), use removeCustomLibrary
        if (lib != DrumLibrary::Unknown && static_cast<int>(lib) >= 1000)
        {
            drumLibraryManager.removeCustomLibrary(lib);
        }
        // For built-in libraries, just remove from mappings
        else if (lib != DrumLibrary::Unknown)
        {
            drumLibraryManager.removeLibraryFromMappings(lib);
        }
    }

    // 2. Add new libraries
    for (const auto& libName : pendingAddedLibraries)
    {
        juce::String jLibName(libName);
        DBG("Adding target library: " + jLibName);
        drumLibraryManager.addCustomLibrary(jLibName);
    }

    // 3. Update all mappings from working copy (skip protected libraries)
    for (const auto& [libName, mappings] : workingMappings)
    {
        // Skip protected libraries - they have fixed mappings
        if (isProtectedLibrary(libName))
            continue;

        DrumLibrary lib = drumLibraryManager.getLibraryFromName(libName);
        if (lib == DrumLibrary::Unknown)
            continue;

        DBG("Updating mappings for target: " + libName + " (" + juce::String(mappings.size()) + " mappings)");

        // CRITICAL FIX: Get existing mappings and remove any that are not in working copy
        auto existingMappings = drumLibraryManager.getExplicitMappingsForLibrary(lib);
        for (const auto& [gmNote, targetNote] : existingMappings)
        {
            // Check if this mapping still exists in working copy
            bool stillExists = false;
            for (const auto& pm : mappings)
            {
                if (pm.gmNote == gmNote)
                {
                    stillExists = true;
                    break;
                }
            }

            // If not in working copy, remove it
            if (!stillExists)
            {
                DBG("Removing deleted mapping: GM " + juce::String(gmNote));
                drumLibraryManager.removeMappingForNote(lib, gmNote);
                drumLibraryManager.clearCustomDrumName(lib, gmNote);
            }
        }

        // Now update/add all mappings from working copy
        for (const auto& pm : mappings)
        {
            drumLibraryManager.updateLibraryMapping(lib, pm.gmNote, pm.targetNote);

            juce::String defaultName = DrumLibraryManager::getGMDrumName(pm.gmNote);
            if (pm.description != defaultName && pm.description.isNotEmpty())
                drumLibraryManager.setCustomDrumName(lib, pm.gmNote, pm.description);
            else
                drumLibraryManager.clearCustomDrumName(lib, pm.gmNote);
        }
    }

    // 4. Save to files
    drumLibraryManager.saveCustomMappings();

    hasUnsavedChanges = false;

    if (onLibrariesChanged)
        onLibrariesChanged();

    pendingAddedLibraries.clear();
    pendingDeletedLibraries.clear();

    DBG("=== Target Library Changes Committed ===");
}

void DrumLibraryMappingEditor::pasteFromClipboard()
{
    if (selectedProductIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "No Library Selected", "Please select a target library first.");
        return;
    }

    juce::String selectedProduct = allProducts[selectedProductIndex];

    if (isProtectedLibrary(selectedProduct))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Protected Library",
                                               "'" + selectedProduct + "' has fixed mappings and cannot be modified.");
        return;
    }

    juce::String clipboardText = juce::SystemClipboard::getTextFromClipboard();

    if (clipboardText.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Empty Clipboard", "Nothing to paste from clipboard.");
        return;
    }

    auto& libMappings = getWorkingMappingsForLibrary(selectedProduct);

    juce::StringArray lines;
    lines.addLines(clipboardText);

    int addedCount = 0;
    int errorCount = 0;

    for (const auto& line : lines)
    {
        if (line.trim().isEmpty())
            continue;

        if (line.containsIgnoreCase("GM") && line.containsIgnoreCase("Target"))
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

        int gmNote = columns[0].trim().getIntValue();
        int targetNote = columns[1].trim().getIntValue();

        if (gmNote < 0 || gmNote > 127 || targetNote < 0 || targetNote > 127)
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
        pm.gmNote = (uint8_t)gmNote;
        pm.targetNote = (uint8_t)targetNote;
        pm.description = description;

        bool found = false;
        for (auto& existing : libMappings)
        {
            if (existing.gmNote == pm.gmNote)
            {
                existing.targetNote = pm.targetNote;
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
    updateMappingsForSelectedProduct();

    juce::String message = juce::String(addedCount) + " mapping(s) imported successfully.";
    if (errorCount > 0)
        message += "\n" + juce::String(errorCount) + " line(s) skipped (invalid format).";

    juce::AlertWindow::showMessageBoxAsync(
        errorCount > 0 ? juce::AlertWindow::WarningIcon : juce::AlertWindow::InfoIcon,
        "Import Complete", message);
}

void DrumLibraryMappingEditor::showEditor(DrumLibraryManager& manager, DrumGrooveProcessor* processor, juce::Component* parent,
                                          std::function<void()> onLibrariesChangedCallback)
{
    auto* editor = new DrumLibraryMappingEditor(manager, processor);
    editor->onLibrariesChanged = onLibrariesChangedCallback;

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(editor);
    options.dialogTitle = "Target MIDI Library Mapping Editor";
    options.dialogBackgroundColour = ColourPalette::mainBackground;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    auto* dialogWindow = options.launchAsync();

    if (parent)
        dialogWindow->centreAroundComponent(parent, editor->getWidth(), editor->getHeight());
}

// ============================================================================
// NoteMappingRow Implementation
// ============================================================================

DrumLibraryMappingEditor::NoteMappingRow::NoteMappingRow(uint8_t gm, uint8_t target,
                                                         const juce::String& description, bool isReadOnly)
: gmNote(gm)
, targetNote(target)
, currentDescription(description)
, readOnly(isReadOnly)
, deleteButton("Delete", juce::DrawableButton::ImageFitted)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    gmNoteLabel.setText(juce::String(gm), juce::dontSendNotification);
    gmNoteLabel.setFont(lnf.getNormalFont().withHeight(12.0f));
    gmNoteLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    gmNoteLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    gmNoteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gmNoteLabel);

    gmNameLabel.setText(description, juce::dontSendNotification);
    gmNameLabel.setFont(lnf.getNormalFont().withHeight(12.0f));

    juce::String defaultName = DrumLibraryManager::getGMDrumName(gm);
    bool isCustom = (description != defaultName && description.isNotEmpty());
    gmNameLabel.setColour(juce::Label::textColourId,
                          isCustom ? ColourPalette::successGreen : ColourPalette::secondaryText);
    gmNameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    gmNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(gmNameLabel);

    arrowLabel.setText("", juce::dontSendNotification);
    arrowLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(arrowLabel);

    targetNoteLabel.setText(juce::String(target), juce::dontSendNotification);
    targetNoteLabel.setFont(lnf.getNormalFont().withHeight(12.0f));
    targetNoteLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    targetNoteLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    targetNoteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(targetNoteLabel);

    editButton.setButtonText("Edit");
    editButton.setTooltip("Edit this mapping");
    editButton.addListener(this);
    editButton.setEnabled(!readOnly);
    editButton.setVisible(!readOnly);  // Hide for protected libraries
    addAndMakeVisible(editButton);

    // ADD PLAY BUTTON
    playButton.setButtonText("Play");
    playButton.setTooltip("Preview this target sound");
    playButton.addListener(this);
    addAndMakeVisible(playButton);

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
    deleteButton.setVisible(!readOnly);  // Hide for protected libraries
    addAndMakeVisible(deleteButton);

    setSize(600, 35);
}

DrumLibraryMappingEditor::NoteMappingRow::~NoteMappingRow()
{
    editButton.removeListener(this);
    deleteButton.removeListener(this);
}

void DrumLibraryMappingEditor::NoteMappingRow::buttonClicked(juce::Button* button)
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
    else if (button == &playButton)
    {
        if (onPlay)
            onPlay();
    }
}

void DrumLibraryMappingEditor::NoteMappingRow::paint(juce::Graphics& g)
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

void DrumLibraryMappingEditor::NoteMappingRow::resized()
{
    auto bounds = getLocalBounds().reduced(5, 0);

    // Layout: GM Note | Description | Arrow | Target Note | Play | Edit | Delete
    gmNoteLabel.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(5);
    gmNameLabel.setBounds(bounds.removeFromLeft(180));
    bounds.removeFromLeft(5);
    arrowLabel.setBounds(bounds.removeFromLeft(30));
    bounds.removeFromLeft(5);
    targetNoteLabel.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(10);

    if (!readOnly)
    {
        deleteButton.setBounds(bounds.removeFromRight(30));
        bounds.removeFromRight(5);
        editButton.setBounds(bounds.removeFromRight(40));
        bounds.removeFromRight(5);
    }

    playButton.setBounds(bounds.removeFromRight(50));
}


