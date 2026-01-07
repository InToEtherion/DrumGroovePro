#include "SampleDownloadDialog.h"
#include "../LookAndFeel/ColourPalette.h"

SampleDownloadDialog::SampleDownloadDialog()
    : progressBar(currentProgress)
{
    downloader = std::make_unique<SampleDownloader>();
    
    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText("Drum Sample Library", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    
    // Status label
    addAndMakeVisible(statusLabel);
    statusLabel.setText("Click 'Download' to install the Salamander Drumkit samples.\nSize: ~200MB (compressed), ~500MB (extracted)", 
                        juce::dontSendNotification);
    statusLabel.setFont(juce::Font(14.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    
    // Progress bar
    addAndMakeVisible(progressBar);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, ColourPalette::mainBackground);
    progressBar.setColour(juce::ProgressBar::foregroundColourId, ColourPalette::primaryBlue);
    
    // Download button
    addAndMakeVisible(downloadButton);
    downloadButton.setButtonText("Download");
    downloadButton.setColour(juce::TextButton::buttonColourId, ColourPalette::primaryBlue);
    downloadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    downloadButton.onClick = [this]() { startDownload(); };
    
    // Cancel button
    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    cancelButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    cancelButton.onClick = [this]() { cancelDownload(); };
    cancelButton.setVisible(false);
    
    // Close button
    addAndMakeVisible(closeButton);
    closeButton.setButtonText("Close");
    closeButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    closeButton.setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    closeButton.onClick = [this]() 
    { 
        if (auto* parent = getParentComponent())
            parent->setVisible(false);
    };
    closeButton.setVisible(false);
    
    // Check if samples already installed
    if (downloader->areSamplesInstalled())
    {
        statusLabel.setText("Samples are already installed!", juce::dontSendNotification);
        downloadButton.setButtonText("Re-download");
        currentProgress = 1.0;
    }
    
    setSize(500, 250);
}

SampleDownloadDialog::~SampleDownloadDialog()
{
    if (downloader)
        downloader->cancelDownload();
}

void SampleDownloadDialog::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::secondaryBackground);
    
    // Draw border
    g.setColour(ColourPalette::borderColour);
    g.drawRect(getLocalBounds(), 2);
}

void SampleDownloadDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    
    // Title
    titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10);
    
    // Status (multi-line, so give it more space)
    statusLabel.setBounds(bounds.removeFromTop(60));
    bounds.removeFromTop(20);
    
    // Progress bar
    progressBar.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(20);
    
    // Buttons
    auto buttonArea = bounds.removeFromTop(40);
    int buttonWidth = 120;
    int buttonSpacing = 10;
    
    if (downloadComplete)
    {
        // Show only close button centered
        closeButton.setBounds(buttonArea.withSizeKeepingCentre(buttonWidth, 40));
    }
    else if (cancelButton.isVisible())
    {
        // Show cancel button centered while downloading
        cancelButton.setBounds(buttonArea.withSizeKeepingCentre(buttonWidth, 40));
    }
    else
    {
        // Show download button centered
        downloadButton.setBounds(buttonArea.withSizeKeepingCentre(buttonWidth, 40));
    }
}

void SampleDownloadDialog::startDownload()
{
    downloadButton.setVisible(false);
    cancelButton.setVisible(true);
    closeButton.setVisible(false);
    
    currentProgress = 0.0;
    downloadComplete = false;
    downloadSuccess = false;
    
    statusLabel.setText("Preparing to download...", juce::dontSendNotification);
    
    downloader->startDownload(
        [this](double progress, juce::String status) { onProgressUpdate(progress, status); },
        [this](bool success, juce::String message) { onDownloadFinished(success, message); }
    );
    
    resized();
}

void SampleDownloadDialog::cancelDownload()
{
    downloader->cancelDownload();
    
    currentProgress = 0.0;
    statusLabel.setText("Download cancelled", juce::dontSendNotification);
    
    updateButtonStates();
}

void SampleDownloadDialog::onProgressUpdate(double progress, juce::String status)
{
    currentProgress = progress;
    statusLabel.setText(status, juce::dontSendNotification);
    repaint();
}

void SampleDownloadDialog::onDownloadFinished(bool success, juce::String message)
{
    downloadComplete = true;
    downloadSuccess = success;
    
    currentProgress = success ? 1.0 : 0.0;
    
    if (success)
    {
        statusLabel.setText(message + "\nSamples installed successfully!", 
                           juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText(message + "\nPlease try again or check your internet connection.", 
                           juce::dontSendNotification);
    }
    
    updateButtonStates();
    
    // Notify parent
    if (onDownloadComplete)
        onDownloadComplete(success);
}

void SampleDownloadDialog::updateButtonStates()
{
    downloadButton.setVisible(!downloadComplete && !cancelButton.isVisible());
    cancelButton.setVisible(false);
    closeButton.setVisible(downloadComplete);
    
    resized();
}