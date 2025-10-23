#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <map>

enum class DrumLibrary
{
    Unknown = 0,
    Bypass = 1,
    GeneralMIDI = 2,
    SuperiorDrummer3 = 3,
    AddictiveDrums2 = 4,
    Battery4 = 5,
    EZdrummer = 6,
    GetGoodDrums = 7,
    StevenSlateDrums = 8,
    Ugritone = 9,
    BFD3 = 10,
    MTPowerDrumKit2 = 11,
    DrumGizmo = 12,
    Sitala = 13,
    KrimhDrums = 14,
    TheMonarchKit = 15,
    ShreddageDrums = 16,
    Damage2 = 17
};

class DrumLibraryManager
{
public:
    DrumLibraryManager();
    ~DrumLibraryManager();
    
    void addRootFolder(const juce::File& folder, DrumLibrary sourceLib);
    void removeRootFolder(int index);
    void rescanFolders();
    
    int getNumRootFolders() const { return static_cast<int>(rootFolders.size()); }
    juce::File getRootFolder(int index) const;
    juce::String getRootFolderName(int index) const;
    DrumLibrary getRootFolderSourceLibrary(int index) const;
    
    // UPDATED: Made const for use in const contexts
    uint8_t mapNoteToLibrary(uint8_t note, DrumLibrary from, DrumLibrary to) const;
	
	static juce::String getLibraryName(DrumLibrary library);
    static juce::StringArray getAllLibraryNames();
    static juce::StringArray getAllSourceLibraryNames();
    static DrumLibrary getLibraryFromName(const juce::String& name);
    
    void loadConfiguration();
    void saveConfiguration();
    
	// Target library persistence
	void setLastSelectedTargetLibrary(DrumLibrary library);
	DrumLibrary getLastSelectedTargetLibrary() const;
private:
    struct FolderInfo
    {
        juce::File folder;
        DrumLibrary sourceLibrary;
    };
    
    std::vector<FolderInfo> rootFolders;
    juce::File getConfigFile() const;
	
	DrumLibrary lastSelectedTargetLibrary = DrumLibrary::GeneralMIDI;
    
    // Note mapping tables - using nested maps instead of 3D vector
    void initializeMappingTables();
    std::map<int, std::map<int, std::map<uint8_t, uint8_t>>> mappings;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumLibraryManager)
};