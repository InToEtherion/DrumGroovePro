#include "DrumGizmoParser.h"
#include <algorithm>

bool DrumGizmoParser::parseKit(const juce::File& kitDirectory)
{
    DBG("=== DrumGizmoParser::parseKit ===");
    DBG("Kit directory: " + kitDirectory.getFullPathName());
    
    kitLoaded = false;
    kit = Kit();
    noteToInstrument.clear();
    kitBaseDirectory = kitDirectory;
    
    if (!kitDirectory.exists() || !kitDirectory.isDirectory())
    {
        DBG("ERROR: Kit directory does not exist");
        return false;
    }
    
    // Find the main kit XML file (same name as directory)
    juce::String kitName = kitDirectory.getFileName();
    juce::File kitXmlFile = kitDirectory.getChildFile(kitName + ".xml");
    
    if (!kitXmlFile.existsAsFile())
    {
        DBG("ERROR: Kit XML not found: " + kitXmlFile.getFullPathName());
        return false;
    }
    
    // Parse main kit XML
    if (!parseKitXml(kitXmlFile))
    {
        DBG("ERROR: Failed to parse kit XML");
        return false;
    }
    
    // Parse MIDI map
    juce::File midimapFile = kitDirectory.getChildFile("Midimap.xml");
    if (midimapFile.existsAsFile())
    {
        if (!parseMidimapXml(midimapFile))
        {
            DBG("WARNING: Failed to parse Midimap.xml, using default mappings");
        }
    }
    else
    {
        DBG("WARNING: No Midimap.xml found");
    }
    
    // Parse each instrument XML
    for (auto& [name, instrument] : kit.instruments)
    {
        juce::File instrumentXml = kitDirectory.getChildFile(instrument.xmlFile);
        if (instrumentXml.existsAsFile())
        {
            if (!parseInstrumentXml(instrumentXml, instrument))
            {
                DBG("WARNING: Failed to parse instrument: " + name);
            }
        }
        else
        {
            DBG("WARNING: Instrument XML not found: " + instrumentXml.getFullPathName());
        }
    }
    
    // Build note-to-instrument lookup cache
    for (const auto& mapping : kit.midiMap)
    {
        noteToInstrument[mapping.midiNote] = mapping.instrumentName;
    }
    
    kitLoaded = true;
    
    DBG("Kit loaded successfully:");
    DBG("  Name: " + kit.name);
    DBG("  Instruments: " + juce::String(static_cast<int>(kit.instruments.size())));
    DBG("  MIDI mappings: " + juce::String(static_cast<int>(kit.midiMap.size())));
    
    return true;
}

bool DrumGizmoParser::parseKitXml(const juce::File& kitXmlFile)
{
    DBG("Parsing kit XML: " + kitXmlFile.getFullPathName());
    
    auto xml = juce::XmlDocument::parse(kitXmlFile);
    if (xml == nullptr || !xml->hasTagName("drumkit"))
    {
        DBG("ERROR: Invalid kit XML format");
        return false;
    }
    
    kit.name = xml->getStringAttribute("name");
    kit.description = xml->getStringAttribute("description");
    
    DBG("  Kit name: " + kit.name);
    
    // Parse channels
    auto* channelsElement = xml->getChildByName("channels");
    if (channelsElement != nullptr)
    {
        for (auto* channelElement : channelsElement->getChildWithTagNameIterator("channel"))
        {
            juce::String channelName = channelElement->getStringAttribute("name");
            kit.channels.push_back(channelName);
        }
        DBG("  Channels: " + juce::String(static_cast<int>(kit.channels.size())));
    }
    
    // Parse instruments
    auto* instrumentsElement = xml->getChildByName("instruments");
    if (instrumentsElement != nullptr)
    {
        for (auto* instrElement : instrumentsElement->getChildWithTagNameIterator("instrument"))
        {
            Instrument instr;
            instr.name = instrElement->getStringAttribute("name");
            instr.xmlFile = instrElement->getStringAttribute("file");
            instr.group = instrElement->getStringAttribute("group");
            
            // Find main output channels from channelmap
            findMainChannels(instrElement, instr);
            
            kit.instruments[instr.name] = instr;
            DBG("  Found instrument: " + instr.name + " (channels L=" + 
                juce::String(instr.mainChannelL) + ", R=" + juce::String(instr.mainChannelR) + ")");
        }
    }
    
    return true;
}

void DrumGizmoParser::findMainChannels(juce::XmlElement* instrumentElement, Instrument& instrument)
{
    // Default to first two channels (AmbL/AmbR which are typically channels 0 and 1)
    instrument.mainChannelL = 0;
    instrument.mainChannelR = 1;
    
    // Look for channelmap entries with main="true"
    int mainChannelIndex = 0;
    bool foundLeft = false;
    bool foundRight = false;
    
    for (auto* channelMap : instrumentElement->getChildWithTagNameIterator("channelmap"))
    {
        juce::String inChannel = channelMap->getStringAttribute("in");
        bool isMain = channelMap->getBoolAttribute("main", false);
        
        if (isMain)
        {
            // Find the index of this channel in the kit's channel list
            int channelIndex = 0;
            for (const auto& ch : kit.channels)
            {
                if (ch == inChannel)
                {
                    if (!foundLeft)
                    {
                        instrument.mainChannelL = channelIndex;
                        foundLeft = true;
                    }
                    else if (!foundRight)
                    {
                        instrument.mainChannelR = channelIndex;
                        foundRight = true;
                    }
                    break;
                }
                channelIndex++;
            }
        }
        mainChannelIndex++;
    }
    
    // If only one main channel found, duplicate it for mono
    if (foundLeft && !foundRight)
    {
        instrument.mainChannelR = instrument.mainChannelL;
    }
}

bool DrumGizmoParser::parseMidimapXml(const juce::File& midimapFile)
{
    DBG("Parsing MIDI map: " + midimapFile.getFullPathName());
    
    auto xml = juce::XmlDocument::parse(midimapFile);
    if (xml == nullptr || !xml->hasTagName("midimap"))
    {
        DBG("ERROR: Invalid midimap XML format");
        return false;
    }
    
    kit.midiMap.clear();
    
    for (auto* mapElement : xml->getChildWithTagNameIterator("map"))
    {
        MidiMapEntry entry;
        entry.midiNote = mapElement->getIntAttribute("note");
        entry.instrumentName = mapElement->getStringAttribute("instr");
        
        kit.midiMap.push_back(entry);
        DBG("  MIDI " + juce::String(entry.midiNote) + " -> " + entry.instrumentName);
    }
    
    return true;
}

bool DrumGizmoParser::parseInstrumentXml(const juce::File& instrumentXmlFile, Instrument& instrument)
{
    DBG("Parsing instrument: " + instrumentXmlFile.getFullPathName());
    
    auto xml = juce::XmlDocument::parse(instrumentXmlFile);
    if (xml == nullptr || !xml->hasTagName("instrument"))
    {
        DBG("ERROR: Invalid instrument XML format");
        return false;
    }
    
    instrument.samples.clear();
    
    // Get the instrument's directory for relative paths
    juce::File instrumentDir = instrumentXmlFile.getParentDirectory();
    juce::String instrumentDirName = instrumentDir.getFileName();
    
    auto* samplesElement = xml->getChildByName("samples");
    if (samplesElement != nullptr)
    {
        for (auto* sampleElement : samplesElement->getChildWithTagNameIterator("sample"))
        {
            Sample sample;
            sample.name = sampleElement->getStringAttribute("name");
            sample.power = static_cast<float>(sampleElement->getDoubleAttribute("power", 1.0));
            
            // Get the sample file path from the first audiofile element
            auto* audioFile = sampleElement->getChildByName("audiofile");
            if (audioFile != nullptr)
            {
                juce::String relativePath = audioFile->getStringAttribute("file");
                // Build full relative path from kit root
                sample.filePath = instrumentDirName + "/" + relativePath;
            }
            
            if (sample.filePath.isNotEmpty())
            {
                instrument.samples.push_back(sample);
            }
        }
    }
    
    DBG("  Loaded " + juce::String(static_cast<int>(instrument.samples.size())) + " samples");
    
    // Sort samples by power and calculate velocity ranges
    calculateVelocityRanges(instrument);
    
    return !instrument.samples.empty();
}

void DrumGizmoParser::calculateVelocityRanges(Instrument& instrument)
{
    if (instrument.samples.empty())
        return;
    
    // Sort samples by power (lowest to highest)
    std::sort(instrument.samples.begin(), instrument.samples.end(),
              [](const Sample& a, const Sample& b) { return a.power < b.power; });
    
    // Get power range
    float minPower = instrument.samples.front().power;
    float maxPower = instrument.samples.back().power;
    float powerRange = maxPower - minPower;
    
    if (powerRange <= 0.0f)
    {
        // All samples have same power - use all velocities
        for (auto& sample : instrument.samples)
        {
            sample.loVel = 1;
            sample.hiVel = 127;
        }
        return;
    }
    
    // Map power to velocity ranges (1-127)
    int numSamples = static_cast<int>(instrument.samples.size());
    
    for (int i = 0; i < numSamples; ++i)
    {
        auto& sample = instrument.samples[i];
        
        // Calculate normalized position (0.0 to 1.0)
        float normalizedPower = (sample.power - minPower) / powerRange;
        
        // Map to velocity (1-127)
        int centerVel = 1 + static_cast<int>(normalizedPower * 126.0f);
        
        // Calculate range boundaries
        if (i == 0)
        {
            sample.loVel = 1;
        }
        else
        {
            // Midpoint between this and previous sample
            float prevNorm = (instrument.samples[i - 1].power - minPower) / powerRange;
            int prevCenter = 1 + static_cast<int>(prevNorm * 126.0f);
            sample.loVel = (prevCenter + centerVel) / 2 + 1;
        }
        
        if (i == numSamples - 1)
        {
            sample.hiVel = 127;
        }
        else
        {
            // Midpoint between this and next sample
            float nextNorm = (instrument.samples[i + 1].power - minPower) / powerRange;
            int nextCenter = 1 + static_cast<int>(nextNorm * 126.0f);
            sample.hiVel = (centerVel + nextCenter) / 2;
        }
        
        // Ensure valid range
        sample.loVel = juce::jmax(1, sample.loVel);
        sample.hiVel = juce::jmin(127, sample.hiVel);
        if (sample.loVel > sample.hiVel)
            sample.loVel = sample.hiVel;
    }
}

const DrumGizmoParser::Instrument* DrumGizmoParser::getInstrumentForNote(int midiNote) const
{
    auto it = noteToInstrument.find(midiNote);
    if (it == noteToInstrument.end())
        return nullptr;
    
    auto instrIt = kit.instruments.find(it->second);
    if (instrIt == kit.instruments.end())
        return nullptr;
    
    return &instrIt->second;
}

const DrumGizmoParser::Sample* DrumGizmoParser::getSampleForNoteAndVelocity(int midiNote, int velocity) const
{
    const Instrument* instr = getInstrumentForNote(midiNote);
    if (instr == nullptr || instr->samples.empty())
        return nullptr;
    
    // Clamp velocity
    velocity = juce::jlimit(1, 127, velocity);
    
    // Find sample matching velocity
    for (const auto& sample : instr->samples)
    {
        if (velocity >= sample.loVel && velocity <= sample.hiVel)
        {
            return &sample;
        }
    }
    
    // Fallback to last sample (highest velocity)
    return &instr->samples.back();
}

juce::String DrumGizmoParser::getSamplePath(int midiNote, int velocity) const
{
    const Sample* sample = getSampleForNoteAndVelocity(midiNote, velocity);
    if (sample != nullptr)
        return sample->filePath;
    
    return {};
}

std::vector<int> DrumGizmoParser::getMappedNotes() const
{
    std::vector<int> notes;
    notes.reserve(kit.midiMap.size());
    
    for (const auto& mapping : kit.midiMap)
    {
        notes.push_back(mapping.midiNote);
    }
    
    return notes;
}
