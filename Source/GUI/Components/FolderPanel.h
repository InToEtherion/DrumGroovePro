#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class DrumGrooveProcessor;

// ============================================================================
// FavoritesListBox - Fixed to bypass viewport event blocking
// ============================================================================
class FavoritesListBox : public juce::ListBox,
                         public juce::ListBoxModel
{
public:
    FavoritesListBox() : juce::ListBox() 
    {
        DBG("=== FavoritesListBox CONSTRUCTOR ===");
        setMultipleSelectionEnabled(false);
        setWantsKeyboardFocus(true);
        setModel(this);
    }
    
    // ListBoxModel overrides - delegate to actual model
    int getNumRows() override
    {
        if (actualModel)
            return actualModel->getNumRows();
        return 0;
    }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (actualModel)
            actualModel->paintListBoxItem(rowNumber, g, width, height, rowIsSelected);
    }
    
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
	{
    DBG("=== listBoxItemClicked ===");
    DBG(juce::String("Row: ") + juce::String(row));
    DBG(juce::String("Right click: ") + (e.mods.isPopupMenu() ? "YES" : "NO"));
    
    if (e.mods.isPopupMenu() && onRightClick)
    {
        DBG("Triggering onRightClick!");
        onRightClick(row);
        return;
    }
    
    if (actualModel)
        actualModel->selectedRowsChanged(row);
}
    
    void selectedRowsChanged(int lastRowSelected) override
    {
        if (actualModel)
            actualModel->selectedRowsChanged(lastRowSelected);
    }
    
    void setActualModel(juce::ListBoxModel* model)
    {
        actualModel = model;
        updateContent();
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (onDeletePressed)
            {
                onDeletePressed();
                return true;
            }
        }
        return juce::ListBox::keyPressed(key);
    }
    
    std::function<void(const juce::String&)> onFolderDropped;
    std::function<void(int)> onRightClick;
    std::function<void(int)> onDoubleClick;
    std::function<void()> onDeletePressed;
    
private:
    juce::ListBoxModel* actualModel = nullptr;
};

// ============================================================================
// FolderListBox - For library folders
// ============================================================================
class FolderListBox : public juce::ListBox
{
public:
    FolderListBox() : juce::ListBox() 
    {
        setMultipleSelectionEnabled(false);
        setWantsKeyboardFocus(true);
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (onDeletePressed)
            {
                onDeletePressed();
                return true;
            }
        }
        return juce::ListBox::keyPressed(key);
    }
    
    std::function<void()> onDeletePressed;
};

// ============================================================================
// FavoritesModel - Model for displaying favorites
// ============================================================================
class FavoritesModel : public juce::ListBoxModel
{
public:
    FavoritesModel(DrumGrooveProcessor& p) : processor(p) {}
    
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override
    {
        if (onRowSelected && lastRowSelected >= 0)
            onRowSelected(lastRowSelected);
    }
    
    std::function<void(int)> onRowSelected;
    
private:
    DrumGrooveProcessor& processor;
};

// ============================================================================
// FolderPanel - Main panel component
// ============================================================================
class FolderPanel : public juce::Component, 
                   public juce::ListBoxModel,
                   public juce::Button::Listener,
				   public juce::Timer
{
public:
    explicit FolderPanel(DrumGrooveProcessor& processor);
    ~FolderPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    
    void refreshFolderList();
    void refreshFavoritesList();
    int getSelectedFolderIndex() const { return selectedFolder; }
    
    std::function<void(const juce::File&)> onFolderSelected;
    void timerCallback() override;
private:
    DrumGrooveProcessor& processor;
    
	int lastFavoritesCount = 0;
	
    juce::TextButton addFolderButton;
    juce::TextButton rescanButton;
    juce::TextButton aboutButton;
    
    juce::GroupComponent folderGroup;
    juce::Label folderCountLabel;
    FolderListBox folderList;
    std::unique_ptr<juce::Viewport> folderViewport;
    
    juce::GroupComponent fileInfoGroup;
    juce::Label favoritesLabel;
    FavoritesListBox favoritesList;
    std::unique_ptr<juce::Viewport> favoritesViewport;
    std::unique_ptr<FavoritesModel> favoritesModel;
    
    juce::StringArray folderNames;
    int selectedFolder = -1;
    std::function<void(int)> folderSelectedCallback;
    
    void removeSelectedFolders();
    void handleFolderDrop(const juce::String& dragDescription);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FolderPanel)
};