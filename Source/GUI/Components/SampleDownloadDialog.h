#pragma once

#include <JuceHeader.h>
#include "../Utils/SampleDownloader.h"

/**
 * Dialog window for downloading drum samples
 * Shows progress bar, status text, and cancel button
 */
class SampleDownloadDialog : public juce::Component
{
public:
    SampleDownloadDialog();
    ~SampleDownloadDialog() override;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Control
    void startDownload();
    void cancelDownload();
    
    // Status
    bool isDownloadComplete() const { return downloadComplete; }
    bool wasSuccessful() const { return downloadSuccess; }
    
    // Callback for when download completes
    std::function<void(bool success)> onDownloadComplete;
    
private:
    // UI Components
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::ProgressBar progressBar;
    juce::TextButton downloadButton;
    juce::TextButton cancelButton;
    juce::TextButton closeButton;
    
    // Downloader
    std::unique_ptr<SampleDownloader> downloader;
    
    // State
    double currentProgress { 0.0 };
    bool downloadComplete { false };
    bool downloadSuccess { false };
    
    // Callbacks
    void onProgressUpdate(double progress, juce::String status);
    void onDownloadFinished(bool success, juce::String message);
    void updateButtonStates();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleDownloadDialog)
};
