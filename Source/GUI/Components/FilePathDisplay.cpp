#include "FilePathDisplay.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"

FilePathDisplay::FilePathDisplay()
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    // Set up path label
    pathLabel.setText("No file selected", juce::dontSendNotification);
    pathLabel.setFont(lnf.getSmallFont().withHeight(11.0f));
    pathLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    pathLabel.setColour(juce::Label::backgroundColourId, ColourPalette::inputBackground);
    pathLabel.setJustificationType(juce::Justification::centredLeft);
    pathLabel.setBorderSize(juce::BorderSize<int>(2, 8, 2, 8));
    addAndMakeVisible(pathLabel);

    // Set up copy button
    copyButton.setButtonText("Copy");
    copyButton.setEnabled(false);
    copyButton.setTooltip("Copy file path to clipboard");
    copyButton.onClick = [this]() { copyPathToClipboard(); };
    addAndMakeVisible(copyButton);
}

FilePathDisplay::~FilePathDisplay() = default;

void FilePathDisplay::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::secondaryBackground);

    // Draw border
    g.setColour(ColourPalette::borderColour);
    g.drawRect(getLocalBounds(), 1);

    // Draw "File Path:" label on the left
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    g.setFont(lnf.getSmallFont().withHeight(11.0f));
    g.setColour(ColourPalette::primaryText);
    g.drawText("File Path:", 8, 0, 60, getHeight(), juce::Justification::centredLeft);
}

void FilePathDisplay::resized()
{
    auto bounds = getLocalBounds().reduced(2);

    // Copy button on the right
    copyButton.setBounds(bounds.removeFromRight(50).reduced(2));

    // "File Path:" label space
    bounds.removeFromLeft(68);

    // Path label fills remaining space
    pathLabel.setBounds(bounds);
}

void FilePathDisplay::setFilePath(const juce::File& file)
{
    currentFile = file;
    updatePathDisplay();
    copyButton.setEnabled(file.existsAsFile());
}

void FilePathDisplay::clearPath()
{
    currentFile = juce::File();
    updatePathDisplay();
    copyButton.setEnabled(false);
}

void FilePathDisplay::updatePathDisplay()
{
    if (currentFile.existsAsFile())
    {
        juce::String fullPath = currentFile.getFullPathName();
        pathLabel.setText(fullPath, juce::dontSendNotification);
        pathLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
        pathLabel.setTooltip(fullPath); // Show full path on hover
    }
    else
    {
        pathLabel.setText("No file selected", juce::dontSendNotification);
        pathLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
        pathLabel.setTooltip("");
    }
}

void FilePathDisplay::copyPathToClipboard()
{
    if (currentFile.existsAsFile())
    {
        juce::SystemClipboard::copyTextToClipboard(currentFile.getFullPathName());
    }
}
