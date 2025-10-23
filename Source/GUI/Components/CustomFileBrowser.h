#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../LookAndFeel/ColourPalette.h"

class CustomFileBrowser : public juce::Component,
                          public juce::FileBrowserListener,
                          public juce::Button::Listener
{
public:
    CustomFileBrowser();
    ~CustomFileBrowser() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void selectionChanged() override;
    void fileClicked(const juce::File& file, const juce::MouseEvent& e) override;
    void fileDoubleClicked(const juce::File& file) override;
    void browserRootChanged(const juce::File& newRoot) override {}
    void buttonClicked(juce::Button* button) override;
    juce::File getSelectedFile() const { return selectedFile; }
    bool wasSelectionConfirmed() const { return selectionConfirmed; }

private:
    std::unique_ptr<juce::FileBrowserComponent> browser;
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    juce::TimeSliceThread directoryThread{"File Browser Thread"};
    juce::ToggleButton showHiddenFiles;
    juce::TextButton selectButton, cancelButton;
    juce::Label pathLabel;
    juce::File selectedFile;
    bool selectionConfirmed = false;
    void updateBrowser();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomFileBrowser)
};

class CustomFileBrowserDialog : public juce::DialogWindow
{
public:
    CustomFileBrowserDialog(std::function<void(const juce::File&)> onFileSelected);
    ~CustomFileBrowserDialog() override;
    void closeButtonPressed() override;
private:
    std::unique_ptr<CustomFileBrowser> browserComponent;
    std::function<void(const juce::File&)> callback;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomFileBrowserDialog)
};