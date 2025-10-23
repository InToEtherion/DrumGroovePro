#include "CustomFileBrowser.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"

CustomFileBrowser::CustomFileBrowser()
{
    directoryThread.startThread();

    fileFilter = std::make_unique<juce::WildcardFileFilter>("*", "*", "All Files");

    browser = std::make_unique<juce::FileBrowserComponent>(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories |
        juce::FileBrowserComponent::useTreeView,
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        fileFilter.get(),
        nullptr
    );

    browser->addListener(this);
    addAndMakeVisible(browser.get());

    showHiddenFiles.setButtonText("Show Hidden Files");
    showHiddenFiles.setToggleState(false, juce::dontSendNotification);
    showHiddenFiles.addListener(this);
    showHiddenFiles.setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    showHiddenFiles.setColour(juce::ToggleButton::tickColourId, ColourPalette::primaryBlue);
    addAndMakeVisible(showHiddenFiles);

    selectButton.setButtonText("SELECT");
    selectButton.addListener(this);
    selectButton.setEnabled(false);
    selectButton.setColour(juce::TextButton::buttonColourId, ColourPalette::successGreen);
    addAndMakeVisible(selectButton);

    cancelButton.setButtonText("CANCEL");
    cancelButton.addListener(this);
    addAndMakeVisible(cancelButton);

    pathLabel.setText("No folder selected", juce::dontSendNotification);
    pathLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    pathLabel.setColour(juce::Label::backgroundColourId, ColourPalette::inputBackground);
    addAndMakeVisible(pathLabel);

    updateBrowser();
}

CustomFileBrowser::~CustomFileBrowser()
{
    directoryThread.stopThread(1000);
}

void CustomFileBrowser::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);

    g.setColour(ColourPalette::borderColour);
    auto browserBounds = getLocalBounds().reduced(10);
    browserBounds.removeFromTop(40);
    browserBounds.removeFromBottom(80);
    g.drawRect(browserBounds);
}

void CustomFileBrowser::resized()
{
    auto bounds = getLocalBounds();

    auto topSection = bounds.removeFromTop(40).reduced(10, 5);
    showHiddenFiles.setBounds(topSection.removeFromLeft(150));

    bounds.removeFromBottom(80);
    browser->setBounds(bounds.reduced(10));

    bounds = getLocalBounds().removeFromBottom(80);

    pathLabel.setBounds(bounds.removeFromTop(30).reduced(10, 5));

    auto buttonArea = bounds.removeFromTop(40).reduced(10, 5);
    cancelButton.setBounds(buttonArea.removeFromRight(100));
    buttonArea.removeFromRight(10);
    selectButton.setBounds(buttonArea.removeFromRight(100));
}

void CustomFileBrowser::selectionChanged()
{
    if (browser->getNumSelectedFiles() > 0)
    {
        selectedFile = browser->getSelectedFile(0);

        if (selectedFile.exists() && selectedFile.isDirectory())
        {
            pathLabel.setText(selectedFile.getFullPathName(), juce::dontSendNotification);
            selectButton.setEnabled(true);
        }
        else
        {
            pathLabel.setText("Please select a folder", juce::dontSendNotification);
            selectButton.setEnabled(false);
        }
    }
}

void CustomFileBrowser::fileClicked(const juce::File& file, const juce::MouseEvent&)
{
    selectedFile = file;
}

void CustomFileBrowser::fileDoubleClicked(const juce::File& file)
{
    if (file.isDirectory())
    {
        browser->setRoot(file);
    }
}

void CustomFileBrowser::buttonClicked(juce::Button* button)
{
    if (button == &showHiddenFiles)
    {
        updateBrowser();
    }
    else if (button == &selectButton)
    {
        selectionConfirmed = true;
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialog->closeButtonPressed();
        }
    }
    else if (button == &cancelButton)
    {
        selectionConfirmed = false;
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialog->closeButtonPressed();
        }
    }
}

void CustomFileBrowser::updateBrowser()
{
    bool showHidden = showHiddenFiles.getToggleState();
    auto currentRoot = browser->getRoot();

    if (showHidden)
    {
        fileFilter = std::make_unique<juce::WildcardFileFilter>("*;.*", "*", "All Files");
    }
    else
    {
        fileFilter = std::make_unique<juce::WildcardFileFilter>("*", "*", "Visible Files");
    }

    browser.reset();
    browser = std::make_unique<juce::FileBrowserComponent>(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories |
        juce::FileBrowserComponent::useTreeView,
        currentRoot,
        fileFilter.get(),
        nullptr
    );

    browser->addListener(this);
    addAndMakeVisible(browser.get());
    resized();
}

//==============================================================================
CustomFileBrowserDialog::CustomFileBrowserDialog(std::function<void(const juce::File&)> onFileSelected)
    : DialogWindow("Select MIDI Folder", ColourPalette::panelBackground, true),
      callback(onFileSelected)
{
    auto* content = new CustomFileBrowser();
    setContentOwned(content, true);

    setSize(700, 500);
    setResizable(true, true);
    centreWithSize(getWidth(), getHeight());
}

CustomFileBrowserDialog::~CustomFileBrowserDialog() = default;

void CustomFileBrowserDialog::closeButtonPressed()
{
    auto* browser = static_cast<CustomFileBrowser*>(getContentComponent());
    if (browser && browser->wasSelectionConfirmed())
    {
        auto selected = browser->getSelectedFile();
        if (selected.exists() && callback)
        {
            callback(selected);
        }
    }
    setVisible(false);
}