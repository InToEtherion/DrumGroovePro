#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

/**
 * Parser for DrumGizmo XML drum kit format
 * Parses the kit XML, midimap XML, and instrument XML files
 * 
 * DrumGizmo format structure:
 *   KitName/
 *   ├── KitName.xml         (main kit definition)
 *   ├── Midimap.xml         (MIDI note to instrument mapping)
 *   └── InstrumentName/
 *       ├── InstrumentName.xml  (instrument samples and velocity layers)
 *       └── samples/
 *           └── *.wav       (multi-channel WAV files)
 */
class DrumGizmoParser
{
public:
    DrumGizmoParser() = default;
    ~DrumGizmoParser() = default;

    // Sample info with velocity/power mapping
    struct Sample
    {
        juce::String name;
        juce::String filePath;      // Relative path to WAV file
        float power = 1.0f;         // DrumGizmo "power" value for velocity mapping
        int loVel = 0;              // Calculated low velocity (0-127)
        int hiVel = 127;            // Calculated high velocity (0-127)
    };

    // Instrument definition
    struct Instrument
    {
        juce::String name;
        juce::String xmlFile;       // Relative path to instrument XML
        juce::String group;         // Choke group (e.g., "hihat")
        std::vector<Sample> samples;
        
        // Main output channels (for multi-channel WAVs)
        int mainChannelL = 0;       // Left channel index in WAV (0-based)
        int mainChannelR = 1;       // Right channel index in WAV (0-based)
    };

    // MIDI mapping entry
    struct MidiMapEntry
    {
        int midiNote = 0;
        juce::String instrumentName;
    };

    // Kit definition
    struct Kit
    {
        juce::String name;
        juce::String description;
        std::vector<juce::String> channels;
        std::map<juce::String, Instrument> instruments;  // name -> Instrument
        std::vector<MidiMapEntry> midiMap;
    };

    // Parse entire kit from directory
    bool parseKit(const juce::File& kitDirectory);
    
    // Get parsed kit data
    const Kit& getKit() const { return kit; }
    
    // Find instrument by MIDI note
    const Instrument* getInstrumentForNote(int midiNote) const;
    
    // Find sample by MIDI note and velocity
    const Sample* getSampleForNoteAndVelocity(int midiNote, int velocity) const;
    
    // Get the sample path for a note/velocity
    juce::String getSamplePath(int midiNote, int velocity) const;
    
    // Check if kit is loaded
    bool isLoaded() const { return kitLoaded; }
    
    // Get all unique MIDI notes mapped
    std::vector<int> getMappedNotes() const;
    
    // Get kit base directory
    const juce::File& getKitDirectory() const { return kitBaseDirectory; }

private:
    // Parse individual XML files
    bool parseKitXml(const juce::File& kitXmlFile);
    bool parseMidimapXml(const juce::File& midimapFile);
    bool parseInstrumentXml(const juce::File& instrumentXmlFile, Instrument& instrument);
    
    // Calculate velocity ranges from power values
    void calculateVelocityRanges(Instrument& instrument);
    
    // Find main output channels from channel mapping
    void findMainChannels(juce::XmlElement* instrumentElement, Instrument& instrument);
    
    Kit kit;
    juce::File kitBaseDirectory;
    bool kitLoaded = false;
    
    // Cache for quick lookups
    std::map<int, juce::String> noteToInstrument;  // midiNote -> instrumentName
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGizmoParser)
};
