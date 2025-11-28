#pragma once

#include <JuceHeader.h>
#include <functional>
#include <atomic>
#include <vector>

/**
 * Manages downloading and extracting drum sample libraries
 * Downloads from GitHub, extracts ZIP, verifies files
 * Supports multiple libraries (Salamander, MuldjordKit, etc.)
 */
class SampleDownloader : public juce::Thread,
                         public juce::URL::DownloadTask::Listener
{
public:
    SampleDownloader();
    ~SampleDownloader() override;
    
    // Download states
    enum class State
    {
        Idle,
        Checking,
        Downloading,
        Extracting,
        Verifying,
        Complete,
        Error
    };
    
    // Library information structure
    struct LibraryInfo
    {
        juce::String name;           // Display name
        juce::String folderName;     // Directory name in Samples folder
        juce::String downloadURL;    // GitHub release URL
        juce::String archiveFormat;  // "zip" or "7z"
        int expectedSizeMB;          // Approximate download size
        juce::String verifyFile;     // File to check (e.g., "ALL.sfz" or "Midimap.xml")
        juce::String verifyFolder;   // Subfolder to verify (e.g., "OH" or "Snare")
    };
    
    // Callback types
    using ProgressCallback = std::function<void(double progress, juce::String status)>;
    using CompletionCallback = std::function<void(bool success, juce::String message)>;
    
    // Multi-library operations
    void startDownload(const juce::String& libraryName, ProgressCallback progressCb, CompletionCallback completionCb);
    
    // Legacy single-library operations (downloads salamanderDrumkit by default)
    void startDownload(ProgressCallback progressCb, CompletionCallback completionCb);
    void cancelDownload();
    
    // Status queries
    State getCurrentState() const { return currentState.load(); }
    bool areSamplesInstalled() const;
    bool isLibraryInstalled(const juce::String& libraryName) const;
    juce::File getSamplesDirectory() const;
    juce::File getSamplesDirectory(const juce::String& libraryName) const;
    juce::String getErrorMessage() const { return errorMessage; }
    
    // Library info
    static std::vector<LibraryInfo> getAvailableLibraries();
    static const LibraryInfo* getLibraryInfo(const juce::String& libraryName);
    
    // File info (legacy)
    static juce::String getDownloadURL();
    static int64_t getExpectedFileSize();
    
private:
    // Thread override
    void run() override;
    
    // URL::DownloadTask::Listener overrides
    void finished(juce::URL::DownloadTask* task, bool success) override;
    void progress(juce::URL::DownloadTask* task, juce::int64 bytesDownloaded, juce::int64 totalLength) override;
    
    // Internal operations
    bool checkExistingSamples() const;
    bool extractZipFile(const juce::File& zipFile, const juce::File& targetDir);
    bool verifySamples(const juce::File& samplesDir, const LibraryInfo& library);
    void updateProgress(double progress, const juce::String& status);
    void notifyCompletion(bool success, const juce::String& message);
    
    // Paths
    juce::File getTempDownloadFile(const juce::String& extension) const;
    juce::File getSamplesRootDirectory() const;
    
    // Internal helper for safe static initialization
    static const std::vector<LibraryInfo>& getAvailableLibrariesInternal();
    
    // State
    std::atomic<State> currentState { State::Idle };
    juce::String errorMessage;
    juce::String currentLibraryName;
    
    // Callbacks
    ProgressCallback progressCallback;
    CompletionCallback completionCallback;
    
    // Download task
    std::unique_ptr<juce::URL::DownloadTask> downloadTask;
    std::atomic<bool> shouldCancel { false };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleDownloader)
};
