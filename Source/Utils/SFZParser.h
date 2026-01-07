#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

/**
 * Parses SFZ drum sample files and builds sample mappings
 * Supports velocity layers, round-robin, and basic SFZ opcodes
 */
class SFZParser
{
public:
    // Sample region definition
    struct Region
    {
        juce::String samplePath;    // Relative path to WAV file
        int midiNote { -1 };         // MIDI note number (key)
        int velocityLow { 0 };       // Minimum velocity (lovel)
        int velocityHigh { 127 };    // Maximum velocity (hivel)
        float randomLow { 0.0f };    // Round-robin range start (lorand)
        float randomHigh { 1.0f };   // Round-robin range end (hirand)
        float volume { 0.0f };       // Volume adjustment in dB
        int chokeGroup { 0 };        // Choke group (for hi-hats, cymbals)
        float ampVeltrack { 100.0f }; // Velocity tracking (0-100%)

        // Check if this region matches the given MIDI note and velocity
        bool matches(int note, int velocity) const
        {
            return note == midiNote &&
            velocity >= velocityLow &&
            velocity <= velocityHigh;
        }

        // Check if random value falls within this region's round-robin range
        bool matchesRandom(float randomValue) const
        {
            return randomValue >= randomLow && randomValue < randomHigh;
        }
    };

    // Group definition (holds common properties for regions)
    struct Group
    {
        int midiNote { -1 };
        int velocityLow { 0 };
        int velocityHigh { 127 };
        float volume { 0.0f };
        int chokeGroup { 0 };
        float ampVeltrack { 100.0f }; // Velocity tracking (0-100%)
        std::vector<Region> regions;
    };

    SFZParser() = default;
    ~SFZParser() = default;

    // Parse an SFZ file and build the region list
    bool parseFile(const juce::File& sfzFile);

    // Get all regions for a specific MIDI note and velocity
    std::vector<const Region*> getRegionsForNote(int midiNote, int velocity) const;

    // Get a random region from the matching regions (round-robin)
    const Region* getRandomRegion(int midiNote, int velocity) const;

    // Get all parsed regions
    const std::vector<Region>& getAllRegions() const { return allRegions; }

    // Get the base directory for sample paths
    juce::File getBaseDirectory() const { return baseDirectory; }

    // Get statistics
    int getRegionCount() const { return static_cast<int>(allRegions.size()); }
    int getUniqueNoteCount() const;

private:
    std::vector<Region> allRegions;
    juce::File baseDirectory;
    mutable juce::Random random;

    // Parsing helpers
    void parseLine(const juce::String& line, Group& currentGroup, Region& currentRegion);
    void parseGroupOpcode(const juce::String& opcode, const juce::String& value, Group& group);
    void parseRegionOpcode(const juce::String& opcode, const juce::String& value, Region& region);
    int parseIntValue(const juce::String& value) const;
    float parseFloatValue(const juce::String& value) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SFZParser)
};
