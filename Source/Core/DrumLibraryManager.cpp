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
    // FIXED: Initialize identity mappings for ALL library pairs (0-18 = libraries 2-20)
    // Enum goes: Unknown=0, Bypass=1, GeneralMIDI=2...DrumLocker=20
    // With -2 offset: indices 0-18 for libraries 2-20
    for (int from = 0; from < 19; ++from)  // FIXED: was 9, now 19
    {
        for (int to = 0; to < 19; ++to)    // FIXED: was 9, now 19
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
    // Ugritone = 9, GM = 2, so indices are 7 and 0 with -2 offset
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
    // Ugritone = 9, SD3 = 3, so indices are 7 and 1 with -2 offset
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
    // Ugritone = 9, EZdrummer = 6, so indices are 7 and 4 with -2 offset
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
    // GM = 2, SD3 = 3, so indices are 0 and 1 with -2 offset
    auto& gmToSD3 = mappings[0][1];
    gmToSD3[36] = 36;
    gmToSD3[38] = 38;
    gmToSD3[42] = 42;
    gmToSD3[46] = 46;
    gmToSD3[49] = 49;
    gmToSD3[51] = 51;

    // General MIDI to Addictive Drums 2
    // GM = 2, AD2 = 4, so indices are 0 and 2 with -2 offset
    auto& gmToAD2 = mappings[0][2];
    gmToAD2[36] = 36;
    gmToAD2[38] = 38;
    gmToAD2[42] = 42;
    gmToAD2[46] = 46;
    gmToAD2[49] = 49;
    gmToAD2[57] = 55;
    gmToAD2[51] = 51;

    // EZdrummer to General MIDI
    // EZdrummer = 6, GM = 2, so indices are 4 and 0 with -2 offset
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
    // EZdrummer = 6, SD3 = 3, so indices are 4 and 1 with -2 offset
    auto& ezdToSD3 = mappings[4][1];
    ezdToSD3[36] = 36;
    ezdToSD3[38] = 38;
    ezdToSD3[42] = 42;
    ezdToSD3[46] = 46;
    ezdToSD3[24] = 36;
    ezdToSD3[26] = 38;

    // EZdrummer to Ugritone
    // EZdrummer = 6, Ugritone = 9, so indices are 4 and 7 with -2 offset
    auto& ezdToUgritone = mappings[4][7];
    ezdToUgritone[36] = 36;
    ezdToUgritone[38] = 38;
    ezdToUgritone[42] = 22;  // Use Ugritone custom hihat
    ezdToUgritone[46] = 26;  // Use Ugritone custom open hihat
    ezdToUgritone[24] = 36;

    // Superior Drummer 3 to EZdrummer
    // SD3 = 3, EZdrummer = 6, so indices are 1 and 4 with -2 offset
    auto& sd3ToEzd = mappings[1][4];
    sd3ToEzd[36] = 36;
    sd3ToEzd[38] = 38;
    sd3ToEzd[42] = 42;
    sd3ToEzd[46] = 46;
	
	// BFD3 to General MIDI
    // BFD3 = 10, GM = 2, so indices are 8 and 0 with -2 offset
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
    
    // BFD3 to Superior Drummer 3
    // BFD3 = 10, SD3 = 3, so indices are 8 and 1 with -2 offset
    auto& bfd3ToSD3 = mappings[8][1];
    
    // Kicks
    bfd3ToSD3[36] = 36;
    
    // Snares
    bfd3ToSD3[38] = 38;
    bfd3ToSD3[40] = 37;  // Rim shot to cross stick
    
    // Hi-hats
    bfd3ToSD3[42] = 42;
    bfd3ToSD3[44] = 46;
    bfd3ToSD3[46] = 44;
    
    // Toms
    bfd3ToSD3[43] = 43;
    bfd3ToSD3[47] = 47;
    bfd3ToSD3[48] = 48;
    bfd3ToSD3[50] = 48;
    
    // Cymbals
    bfd3ToSD3[41] = 52;
    bfd3ToSD3[49] = 49;
    bfd3ToSD3[51] = 51;
    bfd3ToSD3[52] = 57;
    bfd3ToSD3[55] = 53;
    bfd3ToSD3[57] = 57;
    
    // BFD3 to EZdrummer
    // BFD3 = 10, EZdrummer = 6, so indices are 8 and 4 with -2 offset
    auto& bfd3ToEzd = mappings[8][4];
    
    // Kicks
    bfd3ToEzd[36] = 36;
    
    // Snares
    bfd3ToEzd[38] = 38;
    bfd3ToEzd[40] = 38;  // Rim shot to snare
    
    // Hi-hats
    bfd3ToEzd[42] = 42;
    bfd3ToEzd[44] = 46;
    bfd3ToEzd[46] = 44;
    
    // Toms
    bfd3ToEzd[43] = 43;
    bfd3ToEzd[47] = 47;
    bfd3ToEzd[48] = 48;
    bfd3ToEzd[50] = 48;
    
    // Cymbals
    bfd3ToEzd[49] = 49;
    bfd3ToEzd[51] = 51;
    bfd3ToEzd[52] = 49;
    bfd3ToEzd[55] = 51;
    bfd3ToEzd[57] = 49;
    
    // BFD3 to Ugritone
    // BFD3 = 10, Ugritone = 9, so indices are 8 and 7 with -2 offset
    auto& bfd3ToUgritone = mappings[8][7];
    
    // Kicks
    bfd3ToUgritone[36] = 36;
    
    // Snares
    bfd3ToUgritone[38] = 38;
    bfd3ToUgritone[40] = 37;
    
    // Hi-hats - map to Ugritone custom positions
    bfd3ToUgritone[42] = 22;  // Closed -> Ugritone closed
    bfd3ToUgritone[44] = 26;  // Open -> Ugritone open
    bfd3ToUgritone[46] = 44;  // Pedal
    
    // Toms
    bfd3ToUgritone[43] = 43;
    bfd3ToUgritone[47] = 47;
    bfd3ToUgritone[48] = 48;
    bfd3ToUgritone[50] = 48;
    
    // Cymbals
    bfd3ToUgritone[49] = 49;
    bfd3ToUgritone[51] = 51;
    bfd3ToUgritone[52] = 57;
    bfd3ToUgritone[55] = 53;
    bfd3ToUgritone[57] = 57;
	
	// MT Power Drum Kit 2 to General MIDI
    // MTPowerDrumKit2 = 11, GM = 2, so indices are 9 and 0 with -2 offset
    auto& mtPowerToGM = mappings[9][0];
    
    // Kicks
    mtPowerToGM[36] = 36;  // Kick
    
    // Snares
    mtPowerToGM[38] = 38;  // Snare
    mtPowerToGM[40] = 38;  // Snare rim -> Snare
    
    // Hi-hats
    mtPowerToGM[42] = 42;  // Hi-hat closed
    mtPowerToGM[44] = 44;  // Hi-hat pedal
    mtPowerToGM[46] = 46;  // Hi-hat open
    
    // Toms
    mtPowerToGM[41] = 41;  // Floor tom
    mtPowerToGM[43] = 43;  // Floor tom high
    mtPowerToGM[45] = 45;  // Tom low
    mtPowerToGM[47] = 47;  // Tom mid
    mtPowerToGM[48] = 48;  // Tom high
    mtPowerToGM[50] = 50;  // Tom highest
    
    // Cymbals
    mtPowerToGM[49] = 49;  // Crash
    mtPowerToGM[51] = 51;  // Ride
    mtPowerToGM[55] = 49;  // Splash -> Crash
    mtPowerToGM[57] = 49;  // Crash 2 -> Crash
    
    // DrumGizmo to General MIDI
    // DrumGizmo = 12, GM = 2, so indices are 10 and 0 with -2 offset
    auto& drumGizmoToGM = mappings[10][0];
    
    // Kicks
    drumGizmoToGM[36] = 36;  // Kick
    
    // Snares
    drumGizmoToGM[38] = 38;  // Snare
    drumGizmoToGM[37] = 37;  // Side stick
    drumGizmoToGM[40] = 38;  // Rim shot -> Snare
    
    // Hi-hats
    drumGizmoToGM[42] = 42;  // Hi-hat closed
    drumGizmoToGM[44] = 44;  // Hi-hat pedal
    drumGizmoToGM[46] = 46;  // Hi-hat open
    
    // Toms
    drumGizmoToGM[41] = 41;  // Floor tom low
    drumGizmoToGM[43] = 43;  // Floor tom high
    drumGizmoToGM[45] = 45;  // Tom low
    drumGizmoToGM[47] = 47;  // Tom mid
    drumGizmoToGM[48] = 48;  // Tom high
    drumGizmoToGM[50] = 50;  // Tom highest
    
    // Cymbals
    drumGizmoToGM[49] = 49;  // Crash
    drumGizmoToGM[51] = 51;  // Ride
    drumGizmoToGM[52] = 52;  // China
    drumGizmoToGM[53] = 53;  // Ride bell
    drumGizmoToGM[55] = 49;  // Splash -> Crash
    drumGizmoToGM[57] = 49;  // Crash 2 -> Crash
    
    // Sitala to General MIDI
    // Sitala = 13, GM = 2, so indices are 11 and 0 with -2 offset
    auto& sitalaToGM = mappings[11][0];
    
    // Sitala is flexible, uses standard GM mapping by default
    // Kicks
    sitalaToGM[36] = 36;
    
    // Snares
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
    
    // Krimh Drums to General MIDI
    // KrimhDrums = 14, GM = 2, so indices are 12 and 0 with -2 offset
    auto& krimhToGM = mappings[12][0];
    
    // Kicks
    krimhToGM[36] = 36;  // Kick
    
    // Snares
    krimhToGM[38] = 38;  // Snare
    krimhToGM[37] = 37;  // Side stick
    krimhToGM[40] = 38;  // Rim shot -> Snare
    
    // Hi-hats
    krimhToGM[42] = 42;  // Hi-hat closed
    krimhToGM[44] = 44;  // Hi-hat pedal
    krimhToGM[46] = 46;  // Hi-hat open
    
    // Toms
    krimhToGM[41] = 41;  // Floor tom
    krimhToGM[43] = 43;  // Floor tom high
    krimhToGM[45] = 45;  // Tom low
    krimhToGM[47] = 47;  // Tom mid
    krimhToGM[48] = 48;  // Tom high
    krimhToGM[50] = 50;  // Tom highest
    
    // Cymbals
    krimhToGM[49] = 49;  // Crash
    krimhToGM[51] = 51;  // Ride
    krimhToGM[52] = 52;  // China
    krimhToGM[53] = 53;  // Ride bell
    krimhToGM[55] = 49;  // Splash -> Crash
    krimhToGM[57] = 49;  // Crash 2 -> Crash
    
    // The Monarch Kit to General MIDI
    // TheMonarchKit = 15, GM = 2, so indices are 13 and 0 with -2 offset
    auto& monarchToGM = mappings[13][0];
    
    // Kicks
    monarchToGM[36] = 36;  // Kick
    
    // Snares
    monarchToGM[38] = 38;  // Snare
    monarchToGM[37] = 37;  // Side stick
    monarchToGM[40] = 38;  // Rim shot -> Snare
    
    // Hi-hats
    monarchToGM[42] = 42;  // Hi-hat closed
    monarchToGM[44] = 44;  // Hi-hat pedal
    monarchToGM[46] = 46;  // Hi-hat open
    
    // Toms
    monarchToGM[41] = 41;  // Floor tom
    monarchToGM[43] = 43;  // Floor tom high
    monarchToGM[45] = 45;  // Tom low
    monarchToGM[47] = 47;  // Tom mid
    monarchToGM[48] = 48;  // Tom high
    
    // Cymbals
    monarchToGM[49] = 49;  // Crash
    monarchToGM[51] = 51;  // Ride
    monarchToGM[52] = 52;  // China
    monarchToGM[53] = 53;  // Ride bell
    monarchToGM[55] = 49;  // Splash -> Crash
    monarchToGM[57] = 49;  // Crash 2 -> Crash
    
    // Shreddage Drums to General MIDI
    // ShreddageDrums = 16, GM = 2, so indices are 14 and 0 with -2 offset
    auto& shreddageToGM = mappings[14][0];
    
    // Kicks
    shreddageToGM[36] = 36;  // Kick
    
    // Snares
    shreddageToGM[38] = 38;  // Snare
    shreddageToGM[37] = 37;  // Side stick
    shreddageToGM[40] = 38;  // Rim shot -> Snare
    
    // Hi-hats
    shreddageToGM[42] = 42;  // Hi-hat closed
    shreddageToGM[44] = 44;  // Hi-hat pedal
    shreddageToGM[46] = 46;  // Hi-hat open
    
    // Toms
    shreddageToGM[41] = 41;  // Floor tom
    shreddageToGM[43] = 43;  // Floor tom high
    shreddageToGM[45] = 45;  // Tom low
    shreddageToGM[47] = 47;  // Tom mid
    shreddageToGM[48] = 48;  // Tom high
    
    // Cymbals
    shreddageToGM[49] = 49;  // Crash
    shreddageToGM[51] = 51;  // Ride
    shreddageToGM[52] = 52;  // China
    shreddageToGM[53] = 53;  // Ride bell
    shreddageToGM[55] = 49;  // Splash -> Crash
    shreddageToGM[57] = 49;  // Crash 2 -> Crash
    
    // Damage 2 to General MIDI
    // Damage2 = 17, GM = 2, so indices are 15 and 0 with -2 offset
    auto& damage2ToGM = mappings[15][0];
    
    // Kicks
    damage2ToGM[36] = 36;  // Kick
    
    // Snares
    damage2ToGM[38] = 38;  // Snare
    damage2ToGM[37] = 37;  // Side stick
    damage2ToGM[40] = 38;  // Rim shot -> Snare
    
    // Hi-hats
    damage2ToGM[42] = 42;  // Hi-hat closed
    damage2ToGM[44] = 44;  // Hi-hat pedal
    damage2ToGM[46] = 46;  // Hi-hat open
    
    // Toms
    damage2ToGM[41] = 41;  // Floor tom
    damage2ToGM[43] = 43;  // Floor tom high
    damage2ToGM[45] = 45;  // Tom low
    damage2ToGM[47] = 47;  // Tom mid
    damage2ToGM[48] = 48;  // Tom high
    
    // Cymbals
    damage2ToGM[49] = 49;  // Crash
    damage2ToGM[51] = 51;  // Ride
    damage2ToGM[52] = 52;  // China
    damage2ToGM[55] = 49;  // Splash -> Crash
    damage2ToGM[57] = 49;  // Crash 2 -> Crash
    
    // Triaz to General MIDI
    // Triaz = 18, GM = 2, so indices are 16 and 0 with -2 offset
    auto& triazToGM = mappings[16][0];
    
    // Kicks
    triazToGM[36] = 36;  // Kick
    
    // Snares
    triazToGM[38] = 38;  // Snare
    triazToGM[37] = 37;  // Side stick
    triazToGM[40] = 38;  // Rim shot -> Snare
    
    // Hi-hats
    triazToGM[42] = 42;  // Hi-hat closed
    triazToGM[44] = 44;  // Hi-hat pedal
    triazToGM[46] = 46;  // Hi-hat open
    
    // Toms
    triazToGM[41] = 41;  // Floor tom
    triazToGM[43] = 43;  // Floor tom high
    triazToGM[45] = 45;  // Tom low
    triazToGM[47] = 47;  // Tom mid
    triazToGM[48] = 48;  // Tom high
    
    // Cymbals
    triazToGM[49] = 49;  // Crash
    triazToGM[51] = 51;  // Ride
    triazToGM[52] = 52;  // China
    triazToGM[55] = 49;  // Splash -> Crash
    triazToGM[57] = 49;  // Crash 2 -> Crash
    
    // MODO Drum to General MIDI
    // MODODrum = 19, GM = 2, so indices are 17 and 0 with -2 offset
    auto& modoToGM = mappings[17][0];
    
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
    
    // DrumLocker to General MIDI
    // DrumLocker = 20, GM = 2, so indices are 18 and 0 with -2 offset
	auto& drumLockerToGM = mappings[18][0];

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
}

uint8_t DrumLibraryManager::mapNoteToLibrary(uint8_t note, DrumLibrary sourceLibrary, DrumLibrary targetLibrary) const
{
    // Bypass mode - no remapping
    if (targetLibrary == DrumLibrary::Bypass)
        return note;
    
    // Calculate adjusted indices (subtract 2 because Unknown=0, Bypass=1, first mappable is GeneralMIDI=2)
    int sourceIdx = static_cast<int>(sourceLibrary) - 2;
    int targetIdx = static_cast<int>(targetLibrary) - 2;
    
    // FIXED: Bounds check - with DrumLocker=20, max index is 18 (20-2=18)
    if (sourceIdx < 0 || sourceIdx >= 19 || targetIdx < 0 || targetIdx >= 19)
        return note;
    
    // If source and target are the same, no mapping needed
    if (sourceLibrary == targetLibrary)
        return note;
    
    // Try direct mapping first (source -> target)
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

    // If no direct mapping exists, try via General MIDI as intermediate
    uint8_t gmNote = note;
    
    // Step 1: Convert from source library to GM (if source is not GM)
    if (sourceLibrary != DrumLibrary::GeneralMIDI)
    {
        auto toGM = mappings.find(sourceIdx);
        if (toGM != mappings.end())
        {
            // GM has index 0 (2-2=0)
            auto gmMap = toGM->second.find(0);
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
    
    // Step 2: Convert from GM to target library (if target is not GM)
    if (targetLibrary != DrumLibrary::GeneralMIDI && gmNote != note)
    {
        // GM has index 0 (2-2=0)
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
    
    // Return GM note (or original if no mapping found)
    return gmNote;
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
        case DrumLibrary::Triaz: return "Triaz";
        case DrumLibrary::MODODrum: return "MODO Drum";
        case DrumLibrary::DrumLocker: return "Drum Locker";
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
    names.add(getLibraryName(DrumLibrary::DrumLocker));
    names.add(getLibraryName(DrumLibrary::EZdrummer));
    names.add(getLibraryName(DrumLibrary::GeneralMIDI));
    names.add(getLibraryName(DrumLibrary::GetGoodDrums));
    names.add(getLibraryName(DrumLibrary::KrimhDrums));
    names.add(getLibraryName(DrumLibrary::MODODrum));
    names.add(getLibraryName(DrumLibrary::MTPowerDrumKit2));
    names.add(getLibraryName(DrumLibrary::ShreddageDrums));
    names.add(getLibraryName(DrumLibrary::Sitala));
    names.add(getLibraryName(DrumLibrary::StevenSlateDrums));
    names.add(getLibraryName(DrumLibrary::SuperiorDrummer3));
    names.add(getLibraryName(DrumLibrary::TheMonarchKit));
    names.add(getLibraryName(DrumLibrary::Triaz));
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
    names.add(getLibraryName(DrumLibrary::DrumLocker));
    names.add(getLibraryName(DrumLibrary::EZdrummer));
    names.add(getLibraryName(DrumLibrary::GeneralMIDI));
    names.add(getLibraryName(DrumLibrary::GetGoodDrums));
    names.add(getLibraryName(DrumLibrary::KrimhDrums));
    names.add(getLibraryName(DrumLibrary::MODODrum));
    names.add(getLibraryName(DrumLibrary::MTPowerDrumKit2));
    names.add(getLibraryName(DrumLibrary::ShreddageDrums));
    names.add(getLibraryName(DrumLibrary::Sitala));
    names.add(getLibraryName(DrumLibrary::StevenSlateDrums));
    names.add(getLibraryName(DrumLibrary::SuperiorDrummer3));
    names.add(getLibraryName(DrumLibrary::TheMonarchKit));
    names.add(getLibraryName(DrumLibrary::Triaz));
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
    if (name == "Triaz") return DrumLibrary::Triaz;
    if (name == "MODO Drum") return DrumLibrary::MODODrum;
    if (name == "DrumLocker") return DrumLibrary::DrumLocker;
    
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