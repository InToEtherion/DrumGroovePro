#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <map>

// Forward declaration
enum class DrumPartType;

enum class DrumLibrary
{
    Unknown = 0,
    Bypass = 1,
    GeneralMIDI = 2,
    SuperiorDrummer3 = 3,
    AddictiveDrums2 = 4,
    EZdrummer = 5,
    GetGoodDrums = 6,
    StevenSlateDrums = 7,
    Ugritone = 8,
    BFD3 = 9,
    MTPowerDrumKit2 = 10,
    Sitala = 11,
    ShreddageDrums = 12,
    Damage2 = 13,
    MODODrum = 14,
    DrumLocker = 15,
    MLDrums = 16,
    SalamanderDrumkit = 17,
    MuldjordKit3 = 18,
    Aasimonster2 = 19
};

class DrumLibraryManager
{
public:
    DrumLibraryManager();
    ~DrumLibraryManager();

    void addRootFolder(const juce::File& folder, DrumLibrary sourceLib, bool isWritable = false);
    void removeRootFolder(int index);
    void rescanFolders();

    int getNumRootFolders() const { return static_cast<int>(rootFolders.size()); }
    juce::File getRootFolder(int index) const;
    juce::String getRootFolderName(int index) const;
    DrumLibrary getRootFolderSourceLibrary(int index) const;
    bool isRootFolderWritable(int index) const;
    void setRootFolderWritable(int index, bool writable);
    bool isFolderAlreadyAdded(const juce::File& folder) const;

    uint8_t mapNoteToLibrary(uint8_t note, DrumLibrary from, DrumLibrary to) const;

    static juce::String getLibraryName(DrumLibrary library);
    juce::StringArray getAllLibraryNames();
    juce::StringArray getAllSourceLibraryNames();
    DrumLibrary getLibraryFromName(const juce::String& name);

    static juce::String getGMDrumName(uint8_t note);

    void loadConfiguration();
    void saveConfiguration();

    // Target library persistence
    void setLastSelectedTargetLibrary(DrumLibrary library);
    DrumLibrary getLastSelectedTargetLibrary() const;

    // Custom mapping support
    void addCustomLibrary(const juce::String& name);
    void removeCustomLibrary(DrumLibrary library);
    void updateLibraryMapping(DrumLibrary library, uint8_t gmNote, uint8_t productNote);
    void saveCustomMappings();
    void loadCustomMappings();
    bool hasCustomMappings() const;

    // Origin library management (MIDI sources)
    void addOriginLibrary(const juce::String& name);
    void removeOriginLibrary(const juce::String& name);
    juce::StringArray getConfiguredOriginLibraries() const;
    void loadOriginLibraries();
    void saveOriginLibraries();

    // Separate origin mapping management
    void saveOriginMappings();
    void loadOriginMappings();
    void initializeOriginMappingWithGM(DrumLibrary originLibrary);

    // Remove library from all mappings (works for both built-in and custom)
    void removeLibraryFromMappings(DrumLibrary library);

    // Custom drum names
    void setCustomDrumName(DrumLibrary library, uint8_t gmNote, const juce::String& customName);
    juce::String getCustomDrumName(DrumLibrary library, uint8_t gmNote) const;
    bool hasCustomDrumName(DrumLibrary library, uint8_t gmNote) const;
    void clearCustomDrumName(DrumLibrary library, uint8_t gmNote);

    juce::StringArray getLoadedLibraryNames();

    // Get explicit mappings (for editor display)
    std::map<uint8_t, uint8_t> getExplicitMappingsForLibrary(DrumLibrary library) const;
    std::map<uint8_t, uint8_t> getOriginLibraryMappings(DrumLibrary originLibrary) const;
    void removeMappingForNote(DrumLibrary library, uint8_t gmNote);

    juce::String getLibraryNameIncludingCustom(DrumLibrary library) const;
    void initializeTargetMappingWithGM(DrumLibrary targetLibrary);

    // Get a representative note number for a drum part from a specific library
    int getNoteNumberForDrumPart(DrumPartType part, DrumLibrary library) const;

    // Get drum part name for display
    juce::String getDrumPartName(DrumPartType part, DrumLibrary library) const;

    // Check if a drum part has a valid mapping for a library
    bool hasValidMapping(DrumPartType part, DrumLibrary library) const;

private:
    struct FolderInfo
    {
        juce::File folder;
        DrumLibrary sourceLibrary;
        bool isWritable = false;
    };

    std::vector<FolderInfo> rootFolders;
    juce::File getConfigFile() const;
    juce::File getOriginLibrariesFile() const;
    juce::File getOriginMappingsFile() const;

    DrumLibrary lastSelectedTargetLibrary = DrumLibrary::GeneralMIDI;

    void createDefaultMappingsFile();  // Create complete mappings file on first run

    void initializeHardcodedMappings();

    // Custom library support
    struct CustomLibrary
    {
        juce::String name;
        int enumValue; // Starting from 1000+
    };
    std::vector<CustomLibrary> customLibraries;
    int nextCustomEnumValue = 1000;

    // Origin libraries (MIDI sources user has)
    juce::StringArray configuredOriginLibraries;

    // Note mapping tables - using nested maps instead of 3D vector
    void initializeMappingTables();
    std::map<int, std::map<int, std::map<uint8_t, uint8_t>>> mappings;

    // Custom drum names storage: [libraryIndex][gmNote] = customName
    std::map<int, std::map<uint8_t, juce::String>> customDrumNames;

    // Custom mapping file management
    juce::File getCustomMappingsFile() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumLibraryManager)
};
