#include "DrumLibraryManager.h"
#include "MidiDissector.h"

DrumLibraryManager::DrumLibraryManager()
{
    loadConfiguration();
    loadOriginLibraries();

    juce::File mappingFile = getCustomMappingsFile();

    if (!mappingFile.existsAsFile())
    {
        // File doesn't exist - initialize all libraries and create the file
        DBG("No target drum mapping file found - creating complete default file");
        initializeMappingTables();  // Create all libraries with identity mappings
        createDefaultMappingsFile(); // Save to disk
    }
    else
    {
        // File exists - ONLY load what's in it, don't add anything
        DBG("Target drum mapping file exists - loading ONLY from file");
        // DO NOT call initializeHardcodedMappings() - let XML be the only source
        loadCustomMappings();  // Load everything from XML file
    }

    // Load origin mappings AFTER target mappings (so they override defaults)
    loadOriginMappings();
}

DrumLibraryManager::~DrumLibraryManager()
{
    saveConfiguration();
    saveOriginLibraries();
    saveOriginMappings();
}

void DrumLibraryManager::addRootFolder(const juce::File& folder, DrumLibrary sourceLib, bool isWritable)
{
    if (folder.exists() && folder.isDirectory())
    {
        FolderInfo fi;
        fi.folder = folder;
        fi.sourceLibrary = sourceLib;
        fi.isWritable = isWritable;

        rootFolders.push_back(fi);
        saveConfiguration();
    }
}

void DrumLibraryManager::initializeMappingTables()
{
    // Initialize identity mappings for ALL library pairs (0-16 = libraries 2-18)
    // Enum goes: Unknown=0, Bypass=1, GeneralMIDI=2...SalamanderDrumkit=17, MuldjordKit3=18
    // With -2 offset: indices 0-16 for libraries 2-18
    for (int from = 0; from < 18; ++from)
    {
        for (int to = 0; to < 18; ++to)
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
    // Ugritone = 8, GM = 2, so indices are 6 and 0 with -2 offset
    auto& ugritoneToGM = mappings[6][0];

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
    ugritoneToGM[60] = 52;  // China 1 -> GM Chinese cymbal
    ugritoneToGM[61] = 52;  // China 2 / China choke -> GM Chinese cymbal
    ugritoneToGM[62] = 49;  // Crash 3 -> GM crash

    // Percussion
    ugritoneToGM[39] = 39;  // Hand clap
    ugritoneToGM[54] = 54;  // Tambourine
    ugritoneToGM[56] = 50;  // High Tom -> GM High Tom
    ugritoneToGM[58] = 58;  // Vibraslap

    // Extended low range (some Ugritone packs use these)
    ugritoneToGM[32] = 36;  // Extra kick -> GM kick
    ugritoneToGM[33] = 36;  // Extra kick -> GM kick
    ugritoneToGM[34] = 36;  // Extra kick -> GM kick

    // Ugritone to Superior Drummer 3
    // Ugritone = 8, SD3 = 3, so indices are 6 and 1 with -2 offset
    auto& ugritoneToSD3 = mappings[6][1];

    // Kicks
    ugritoneToSD3[35] = 36;
    ugritoneToSD3[36] = 36;

    // Snares
    ugritoneToSD3[37] = 37;
    ugritoneToSD3[38] = 38;
    ugritoneToSD3[40] = 40;

    // Hi-hats - SD3 uses standard GM mapping
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
    ugritoneToSD3[60] = 52;  // China 1 -> SD3 China
    ugritoneToSD3[61] = 52;  // China 2 -> SD3 China
    ugritoneToSD3[62] = 57;  // Crash 3 -> SD3 Crash 2

    // Extended low range
    ugritoneToSD3[32] = 36;  // Extra kick -> kick
    ugritoneToSD3[33] = 36;
    ugritoneToSD3[34] = 36;

    // ==================== SUPERIOR DRUMMER 3 MAPPINGS ====================

    // Superior Drummer 3 to General MIDI
    // SD3 = 3, GM = 2, so indices are 1 and 0
    auto& sd3ToGM = mappings[1][0];

    // Kicks
    sd3ToGM[35] = 36;  // Kick L
    sd3ToGM[36] = 36;  // Kick

    // Snares
    sd3ToGM[37] = 37;  // Side stick
    sd3ToGM[38] = 38;  // Snare
    sd3ToGM[40] = 38;  // Rim shot -> Snare

    // Hi-hats
    sd3ToGM[42] = 42;  // Hi-hat closed
    sd3ToGM[44] = 44;  // Hi-hat pedal
    sd3ToGM[46] = 46;  // Hi-hat open

    // Toms
    sd3ToGM[41] = 41;  // Floor tom
    sd3ToGM[43] = 43;  // Floor tom high
    sd3ToGM[45] = 45;  // Tom low
    sd3ToGM[47] = 47;  // Tom mid
    sd3ToGM[48] = 48;  // Tom high
    sd3ToGM[50] = 50;  // Tom highest

    // Cymbals
    sd3ToGM[49] = 49;  // Crash
    sd3ToGM[51] = 51;  // Ride
    sd3ToGM[52] = 52;  // China
    sd3ToGM[53] = 53;  // Ride bell
    sd3ToGM[55] = 49;  // Splash -> Crash
    sd3ToGM[57] = 49;  // Crash 2 -> Crash
    sd3ToGM[59] = 51;  // Ride 2 -> Ride

    // ==================== ADDICTIVE DRUMS 2 MAPPINGS ====================

    // Addictive Drums 2 to General MIDI
    // AD2 = 4, GM = 2, so indices are 2 and 0
    auto& ad2ToGM = mappings[2][0];

    // AD2 uses mostly standard GM mapping
    // Only mapping differences from GM
    ad2ToGM[35] = 36;  // Kick alt -> GM Kick
    ad2ToGM[37] = 37;  // Side stick
    ad2ToGM[38] = 38;  // Snare
    ad2ToGM[40] = 38;  // Rim shot -> Snare

    // Hi-hats
    ad2ToGM[42] = 42;  // Closed
    ad2ToGM[44] = 44;  // Pedal
    ad2ToGM[46] = 46;  // Open

    // Toms
    ad2ToGM[41] = 41;
    ad2ToGM[43] = 43;
    ad2ToGM[45] = 45;
    ad2ToGM[47] = 47;
    ad2ToGM[48] = 48;
    ad2ToGM[50] = 50;

    // Cymbals
    ad2ToGM[49] = 49;
    ad2ToGM[51] = 51;
    ad2ToGM[52] = 52;
    ad2ToGM[53] = 53;
    ad2ToGM[55] = 55;
    ad2ToGM[57] = 57;
    ad2ToGM[59] = 59;

    // ==================== EZDRUMMER MAPPINGS ====================

    // EZdrummer to General MIDI
    // EZdrummer = 5, GM = 2, so indices are 3 and 0
    auto& ezdToGM = mappings[3][0];

    // EZdrummer uses standard GM mapping
    ezdToGM[36] = 36;  // Kick
    ezdToGM[37] = 37;  // Side stick
    ezdToGM[38] = 38;  // Snare
    ezdToGM[40] = 40;

    // Hi-hats
    ezdToGM[42] = 42;
    ezdToGM[44] = 44;
    ezdToGM[46] = 46;

    // Toms
    ezdToGM[41] = 41;
    ezdToGM[43] = 43;
    ezdToGM[45] = 45;
    ezdToGM[47] = 47;
    ezdToGM[48] = 48;
    ezdToGM[50] = 50;

    // Cymbals
    ezdToGM[49] = 49;
    ezdToGM[51] = 51;
    ezdToGM[52] = 52;
    ezdToGM[53] = 53;
    ezdToGM[55] = 55;
    ezdToGM[57] = 57;
    ezdToGM[59] = 59;

    // ==================== GETGOOD DRUMS MAPPINGS ====================

    // GetGood Drums to General MIDI
    // GGD = 6, GM = 2, so indices are 4 and 0
    auto& ggdToGM = mappings[4][0];

    // GGD uses GM standard
    ggdToGM[36] = 36;
    ggdToGM[37] = 37;
    ggdToGM[38] = 38;
    ggdToGM[40] = 40;

    // Hi-hats
    ggdToGM[42] = 42;
    ggdToGM[44] = 44;
    ggdToGM[46] = 46;

    // Toms
    ggdToGM[41] = 41;
    ggdToGM[43] = 43;
    ggdToGM[45] = 45;
    ggdToGM[47] = 47;
    ggdToGM[48] = 48;
    ggdToGM[50] = 50;

    // Cymbals
    ggdToGM[49] = 49;
    ggdToGM[51] = 51;
    ggdToGM[52] = 52;
    ggdToGM[53] = 53;
    ggdToGM[55] = 55;
    ggdToGM[57] = 57;
    ggdToGM[59] = 59;

    // ==================== STEVEN SLATE DRUMS MAPPINGS ====================

    // Steven Slate Drums to General MIDI
    // SSD = 7, GM = 2, so indices are 5 and 0
    auto& ssdToGM = mappings[5][0];

    // SSD uses GM standard
    ssdToGM[36] = 36;
    ssdToGM[37] = 37;
    ssdToGM[38] = 38;
    ssdToGM[40] = 40;

    // Hi-hats
    ssdToGM[42] = 42;
    ssdToGM[44] = 44;
    ssdToGM[46] = 46;

    // Toms
    ssdToGM[41] = 41;
    ssdToGM[43] = 43;
    ssdToGM[45] = 45;
    ssdToGM[47] = 47;
    ssdToGM[48] = 48;
    ssdToGM[50] = 50;

    // Cymbals
    ssdToGM[49] = 49;
    ssdToGM[51] = 51;
    ssdToGM[52] = 52;
    ssdToGM[53] = 53;
    ssdToGM[55] = 55;
    ssdToGM[57] = 57;
    ssdToGM[59] = 59;

    // ==================== BFD3 MAPPINGS ====================

    // BFD3 to General MIDI
    // BFD3 = 9, GM = 2, so indices are 7 and 0
    auto& bfd3ToGM = mappings[7][0];

    // BFD3 uses standard GM mapping
    bfd3ToGM[36] = 36;
    bfd3ToGM[37] = 37;
    bfd3ToGM[38] = 38;
    bfd3ToGM[40] = 40;

    // Hi-hats
    bfd3ToGM[42] = 42;
    bfd3ToGM[44] = 44;
    bfd3ToGM[46] = 46;

    // Toms
    bfd3ToGM[41] = 41;
    bfd3ToGM[43] = 43;
    bfd3ToGM[45] = 45;
    bfd3ToGM[47] = 47;
    bfd3ToGM[48] = 48;
    bfd3ToGM[50] = 50;

    // Cymbals
    bfd3ToGM[49] = 49;
    bfd3ToGM[51] = 51;
    bfd3ToGM[52] = 52;
    bfd3ToGM[53] = 53;
    bfd3ToGM[55] = 55;
    bfd3ToGM[57] = 57;
    bfd3ToGM[59] = 59;

    // ==================== MT POWER DRUM KIT 2 MAPPINGS ====================

    // MT Power Drum Kit 2 to General MIDI
    // MTPowerDrumKit2 = 10, GM = 2, so indices are 8 and 0
    auto& mtpdkToGM = mappings[8][0];

    // MTPDK2 uses GM standard
    mtpdkToGM[36] = 36;
    mtpdkToGM[37] = 37;
    mtpdkToGM[38] = 38;
    mtpdkToGM[40] = 40;

    // Hi-hats
    mtpdkToGM[42] = 42;
    mtpdkToGM[44] = 44;
    mtpdkToGM[46] = 46;

    // Toms
    mtpdkToGM[41] = 41;
    mtpdkToGM[43] = 43;
    mtpdkToGM[45] = 45;
    mtpdkToGM[47] = 47;
    mtpdkToGM[48] = 48;
    mtpdkToGM[50] = 50;

    // Cymbals
    mtpdkToGM[49] = 49;
    mtpdkToGM[51] = 51;
    mtpdkToGM[52] = 52;
    mtpdkToGM[53] = 53;
    mtpdkToGM[55] = 55;
    mtpdkToGM[57] = 57;
    mtpdkToGM[59] = 59;

    // ==================== SITALA MAPPINGS ====================

    // Sitala to General MIDI
    // Sitala = 11, GM = 2, so indices are 9 and 0
    auto& sitalaToGM = mappings[9][0];

    // Sitala uses GM standard
    sitalaToGM[36] = 36;
    sitalaToGM[37] = 37;
    sitalaToGM[38] = 38;
    sitalaToGM[40] = 40;

    // Hi-hats
    sitalaToGM[42] = 42;
    sitalaToGM[44] = 44;
    sitalaToGM[46] = 46;

    // Toms
    sitalaToGM[41] = 41;
    sitalaToGM[43] = 43;
    sitalaToGM[45] = 45;
    sitalaToGM[47] = 47;
    sitalaToGM[48] = 48;
    sitalaToGM[50] = 50;

    // Cymbals
    sitalaToGM[49] = 49;
    sitalaToGM[51] = 51;
    sitalaToGM[52] = 52;
    sitalaToGM[53] = 53;
    sitalaToGM[55] = 55;
    sitalaToGM[57] = 57;
    sitalaToGM[59] = 59;

    // ==================== SHREDDAGE DRUMS MAPPINGS ====================

    // Shreddage Drums to General MIDI
    // ShreddageDrums = 12, GM = 2, so indices are 10 and 0
    auto& shreddageToGM = mappings[10][0];

    // Shreddage uses GM standard
    shreddageToGM[36] = 36;
    shreddageToGM[37] = 37;
    shreddageToGM[38] = 38;
    shreddageToGM[40] = 40;

    // Hi-hats
    shreddageToGM[42] = 42;
    shreddageToGM[44] = 44;
    shreddageToGM[46] = 46;

    // Toms
    shreddageToGM[41] = 41;
    shreddageToGM[43] = 43;
    shreddageToGM[45] = 45;
    shreddageToGM[47] = 47;
    shreddageToGM[48] = 48;
    shreddageToGM[50] = 50;

    // Cymbals
    shreddageToGM[49] = 49;
    shreddageToGM[51] = 51;
    shreddageToGM[52] = 52;
    shreddageToGM[53] = 53;
    shreddageToGM[55] = 55;
    shreddageToGM[57] = 57;
    shreddageToGM[59] = 59;

    // ==================== DAMAGE 2 MAPPINGS ====================

    // Damage 2 to General MIDI
    // Damage2 = 13, GM = 2, so indices are 11 and 0
    auto& damage2ToGM = mappings[11][0];

    // Damage 2 uses GM standard
    damage2ToGM[36] = 36;
    damage2ToGM[37] = 37;
    damage2ToGM[38] = 38;
    damage2ToGM[40] = 40;

    // Hi-hats
    damage2ToGM[42] = 42;
    damage2ToGM[44] = 44;
    damage2ToGM[46] = 46;

    // Toms
    damage2ToGM[41] = 41;
    damage2ToGM[43] = 43;
    damage2ToGM[45] = 45;
    damage2ToGM[47] = 47;
    damage2ToGM[48] = 48;
    damage2ToGM[50] = 50;

    // Cymbals
    damage2ToGM[49] = 49;
    damage2ToGM[51] = 51;
    damage2ToGM[52] = 52;
    damage2ToGM[53] = 53;
    damage2ToGM[55] = 55;
    damage2ToGM[57] = 57;
    damage2ToGM[59] = 59;

    // ==================== MODO DRUM MAPPINGS ====================

    // MODO Drum to General MIDI
    // MODO = 14, GM = 2, so indices are 12 and 0
    auto& modoToGM = mappings[12][0];

    // Kicks
    modoToGM[36] = 36;  // Kick

    // Snares
    modoToGM[38] = 38;  // Snare
    modoToGM[37] = 37;  // Side stick
    modoToGM[40] = 38;  // Rim shot -> Snare

    // Hi-hats
    modoToGM[42] = 42;  // Hi-hat closed
    modoToGM[44] = 44;  // Hi-hat pedal
    modoToGM[46] = 46;  // Hi-hat open

    // Toms
    modoToGM[41] = 41;  // Floor tom
    modoToGM[43] = 43;  // Floor tom high
    modoToGM[45] = 45;  // Tom low
    modoToGM[47] = 47;  // Tom mid
    modoToGM[48] = 48;  // Tom high

    // Cymbals
    modoToGM[49] = 49;  // Crash
    modoToGM[51] = 51;  // Ride
    modoToGM[52] = 52;  // China
    modoToGM[53] = 53;  // Ride bell
    modoToGM[55] = 49;  // Splash -> Crash
    modoToGM[57] = 49;  // Crash 2 -> Crash

    // ==================== DRUMLOCKER MAPPINGS ====================

    // DrumLocker to General MIDI
    // DrumLocker = 15, GM = 2, so indices are 13 and 0
    auto& drumLockerToGM = mappings[13][0];

    // Kicks
    drumLockerToGM[35] = 36;  // Kick Sub -> GM Kick
    drumLockerToGM[36] = 36;  // Kick -> GM Kick

    // Snares
    drumLockerToGM[38] = 38;  // Snare -> GM Snare
    drumLockerToGM[37] = 37;  // Rim Shot -> GM Cross Stick
    drumLockerToGM[40] = 40;  // Snare Rim -> GM Snare Rim

    // Hi-hats
    drumLockerToGM[42] = 42;  // Hi-Hat Closed -> GM Closed
    drumLockerToGM[44] = 44;  // Hi-Hat Pedal -> GM Pedal
    drumLockerToGM[46] = 46;  // Hi-Hat Open -> GM Open

    // Toms
    drumLockerToGM[41] = 41;  // Tom 4 -> GM Low Floor Tom
    drumLockerToGM[43] = 43;  // Tom 3 -> GM High Floor Tom
    drumLockerToGM[45] = 45;  // Tom 2 -> GM Low Tom
    drumLockerToGM[47] = 47;  // Tom 2 -> GM Low-Mid Tom
    drumLockerToGM[48] = 48;  // Tom 1 -> GM Hi-Mid Tom

    // Cymbals
    drumLockerToGM[49] = 49;  // Crash L -> GM Crash
    drumLockerToGM[51] = 51;  // Ride -> GM Ride
    drumLockerToGM[52] = 49;  // China -> GM Crash
    drumLockerToGM[53] = 53;  // Ride Bell -> GM Ride Bell
    drumLockerToGM[54] = 49;  // Crash L Choke -> GM Crash
    drumLockerToGM[55] = 49;  // Splash -> GM Crash
    drumLockerToGM[57] = 49;  // Crash R -> GM Crash
    drumLockerToGM[58] = 49;  // Crash R Choke -> GM Crash

    // ==================== ML DRUMS MAPPING ====================
    // ML Drums = 16, GM = 2, so indices are 14 and 0 with -2 offset
    auto& mlDrumsToGM = mappings[14][0];

    // Drums
    mlDrumsToGM[36] = 36;  // Kick Drum
    mlDrumsToGM[38] = 38;  // Snare
    mlDrumsToGM[37] = 37;  // Snare Sidestick

    // Toms
    mlDrumsToGM[48] = 48;  // Rack Tom 1
    mlDrumsToGM[47] = 47;  // Rack Tom 2
    mlDrumsToGM[45] = 45;  // Rack Tom 3
    mlDrumsToGM[43] = 43;  // Floor Tom 1
    mlDrumsToGM[41] = 41;  // Floor Tom 2

    // Hi-hats (map all variations to GM equivalents)
    mlDrumsToGM[44] = 44;  // Hihat Pedal
    mlDrumsToGM[66] = 42;  // Hihat Closed Tip -> GM Closed
    mlDrumsToGM[42] = 42;  // Hihat Closed Edge
    mlDrumsToGM[67] = 42;  // Hihat Closed Edge Alt -> GM Closed
    mlDrumsToGM[68] = 46;  // Hihat Semi Open -> GM Open
    mlDrumsToGM[69] = 46;  // Hihat Semi Open 2 -> GM Open
    mlDrumsToGM[70] = 46;  // Hihat Semi Open 3 -> GM Open
    mlDrumsToGM[46] = 46;  // Hihat Open
    mlDrumsToGM[71] = 46;  // Hihat Open Alt -> GM Open

    // Ride
    mlDrumsToGM[52] = 51;  // Ride -> GM Ride
    mlDrumsToGM[53] = 53;  // Ride Bell
    mlDrumsToGM[60] = 51;  // Ride Edge -> GM Ride

    // Crashes
    mlDrumsToGM[49] = 49;  // Crash 1
    mlDrumsToGM[61] = 49;  // Crash 1 Choke -> Crash 1
    mlDrumsToGM[50] = 49;  // Crash 2 -> GM Crash
    mlDrumsToGM[62] = 49;  // Crash 2 Choke -> GM Crash
    mlDrumsToGM[51] = 57;  // Crash 3 -> GM Crash 2
    mlDrumsToGM[63] = 57;  // Crash 3 Choke -> GM Crash 2

    // Chinas
    mlDrumsToGM[55] = 52;  // China L -> GM Chinese
    mlDrumsToGM[64] = 52;  // China L Choke -> GM Chinese
    mlDrumsToGM[57] = 52;  // China R -> GM Chinese
    mlDrumsToGM[65] = 52;  // China R Choke -> GM Chinese

    // Splashes and special
    mlDrumsToGM[54] = 55;  // Splash 1 -> GM Splash
    mlDrumsToGM[56] = 55;  // Splash 2 -> GM Splash
    mlDrumsToGM[58] = 52;  // Stax -> GM Chinese
    mlDrumsToGM[59] = 52;  // Mini China -> GM Chinese
    mlDrumsToGM[72] = 53;  // Bell -> GM Ride Bell


    // ==================== SALAMANDER DRUMKIT COMPLETE MAPPING ====================
    // Salamander = 17, GM = 2, so indices are 15 and 0 with -2 offset
    auto& salamanderToGM = mappings[15][0];

    // Kicks
    salamanderToGM[35] = 35;  // Kick
    salamanderToGM[36] = 36;  // Kick2

    // Snares
    salamanderToGM[39] = 37;  // Snare OFF -> GM Side Stick
    salamanderToGM[40] = 38;  // Snare -> GM Snare

    // Hi-hats
    salamanderToGM[42] = 42;  // Hi-hat closed
    salamanderToGM[44] = 44;  // Hi-hat pedal
    salamanderToGM[46] = 46;  // Hi-hat open

    // Toms
    salamanderToGM[41] = 41;  // Tom 1 (floor tom low)
    salamanderToGM[43] = 43;  // Tom 2 (floor tom high)
    salamanderToGM[45] = 45;  // Tom 3 (low tom)
    salamanderToGM[48] = 48;  // Tom 4 (mid tom)
    salamanderToGM[50] = 50;  // Tom 5 (high tom)

    // Cowbell
    salamanderToGM[47] = 56;  // Cowbell -> GM Cowbell

    // Rides
    salamanderToGM[49] = 51;  // Ride1Crash -> GM Ride
    salamanderToGM[51] = 51;  // Ride2CrashChoke -> GM Ride
    salamanderToGM[52] = 51;  // Ride2 -> GM Ride

    // Crashes (main articulations)
    salamanderToGM[55] = 49;  // Crash1 -> GM Crash
    salamanderToGM[57] = 49;  // Crash2 -> GM Crash
    salamanderToGM[62] = 49;  // Crash3 -> GM Crash

    // Crash chokes
    salamanderToGM[54] = 49;  // Crash1 Choke -> GM Crash
    salamanderToGM[56] = 49;  // Crash2 Choke -> GM Crash

    // Chinas
    salamanderToGM[58] = 52;  // China1 Choke -> GM Chinese
    salamanderToGM[59] = 52;  // China1 -> GM Chinese
    salamanderToGM[60] = 52;  // China2 -> GM Chinese
    salamanderToGM[61] = 52;  // China2 Choke -> GM Chinese

    // Splash and special
    salamanderToGM[63] = 55;  // Splash1 -> GM Splash
    salamanderToGM[64] = 53;  // Bellchime -> GM Ride Bell

    // ==================== MULDJORDKIT3 COMPLETE MAPPING ====================
    // MuldjordKit3 = 18, GM = 2, so indices are 16 and 0 with -2 offset
    // Based on Midimap.xml from DrumGizmo format
    auto& muldjordToGM = mappings[16][0];

    // Kicks - MuldjordKit has separate L/R kicks
    muldjordToGM[35] = 35;  // KdrumL -> GM Acoustic Bass Drum
    muldjordToGM[36] = 36;  // KdrumR -> GM Bass Drum 1

    // Snares
    muldjordToGM[37] = 37;  // SnareRest (Cross-stick) -> GM Side Stick
    muldjordToGM[38] = 38;  // Snare -> GM Acoustic Snare

    // Hi-hats
    muldjordToGM[42] = 42;  // HihatClosed -> GM Closed Hi-Hat
    muldjordToGM[46] = 46;  // HihatOpen -> GM Open Hi-Hat

    // Toms (MuldjordKit: Tom1=highest, Tom4=lowest/floor)
    muldjordToGM[41] = 41;  // Tom4 (Floor Tom) -> GM Low Floor Tom
    muldjordToGM[45] = 45;  // Tom3 -> GM Low Tom
    muldjordToGM[47] = 47;  // Tom2 -> GM Low-Mid Tom
    muldjordToGM[48] = 48;  // Tom1 -> GM Hi-Mid Tom

    // Crashes
    muldjordToGM[49] = 49;  // CrashL -> GM Crash Cymbal 1
    muldjordToGM[57] = 57;  // CrashR -> GM Crash Cymbal 2

    // Rides
    muldjordToGM[51] = 51;  // RideR -> GM Ride Cymbal 1
    muldjordToGM[59] = 59;  // RideL -> GM Ride Cymbal 2

    // Ride Bells
    muldjordToGM[53] = 53;  // RideRBell -> GM Ride Bell
    muldjordToGM[55] = 53;  // RideLBell -> GM Ride Bell (both bells to same GM note)

    // China
    muldjordToGM[52] = 52;  // China -> GM Chinese Cymbal

    // ==================== AASIMONSTER2 MAPPING ====================
    // Aasimonster2 = 19, GM = 2, so indices are 17 and 0 with -2 offset
    auto& aasimonsterToGM = mappings[17][0];

    // Kicks (Aasimonster has separate kick_l and kick_r)
    aasimonsterToGM[35] = 36;  // kick_r -> GM Kick
    aasimonsterToGM[36] = 36;  // kick_l -> GM Kick

    // Snares
    aasimonsterToGM[38] = 38;  // snare_on_center -> GM Snare
    aasimonsterToGM[40] = 40;  // snare_off_center -> GM Electric Snare
    aasimonsterToGM[37] = 37;  // snare_rim_shot -> GM Side Stick

    // Hi-hats
    aasimonsterToGM[42] = 42;  // hihat_closed1 -> GM Closed Hi-Hat
    aasimonsterToGM[46] = 46;  // hihat_open -> GM Open Hi-Hat

    // Toms
    aasimonsterToGM[43] = 43;  // tom_4 (floor) -> GM High Floor Tom
    aasimonsterToGM[45] = 45;  // tom_3 -> GM Low Tom
    aasimonsterToGM[47] = 47;  // tom_2 -> GM Low-Mid Tom
    aasimonsterToGM[48] = 48;  // tom_1 -> GM Hi-Mid Tom

    // Crashes
    aasimonsterToGM[49] = 49;  // crash1 -> GM Crash Cymbal 1
    aasimonsterToGM[50] = 49;  // crash1_stop -> GM Crash Cymbal 1
    aasimonsterToGM[57] = 57;  // crash2 -> GM Crash Cymbal 2
    aasimonsterToGM[58] = 57;  // crash2_stop -> GM Crash Cymbal 2

    // Rides
    aasimonsterToGM[51] = 51;  // ride -> GM Ride Cymbal 1
    aasimonsterToGM[53] = 53;  // ride_bell2 -> GM Ride Bell

    // China & Zilbel
    aasimonsterToGM[52] = 52;  // china_18_inch -> GM Chinese Cymbal
    aasimonsterToGM[55] = 80;  // zilbel -> GM Mute Triangle


    // ==================== CREATE REVERSE MAPPINGS (GM -> Libraries) ====================
    // Create reverse mapping for EVERY note with "first wins" priority

    DBG("Creating reverse mappings (GM -> Libraries)...");

    for (int libIdx = 0; libIdx < 17; ++libIdx)
    {
        // Skip if this library has no mappings TO GM
        if (mappings.find(libIdx) == mappings.end() ||
            mappings[libIdx].find(0) == mappings[libIdx].end())
            continue;

        auto& libraryToGM = mappings[libIdx][0];
        auto& gmToLibrary = mappings[0][libIdx];

        int reverseMappingsCreated = 0;

        // First pass: Standard notes (35-52) - FIRST WINS
        for (const auto& notePair : libraryToGM)
        {
            uint8_t libraryNote = notePair.first;
            uint8_t gmNote = notePair.second;

            if (libraryNote >= 35 && libraryNote <= 52)
            {
                if (gmToLibrary.find(gmNote) == gmToLibrary.end())
                {
                    gmToLibrary[gmNote] = libraryNote;
                    reverseMappingsCreated++;
                }
            }
        }

        // Second pass: Extended notes (53-64)
        for (const auto& notePair : libraryToGM)
        {
            uint8_t libraryNote = notePair.first;
            uint8_t gmNote = notePair.second;

            if (libraryNote >= 53 && libraryNote <= 64)
            {
                if (gmToLibrary.find(gmNote) == gmToLibrary.end())
                {
                    gmToLibrary[gmNote] = libraryNote;
                    reverseMappingsCreated++;
                }
            }
        }

        // Third pass: Everything else
        for (const auto& notePair : libraryToGM)
        {
            uint8_t libraryNote = notePair.first;
            uint8_t gmNote = notePair.second;

            if (libraryNote < 35 || libraryNote > 64)
            {
                if (gmToLibrary.find(gmNote) == gmToLibrary.end())
                {
                    gmToLibrary[gmNote] = libraryNote;
                    reverseMappingsCreated++;
                }
            }
        }

        int enumValue = libIdx + 2;
        DrumLibrary lib = static_cast<DrumLibrary>(enumValue);
        juce::String libName = getLibraryName(lib);

        DBG("  " + libName + ": created " + juce::String(reverseMappingsCreated) + " reverse mappings");
    }

    // ==================== POST-PROCESS CRITICAL FIXES ====================
    // Fix identity mappings that conflict with actual instrument assignments

    // Salamander: GM 47 (tom) must NOT map to Salamander 47 (cowbell)
    mappings[0][15][47] = 48;  // GM Tom 47 -> Salamander Tom 48

    // Salamander: GM 56 (cowbell) must map to Salamander 47 (cowbell)
    mappings[0][15][56] = 47;  // GM Cowbell 56 -> Salamander Cowbell 47
}

void DrumLibraryManager::initializeHardcodedMappings()
{
    // ONLY initialize hardcoded special mappings (Ugritone, etc.)
    // Do NOT create identity mappings for all libraries
    // This is used when loading from file to preserve hardcoded mappings

    // Initialize GM to itself
    for (uint8_t note = 0; note < 128; ++note)
    {
        mappings[0][0][note] = note;
    }

    // ==================== UGRITONE COMPLETE MAPPING ====================
    // Ugritone = 8, so index is 6 with -2 offset
    auto& ugritoneToGM = mappings[6][0];

    // Kicks
    ugritoneToGM[35] = 36;
    ugritoneToGM[36] = 36;

    // Snares
    ugritoneToGM[37] = 38;
    ugritoneToGM[38] = 38;
    ugritoneToGM[40] = 38;

    // Hi-hats
    ugritoneToGM[22] = 42;
    ugritoneToGM[26] = 46;
    ugritoneToGM[42] = 42;
    ugritoneToGM[44] = 42;
    ugritoneToGM[46] = 46;

    // Toms
    ugritoneToGM[41] = 41;
    ugritoneToGM[43] = 43;
    ugritoneToGM[45] = 45;
    ugritoneToGM[47] = 47;
    ugritoneToGM[48] = 48;
    ugritoneToGM[50] = 50;

    // Cymbals
    ugritoneToGM[49] = 49;
    ugritoneToGM[51] = 51;
    ugritoneToGM[52] = 49;
    ugritoneToGM[53] = 51;
    ugritoneToGM[55] = 49;
    ugritoneToGM[57] = 49;
    ugritoneToGM[59] = 51;

    // Percussion
    ugritoneToGM[39] = 39;
    ugritoneToGM[54] = 54;
    ugritoneToGM[56] = 50;  // High Tom -> GM High Tom

    // Reverse mapping (Ugritone from GM)
    auto& gmToUgritone = mappings[0][6];
    for (const auto& pair : ugritoneToGM)
    {
        gmToUgritone[pair.second] = pair.first;
    }

    // ==================== SALAMANDER COMPLETE TARGET MAPPING ====================
    // Salamander = 17, so index is 15 with -2 offset
    // This maps GM notes to Salamander notes when Salamander is the TARGET
    auto& gmToSalamander = mappings[0][15];

    // Kicks - direct mapping
    gmToSalamander[35] = 35;  // GM Acoustic Bass Drum -> Salamander Kick
    gmToSalamander[36] = 36;  // GM Bass Drum 1 -> Salamander Kick2

    // Snares
    gmToSalamander[37] = 37;  // GM Side Stick -> Salamander snare2OFF (cross-stick)
    gmToSalamander[38] = 38;  // GM Acoustic Snare -> Salamander snare2
    gmToSalamander[40] = 40;  // GM Electric Snare -> Salamander snare (main)

    // Hi-hats
    gmToSalamander[42] = 42;  // GM Closed Hi-Hat -> Salamander hihatClosed
    gmToSalamander[44] = 44;  // GM Pedal Hi-Hat -> Salamander hihatFoot
    gmToSalamander[46] = 46;  // GM Open Hi-Hat -> Salamander hihatOpen

    // Toms - Salamander has limited toms, map appropriately
    gmToSalamander[41] = 41;  // GM Low Floor Tom -> Salamander (if exists, else pass through)
    gmToSalamander[43] = 43;  // GM High Floor Tom -> Salamander loTom
    gmToSalamander[45] = 45;  // GM Low Tom -> Salamander hiTom
    gmToSalamander[47] = 48;  // GM Low-Mid Tom -> Salamander (mapped to avoid cowbell conflict)
    gmToSalamander[48] = 48;  // GM Hi-Mid Tom -> Salamander (pass through)
    gmToSalamander[50] = 50;  // GM High Tom -> Salamander (pass through)

    // Crashes - GM crashes map to Salamander crash positions
    gmToSalamander[49] = 55;  // GM Crash Cymbal 1 -> Salamander crash1
    gmToSalamander[57] = 57;  // GM Crash Cymbal 2 -> Salamander crash2

    // Rides
    gmToSalamander[51] = 52;  // GM Ride Cymbal 1 -> Salamander ride1
    gmToSalamander[53] = 53;  // GM Ride Bell -> Salamander ride1Bell
    gmToSalamander[59] = 48;  // GM Ride Cymbal 2 -> Salamander ride2

    // Other cymbals
    gmToSalamander[52] = 59;  // GM Chinese Cymbal -> Salamander china1
    gmToSalamander[55] = 63;  // GM Splash Cymbal -> Salamander splash1

    // Percussion
    gmToSalamander[56] = 47;  // GM Cowbell -> Salamander cowbell

    DBG("Initialized GM -> Salamander target mappings");
}

uint8_t DrumLibraryManager::mapNoteToLibrary(uint8_t note, DrumLibrary sourceLibrary, DrumLibrary targetLibrary) const
{
    // CRITICAL: Bypass mode - return note unchanged (no remapping at all)
    if (targetLibrary == DrumLibrary::Bypass)
        return note;

    // Same library - no remapping needed
    // This prevents unnecessary double-mapping (source->GM->target) which can introduce errors
    if (sourceLibrary == targetLibrary)
        return note;

    // Calculate adjusted indices (subtract 2 because Unknown=0, Bypass=1, first mappable is GeneralMIDI=2)
    int sourceIdx = static_cast<int>(sourceLibrary) - 2;
    int targetIdx = static_cast<int>(targetLibrary) - 2;


    // Bounds check - with MuldjordKit3=18, max index is 16 (18-2=16)
    if (sourceIdx < 0 || sourceIdx >= 17 || targetIdx < 0 || targetIdx >= 17)
        return note; // Return unmapped if out of bounds

        // First map source -> GM
        uint8_t gmNote = note;
    if (mappings.find(sourceIdx) != mappings.end() &&
        mappings.at(sourceIdx).find(0) != mappings.at(sourceIdx).end())
    {
        const auto& sourceToGM = mappings.at(sourceIdx).at(0);
        if (sourceToGM.find(note) != sourceToGM.end())
            gmNote = sourceToGM.at(note);
    }

    // Then map GM -> target
    uint8_t targetNote = gmNote;
    if (mappings.find(0) != mappings.end() &&
        mappings.at(0).find(targetIdx) != mappings.at(0).end())
    {
        const auto& gmToTarget = mappings.at(0).at(targetIdx);
        if (gmToTarget.find(gmNote) != gmToTarget.end())
            targetNote = gmToTarget.at(gmNote);
    }

    return targetNote;
}

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
    return "";
}

DrumLibrary DrumLibraryManager::getRootFolderSourceLibrary(int index) const
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
        return rootFolders[index].sourceLibrary;
    return DrumLibrary::Unknown;
}

bool DrumLibraryManager::isRootFolderWritable(int index) const
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
        return rootFolders[index].isWritable;
    return false;  // Default to read-only if invalid index
}

void DrumLibraryManager::setRootFolderWritable(int index, bool writable)
{
    if (index >= 0 && index < static_cast<int>(rootFolders.size()))
    {
        rootFolders[index].isWritable = writable;
        saveConfiguration();  // Save immediately when changed
    }
}

bool DrumLibraryManager::isFolderAlreadyAdded(const juce::File& folder) const
{
    for (const auto& folderInfo : rootFolders)
    {
        if (folderInfo.folder.getFullPathName() == folder.getFullPathName())
        {
            return true;
        }
    }
    return false;
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

juce::File DrumLibraryManager::getOriginLibrariesFile() const
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File pluginDir = appDataDir.getChildFile("DrumGroovePro");

    if (!pluginDir.exists())
        pluginDir.createDirectory();

    return pluginDir.getChildFile("OriginLibraries.xml");
}

juce::File DrumLibraryManager::getOriginMappingsFile() const
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File pluginDir = appDataDir.getChildFile("DrumGroovePro");

    if (!pluginDir.exists())
        pluginDir.createDirectory();

    return pluginDir.getChildFile("OriginLibraryMappings.xml");
}

void DrumLibraryManager::loadConfiguration()
{
    juce::File configFile = getConfigFile();

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
                    info.isWritable = folderElement->getBoolAttribute("isWritable", false);
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

    // NOTE: Origin libraries are now loaded from a separate file (OriginLibraries.xml)
    // See loadOriginLibraries() function

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
        folderElement->setAttribute("isWritable", folderInfo.isWritable);
    }

    // Save last selected target library
    config->setAttribute("lastSelectedTargetLibrary", static_cast<int>(lastSelectedTargetLibrary));

    // NOTE: Origin libraries are now saved in a separate file (OriginLibraries.xml)
    // See saveOriginLibraries() function

    // Ensure directory exists
    juce::File configFile = getConfigFile();
    configFile.getParentDirectory().createDirectory();

    if (config->writeToFile(configFile, ""))
    {
        DBG("Configuration saved successfully to: " + configFile.getFullPathName());
    }
    else
    {
        DBG("ERROR: Failed to save configuration");
    }
}

juce::StringArray DrumLibraryManager::getLoadedLibraryNames()
{
    juce::StringArray names;

    // ALWAYS add Bypass first (it's a special pass-through mode, always available)
    names.add(getLibraryName(DrumLibrary::Bypass));

    // ALWAYS add General MIDI second (it's always available)
    names.add(getLibraryName(DrumLibrary::GeneralMIDI));

    // Collect other built-in libraries that have mappings loaded
    juce::StringArray otherLibraries;

    // Check built-in libraries (indices 0-16 = enums 2-18)
    // Skip index 0 (General MIDI) since we already added it
    for (int libIdx = 1; libIdx < 18; ++libIdx)
    {
        // Check if this library has any mappings
        bool hasMapping = false;

        // Check if there are mappings from this library to GM (libIdx -> 0)
        if (mappings.find(libIdx) != mappings.end() &&
            mappings[libIdx].find(0) != mappings[libIdx].end() &&
            !mappings[libIdx][0].empty())
        {
            hasMapping = true;
        }

        // Check if there are mappings from GM to this library (0 -> libIdx)
        if (mappings.find(0) != mappings.end() &&
            mappings[0].find(libIdx) != mappings[0].end() &&
            !mappings[0][libIdx].empty())
        {
            hasMapping = true;
        }

        if (hasMapping)
        {
            int enumValue = libIdx + 2;
            DrumLibrary lib = static_cast<DrumLibrary>(enumValue);
            juce::String name = getLibraryName(lib);

            // Don't add duplicates
            if (!otherLibraries.contains(name))
                otherLibraries.add(name);
        }
    }

    // Sort other libraries alphabetically
    otherLibraries.sortNatural();

    // Add sorted libraries to main list
    names.addArray(otherLibraries);

    // Add all custom libraries (enum values 1000+), also sorted
    juce::StringArray customNames;
    for (const auto& custom : customLibraries)
    {
        if (!names.contains(custom.name))
        {
            customNames.add(custom.name);
            DBG("Added custom library to loaded list: " + custom.name + " (enum=" + juce::String(custom.enumValue) + ")");
        }
    }
    customNames.sortNatural();
    names.addArray(customNames);

    DBG("Loaded libraries for dropdown:");
    for (const auto& name : names)
        DBG("  - " + name);

    return names;
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
        case DrumLibrary::EZdrummer: return "EZdrummer";
        case DrumLibrary::GetGoodDrums: return "GetGood Drums";
        case DrumLibrary::StevenSlateDrums: return "Steven Slate Drums";
        case DrumLibrary::Ugritone: return "Ugritone";
        case DrumLibrary::BFD3: return "BFD3";
        case DrumLibrary::MTPowerDrumKit2: return "MT Power Drum Kit 2";
        case DrumLibrary::Sitala: return "Sitala";
        case DrumLibrary::ShreddageDrums: return "Shreddage Drums";
        case DrumLibrary::Damage2: return "Damage 2";
        case DrumLibrary::MODODrum: return "MODO Drum";
        case DrumLibrary::DrumLocker: return "Drum Locker";
        case DrumLibrary::MLDrums: return "ML Drums";
        case DrumLibrary::SalamanderDrumkit: return "Salamander Drumkit";
        case DrumLibrary::MuldjordKit3: return "MuldjordKit";
        case DrumLibrary::Aasimonster2: return "The Aasimonster";
        default: return "Unknown";
    }
}

bool DrumLibraryManager::hasValidMapping(DrumPartType part, DrumLibrary library) const
{
    // Get the origin note number for this part
    int originNote = getNoteNumberForDrumPart(part, library);

    if (originNote <= 0 || originNote >= 128)
        return false;

    // For General MIDI, all standard parts are valid
    if (library == DrumLibrary::GeneralMIDI)
        return true;

    // For origin libraries, check if this note has a valid description
    int libIndex = static_cast<int>(library) - 2;

    // First, find what GM note this origin note maps to
    uint8_t gmNote = originNote;  // Default
    if (libIndex >= 0 && mappings.find(libIndex) != mappings.end())
    {
        const auto& libMappings = mappings.at(libIndex);
        if (libMappings.find(0) != libMappings.end())  // 0 = GM
        {
            const auto& originToGM = libMappings.at(0);
            if (originToGM.find(originNote) != originToGM.end())
            {
                gmNote = originToGM.at(originNote);
            }
        }
    }

    // Check if there's a custom description for this GM note
    if (libIndex >= 0 && customDrumNames.find(libIndex) != customDrumNames.end())
    {
        const auto& libCustomNames = customDrumNames.at(libIndex);
        if (libCustomNames.find(gmNote) != libCustomNames.end())
        {
            juce::String customName = libCustomNames.at(gmNote);
            if (customName.isNotEmpty())
                return true;  // Has custom description
        }
    }

    // No custom name, check if GM has a name for this note
    juce::String gmName = getGMDrumName(gmNote);
    return gmName.isNotEmpty();  // Valid if GM recognizes it
}

juce::String DrumLibraryManager::getLibraryNameIncludingCustom(DrumLibrary library) const
{
    juce::String name = getLibraryName(library);

    if (name == "Unknown")
    {
        int enumValue = static_cast<int>(library);
        if (enumValue >= 1000)
        {
            for (const auto& custom : customLibraries)
            {
                if (custom.enumValue == enumValue)
                    return custom.name;
            }
        }
    }

    return name;
}



juce::String DrumLibraryManager::getGMDrumName(uint8_t note)
{
    // Standard GM drum kit names
    static const std::map<uint8_t, juce::String> gmDrumNames = {
        {35, "Acoustic Bass Drum"}, {36, "Bass Drum 1"}, {37, "Side Stick"}, {38, "Acoustic Snare"},
        {39, "Hand Clap"}, {40, "Electric Snare"}, {41, "Low Floor Tom"}, {42, "Closed Hi-Hat"},
        {43, "High Floor Tom"}, {44, "Pedal Hi-Hat"}, {45, "Low Tom"}, {46, "Open Hi-Hat"},
        {47, "Low-Mid Tom"}, {48, "Hi-Mid Tom"}, {49, "Crash Cymbal 1"}, {50, "High Tom"},
        {51, "Ride Cymbal 1"}, {52, "Chinese Cymbal"}, {53, "Ride Bell"}, {54, "Tambourine"},
        {55, "Splash Cymbal"}, {56, "Cowbell"}, {57, "Crash Cymbal 2"}, {58, "Vibraslap"},
        {59, "Ride Cymbal 2"}, {60, "Hi Bongo"}, {61, "Low Bongo"}, {62, "Mute Hi Conga"},
        {63, "Open Hi Conga"}, {64, "Low Conga"}, {65, "High Timbale"}, {66, "Low Timbale"},
        {67, "High Agogo"}, {68, "Low Agogo"}, {69, "Cabasa"}, {70, "Maracas"},
        {71, "Short Whistle"}, {72, "Long Whistle"}, {73, "Short Guiro"}, {74, "Long Guiro"},
        {75, "Claves"}, {76, "Hi Wood Block"}, {77, "Low Wood Block"}, {78, "Mute Cuica"},
        {79, "Open Cuica"}, {80, "Mute Triangle"}, {81, "Open Triangle"}
    };

    auto it = gmDrumNames.find(note);
    return it != gmDrumNames.end() ? it->second : "";
}

juce::StringArray DrumLibraryManager::getAllLibraryNames()
{
    juce::StringArray names;

    // Add Bypass first (special case - always at top)
    names.add(getLibraryName(DrumLibrary::Bypass));

    // Add General MIDI second
    names.add(getLibraryName(DrumLibrary::GeneralMIDI));

    // Collect other built-in libraries
    juce::StringArray otherLibraries;
    otherLibraries.add(getLibraryName(DrumLibrary::Aasimonster2));
    otherLibraries.add(getLibraryName(DrumLibrary::AddictiveDrums2));
    otherLibraries.add(getLibraryName(DrumLibrary::BFD3));
    otherLibraries.add(getLibraryName(DrumLibrary::Damage2));
    otherLibraries.add(getLibraryName(DrumLibrary::DrumLocker));
    otherLibraries.add(getLibraryName(DrumLibrary::EZdrummer));
    otherLibraries.add(getLibraryName(DrumLibrary::GetGoodDrums));
    otherLibraries.add(getLibraryName(DrumLibrary::MLDrums));
    otherLibraries.add(getLibraryName(DrumLibrary::MODODrum));
    otherLibraries.add(getLibraryName(DrumLibrary::MTPowerDrumKit2));
    otherLibraries.add(getLibraryName(DrumLibrary::MuldjordKit3));
    otherLibraries.add(getLibraryName(DrumLibrary::SalamanderDrumkit));
    otherLibraries.add(getLibraryName(DrumLibrary::ShreddageDrums));
    otherLibraries.add(getLibraryName(DrumLibrary::Sitala));
    otherLibraries.add(getLibraryName(DrumLibrary::StevenSlateDrums));
    otherLibraries.add(getLibraryName(DrumLibrary::SuperiorDrummer3));
    otherLibraries.add(getLibraryName(DrumLibrary::Ugritone));

    // Sort other libraries alphabetically
    otherLibraries.sortNatural();

    // Add sorted libraries to main list
    names.addArray(otherLibraries);

    // Add all custom libraries (also sorted)
    juce::StringArray customNames;
    for (const auto& custom : customLibraries)
    {
        customNames.add(custom.name);
    }
    customNames.sortNatural();
    names.addArray(customNames);

    return names;
}

juce::StringArray DrumLibraryManager::getAllSourceLibraryNames()
{
    // Return user-configured origin libraries, always sorted alphabetically
    if (configuredOriginLibraries.isEmpty())
    {
        // Return defaults in alphabetical order
        juce::StringArray defaults;
        defaults.add("General MIDI");
        defaults.add("Unknown");
        return defaults;
    }

    // Create a copy and sort it
    juce::StringArray sortedOrigins = configuredOriginLibraries;
    sortedOrigins.sortNatural();

    return sortedOrigins;
}

DrumLibrary DrumLibraryManager::getLibraryFromName(const juce::String& name)
{
    // Map library names back to enum values
    if (name == "General MIDI") return DrumLibrary::GeneralMIDI;
    if (name == "Bypass (No Remapping)") return DrumLibrary::Bypass;
    if (name == "Superior Drummer 3") return DrumLibrary::SuperiorDrummer3;
    if (name == "Addictive Drums 2") return DrumLibrary::AddictiveDrums2;
    if (name == "EZdrummer") return DrumLibrary::EZdrummer;
    if (name == "GetGood Drums") return DrumLibrary::GetGoodDrums;
    if (name == "Steven Slate Drums") return DrumLibrary::StevenSlateDrums;
    if (name == "Ugritone") return DrumLibrary::Ugritone;
    if (name == "BFD3") return DrumLibrary::BFD3;
    if (name == "MT Power Drum Kit 2") return DrumLibrary::MTPowerDrumKit2;
    if (name == "Sitala") return DrumLibrary::Sitala;
    if (name == "Shreddage Drums") return DrumLibrary::ShreddageDrums;
    if (name == "Damage 2") return DrumLibrary::Damage2;
    if (name == "MODO Drum") return DrumLibrary::MODODrum;
    if (name == "Drum Locker") return DrumLibrary::DrumLocker;
    if (name == "ML Drums") return DrumLibrary::MLDrums;
    if (name == "Salamander Drumkit") return DrumLibrary::SalamanderDrumkit;
    if (name == "MuldjordKit") return DrumLibrary::MuldjordKit3;
    if (name == "The Aasimonster") return DrumLibrary::Aasimonster2;

    // Check custom libraries
    for (const auto& custom : customLibraries)
    {
        if (custom.name == name)
            return static_cast<DrumLibrary>(custom.enumValue);
    }

    return DrumLibrary::Unknown;
}

void DrumLibraryManager::setLastSelectedTargetLibrary(DrumLibrary library)
{
    lastSelectedTargetLibrary = library;
    saveConfiguration();
}

DrumLibrary DrumLibraryManager::getLastSelectedTargetLibrary() const
{
    return lastSelectedTargetLibrary;
}

juce::File DrumLibraryManager::getCustomMappingsFile() const
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File pluginDir = appDataDir.getChildFile("DrumGroovePro");

    if (!pluginDir.exists())
        pluginDir.createDirectory();

    juce::File newFile = pluginDir.getChildFile("TargetDrumMapping.xml");
    juce::File oldFile = pluginDir.getChildFile("CustomDrumMappings.xml");

    // MIGRATION: If old file exists and new file doesn't, rename it
    if (oldFile.existsAsFile() && !newFile.existsAsFile())
    {
        DBG("Migrating old file: " + oldFile.getFullPathName() + " to " + newFile.getFullPathName());
        if (oldFile.moveFileTo(newFile))
        {
            DBG("Successfully migrated to new filename");
        }
        else
        {
            DBG("ERROR: Failed to migrate file - will use old filename");
            return oldFile;
        }
    }

    return newFile;
}

void DrumLibraryManager::addCustomLibrary(const juce::String& name)
{
    if (name.isEmpty())
        return;

    // Check if library already exists
    for (const auto& custom : customLibraries)
    {
        if (custom.name == name)
            return; // Already exists
    }

    CustomLibrary newLib;
    newLib.name = name;
    newLib.enumValue = nextCustomEnumValue++;

    customLibraries.push_back(newLib);

    DBG("Added custom library: " + name + " (enum=" + juce::String(newLib.enumValue) + ")");
    // Initialize the new library with GM identity mapping
    DrumLibrary newLibEnum = static_cast<DrumLibrary>(newLib.enumValue);
    initializeTargetMappingWithGM(newLibEnum);
    saveCustomMappings();
}

void DrumLibraryManager::removeCustomLibrary(DrumLibrary library)
{
    int enumValue = static_cast<int>(library);

    for (auto it = customLibraries.begin(); it != customLibraries.end(); ++it)
    {
        if (it->enumValue == enumValue)
        {
            DBG("Removing custom library: " + it->name);
            customLibraries.erase(it);
            saveCustomMappings();
            return;
        }
    }
}

void DrumLibraryManager::updateLibraryMapping(DrumLibrary library, uint8_t gmNote, uint8_t productNote)
{
    int libIndex = static_cast<int>(library) - 2;

    // Store both directions
    mappings[0][libIndex][gmNote] = productNote;  // GM -> Library
    mappings[libIndex][0][productNote] = gmNote;  // Library -> GM

    DBG("=== updateLibraryMapping ===");
    DBG("Library: " + getLibraryName(library) + " (libIndex=" + juce::String(libIndex) + ")");
    DBG("GM Note: " + juce::String(gmNote) + ", Product/Origin Note: " + juce::String(productNote));
    DBG("Stored: mappings[0][" + juce::String(libIndex) + "][" + juce::String(gmNote) + "] = " + juce::String(productNote));
    DBG("Stored: mappings[" + juce::String(libIndex) + "][0][" + juce::String(productNote) + "] = " + juce::String(gmNote));
}

void DrumLibraryManager::saveCustomMappings()
{
    juce::File mappingFile = getCustomMappingsFile();

    DBG("Saving target drum mappings to: " + mappingFile.getFullPathName());

    juce::XmlElement root("DrumLibraryMappings");

    // Save all built-in libraries
    auto allLibraries = getAllLibraryNames();

    for (const auto& libraryName : allLibraries)
    {
        if (libraryName == "Bypass (No Remapping)" || libraryName == "Unknown")
            continue;

        DrumLibrary library = getLibraryFromName(libraryName);
        int libIndex = static_cast<int>(library) - 2;

        auto* libElement = root.createNewChildElement("Library");
        libElement->setAttribute("name", libraryName);
        libElement->setAttribute("enum", static_cast<int>(library));

        // Save all GM -> Library mappings for this library
        if (mappings.find(0) != mappings.end() &&
            mappings[0].find(libIndex) != mappings[0].end())
        {
            const auto& gmToLibrary = mappings[0][libIndex];

            for (const auto& notePair : gmToLibrary)
            {
                auto* noteElement = libElement->createNewChildElement("NoteMap");
                noteElement->setAttribute("gmNote", notePair.first);
                noteElement->setAttribute("productNote", notePair.second);

                // Save custom drum name if exists
                if (hasCustomDrumName(library, notePair.first))
                {
                    noteElement->setAttribute("customName", getCustomDrumName(library, notePair.first));
                }
            }
        }
    }

    // Save custom libraries
    for (const auto& custom : customLibraries)
    {
        auto* libElement = root.createNewChildElement("CustomLibrary");
        libElement->setAttribute("name", custom.name);
        libElement->setAttribute("enum", custom.enumValue);

        // Save mappings for this custom library
        int libIndex = custom.enumValue - 2;

        if (mappings.find(0) != mappings.end() &&
            mappings[0].find(libIndex) != mappings[0].end())
        {
            const auto& gmToLibrary = mappings[0][libIndex];

            for (const auto& notePair : gmToLibrary)
            {
                auto* noteElement = libElement->createNewChildElement("NoteMap");
                noteElement->setAttribute("gmNote", notePair.first);
                noteElement->setAttribute("productNote", notePair.second);

                // Save custom drum name if exists
                DrumLibrary customLib = static_cast<DrumLibrary>(custom.enumValue);
                if (hasCustomDrumName(customLib, notePair.first))
                {
                    noteElement->setAttribute("customName", getCustomDrumName(customLib, notePair.first));
                }
            }
        }
    }

    // Ensure directory exists
    mappingFile.getParentDirectory().createDirectory();

    if (root.writeToFile(mappingFile, ""))
    {
        DBG("Target drum mappings saved successfully");
    }
    else
    {
        DBG("ERROR: Failed to save target drum mappings");
    }
}

void DrumLibraryManager::loadCustomMappings()
{
    juce::File mappingFile = getCustomMappingsFile();

    if (!mappingFile.existsAsFile())
    {
        DBG("No target drum mapping file found at: " + mappingFile.getFullPathName());
        return;
    }

    auto root = juce::XmlDocument::parse(mappingFile);

    if (!root || !root->hasTagName("DrumLibraryMappings"))
    {
        DBG("Invalid target drum mapping file format");
        return;
    }

    DBG("Loading target drum mappings from: " + mappingFile.getFullPathName());

    // Clear existing mappings to avoid mixing old and new
    mappings.clear();

    // Initialize GM to itself (always needed)
    for (uint8_t note = 0; note < 128; ++note)
    {
        mappings[0][0][note] = note;
    }

    int librariesLoaded = 0;
    int totalMappingsLoaded = 0;

    // Load built-in library mappings
    for (auto* libElement : root->getChildIterator())
    {
        if (libElement->hasTagName("Library"))
        {
            juce::String libraryName = libElement->getStringAttribute("name");
            DrumLibrary library = getLibraryFromName(libraryName);

            if (library == DrumLibrary::Unknown)
            {
                DBG("  Skipping unknown library: " + libraryName);
                continue;
            }

            int libIndex = static_cast<int>(library) - 2;
            int mappingCount = 0;

            for (auto* noteElement : libElement->getChildIterator())
            {
                if (noteElement->hasTagName("NoteMap"))
                {
                    uint8_t gmNote = static_cast<uint8_t>(noteElement->getIntAttribute("gmNote"));
                    uint8_t productNote = static_cast<uint8_t>(noteElement->getIntAttribute("productNote"));

                    // FIXED: Only store GM -> Library mapping for TARGET libraries
                    // DO NOT create reverse mapping here - that corrupts origin mappings!
                    // Origin mappings (Library -> GM) are handled by initializeHardcodedMappings()
                    mappings[0][libIndex][gmNote] = productNote;  // GM -> Library (TARGET only)

                    // Load custom drum name if present
                    juce::String customName = noteElement->getStringAttribute("customName");
                    if (customName.isNotEmpty())
                    {
                        customDrumNames[libIndex][gmNote] = customName;
                    }

                    mappingCount++;
                    totalMappingsLoaded++;
                }
            }

            librariesLoaded++;
            DBG("  Loaded " + juce::String(mappingCount) + " mappings for " + libraryName);
        }
        else if (libElement->hasTagName("CustomLibrary"))
        {
            // Load custom library
            juce::String name = libElement->getStringAttribute("name");
            int enumValue = libElement->getIntAttribute("enum");

            CustomLibrary custom;
            custom.name = name;
            custom.enumValue = enumValue;
            customLibraries.push_back(custom);

            if (enumValue >= nextCustomEnumValue)
                nextCustomEnumValue = enumValue + 1;

            int libIndex = enumValue - 2;
            int mappingCount = 0;

            for (auto* noteElement : libElement->getChildIterator())
            {
                if (noteElement->hasTagName("NoteMap"))
                {
                    uint8_t gmNote = static_cast<uint8_t>(noteElement->getIntAttribute("gmNote"));
                    uint8_t productNote = static_cast<uint8_t>(noteElement->getIntAttribute("productNote"));

                    // FIXED: Only store GM -> Library mapping for custom TARGET libraries
                    mappings[0][libIndex][gmNote] = productNote;

                    // Load custom drum name if present
                    juce::String customName = noteElement->getStringAttribute("customName");
                    if (customName.isNotEmpty())
                    {
                        customDrumNames[libIndex][gmNote] = customName;
                    }

                    mappingCount++;
                    totalMappingsLoaded++;
                }
            }

            DBG("  Loaded custom library: " + name + " with " + juce::String(mappingCount) + " mappings");
        }
    }

    DBG("Loaded " + juce::String(librariesLoaded) + " libraries with " +
    juce::String(totalMappingsLoaded) + " total mappings");

    // IMPORTANT: Apply hardcoded special mappings AFTER loading from file
    // This ensures Ugritone, Salamander and other special ORIGIN mappings are always present
    initializeHardcodedMappings();
}

bool DrumLibraryManager::hasCustomMappings() const
{
    return !customLibraries.empty();
}

void DrumLibraryManager::removeLibraryFromMappings(DrumLibrary library)
{
    int libIndex = static_cast<int>(library) - 2;

    // Remove from GM -> Library mappings
    if (mappings.find(0) != mappings.end())
    {
        mappings[0].erase(libIndex);
    }

    // Remove from Library -> GM mappings
    mappings.erase(libIndex);

    DBG("Removed all mappings for library index " + juce::String(libIndex));

    saveCustomMappings();
}

void DrumLibraryManager::addOriginLibrary(const juce::String& name)
{
    if (name.isEmpty())
        return;

    // Don't allow duplicates
    if (configuredOriginLibraries.contains(name))
    {
        DBG("Origin library already exists: " + name);
        return;
    }

    configuredOriginLibraries.add(name);
    configuredOriginLibraries.sortNatural();

    DBG("Added origin library: " + name);

    // Check if this is a new custom origin library (not a built-in one)
    DrumLibrary lib = getLibraryFromName(name);
    if (lib == DrumLibrary::Unknown)
    {
        // This is a custom origin library - add to customLibraries too
        addCustomLibrary(name);
    }

    saveOriginLibraries();
}

void DrumLibraryManager::removeOriginLibrary(const juce::String& name)
{
    // Protect essential libraries
    if (name == "General MIDI" || name == "Unknown")
    {
        DBG("Cannot remove protected origin library: " + name);
        return;
    }

    int index = configuredOriginLibraries.indexOf(name);
    if (index >= 0)
    {
        configuredOriginLibraries.remove(index);
        DBG("Removed origin library: " + name);
        saveOriginLibraries();
    }
}

juce::StringArray DrumLibraryManager::getConfiguredOriginLibraries() const
{
    return configuredOriginLibraries;
}

void DrumLibraryManager::loadOriginLibraries()
{
    juce::File originFile = getOriginLibrariesFile();

    if (!originFile.existsAsFile())
    {
        DBG("No origin libraries file found - creating complete default file with all libraries");

        // Initialize with ALL available libraries (same behavior as target libraries)
        configuredOriginLibraries.clear();

        // Add all built-in libraries in alphabetical order
        configuredOriginLibraries.add("Addictive Drums 2");
        configuredOriginLibraries.add("BFD3");
        configuredOriginLibraries.add("Damage 2");
        configuredOriginLibraries.add("Drum Locker");
        configuredOriginLibraries.add("EZdrummer");
        configuredOriginLibraries.add("General MIDI");
        configuredOriginLibraries.add("GetGood Drums");
        configuredOriginLibraries.add("ML Drums");
        configuredOriginLibraries.add("MODO Drum");
        configuredOriginLibraries.add("MT Power Drum Kit 2");
        configuredOriginLibraries.add("MuldjordKit");
        configuredOriginLibraries.add("Salamander Drumkit");
        configuredOriginLibraries.add("Shreddage Drums");
        configuredOriginLibraries.add("Sitala");
        configuredOriginLibraries.add("Steven Slate Drums");
        configuredOriginLibraries.add("Superior Drummer 3");
        configuredOriginLibraries.add("The Aasimonster");
        configuredOriginLibraries.add("Ugritone");
        configuredOriginLibraries.add("Unknown");

        // Save to file
        saveOriginLibraries();

        DBG("Created origin libraries file with " + juce::String(configuredOriginLibraries.size()) + " libraries");
        return;
    }

    // File exists - load ONLY from XML (don't add hardcoded values)
    auto root = juce::XmlDocument::parse(originFile);

    if (!root || !root->hasTagName("OriginLibraries"))
    {
        DBG("Invalid origin libraries file format");
        return;
    }

    DBG("Loading origin libraries from: " + originFile.getFullPathName());

    configuredOriginLibraries.clear();

    for (auto* originElement : root->getChildIterator())
    {
        if (originElement->hasTagName("Origin"))
        {
            juce::String name = originElement->getStringAttribute("name");
            if (name.isNotEmpty())
            {
                configuredOriginLibraries.add(name);
                DBG("Loaded origin library: " + name);
            }
        }
    }

    // Sort alphabetically after loading
    configuredOriginLibraries.sortNatural();

    DBG("Total origin libraries loaded from XML: " + juce::String(configuredOriginLibraries.size()));
}

void DrumLibraryManager::saveOriginLibraries()
{
    juce::File originFile = getOriginLibrariesFile();

    auto root = std::make_unique<juce::XmlElement>("OriginLibraries");

    // Save all configured origin libraries
    for (const auto& originLib : configuredOriginLibraries)
    {
        auto* originElement = root->createNewChildElement("Origin");
        originElement->setAttribute("name", originLib);

        // Get the library enum if it's a built-in library
        DrumLibrary lib = getLibraryFromName(originLib);
        if (lib != DrumLibrary::Unknown)
        {
            originElement->setAttribute("enum", static_cast<int>(lib));
        }
    }

    // Ensure directory exists
    originFile.getParentDirectory().createDirectory();

    if (root->writeToFile(originFile, ""))
    {
        DBG("Origin libraries saved to: " + originFile.getFullPathName());
    }
    else
    {
        DBG("ERROR: Failed to save origin libraries");
    }
}

void DrumLibraryManager::saveOriginMappings()
{
    juce::File mappingFile = getOriginMappingsFile();

    DBG("Saving origin library mappings to: " + mappingFile.getFullPathName());

    juce::XmlElement root("OriginLibraryMappings");

    // Save mappings for each configured origin library
    for (const auto& originLibName : configuredOriginLibraries)
    {
        if (originLibName == "Unknown")
            continue;

        DrumLibrary lib = getLibraryFromName(originLibName);
        if (lib == DrumLibrary::Unknown || lib == DrumLibrary::GeneralMIDI)
            continue; // Skip unknown and GM (GM doesn't need origin mappings)

            int libIndex = static_cast<int>(lib) - 2;

        // Check if there are mappings from this library to GM
        if (mappings.find(libIndex) == mappings.end() ||
            mappings[libIndex].find(0) == mappings[libIndex].end())
            continue;

        auto* libElement = root.createNewChildElement("OriginLibrary");
        libElement->setAttribute("name", originLibName);
        libElement->setAttribute("enum", static_cast<int>(lib));

        const auto& originToGM = mappings[libIndex][0];

        // Save ALL notes (0-127) - no filtering
        for (const auto& notePair : originToGM)
        {
            uint8_t originNote = notePair.first;
            uint8_t gmNote = notePair.second;

            auto* noteElement = libElement->createNewChildElement("NoteMap");
            noteElement->setAttribute("originNote", originNote);
            noteElement->setAttribute("gmNote", gmNote);

            // Save custom drum name if exists
            if (hasCustomDrumName(lib, gmNote))
            {
                noteElement->setAttribute("customName", getCustomDrumName(lib, gmNote));
            }
        }
    }

    // Ensure directory exists
    mappingFile.getParentDirectory().createDirectory();

    if (root.writeToFile(mappingFile, ""))
    {
        DBG("Origin library mappings saved successfully");
    }
    else
    {
        DBG("ERROR: Failed to save origin library mappings");
    }
}

void DrumLibraryManager::loadOriginMappings()
{
    juce::File mappingFile = getOriginMappingsFile();

    if (!mappingFile.existsAsFile())
    {
        DBG("No origin library mappings file found");
        return;
    }

    auto root = juce::XmlDocument::parse(mappingFile);

    if (!root || !root->hasTagName("OriginLibraryMappings"))
    {
        DBG("Invalid origin library mappings file format");
        return;
    }

    DBG("Loading origin library mappings from: " + mappingFile.getFullPathName());

    int librariesLoaded = 0;

    for (auto* libElement : root->getChildIterator())
    {
        if (!libElement->hasTagName("OriginLibrary"))
            continue;

        juce::String libraryName = libElement->getStringAttribute("name");
        DrumLibrary library = getLibraryFromName(libraryName);

        if (library == DrumLibrary::Unknown)
        {
            DBG("  Skipping unknown origin library: " + libraryName);
            continue;
        }

        int originIndex = static_cast<int>(library) - 2;

        for (auto* noteElement : libElement->getChildIterator())
        {
            if (!noteElement->hasTagName("NoteMap"))
                continue;

            uint8_t originNote = static_cast<uint8_t>(noteElement->getIntAttribute("originNote"));
            uint8_t gmNote = static_cast<uint8_t>(noteElement->getIntAttribute("gmNote"));

            // Store bidirectional mapping
            mappings[originIndex][0][originNote] = gmNote;  // Origin -> GM
            mappings[0][originIndex][gmNote] = originNote;  // GM -> Origin

            // Load custom drum name if present
            juce::String customName = noteElement->getStringAttribute("customName");
            if (customName.isNotEmpty())
            {
                customDrumNames[originIndex][gmNote] = customName;
            }
        }

        librariesLoaded++;
        DBG("Loaded mappings for origin library: " + libraryName);
    }

    DBG("Loaded " + juce::String(librariesLoaded) + " origin library mappings");
}

void DrumLibraryManager::initializeOriginMappingWithGM(DrumLibrary originLibrary)
{
    if (originLibrary == DrumLibrary::Unknown || originLibrary == DrumLibrary::GeneralMIDI)
        return;

    int originIndex = static_cast<int>(originLibrary) - 2;

    DBG("Initializing " + getLibraryName(originLibrary) + " with GM identity mapping (notes 35-81 ONLY)");

    // Create identity mapping ONLY for standard drum range (35-81)
    for (uint8_t note = 35; note <= 81; ++note)
    {
        mappings[originIndex][0][note] = note;  // Origin -> GM (identity)
        mappings[0][originIndex][note] = note;  // GM -> Origin (identity)
    }

    saveOriginMappings();

    DBG("Origin library initialized with GM mapping (35-81 only)");
}

void DrumLibraryManager::initializeTargetMappingWithGM(DrumLibrary targetLibrary)
{
    if (targetLibrary == DrumLibrary::Unknown || targetLibrary == DrumLibrary::Bypass)
        return;

    int targetIndex = static_cast<int>(targetLibrary) - 2;

    DBG("Initializing target library with GM identity mapping (notes 35-81)");

    // Create identity mapping for standard drum range (35-81)
    for (uint8_t note = 35; note <= 81; ++note)
    {
        mappings[0][targetIndex][note] = note;  // GM -> Target
        mappings[targetIndex][0][note] = note;  // Target -> GM
    }

    saveCustomMappings();
}

void DrumLibraryManager::setCustomDrumName(DrumLibrary library, uint8_t gmNote, const juce::String& customName)
{
    int libIndex = static_cast<int>(library) - 2;

    if (customName.isEmpty())
    {
        // Remove custom name if empty
        if (customDrumNames.find(libIndex) != customDrumNames.end())
        {
            customDrumNames[libIndex].erase(gmNote);
        }
    }
    else
    {
        customDrumNames[libIndex][gmNote] = customName;
        DBG("Set custom drum name for " + getLibraryName(library) +
        " note " + juce::String(gmNote) + ": " + customName);
    }
}

juce::String DrumLibraryManager::getCustomDrumName(DrumLibrary library, uint8_t gmNote) const
{
    int libIndex = static_cast<int>(library) - 2;

    if (customDrumNames.find(libIndex) != customDrumNames.end())
    {
        const auto& libNames = customDrumNames.at(libIndex);
        if (libNames.find(gmNote) != libNames.end())
        {
            return libNames.at(gmNote);
        }
    }

    return ""; // No custom name set
}

bool DrumLibraryManager::hasCustomDrumName(DrumLibrary library, uint8_t gmNote) const
{
    int libIndex = static_cast<int>(library) - 2;

    if (customDrumNames.find(libIndex) != customDrumNames.end())
    {
        return customDrumNames.at(libIndex).find(gmNote) != customDrumNames.at(libIndex).end();
    }

    return false;
}

void DrumLibraryManager::clearCustomDrumName(DrumLibrary library, uint8_t gmNote)
{
    int libIndex = static_cast<int>(library) - 2;

    if (customDrumNames.find(libIndex) != customDrumNames.end())
    {
        customDrumNames[libIndex].erase(gmNote);
        DBG("Cleared custom drum name for " + getLibraryName(library) + " note " + juce::String(gmNote));
    }
}

void DrumLibraryManager::createDefaultMappingsFile()
{
    juce::File mappingFile = getCustomMappingsFile();

    DBG("========================================");
    DBG("Creating COMPLETE target drum mapping file");
    DBG("File: " + mappingFile.getFullPathName());
    DBG("========================================");

    juce::XmlElement root("DrumLibraryMappings");

    // Get all built-in libraries in alphabetical order
    auto allLibraries = getAllLibraryNames();

    int totalMappingsCreated = 0;

    for (const auto& libraryName : allLibraries)
    {
        if (libraryName == "Bypass (No Remapping)" || libraryName == "Unknown")
            continue; // Skip these

            DrumLibrary library = getLibraryFromName(libraryName);
        int libIndex = static_cast<int>(library) - 2;

        auto* libElement = root.createNewChildElement("Library");
        libElement->setAttribute("name", libraryName);
        libElement->setAttribute("enum", static_cast<int>(library));

        int libraryMappingCount = 0;

        // Save ALL notes from 35 to 81 (the standard GM drum range)
        for (uint8_t gmNote = 35; gmNote <= 81; ++gmNote)
        {
            // Default to identity mapping
            uint8_t productNote = gmNote;

            // Check if reverse mapping exists (GM -> Library)
            if (mappings.find(0) != mappings.end() &&
                mappings[0].find(libIndex) != mappings[0].end())
            {
                const auto& gmToLibrary = mappings[0][libIndex];

                // If mapping exists, use it
                if (gmToLibrary.find(gmNote) != gmToLibrary.end())
                {
                    productNote = gmToLibrary.at(gmNote);
                }
            }

            // ALWAYS save - don't check if mapping exists
            auto* noteElement = libElement->createNewChildElement("NoteMap");
            noteElement->setAttribute("gmNote", gmNote);
            noteElement->setAttribute("productNote", productNote);

            libraryMappingCount++;
            totalMappingsCreated++;
        }

        DBG("  " + libraryName + ": " + juce::String(libraryMappingCount) + " notes");
    }

    // Ensure directory exists
    mappingFile.getParentDirectory().createDirectory();

    if (root.writeToFile(mappingFile, ""))
    {
        DBG("========================================");
        DBG("SUCCESS: Target drum mapping file created");
        DBG("Total libraries: " + juce::String(allLibraries.size() - 2));
        DBG("Total note mappings: " + juce::String(totalMappingsCreated));
        DBG("Expected: " + juce::String((allLibraries.size() - 2) * 47) + " (libraries ÃƒÆ’Ã¢â‚¬â€ 47 notes)");
        DBG("Each library has ALL 47 notes (35-81), including identity mappings");
        DBG("File location: " + mappingFile.getFullPathName());
        DBG("========================================");
    }
    else
    {
        DBG("========================================");
        DBG("ERROR: Failed to create target drum mapping file");
        DBG("========================================");
    }
}

std::map<uint8_t, uint8_t> DrumLibraryManager::getExplicitMappingsForLibrary(DrumLibrary library) const
{
    std::map<uint8_t, uint8_t> result;

    int libIndex = static_cast<int>(library) - 2;

    // Get GM (index 0) to this library mappings
    if (mappings.find(0) != mappings.end() &&
        mappings.at(0).find(libIndex) != mappings.at(0).end())
    {
        const auto& gmToLibrary = mappings.at(0).at(libIndex);

        for (const auto& notePair : gmToLibrary)
        {
            uint8_t gmNote = notePair.first;
            uint8_t productNote = notePair.second;

            // Include if:
            // 1. Note is in standard GM drum range (35-81) - always show these
            // 2. OR note is outside range but has non-identity mapping (user changed it)
            // 3. OR note has custom drum name (user explicitly set description)
            bool isInStandardRange = (gmNote >= 35 && gmNote <= 81);
            bool isNonIdentity = (gmNote != productNote);
            bool hasCustomName = hasCustomDrumName(library, gmNote);

            if (isInStandardRange || isNonIdentity || hasCustomName)
            {
                result[gmNote] = productNote;
            }
        }
    }

    return result;
}

void DrumLibraryManager::removeMappingForNote(DrumLibrary library, uint8_t gmNote)
{
    int libIndex = static_cast<int>(library) - 2;

    // Remove from GM -> Library mappings
    if (mappings.find(0) != mappings.end() &&
        mappings[0].find(libIndex) != mappings[0].end())
    {
        mappings[0][libIndex].erase(gmNote);
        DBG("Removed mapping for GM note " + juce::String(gmNote) +
        " from " + getLibraryName(library));
    }

    // Save immediately
    saveCustomMappings();
}

std::map<uint8_t, uint8_t> DrumLibraryManager::getOriginLibraryMappings(DrumLibrary originLibrary) const
{
    std::map<uint8_t, uint8_t> result;

    if (originLibrary == DrumLibrary::Unknown)
        return result;

    int libIndex = static_cast<int>(originLibrary) - 2;

    DBG("getOriginLibraryMappings for " + getLibraryName(originLibrary) + " (libIndex=" + juce::String(libIndex) + ")");

    // Get ORIGIN library (libIndex) to GM (index 0) mappings
    if (mappings.find(libIndex) != mappings.end() &&
        mappings.at(libIndex).find(0) != mappings.at(libIndex).end())
    {
        const auto& originToGM = mappings.at(libIndex).at(0);

        DBG("  Found " + juce::String(originToGM.size()) + " total mappings in memory");

        for (const auto& notePair : originToGM)
        {
            uint8_t originNote = notePair.first;
            uint8_t gmNote = notePair.second;

            // Include if:
            // 1. Note is in standard GM drum range (35-81) - always show these
            // 2. OR note is outside range but has non-identity mapping (user changed it)
            // 3. OR note has custom drum name (user explicitly set description)
            bool isInStandardRange = (originNote >= 35 && originNote <= 81);
            bool isNonIdentity = (originNote != gmNote);
            bool hasCustomName = hasCustomDrumName(originLibrary, gmNote);

            if (isInStandardRange || isNonIdentity || hasCustomName)
            {
                result[originNote] = gmNote;
            }
            else
            {
                DBG("  EXCLUDED: Origin " + juce::String(originNote) + " -> GM " + juce::String(gmNote) +
                " (inRange=" + juce::String(isInStandardRange ? "Y" : "N") +
                ", nonIdentity=" + juce::String(isNonIdentity ? "Y" : "N") +
                ", hasName=" + juce::String(hasCustomName ? "Y" : "N") + ")");
            }
        }

        DBG("  Returning " + juce::String(result.size()) + " filtered mappings");
    }
    else
    {
        DBG("  No mappings found for this library");
    }

    return result;
}

int DrumLibraryManager::getNoteNumberForDrumPart(DrumPartType part, DrumLibrary library) const
{
    if (library == DrumLibrary::Unknown || library == DrumLibrary::Bypass)
        return -1;

    // For General MIDI, use standard note numbers
    if (library == DrumLibrary::GeneralMIDI)
    {
        for (int note = 35; note <= 81; ++note)
        {
            if (MidiDissector::getPartTypeFromNote(note, library) == part)
                return note;
        }
    }
    else
    {
        // For origin libraries, search the mappings
        int libIndex = static_cast<int>(library) - 2;

        if (mappings.find(libIndex) != mappings.end())
        {
            const auto& libMappings = mappings.at(libIndex);
            if (libMappings.find(0) != libMappings.end())
            {
                const auto& originToGM = libMappings.at(0);

                // Search all origin notes
                for (const auto& [originNote, gmNote] : originToGM)
                {
                    // IMPORTANT: Check if this GM note has a valid name (either custom or GM standard)
                    bool hasValidName = false;

                    // Check custom name first
                    if (customDrumNames.find(libIndex) != customDrumNames.end())
                    {
                        const auto& libCustomNames = customDrumNames.at(libIndex);
                        if (libCustomNames.find(gmNote) != libCustomNames.end())
                        {
                            if (libCustomNames.at(gmNote).isNotEmpty())
                                hasValidName = true;
                        }
                    }

                    // Check GM name if no custom name
                    if (!hasValidName)
                    {
                        juce::String gmName = getGMDrumName(gmNote);
                        hasValidName = gmName.isNotEmpty();
                    }

                    // Skip notes without valid names
                    if (!hasValidName)
                        continue;

                    // Now check if this is the drum part we're looking for
                    DrumPartType gmPartType = MidiDissector::getPartTypeFromNote(gmNote, DrumLibrary::GeneralMIDI);
                    if (gmPartType == part)
                    {
                        return originNote;
                    }
                }
            }
        }
    }

    // Fallback
    switch (part)
    {
        case DrumPartType::Kick: return 36;
        case DrumPartType::Snare: return 38;
        case DrumPartType::HiHatClosed: return 42;
        case DrumPartType::HiHatOpen: return 46;
        case DrumPartType::Crash: return 49;
        case DrumPartType::Ride: return 51;
        case DrumPartType::Tom1: return 48;
        case DrumPartType::Tom2: return 45;
        case DrumPartType::Tom3: return 43;
        case DrumPartType::FloorTom: return 41;
        case DrumPartType::Cowbell: return 56;
        case DrumPartType::Clap: return 39;
        case DrumPartType::Shaker: return 70;
        default: return 60;
    }
}

juce::String DrumLibraryManager::getDrumPartName(DrumPartType part, DrumLibrary library) const
{
    // Get the first origin note number for this drum part in this library
    int originNoteNumber = getNoteNumberForDrumPart(part, library);

    if (originNoteNumber <= 0 || originNoteNumber >= 128)
    {
        // Fallback to generic name
        switch (part)
        {
            case DrumPartType::Kick: return "Kick";
            case DrumPartType::Snare: return "Snare";
            case DrumPartType::HiHatClosed: return "Hi-Hat (Closed)";
            case DrumPartType::HiHatOpen: return "Hi-Hat (Open)";
            case DrumPartType::Crash: return "Crash";
            case DrumPartType::Ride: return "Ride";
            case DrumPartType::Tom1: return "Tom 1";
            case DrumPartType::Tom2: return "Tom 2";
            case DrumPartType::Tom3: return "Tom 3";
            case DrumPartType::FloorTom: return "Floor Tom";
            case DrumPartType::Cowbell: return "Cowbell";
            case DrumPartType::Clap: return "Clap";
            case DrumPartType::Shaker: return "Shaker";
            case DrumPartType::Other: return "Other";
            default: return "Unknown";
        }
    }

    // Now map origin note to GM note to look up custom name
    int libIndex = static_cast<int>(library) - 2;  // Offset: enums start at 2
    uint8_t gmNote = originNoteNumber;  // Default to same note

    // Look up the GM note this origin note maps to
    if (libIndex >= 0 && mappings.find(libIndex) != mappings.end())
    {
        const auto& libMappings = mappings.at(libIndex);
        if (libMappings.find(0) != libMappings.end())  // 0 = GM
        {
            const auto& originToGM = libMappings.at(0);
            if (originToGM.find(originNoteNumber) != originToGM.end())
            {
                gmNote = originToGM.at(originNoteNumber);
            }
        }
    }

    // Check for custom name stored for this library and GM note
    if (libIndex >= 0 && customDrumNames.find(libIndex) != customDrumNames.end())
    {
        const auto& libCustomNames = customDrumNames.at(libIndex);
        if (libCustomNames.find(gmNote) != libCustomNames.end())
        {
            juce::String customName = libCustomNames.at(gmNote);
            if (customName.isNotEmpty())
            {
                // Found custom name in XML!
                return customName;
            }
        }
    }

    // No custom name found, use General MIDI descriptive name
    return getGMDrumName(gmNote);
}
