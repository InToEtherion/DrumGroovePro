#include "SampleDownloader.h"

SampleDownloader::SampleDownloader()
: juce::Thread("SampleDownloader")
{
}

SampleDownloader::~SampleDownloader()
{
    cancelDownload();
    stopThread(5000);
}

const std::vector<SampleDownloader::LibraryInfo>& SampleDownloader::getAvailableLibrariesInternal()
{
    // Use local static to avoid static initialization order issues
    // This ensures the vector is created on first access
    static const std::vector<LibraryInfo> libraries = {
        {
            "Salamander Drumkit",                   // name
            "salamanderDrumkit",                    // folderName
            "https://github.com/InToEtherion/DrumGroovePro-Samples/releases/download/V1.0/salamanderDrumkit.zip",
            "zip",                                  // archiveFormat
            389,                                    // expectedSizeMB
            "ALL.sfz",                              // verifyFile
            "OH"                                    // verifyFolder
        },
        {
            "MuldjordKit",                          // name (display name)
            "MuldjordKit3",                         // folderName (internal)
            "https://github.com/InToEtherion/DrumGroovePro-Samples/releases/download/V1.0/MuldjordKit3.zip",
            "zip",                                  // archiveFormat
            76,                                    // expectedSizeMB (~100MB stereo)
            "Midimap.xml",                          // verifyFile
            "Snare"                                 // verifyFolder
        },
        {
            "Aasimonster",                          // name (display name)
            "aasimonster2",                         // folderName (internal)
            "https://github.com/InToEtherion/DrumGroovePro-Samples/releases/download/V1.0/aasimonster2.zip",
            "zip",                                  // archiveFormat
            50,                                    // expectedSizeMB
            "Midimap.xml",                          // verifyFile
            "kick_l"                                // verifyFolder (folder name for one of the instruments)
        }
    };
    return libraries;
}

std::vector<SampleDownloader::LibraryInfo> SampleDownloader::getAvailableLibraries()
{
    return getAvailableLibrariesInternal();
}

const SampleDownloader::LibraryInfo* SampleDownloader::getLibraryInfo(const juce::String& libraryName)
{
    const auto& libraries = getAvailableLibrariesInternal();
    for (const auto& lib : libraries)
    {
        if (lib.name == libraryName || lib.folderName == libraryName)
            return &lib;
    }
    return nullptr;
}

void SampleDownloader::startDownload(const juce::String& libraryName,
                                     ProgressCallback progressCb,
                                     CompletionCallback completionCb)
{
    if (currentState.load() == State::Downloading ||
        currentState.load() == State::Extracting)
    {
        return;
    }

    currentLibraryName = libraryName;
    progressCallback = progressCb;
    completionCallback = completionCb;
    shouldCancel = false;
    errorMessage.clear();

    startThread(juce::Thread::Priority::normal);
}

// Legacy compatibility - downloads salamanderDrumkit by default
void SampleDownloader::startDownload(ProgressCallback progressCb, CompletionCallback completionCb)
{
    startDownload("salamanderDrumkit", progressCb, completionCb);
}

void SampleDownloader::cancelDownload()
{
    shouldCancel = true;

    if (downloadTask != nullptr)
        downloadTask = nullptr;

    signalThreadShouldExit();
}

bool SampleDownloader::areSamplesInstalled() const
{
    return isLibraryInstalled("salamanderDrumkit");
}

bool SampleDownloader::isLibraryInstalled(const juce::String& libraryName) const
{
    const auto* library = getLibraryInfo(libraryName);
    if (library == nullptr)
        return false;

    auto samplesDir = getSamplesRootDirectory().getChildFile(library->folderName);

    if (!samplesDir.exists())
        return false;

    auto verifyFile = samplesDir.getChildFile(library->verifyFile);
    if (!verifyFile.exists())
        return false;

    auto verifyFolder = samplesDir.getChildFile(library->verifyFolder);
    if (!verifyFolder.exists() || !verifyFolder.isDirectory())
        return false;

    return true;
}

juce::File SampleDownloader::getSamplesDirectory() const
{
    return getSamplesDirectory("salamanderDrumkit");
}

juce::File SampleDownloader::getSamplesDirectory(const juce::String& libraryName) const
{
    const auto* library = getLibraryInfo(libraryName);
    if (library != nullptr)
        return getSamplesRootDirectory().getChildFile(library->folderName);

    return getSamplesRootDirectory().getChildFile(libraryName);
}

juce::String SampleDownloader::getDownloadURL()
{
    const auto& libs = getAvailableLibrariesInternal();
    if (!libs.empty())
        return libs[0].downloadURL;
    return {};
}

int64_t SampleDownloader::getExpectedFileSize()
{
    const auto& libs = getAvailableLibrariesInternal();
    if (!libs.empty())
        return libs[0].expectedSizeMB * 1024 * 1024;
    return 0;
}

void SampleDownloader::run()
{
    // Find the library info
    const auto* library = getLibraryInfo(currentLibraryName);
    if (library == nullptr)
    {
        currentState = State::Error;
        errorMessage = "Unknown library: " + currentLibraryName;
        notifyCompletion(false, errorMessage);
        return;
    }

    currentState = State::Checking;
    updateProgress(0.0, "Checking for existing samples...");

    if (isLibraryInstalled(currentLibraryName))
    {
        currentState = State::Complete;
        notifyCompletion(true, library->name + " already installed");
        return;
    }

    if (shouldCancel) return;

    auto samplesRoot = getSamplesRootDirectory();
    if (!samplesRoot.exists())
        samplesRoot.createDirectory();

    auto tempFile = getTempDownloadFile(".zip");
    if (tempFile.exists())
        tempFile.deleteFile();

    tempFile.getParentDirectory().createDirectory();

    currentState = State::Downloading;
    updateProgress(0.05, "Starting download...");

    juce::URL url(library->downloadURL);

    #if JUCE_LINUX
    // Use curl on Linux
    juce::String curlCommand = "curl -L --progress-bar -o \"" +
    tempFile.getFullPathName() + "\" \"" +
    library->downloadURL + "\" 2>&1";

    updateProgress(0.1, "Downloading...");

    FILE* pipe = popen(curlCommand.toRawUTF8(), "r");
    if (pipe != nullptr)
    {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr && !shouldCancel)
        {
            juce::String line(buffer);

            if (line.contains("%"))
            {
                auto percent = line.upToFirstOccurrenceOf("%", false, false).trim();
                if (percent.length() > 0)
                {
                    float progress = percent.getFloatValue() / 100.0f;
                    updateProgress(0.1 + (progress * 0.7), "Downloading: " + percent + "%");
                }
            }
        }

        int result = pclose(pipe);

        if (result != 0 || !tempFile.exists() || tempFile.getSize() == 0)
        {
            currentState = State::Error;
            errorMessage = "Download failed. Check network connection.";
            notifyCompletion(false, errorMessage);
            return;
        }
    }
    else
    {
        currentState = State::Error;
        errorMessage = "Failed to start download. Is curl installed?";
        notifyCompletion(false, errorMessage);
        return;
    }

    #else
    // Windows/macOS: Use JUCE download
    downloadTask = url.downloadToFile(tempFile,
                                      juce::URL::DownloadTaskOptions()
                                      .withListener(this));

    if (downloadTask == nullptr)
    {
        currentState = State::Error;
        errorMessage = "Failed to start download";
        notifyCompletion(false, errorMessage);
        return;
    }

    while (downloadTask != nullptr && !shouldCancel)
    {
        juce::Thread::sleep(100);
    }
    #endif

    if (shouldCancel)
    {
        currentState = State::Idle;
        if (tempFile.exists())
            tempFile.deleteFile();
        notifyCompletion(false, "Download cancelled");
        return;
    }

    if (!tempFile.exists() || tempFile.getSize() == 0)
    {
        currentState = State::Error;
        errorMessage = "Download failed - file is empty";
        notifyCompletion(false, errorMessage);
        return;
    }

    currentState = State::Extracting;
    updateProgress(0.8, "Extracting samples...");

    if (!extractZipFile(tempFile, samplesRoot))
    {
        currentState = State::Error;
        tempFile.deleteFile();
        notifyCompletion(false, errorMessage);
        return;
    }

    if (shouldCancel)
    {
        currentState = State::Idle;
        tempFile.deleteFile();
        notifyCompletion(false, "Extraction cancelled");
        return;
    }

    currentState = State::Verifying;
    updateProgress(0.95, "Verifying installation...");

    auto samplesDir = getSamplesRootDirectory().getChildFile(library->folderName);
    if (!verifySamples(samplesDir, *library))
    {
        currentState = State::Error;
        tempFile.deleteFile();
        notifyCompletion(false, errorMessage);
        return;
    }

    tempFile.deleteFile();

    currentState = State::Complete;
    updateProgress(1.0, "Installation complete!");
    notifyCompletion(true, library->name + " installed successfully");
}

void SampleDownloader::finished(juce::URL::DownloadTask* task, bool success)
{
    juce::ignoreUnused(task);

    if (!success && !shouldCancel)
    {
        errorMessage = "Download failed - network error";
    }

    downloadTask = nullptr;
}

void SampleDownloader::progress(juce::URL::DownloadTask* task,
                                juce::int64 bytesDownloaded,
                                juce::int64 totalLength)
{
    juce::ignoreUnused(task);

    if (totalLength > 0)
    {
        double downloadProgress = static_cast<double>(bytesDownloaded) / totalLength;
        double overallProgress = 0.05 + (downloadProgress * 0.70);

        auto mbDownloaded = bytesDownloaded / (1024.0 * 1024.0);
        auto mbTotal = totalLength / (1024.0 * 1024.0);

        juce::String status = juce::String::formatted("Downloading: %.1f MB / %.1f MB",
                                                      mbDownloaded, mbTotal);

        updateProgress(overallProgress, status);
    }
}

bool SampleDownloader::extractZipFile(const juce::File& zipFile, const juce::File& targetDir)
{
    juce::FileInputStream* inputStream = new juce::FileInputStream(zipFile);

    if (inputStream == nullptr || inputStream->failedToOpen())
    {
        delete inputStream;
        errorMessage = "Failed to open ZIP file";
        return false;
    }

    juce::ZipFile zip(inputStream, true);

    if (zip.getNumEntries() == 0)
    {
        errorMessage = "ZIP file is empty or corrupted";
        return false;
    }

    juce::Result result = zip.uncompressTo(targetDir);

    if (result.failed())
    {
        errorMessage = "Failed to extract ZIP: " + result.getErrorMessage();
        return false;
    }

    return true;
}

bool SampleDownloader::verifySamples(const juce::File& samplesDir, const LibraryInfo& library)
{
    if (!samplesDir.exists())
    {
        errorMessage = "Samples folder not found after extraction: " + samplesDir.getFullPathName();
        return false;
    }

    auto verifyFile = samplesDir.getChildFile(library.verifyFile);
    if (!verifyFile.exists())
    {
        errorMessage = library.verifyFile + " not found";
        return false;
    }

    auto verifyFolder = samplesDir.getChildFile(library.verifyFolder);
    if (!verifyFolder.exists())
    {
        errorMessage = library.verifyFolder + " folder not found";
        return false;
    }

    // Check for WAV files - either directly in folder or in samples/ subfolder (DrumGizmo format)
    auto wavFiles = verifyFolder.findChildFiles(juce::File::findFiles, false, "*.wav");

    // If not found directly, check in samples/ subfolder (DrumGizmo structure)
    if (wavFiles.size() < 5)
    {
        auto samplesSubfolder = verifyFolder.getChildFile("samples");
        if (samplesSubfolder.exists())
        {
            wavFiles = samplesSubfolder.findChildFiles(juce::File::findFiles, false, "*.wav");
        }
    }

    if (wavFiles.size() < 5)
    {
        errorMessage = "Not enough sample files found in " + library.verifyFolder;
        return false;
    }

    return true;
}

void SampleDownloader::updateProgress(double progress, const juce::String& status)
{
    if (progressCallback)
    {
        juce::MessageManager::callAsync([this, progress, status]()
        {
            if (progressCallback)
                progressCallback(progress, status);
        });
    }
}

void SampleDownloader::notifyCompletion(bool success, const juce::String& message)
{
    if (completionCallback)
    {
        juce::MessageManager::callAsync([this, success, message]()
        {
            if (completionCallback)
                completionCallback(success, message);
        });
    }
}

juce::File SampleDownloader::getTempDownloadFile(const juce::String& extension) const
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
    .getChildFile("DrumGroovePro_download_temp" + extension);
}

juce::File SampleDownloader::getSamplesRootDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("DrumGroovePro")
    .getChildFile("Samples");
}
