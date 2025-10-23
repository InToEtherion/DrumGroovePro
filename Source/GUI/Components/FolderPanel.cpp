#include "FolderPanel.h"
#include "Timeline.h"
#include "AddFolderDialog.h"
#include "AboutDialog.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include "../../PluginProcessor.h"

// FavoritesModel implementation
int FavoritesModel::getNumRows()
{
    return processor.favoritesManager.getNumFavorites();
}

void FavoritesModel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                      int width, int height, bool rowIsSelected)
{
    if (rowNumber >= processor.favoritesManager.getNumFavorites())
        return;
    
    juce::String text = processor.favoritesManager.getFavoriteName(rowNumber);
    
    if (rowIsSelected)
    {
        g.fillAll(ColourPalette::primaryBlue);
        g.setColour(ColourPalette::primaryText);
    }
    else
    {
        g.setColour(ColourPalette::primaryText);
    }
    
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    g.setFont(lnf.getNormalFont());
    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft);
    
    g.setColour(ColourPalette::separator);
    g.drawLine(0.0f, static_cast<float>(height - 1), static_cast<float>(width), static_cast<float>(height - 1));
}

// FolderPanel implementation
FolderPanel::FolderPanel(DrumGrooveProcessor& p)
    : processor(p)
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    
    addFolderButton.setButtonText("ADD FOLDER");
    addFolderButton.addListener(this);
    addAndMakeVisible(addFolderButton);
    
    rescanButton.setButtonText("RESCAN");
    rescanButton.addListener(this);
    addAndMakeVisible(rescanButton);
    
    aboutButton.setButtonText("ABOUT");
    aboutButton.addListener(this);
    addAndMakeVisible(aboutButton);
    
    folderGroup.setText("Library Folders");
    folderGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::primaryBlue);
    addAndMakeVisible(folderGroup);
    
    folderCountLabel.setText("0 folder(s) loaded", juce::dontSendNotification);
    folderCountLabel.setFont(lnf.getSmallFont());
    folderCountLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(folderCountLabel);
    
    folderList.setModel(this);
    folderList.setRowHeight(24);
    folderList.setColour(juce::ListBox::backgroundColourId, ColourPalette::secondaryBackground);
    
    // Setup viewport for folder list scrolling
    folderViewport = std::make_unique<juce::Viewport>();
    folderViewport->setViewedComponent(&folderList, false);
    folderViewport->setScrollBarsShown(false, true);  // Vertical scrollbar only
    
    // Style the scrollbars to match the theme
    folderViewport->getVerticalScrollBar().setColour(juce::ScrollBar::backgroundColourId, ColourPalette::secondaryBackground);
    folderViewport->getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, ColourPalette::borderColour);
    
    addAndMakeVisible(folderViewport.get());
    
    folderList.onDeletePressed = [this]() {
        removeSelectedFolders();
    };
    
    folderSelectedCallback = [this](int index) {
        if (index >= 0 && index < processor.drumLibraryManager.getNumRootFolders())
        {
            auto folder = processor.drumLibraryManager.getRootFolder(index);
            if (onFolderSelected)
                onFolderSelected(folder);
        }
    };
    
    fileInfoGroup.setText("Favorites");
    fileInfoGroup.setColour(juce::GroupComponent::textColourId, ColourPalette::primaryBlue);
    addAndMakeVisible(fileInfoGroup);
    
    favoritesModel = std::make_unique<FavoritesModel>(processor);
    
    favoritesModel->onRowSelected = [this](int row) {
        // When favorite is selected, clear library folders selection  
        folderList.deselectAllRows(); // Clear library folders selection
        selectedFolder = -1; // Clear the selected folder index
        
        auto path = processor.favoritesManager.getFavoritePath(row);
        if (path.exists() && onFolderSelected)
            onFolderSelected(path);
    };
    
    favoritesList.setActualModel(favoritesModel.get());
    favoritesList.setRowHeight(24);
    favoritesList.setColour(juce::ListBox::backgroundColourId, ColourPalette::secondaryBackground);
    
    // Setup viewport for favorites list scrolling
    favoritesViewport = std::make_unique<juce::Viewport>();
    favoritesViewport->setViewedComponent(&favoritesList, false);
    favoritesViewport->setScrollBarsShown(false, true);  // Vertical scrollbar only
    
    // Style the scrollbars to match the theme
    favoritesViewport->getVerticalScrollBar().setColour(juce::ScrollBar::backgroundColourId, ColourPalette::secondaryBackground);
    favoritesViewport->getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, ColourPalette::borderColour);
    
    addAndMakeVisible(favoritesViewport.get());
    
    favoritesList.onDoubleClick = [this](int row) {
        auto path = processor.favoritesManager.getFavoritePath(row);
        if (path.exists() && onFolderSelected)
            onFolderSelected(path);
    };
    
    favoritesList.onRightClick = [this](int row) {
        DBG("=== onRightClick CALLBACK TRIGGERED ===");
        DBG("Row: " + juce::String(row));
        DBG("Num favorites: " + juce::String(processor.favoritesManager.getNumFavorites()));
        
        if (row < 0 || row >= processor.favoritesManager.getNumFavorites())
        {
            DBG("ERROR: Row out of range!");
            return;
        }
        
        auto favoriteName = processor.favoritesManager.getFavoriteName(row);
        DBG("Favorite name: " + favoriteName);
        
        DBG("Creating popup menu...");
        juce::PopupMenu menu;
        menu.addItem(1, "Rename");
        menu.addItem(2, "Remove from Favorites");
        
        DBG("Showing menu asynchronously...");
        
        // Show menu at current mouse position
        auto mousePos = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
        DBG("Mouse position: " + juce::String(mousePos.x) + ", " + juce::String(mousePos.y));
        
        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withTargetScreenArea(juce::Rectangle<int>(static_cast<int>(mousePos.x), 
                                                           static_cast<int>(mousePos.y), 1, 1)),
            [this, row](int result)
            {
                DBG("=== MENU RESULT: " + juce::String(result) + " ===");
                
                if (result == 1) // Rename
                {
                    DBG("Rename selected");
                    auto currentName = processor.favoritesManager.getFavoriteName(row);
                    auto id = processor.favoritesManager.getFavoriteId(row);
                    
                    DBG("Current name: " + currentName);
                    DBG("ID: " + id);
                    
                    // Show rename dialog on message thread
                    juce::MessageManager::callAsync([this, id, currentName]()
                    {
                        DBG("Showing rename dialog...");
                        
                        juce::AlertWindow w("Rename Favorite", "Enter new name:", juce::AlertWindow::NoIcon);
                        w.addTextEditor("name", currentName);
                        w.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                        w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                        
                        if (w.runModalLoop() == 1)
                        {
                            auto newName = w.getTextEditorContents("name");
                            DBG("New name entered: " + newName);
                            
                            if (newName.isNotEmpty())
                            {
                                processor.favoritesManager.renameFavorite(id, newName);
                                refreshFavoritesList();
                                DBG("Favorite renamed successfully");
                            }
                            else
                            {
                                DBG("New name was empty, not renaming");
                            }
                        }
                        else
                        {
                            DBG("Rename cancelled");
                        }
                    });
                }
                else if (result == 2) // Remove
                {
                    DBG("Remove selected");
                    auto id = processor.favoritesManager.getFavoriteId(row);
                    DBG("Removing favorite with ID: " + id);
                    
                    processor.favoritesManager.removeFavorite(id);
                    refreshFavoritesList();
                    
                    DBG("Favorite removed successfully");
                }
                else if (result == 0)
                {
                    DBG("Menu dismissed (no selection)");
                }
            });
        
        DBG("Menu shown, waiting for user selection...");
    };

    DBG("=== favoritesList.onRightClick callback SET ===");
    
    favoritesList.onDeletePressed = [this]() {
        auto selectedRow = favoritesList.getSelectedRow();
        if (selectedRow >= 0)
        {
            auto id = processor.favoritesManager.getFavoriteId(selectedRow);
            processor.favoritesManager.removeFavorite(id);
            refreshFavoritesList();
        }
    };
    
    favoritesList.onFolderDropped = [this](const juce::String& description) {
        handleFolderDrop(description);
    };
    
    refreshFolderList();
    refreshFavoritesList();
    
    lastFavoritesCount = processor.favoritesManager.getNumFavorites();
    startTimer(100);
}

FolderPanel::~FolderPanel() = default;

void FolderPanel::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);
}

void FolderPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    
    auto buttonArea = bounds.removeFromTop(35);
    
    int totalSpacing = 10;
    int availableWidth = buttonArea.getWidth() - totalSpacing;
    int buttonWidth = availableWidth / 3;
    int buttonSpacing = 5;
    
    addFolderButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    rescanButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    aboutButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    
    bounds.removeFromTop(10);
    
    // Rest of the layout
    auto folderSection = bounds.removeFromTop(200);
    folderGroup.setBounds(folderSection);
    
    auto folderContent = folderSection.reduced(10).withTrimmedTop(15);
    folderCountLabel.setBounds(folderContent.removeFromTop(18));
    folderContent.removeFromTop(2);
    folderViewport->setBounds(folderContent);
       
    int numRows = this->getNumRows();
    int rowHeight = folderList.getRowHeight();
    int listHeight = juce::jmax(folderContent.getHeight(), numRows * rowHeight);
    folderList.setBounds(0, 0, folderContent.getWidth(), listHeight);
    
    bounds.removeFromTop(10);
    
    fileInfoGroup.setBounds(bounds);
    auto infoContent = bounds.reduced(10).withTrimmedTop(15);
    
    favoritesViewport->setBounds(infoContent);
       
    int numFavorites = favoritesModel->getNumRows();
    int favRowHeight = favoritesList.getRowHeight();
    int favListHeight = juce::jmax(infoContent.getHeight(), numFavorites * favRowHeight);
    favoritesList.setBounds(0, 0, infoContent.getWidth(), favListHeight);
}

void FolderPanel::buttonClicked(juce::Button* button)
{
    if (button == &addFolderButton)
    {
        auto* dialog = new AddFolderDialog(processor);
        dialog->onFolderAdded = [this]() {
            refreshFolderList();
        };
        dialog->setVisible(true);
    }
    else if (button == &rescanButton)
    {
        processor.drumLibraryManager.rescanFolders();
        refreshFolderList();
    }
    else if (button == &aboutButton)
    {
        auto* dialog = new AboutDialog();
        dialog->setVisible(true);
    }
}

void FolderPanel::removeSelectedFolders()
{
    auto selectedRows = folderList.getSelectedRows();
    
    if (selectedRows.size() == 0)
        return;
    
    juce::String message;
    if (selectedRows.size() == 1)
    {
        int rowIndex = selectedRows[0];
        if (rowIndex >= 0 && rowIndex < folderNames.size())
        {
            message = "Are you sure you want to remove \"" + folderNames[rowIndex] + "\" from the library?";
        }
    }
    else
    {
        message = "Are you sure you want to remove " + juce::String(selectedRows.size()) + " folders from the library?";
    }
    
    int result = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon,
        selectedRows.size() > 1 ? "Remove Folders" : "Remove Folder",
        message,
        "Remove",
        "Cancel");
    
    if (result == 1)
    {
        juce::Array<int> sortedRows;
        for (int i = 0; i < selectedRows.size(); ++i)
        {
            sortedRows.add(selectedRows[i]);
        }
        sortedRows.sort();
        
        for (int i = sortedRows.size() - 1; i >= 0; --i)
        {
            int folderIndex = sortedRows[i];
            if (folderIndex >= 0 && folderIndex < processor.drumLibraryManager.getNumRootFolders())
            {
                processor.drumLibraryManager.removeRootFolder(folderIndex);
            }
        }
        
        refreshFolderList();
    }
}


void FolderPanel::handleFolderDrop(const juce::String& dragDescription)
{
    auto parts = juce::StringArray::fromTokens(dragDescription, "|", "");
    if (parts.size() >= 3 && parts[1] == "FOLDER")
    {
        juce::File folder(parts[2]);
        if (folder.exists() && folder.isDirectory())
        {
            processor.favoritesManager.addFavorite(folder);
            refreshFavoritesList();
        }
    }
}

int FolderPanel::getNumRows()
{
    return folderNames.size();
}

void FolderPanel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                   int width, int height, bool rowIsSelected)
{
    if (rowNumber >= folderNames.size())
        return;
    
    if (rowIsSelected)
    {
        g.fillAll(ColourPalette::primaryBlue);
        g.setColour(ColourPalette::primaryText);
    }
    else
    {
        g.setColour(ColourPalette::primaryText);
    }
    
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    g.setFont(lnf.getNormalFont());
    g.drawText(folderNames[rowNumber], 4, 0, width - 8, height, juce::Justification::centredLeft);
    
    g.setColour(ColourPalette::separator);
    g.drawLine(0.0f, static_cast<float>(height - 1), static_cast<float>(width), static_cast<float>(height - 1));
}

void FolderPanel::selectedRowsChanged(int lastRowSelected)
{
    selectedFolder = lastRowSelected;
    
    // When library folder is selected, clear favorites selection
    if (lastRowSelected >= 0)
    {
        favoritesList.deselectAllRows(); // Clear favorites selection
    }
    
    if (selectedFolder >= 0 && selectedFolder < folderNames.size())
    {
        if (folderSelectedCallback)
            folderSelectedCallback(selectedFolder);
    }
}

void FolderPanel::refreshFolderList()
{
    folderNames.clear();
    
    auto& library = processor.drumLibraryManager;
    for (int i = 0; i < library.getNumRootFolders(); ++i)
    {
        folderNames.add(library.getRootFolderName(i));
    }
    
    folderCountLabel.setText(juce::String(folderNames.size()) + " folder(s) loaded",
                             juce::dontSendNotification);
    folderList.updateContent();
    folderList.repaint();
}

void FolderPanel::refreshFavoritesList()
{
    favoritesList.updateContent();
    favoritesList.repaint();
}

void FolderPanel::timerCallback()
{
    int currentCount = processor.favoritesManager.getNumFavorites();
    if (currentCount != lastFavoritesCount)
    {
        lastFavoritesCount = currentCount;
        refreshFavoritesList();
    }
}