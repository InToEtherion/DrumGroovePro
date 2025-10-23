#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FilePathDisplay : public juce::Component
{
public:
    FilePathDisplay();
    ~FilePathDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Set the file path to display
    void setFilePath(const juce::File& file);
    void clearPath();

    // Get current file
    juce::File getCurrentFile() const { return currentFile; }

private:
    juce::File currentFile;
    juce::Label pathLabel;
    juce::TextButton copyButton;

    void updatePathDisplay();
    void copyPathToClipboard();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilePathDisplay)
};
