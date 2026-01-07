#include "SFZParser.h"

bool SFZParser::parseFile(const juce::File& sfzFile)
{
    if (!sfzFile.existsAsFile())
    {
        DBG("SFZ file not found: " + sfzFile.getFullPathName());
        return false;
    }

    baseDirectory = sfzFile.getParentDirectory();
    allRegions.clear();

    // Read the entire file
    juce::String content = sfzFile.loadFileAsString();
    juce::StringArray lines;
    lines.addLines(content);

    Group currentGroup;
    Region currentRegion;
    bool inGroup = false;
    bool inRegion = false;

    DBG("Parsing SFZ file: " + sfzFile.getFullPathName());

    for (const auto& line : lines)
    {
        juce::String trimmed = line.trim();

        // Skip empty lines and comments
        if (trimmed.isEmpty() || trimmed.startsWith("//"))
            continue;

        // Handle <group> tag
        if (trimmed.contains("<group>"))
        {
            // Save previous group/region if any
            if (inRegion && currentRegion.midiNote != -1)
            {
                currentGroup.regions.push_back(currentRegion);
            }

            // Save previous group
            if (inGroup)
            {
                for (auto& region : currentGroup.regions)
                    allRegions.push_back(region);
            }

            // Start new group
            currentGroup = Group();
            currentRegion = Region();
            inGroup = true;
            inRegion = false;

            // Parse opcodes on the same line as <group>
            auto groupLine = trimmed.fromFirstOccurrenceOf("<group>", false, false).trim();
            parseLine(groupLine, currentGroup, currentRegion);

            continue;
        }

        // Handle <region> tag
        if (trimmed.contains("<region>"))
        {
            // Save previous region if any
            if (inRegion && currentRegion.midiNote != -1)
            {
                currentGroup.regions.push_back(currentRegion);
            }

            // Start new region, inherit from group
            currentRegion = Region();
            currentRegion.midiNote = currentGroup.midiNote;
            currentRegion.velocityLow = currentGroup.velocityLow;
            currentRegion.velocityHigh = currentGroup.velocityHigh;
            currentRegion.volume = currentGroup.volume;
            currentRegion.chokeGroup = currentGroup.chokeGroup;
            currentRegion.ampVeltrack = currentGroup.ampVeltrack;
            inRegion = true;

            // Parse opcodes on the same line as <region>
            auto regionLine = trimmed.fromFirstOccurrenceOf("<region>", false, false).trim();
            parseLine(regionLine, currentGroup, currentRegion);

            continue;
        }

        // Parse opcodes
        if (inGroup || inRegion)
        {
            parseLine(trimmed, currentGroup, currentRegion);
        }
    }

    // Save final region and group
    if (inRegion && currentRegion.midiNote != -1)
    {
        currentGroup.regions.push_back(currentRegion);
    }

    if (inGroup)
    {
        for (auto& region : currentGroup.regions)
            allRegions.push_back(region);
    }

    DBG("Parsed " + juce::String(allRegions.size()) + " regions");
    return !allRegions.empty();
}

void SFZParser::parseLine(const juce::String& line, Group& currentGroup, Region& currentRegion)
{
    // Split line into opcode=value pairs
    juce::StringArray tokens;
    tokens.addTokens(line, " \t", "\"");

    for (const auto& token : tokens)
    {
        if (token.contains("="))
        {
            auto opcode = token.upToFirstOccurrenceOf("=", false, false).trim();
            auto value = token.fromFirstOccurrenceOf("=", false, false).trim();

            // Determine if this is a group or region opcode
            if (!currentRegion.samplePath.isEmpty() || opcode == "sample")
            {
                // Region-specific
                parseRegionOpcode(opcode, value, currentRegion);
            }
            else
            {
                // Group-level
                parseGroupOpcode(opcode, value, currentGroup);
            }
        }
    }
}

void SFZParser::parseGroupOpcode(const juce::String& opcode, const juce::String& value, Group& group)
{
    if (opcode == "key")
        group.midiNote = parseIntValue(value);
    else if (opcode == "lovel")
        group.velocityLow = parseIntValue(value);
    else if (opcode == "hivel")
        group.velocityHigh = parseIntValue(value);
    else if (opcode == "volume")
        group.volume = parseFloatValue(value);
    else if (opcode == "amp_veltrack")
        group.ampVeltrack = parseFloatValue(value);
    else if (opcode == "group" || opcode == "off_by")
        group.chokeGroup = parseIntValue(value);
}

void SFZParser::parseRegionOpcode(const juce::String& opcode, const juce::String& value, Region& region)
{
    if (opcode == "sample")
        region.samplePath = value.replace("\\", "/"); // Normalize path separators
        else if (opcode == "key")
            region.midiNote = parseIntValue(value);
    else if (opcode == "lovel")
        region.velocityLow = parseIntValue(value);
    else if (opcode == "hivel")
        region.velocityHigh = parseIntValue(value);
    else if (opcode == "lorand")
        region.randomLow = parseFloatValue(value);
    else if (opcode == "hirand")
        region.randomHigh = parseFloatValue(value);
    else if (opcode == "volume")
        region.volume = parseFloatValue(value);
    else if (opcode == "amp_veltrack")
        region.ampVeltrack = parseFloatValue(value);
    else if (opcode == "group" || opcode == "off_by")
        region.chokeGroup = parseIntValue(value);
}

int SFZParser::parseIntValue(const juce::String& value) const
{
    return value.getIntValue();
}

float SFZParser::parseFloatValue(const juce::String& value) const
{
    return value.getFloatValue();
}

std::vector<const SFZParser::Region*> SFZParser::getRegionsForNote(int midiNote, int velocity) const
{
    std::vector<const Region*> matchingRegions;

    for (const auto& region : allRegions)
    {
        if (region.matches(midiNote, velocity))
        {
            matchingRegions.push_back(&region);
        }
    }

    return matchingRegions;
}

const SFZParser::Region* SFZParser::getRandomRegion(int midiNote, int velocity) const
{
    auto matchingRegions = getRegionsForNote(midiNote, velocity);

    if (matchingRegions.empty())
        return nullptr;

    // Generate random value between 0.0 and 1.0 for round-robin
    float randomValue = random.nextFloat();

    // Find region that matches the random value
    for (auto* region : matchingRegions)
    {
        if (region->matchesRandom(randomValue))
            return region;
    }

    // Fallback to first matching region
    return matchingRegions[0];
}

int SFZParser::getUniqueNoteCount() const
{
    std::set<int> uniqueNotes;
    for (const auto& region : allRegions)
    {
        if (region.midiNote != -1)
            uniqueNotes.insert(region.midiNote);
    }
    return static_cast<int>(uniqueNotes.size());
}
