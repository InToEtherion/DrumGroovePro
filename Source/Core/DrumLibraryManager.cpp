#include "DrumLibraryManager.h"

DrumLibraryManager::DrumLibraryManager()
{
    initializeMappingTables();
    loadConfiguration();
}

DrumLibraryManager::~DrumLibraryManager()
{
    saveConfiguration();
}

void DrumLibraryManager::addRootFolder(const juce::File& folder, DrumLibrary sourceLib)
{
    if (folder.exists() && folder.isDirectory())
    {
        FolderInfo fi;
        fi.folder = folder;
        fi.sourceLibrary = sourceLib;
        
        rootFolders.push_back(fi);
        saveConfiguration();
    }
}

void DrumLibraryManager::initializeMappingTables()
{
    // Initialize identity mappings for all library pairs
    for (int from = 0; from < 9; ++from)
    {
        for (int to = 0; to < 9; ++to)
        {
            for (uint8_t note = 0; note < 128; ++note)
            {
                mappings[from][to][note] = note; // Default identity mapping
            }
        }
    }
    
    // ==================== UGRITONE COMPLETE MAPPING ====================
    // Ugritone uses non-standard MIDI note assignments, this is the FULL mapping
    
    // Ugritone to General MIDI - COMPLETE MAPPING
    auto& ugritoneToGM = mappings[7][0];
    
    // Kicks
    ugritoneToGM[35] = 36;  // Kick 2 -> GM Kick
    ugritoneToGM[36] = 36;  // Kick 1 -> GM Kick
    
    // Snares
    ugritoneToGM[37] = 38;  // Cross stick -> GM Cross stick
    ugritoneToGM[38] = 38;  // Snare 1 -> GM Snare
    ugritoneToGM[40] = 38;  // Snare 2 -> GM Snare
    
    // Hi-hats
    ugritoneToGM[22] = 42;  // Hi-hat closed (Ugritone custom) -> GM closed
    ugritoneToGM[26] = 46;  // Hi-hat open (Ugritone custom) -> GM open
    ugritoneToGM[42] = 42;  // Hi-hat closed standard -> GM closed
    ugritoneToGM[44] = 42;  // Hi-hat pedal -> GM closed
    ugritoneToGM[46] = 46;  // Hi-hat open standard -> GM open
    
    // Toms
    ugritoneToGM[41] = 41;  // Low floor tom
    ugritoneToGM[43] = 43;  // High floor tom
    ugritoneToGM[45] = 45;  // Low tom
    ugritoneToGM[47] = 47;  // Low-mid tom
    ugritoneToGM[48] = 48;  // Hi-mid tom
    ugritoneToGM[50] = 50;  // High tom
    
    // Cymbals
    ugritoneToGM[49] = 49;  // Crash 1
    ugritoneToGM[51] = 51;  // Ride 1
    ugritoneToGM[52] = 49;  // Crash cymbal 1 edge -> GM crash
    ugritoneToGM[53] = 51;  // Ride bell -> GM ride
    ugritoneToGM[55] = 49;  // Splash -> GM crash
    ugritoneToGM[57] = 49;  // Crash 2 -> GM crash
    ugritoneToGM[59] = 51;  // Ride 2 -> GM ride
    
    // Percussion
    ugritoneToGM[39] = 39;  // Hand clap
    ugritoneToGM[54] = 54;  // Tambourine
    ugritoneToGM[56] = 56;  // Cowbell
    ugritoneToGM[58] = 58;  // Vibraslap
    
    // Ugritone to Superior Drummer 3
    auto& ugritoneToSD3 = mappings[7][1];
    
    // Kicks
    ugritoneToSD3[35] = 36;
    ugritoneToSD3[36] = 36;
    
    // Snares
    ugritoneToSD3[37] = 37;
    ugritoneToSD3[38] = 38;
    ugritoneToSD3[40] = 40;
    
    // Hi-hats
    ugritoneToSD3[22] = 42;
    ugritoneToSD3[26] = 46;
    ugritoneToSD3[42] = 42;
    ugritoneToSD3[44] = 44;
    ugritoneToSD3[46] = 46;
    
    // Toms
    ugritoneToSD3[41] = 41;
    ugritoneToSD3[43] = 43;
    ugritoneToSD3[45] = 45;
    ugritoneToSD3[47] = 47;
    ugritoneToSD3[48] = 48;
    ugritoneToSD3[50] = 50;
    
    // Cymbals
    ugritoneToSD3[49] = 49;
    ugritoneToSD3[51] = 51;
    ugritoneToSD3[52] = 52;
    ugritoneToSD3[53] = 53;
    ugritoneToSD3[55] = 55;
    ugritoneToSD3[57] = 57;
    ugritoneToSD3[59] = 59;
    
    // Ugritone to EZdrummer
    auto& ugritoneToEzd = mappings[7][4];
    
    // Kicks
    ugritoneToEzd[35] = 36;
    ugritoneToEzd[36] = 36;
    
    // Snares
    ugritoneToEzd[37] = 37;
    ugritoneToEzd[38] = 38;
    ugritoneToEzd[40] = 38;  // Snare 2 -> EZD main snare
    
    // Hi-hats
    ugritoneToEzd[22] = 42;
    ugritoneToEzd[26] = 46;
    ugritoneToEzd[42] = 42;
    ugritoneToEzd[44] = 44;
    ugritoneToEzd[46] = 46;
    
    // Toms
    ugritoneToEzd[41] = 41;
    ugritoneToEzd[43] = 43;
    ugritoneToEzd[45] = 45;
    ugritoneToEzd[47] = 47;
    ugritoneToEzd[48] = 48;
    ugritoneToEzd[50] = 50;
    
    // Cymbals
    ugritoneToEzd[49] = 49;
    ugritoneToEzd[51] = 51;
    ugritoneToEzd[52] = 49;
    ugritoneToEzd[55] = 49;
    ugritoneToEzd[57] = 57;
    ugritoneToEzd[59] = 59;
    
    // ==================== OTHER LIBRARY MAPPINGS ====================
    
    // General MIDI to Superior Drummer 3
    auto& gmToSD3 = mappings[0][1];
    gmToSD3[36] = 36;
    gmToSD3[38] = 38;
    gmToSD3[42] = 42;
    gmToSD3[46] = 46;
    gmToSD3[49] = 49;
    gmToSD3[51] = 51;

    // General MIDI to Addictive Drums 2
    auto& gmToAD2 = mappings[0][2];
    gmToAD2[36] = 36;
    gmToAD2[38] = 38;
    gmToAD2[42] = 42;
    gmToAD2[46] = 46;
    gmToAD2[49] = 49;
    gmToAD2[57] = 55;
    gmToAD2[51] = 51;

    // EZdrummer to General MIDI
    auto& ezdToGM = mappings[4][0];
    ezdToGM[36] = 36;
    ezdToGM[38] = 38;
    ezdToGM[42] = 42;
    ezdToGM[46] = 46;
    ezdToGM[49] = 49;
    ezdToGM[51] = 51;
    ezdToGM[24] = 36;
    ezdToGM[26] = 38;

    // EZdrummer to Superior Drummer 3
    auto& ezdToSD3 = mappings[4][1];
    ezdToSD3[36] = 36;
    ezdToSD3[38] = 38;
    ezdToSD3[42] = 42;
    ezdToSD3[46] = 46;
    ezdToSD3[24] = 36;
    ezdToSD3[26] = 38;

    // EZdrummer to Ugritone
    auto& ezdToUgritone = mappings[4][7];
    ezdToUgritone[36] = 36;
    ezdToUgritone[38] = 38;
    ezdToUgritone[42] = 22;  // Use Ugritone custom hihat
    ezdToUgritone[46] = 26;  // Use Ugritone custom open hihat
    ezdToUgritone[24] = 36;

    // Superior Drummer 3 to EZdrummer
    auto& sd3ToEzd = mappings[1][4];
    sd3ToEzd[36] = 36;
    sd3ToEzd[38] = 38;
    sd3ToEzd[42] = 42;
    sd3ToEzd[46] = 46;
	
	  // BFD3 to General MIDI (index 8)
    auto& bfd3ToGM = mappings[8][0];
    
    // Kicks
    bfd3ToGM[36] = 36;  // Kick
    
    // Snares
    bfd3ToGM[38] = 38;  // Snare center
    bfd3ToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    bfd3ToGM[42] = 42;  // Hi-hat closed
    bfd3ToGM[44] = 46;  // Hi-hat open
    bfd3ToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    bfd3ToGM[43] = 43;  // Tom 3 (floor)
    bfd3ToGM[47] = 47;  // Tom 2
    bfd3ToGM[48] = 48;  // Tom 1
    bfd3ToGM[50] = 48;  // Tom 1 rim -> Tom 1
    
    // Cymbals
    bfd3ToGM[41] = 52;  // China
    bfd3ToGM[49] = 49;  // Crash 1
    bfd3ToGM[51] = 51;  // Ride bow
    bfd3ToGM[52] = 49;  // Crash 2
    bfd3ToGM[55] = 53;  // Ride bell
    bfd3ToGM[57] = 49;  // Crash 2 edge -> Crash
    
    // ========================================
    
    // MT Power Drum Kit 2 to General MIDI (index 9)
    auto& mtpdk2ToGM = mappings[9][0];
    
    // Kicks
    mtpdk2ToGM[35] = 36;  // Bass drum 2
    mtpdk2ToGM[36] = 36;  // Kick
    
    // Snares
    mtpdk2ToGM[38] = 38;  // Snare
    mtpdk2ToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    mtpdk2ToGM[42] = 42;  // Hi-hat closed
    mtpdk2ToGM[44] = 46;  // Hi-hat open
    mtpdk2ToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    mtpdk2ToGM[43] = 43;  // Tom 3 (floor)
    mtpdk2ToGM[45] = 47;  // Tom 2
    mtpdk2ToGM[48] = 48;  // Tom 1
    
    // Cymbals
    mtpdk2ToGM[49] = 49;  // Crash 1
    mtpdk2ToGM[50] = 49;  // Crash 2
    mtpdk2ToGM[51] = 51;  // Ride
    mtpdk2ToGM[52] = 52;  // China
    mtpdk2ToGM[57] = 53;  // Ride bell
    mtpdk2ToGM[59] = 51;  // Ride edge -> Ride
    
    // ========================================
    
    // DrumGizmo to General MIDI (index 10)
    auto& drumgizmoToGM = mappings[10][0];
    
    // Kicks
    drumgizmoToGM[36] = 36;  // Kick
    
    // Snares
    drumgizmoToGM[38] = 38;  // Snare
    drumgizmoToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    drumgizmoToGM[42] = 42;  // Hi-hat closed
    drumgizmoToGM[44] = 46;  // Hi-hat open
    drumgizmoToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    drumgizmoToGM[43] = 43;  // Tom 3 (floor)
    drumgizmoToGM[45] = 47;  // Tom 2
    drumgizmoToGM[48] = 48;  // Tom 1
    
    // Cymbals
    drumgizmoToGM[49] = 49;  // Crash 1
    drumgizmoToGM[51] = 51;  // Ride
    drumgizmoToGM[52] = 49;  // Crash 2
    drumgizmoToGM[55] = 53;  // Ride bell
    drumgizmoToGM[57] = 49;  // Crash 3
    
    // ========================================
    
    // Sitala to General MIDI (index 11)
    auto& sitalaToGM = mappings[11][0];
    
    // Kicks
    sitalaToGM[36] = 36;  // Kick
    
    // Snares
    sitalaToGM[38] = 38;  // Snare
    
    // Hi-hats
    sitalaToGM[42] = 42;  // Hi-hat closed
    sitalaToGM[44] = 46;  // Hi-hat open
    sitalaToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    sitalaToGM[43] = 43;  // Tom 3 (floor)
    sitalaToGM[45] = 47;  // Tom 2
    sitalaToGM[48] = 48;  // Tom 1
    
    // Cymbals
    sitalaToGM[49] = 49;  // Crash 1
    sitalaToGM[51] = 51;  // Ride
    sitalaToGM[52] = 49;  // Crash 2
    
    // ========================================
    
    // Krimh Drums to General MIDI (index 12)
    auto& krimhToGM = mappings[12][0];
    
    // Kicks
    krimhToGM[36] = 36;  // Kick
    
    // Snares
    krimhToGM[37] = 38;  // Snare ghost
    krimhToGM[38] = 38;  // Snare center
    krimhToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    krimhToGM[42] = 42;  // Hi-hat closed
    krimhToGM[44] = 46;  // Hi-hat open
    krimhToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    krimhToGM[45] = 43;  // Tom 3 (floor)
    krimhToGM[47] = 47;  // Tom 2
    krimhToGM[48] = 48;  // Tom 1
    
    // Cymbals
    krimhToGM[49] = 49;  // Crash 1
    krimhToGM[51] = 51;  // Ride bow
    krimhToGM[52] = 49;  // Crash 2
    krimhToGM[55] = 53;  // Ride bell
    krimhToGM[57] = 49;  // Crash 3
    
    // ========================================
    
    // The Monarch Kit to General MIDI (index 13)
    auto& monarchToGM = mappings[13][0];
    
    // Kicks
    monarchToGM[36] = 36;  // Kick
    
    // Snares
    monarchToGM[38] = 38;  // Snare center
    monarchToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    monarchToGM[42] = 42;  // Hi-hat closed
    monarchToGM[44] = 46;  // Hi-hat open
    monarchToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    monarchToGM[45] = 43;  // Tom 3 (floor)
    monarchToGM[47] = 47;  // Tom 2
    monarchToGM[48] = 48;  // Tom 1
    
    // Cymbals
    monarchToGM[49] = 49;  // Crash 1 bow
    monarchToGM[51] = 51;  // Ride
    monarchToGM[52] = 49;  // Crash 2
    monarchToGM[55] = 53;  // Ride bell
    monarchToGM[57] = 52;  // China
    monarchToGM[59] = 55;  // Splash
    
    // ========================================
    
    // Shreddage Drums to General MIDI (index 14)
    auto& shreddageToGM = mappings[14][0];
    
    // Kicks
    shreddageToGM[35] = 36;  // Kick alt
    shreddageToGM[36] = 36;  // Kick
    
    // Snares
    shreddageToGM[38] = 38;  // Snare center
    shreddageToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    shreddageToGM[42] = 42;  // Hi-hat closed
    shreddageToGM[44] = 46;  // Hi-hat open
    shreddageToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    shreddageToGM[43] = 43;  // Tom 3 (floor)
    shreddageToGM[45] = 47;  // Tom 2
    shreddageToGM[48] = 48;  // Tom 1
    
    // Cymbals
    shreddageToGM[49] = 49;  // Crash 1
    shreddageToGM[50] = 51;  // Ride bow
    shreddageToGM[51] = 49;  // Crash 2
    shreddageToGM[52] = 53;  // Ride bell
    shreddageToGM[55] = 49;  // Crash choke
    shreddageToGM[57] = 52;  // China
    
    // ========================================
    
    // Damage 2 to General MIDI (index 15)
    auto& damage2ToGM = mappings[15][0];
    
    // Kicks
    damage2ToGM[36] = 36;  // Kick
    
    // Snares
    damage2ToGM[38] = 38;  // Snare center
    damage2ToGM[40] = 40;  // Snare rim
    
    // Hi-hats
    damage2ToGM[42] = 42;  // Hi-hat closed
    damage2ToGM[44] = 46;  // Hi-hat open
    damage2ToGM[46] = 44;  // Hi-hat pedal
    
    // Toms
    damage2ToGM[45] = 43;  // Tom 3 (floor)
    damage2ToGM[47] = 47;  // Tom 2
    damage2ToGM[48] = 48;  // Tom 1
    
    // Cymbals
    damage2ToGM[49] = 49;  // Crash 1
    damage2ToGM[51] = 51;  // Ride
    damage2ToGM[52] = 49;  // Crash 2
    damage2ToGM[55] = 53;  // Ride bell
    // Note: Keyswitches 60, 61 are ignored (effects and layers)
}

uint8_t DrumLibraryManager::mapNoteToLibrary(uint8_t note, DrumLibrary sourceLibrary, DrumLibrary targetLibrary) const
{
    // Bypass mode - no remapping
    if (targetLibrary == DrumLibrary::Bypass)
        return note;
    
    // Rest of existing function...
    int sourceIdx = static_cast<int>(sourceLibrary) - 2; // -2 because Bypass=1, GeneralMIDI=2
    int targetIdx = static_cast<int>(targetLibrary) - 2;
    
    if (sourceIdx < 0 || sourceIdx >= 16 || targetIdx < 0 || targetIdx >= 16)
        return note;
    
    if (sourceLibrary == targetLibrary)
        return note;
    
    // Try direct mapping
    auto sourceIt = mappings.find(sourceIdx);
    if (sourceIt != mappings.end())
    {
        auto targetIt = sourceIt->second.find(targetIdx);
        if (targetIt != sourceIt->second.end())
        {
            auto noteIt = targetIt->second.find(note);
            if (noteIt != targetIt->second.end())
            {
                return noteIt->second;
            }
        }
    }

    // Try via General MIDI as intermediate
    uint8_t gmNote = note;
    
    if (sourceLibrary != DrumLibrary::GeneralMIDI)
    {
        auto toGM = mappings.find(sourceIdx);
        if (toGM != mappings.end())
        {
            auto gmMap = toGM->second.find(0); // 0 = GM index
            if (gmMap != toGM->second.end())
            {
                auto noteInGM = gmMap->second.find(note);
                if (noteInGM != gmMap->second.end())
                {
                    gmNote = noteInGM->second;
                }
            }
        }
    }
    
    if (targetLibrary != DrumLibrary::GeneralMIDI && gmNote != note)
    {
        auto fromGM = mappings.find(0);
        if (fromGM != mappings.end())
        {
            auto targetMap = fromGM->second.find(targetIdx);
            if (targetMap != fromGM->second.end())
            {
                auto finalNote = targetMap->second.find(gmNote);
                if (finalNote != targetMap->second.end())
                {
                    return finalNote->second;
                }
            }
        }
    }
    
    return gmNote;
}

// Rest of the DrumLibraryManager implementation...
juce::File DrumLibraryManager::getRootFolder(int index) const
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
        return rootFolders[index].folder;
    return juce::File();
}

juce::String DrumLibraryManager::getRootFolderName(int index) const
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
        return rootFolders[index].folder.getFileName();
    return {};
}

DrumLibrary DrumLibraryManager::getRootFolderSourceLibrary(int index) const
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
        return rootFolders[index].sourceLibrary;
    return DrumLibrary::Unknown;
}

void DrumLibraryManager::removeRootFolder(int index)
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
    {
        rootFolders.erase(rootFolders.begin() + index);
        saveConfiguration();
    }
}

void DrumLibraryManager::rescanFolders()
{
    // Placeholder for future folder scanning
}

juce::File DrumLibraryManager::getConfigFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DrumGroovePro")
        .getChildFile("config.xml");
}

void DrumLibraryManager::loadConfiguration()
{
    juce::File configFile = getConfigFile();  // NOT getConfigFilePath()

    if (!configFile.existsAsFile())
    {
        DBG("No configuration file found at: " + configFile.getFullPathName());
        return;
    }

    auto config = juce::XmlDocument::parse(configFile);
    if (config == nullptr)
    {
        DBG("ERROR: Failed to parse configuration file");
        return;
    }

    if (!config->hasTagName("DrumLibraryManagerConfig"))
    {
        DBG("ERROR: Invalid configuration file format");
        return;
    }

    // Load root folders
    rootFolders.clear();

    if (auto* foldersElement = config->getChildByName("RootFolders"))
    {
        for (auto* folderElement : foldersElement->getChildIterator())
        {
            if (folderElement->hasTagName("Folder"))
            {
                juce::String path = folderElement->getStringAttribute("path");
                int sourceLib = folderElement->getIntAttribute("sourceLibrary", 0);

                juce::File folder(path);
                if (folder.exists())
                {
                    FolderInfo info;
                    info.folder = folder;
                    info.sourceLibrary = static_cast<DrumLibrary>(sourceLib);
                    rootFolders.push_back(info);
                    
                    DBG("Loaded root folder: " + folder.getFileName() + " (" + path + ")");
                }
            }
        }
    }

    // Load last selected target library
    int savedTargetLib = config->getIntAttribute("lastSelectedTargetLibrary", static_cast<int>(DrumLibrary::GeneralMIDI));
    lastSelectedTargetLibrary = static_cast<DrumLibrary>(savedTargetLib);
    
    DBG("Loaded last selected target library: " + juce::String(savedTargetLib) + 
        " (" + DrumLibraryManager::getLibraryName(lastSelectedTargetLibrary) + ")");

    DBG("Configuration loaded successfully");
}

void DrumLibraryManager::saveConfiguration()
{
    auto config = std::make_unique<juce::XmlElement>("DrumLibraryManagerConfig");

    // Save root folders
    auto* foldersElement = config->createNewChildElement("RootFolders");
    for (const auto& folderInfo : rootFolders)
    {
        auto* folderElement = foldersElement->createNewChildElement("Folder");
        folderElement->setAttribute("path", folderInfo.folder.getFullPathName());
        folderElement->setAttribute("sourceLibrary", static_cast<int>(folderInfo.sourceLibrary));
    }

    // Save last selected target library
    config->setAttribute("lastSelectedTargetLibrary", static_cast<int>(lastSelectedTargetLibrary));

    // Save to file
    juce::File configFile = getConfigFile();  // NOT getConfigFilePath()
    
    // Ensure directory exists
    configFile.getParentDirectory().createDirectory();
    
    if (config->writeTo(configFile))
    {
        DBG("Configuration saved successfully to: " + configFile.getFullPathName());
    }
    else
    {
        DBG("ERROR: Failed to save configuration to: " + configFile.getFullPathName());
    }
}

juce::String DrumLibraryManager::getLibraryName(DrumLibrary library)
{
    switch (library)
    {
        case DrumLibrary::Unknown: return "Unknown";
        case DrumLibrary::Bypass: return "Bypass (No Remapping)";
        case DrumLibrary::GeneralMIDI: return "General MIDI";
        case DrumLibrary::SuperiorDrummer3: return "Superior Drummer 3";
        case DrumLibrary::AddictiveDrums2: return "Addictive Drums 2";
        case DrumLibrary::Battery4: return "Battery 4";
        case DrumLibrary::EZdrummer: return "EZdrummer";
        case DrumLibrary::GetGoodDrums: return "GetGood Drums";
        case DrumLibrary::StevenSlateDrums: return "Steven Slate Drums";
        case DrumLibrary::Ugritone: return "Ugritone";
        case DrumLibrary::BFD3: return "BFD3";
        case DrumLibrary::MTPowerDrumKit2: return "MT Power Drum Kit 2";
        case DrumLibrary::DrumGizmo: return "DrumGizmo";
        case DrumLibrary::Sitala: return "Sitala";
        case DrumLibrary::KrimhDrums: return "Krimh Drums";
        case DrumLibrary::TheMonarchKit: return "The Monarch Kit";
        case DrumLibrary::ShreddageDrums: return "Shreddage Drums";
        case DrumLibrary::Damage2: return "Damage 2";
        default: return "Unknown";
    }
}

juce::StringArray DrumLibraryManager::getAllLibraryNames()
{
    juce::StringArray names;
    // Alphabetically sorted, excluding Unknown
    names.add(getLibraryName(DrumLibrary::AddictiveDrums2));
    names.add(getLibraryName(DrumLibrary::Battery4));
    names.add(getLibraryName(DrumLibrary::BFD3));
    names.add(getLibraryName(DrumLibrary::Bypass));
    names.add(getLibraryName(DrumLibrary::Damage2));
    names.add(getLibraryName(DrumLibrary::DrumGizmo));
    names.add(getLibraryName(DrumLibrary::EZdrummer));
    names.add(getLibraryName(DrumLibrary::GeneralMIDI));
    names.add(getLibraryName(DrumLibrary::GetGoodDrums));
    names.add(getLibraryName(DrumLibrary::KrimhDrums));
    names.add(getLibraryName(DrumLibrary::MTPowerDrumKit2));
    names.add(getLibraryName(DrumLibrary::ShreddageDrums));
    names.add(getLibraryName(DrumLibrary::Sitala));
    names.add(getLibraryName(DrumLibrary::StevenSlateDrums));
    names.add(getLibraryName(DrumLibrary::SuperiorDrummer3));
    names.add(getLibraryName(DrumLibrary::TheMonarchKit));
    names.add(getLibraryName(DrumLibrary::Ugritone));
    return names;
}

// For Add Folder dialog - includes Unknown
juce::StringArray DrumLibraryManager::getAllSourceLibraryNames()
{
    juce::StringArray names;
    // Alphabetically sorted, including Unknown
    names.add(getLibraryName(DrumLibrary::AddictiveDrums2));
    names.add(getLibraryName(DrumLibrary::Battery4));
    names.add(getLibraryName(DrumLibrary::BFD3));
    names.add(getLibraryName(DrumLibrary::Damage2));
    names.add(getLibraryName(DrumLibrary::DrumGizmo));
    names.add(getLibraryName(DrumLibrary::EZdrummer));
    names.add(getLibraryName(DrumLibrary::GeneralMIDI));
    names.add(getLibraryName(DrumLibrary::GetGoodDrums));
    names.add(getLibraryName(DrumLibrary::KrimhDrums));
    names.add(getLibraryName(DrumLibrary::MTPowerDrumKit2));
    names.add(getLibraryName(DrumLibrary::ShreddageDrums));
    names.add(getLibraryName(DrumLibrary::Sitala));
    names.add(getLibraryName(DrumLibrary::StevenSlateDrums));
    names.add(getLibraryName(DrumLibrary::SuperiorDrummer3));
    names.add(getLibraryName(DrumLibrary::TheMonarchKit));
    names.add(getLibraryName(DrumLibrary::Ugritone));
    names.add(getLibraryName(DrumLibrary::Unknown));
    return names;
}

DrumLibrary DrumLibraryManager::getLibraryFromName(const juce::String& name)
{
    // Map library names back to enum values
    if (name == "General MIDI") return DrumLibrary::GeneralMIDI;
    if (name == "Bypass (No Remapping)") return DrumLibrary::Bypass;
    if (name == "Superior Drummer 3") return DrumLibrary::SuperiorDrummer3;
    if (name == "Addictive Drums 2") return DrumLibrary::AddictiveDrums2;
    if (name == "Battery 4") return DrumLibrary::Battery4;
    if (name == "EZdrummer") return DrumLibrary::EZdrummer;
    if (name == "GetGood Drums") return DrumLibrary::GetGoodDrums;
    if (name == "Steven Slate Drums") return DrumLibrary::StevenSlateDrums;
    if (name == "Ugritone") return DrumLibrary::Ugritone;
    if (name == "BFD3") return DrumLibrary::BFD3;
    if (name == "MT Power Drum Kit 2") return DrumLibrary::MTPowerDrumKit2;
    if (name == "DrumGizmo") return DrumLibrary::DrumGizmo;
    if (name == "Sitala") return DrumLibrary::Sitala;
    if (name == "Krimh Drums") return DrumLibrary::KrimhDrums;
    if (name == "The Monarch Kit") return DrumLibrary::TheMonarchKit;
    if (name == "Shreddage Drums") return DrumLibrary::ShreddageDrums;
    if (name == "Damage 2") return DrumLibrary::Damage2;
    
    return DrumLibrary::Unknown;
}

void DrumLibraryManager::setLastSelectedTargetLibrary(DrumLibrary library)
{
    lastSelectedTargetLibrary = library;
    saveConfiguration();  // Auto-save when changed
    
    DBG("Target library set to: " + getLibraryName(library));
}

DrumLibrary DrumLibraryManager::getLastSelectedTargetLibrary() const
{
    return lastSelectedTargetLibrary;
}