#include "EQPresetManager.h"

EQPresetManager::EQPresetManager()
{
    // Use same location as favorites and mappings (platform-specific application data)
    presetsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
    .getChildFile("DrumGroovePro")
    .getChildFile("MixerPresets");

    if (!presetsDir.exists())
        presetsDir.createDirectory();
}

juce::File EQPresetManager::getPresetsDirectory() const
{
    return presetsDir;
}

juce::File EQPresetManager::getPresetFile(const juce::String& name) const
{
    // Sanitize the name for use as a filename
    juce::String safeName = name.replaceCharacters("\\/:*?\"<>|", "_________");
    return presetsDir.getChildFile(safeName + ".xml");
}

void EQPresetManager::savePreset(const EQPreset& preset)
{
    if (preset.name.isEmpty())
        return;

    juce::XmlElement root("MixerPreset");
    root.setAttribute("name", preset.name);
    root.setAttribute("version", 3);  // Version 3: 7 channels with SimpleEQ and Compressor

    // Master settings
    root.setAttribute("masterVolume", preset.masterVolume);
    root.setAttribute("masterEQEnabled", preset.masterEQEnabled);

    // 8-band Master EQ gains
    auto* masterEQElement = root.createNewChildElement("MasterEQ");
    for (int i = 0; i < 8; ++i)
    {
        masterEQElement->setAttribute("band" + juce::String(i), preset.masterEQGains[i]);
    }

    // Reverb settings
    auto* reverbElement = root.createNewChildElement("Reverb");
    reverbElement->setAttribute("roomSize", preset.reverbRoomSize);
    reverbElement->setAttribute("damping", preset.reverbDamping);
    reverbElement->setAttribute("wetLevel", preset.reverbWetLevel);

    // 7 Channel settings
    auto* channelsElement = root.createNewChildElement("Channels");
    for (int ch = 0; ch < 7; ++ch)
    {
        const auto& channel = preset.channels[ch];
        auto* channelElement = channelsElement->createNewChildElement("Channel");
        channelElement->setAttribute("index", ch);

        // Mixer settings
        channelElement->setAttribute("volume", channel.volume);
        channelElement->setAttribute("pan", channel.pan);
        channelElement->setAttribute("reverbSend", channel.reverbSend);

        // SimpleEQ (3-band)
        channelElement->setAttribute("eqLowGain", channel.eqLowGain);
        channelElement->setAttribute("eqMidGain", channel.eqMidGain);
        channelElement->setAttribute("eqHighGain", channel.eqHighGain);

        // Compressor
        channelElement->setAttribute("compEnabled", channel.compEnabled);
        channelElement->setAttribute("compThreshold", channel.compThreshold);
        channelElement->setAttribute("compRatio", channel.compRatio);
        channelElement->setAttribute("compAttack", channel.compAttack);
        channelElement->setAttribute("compRelease", channel.compRelease);
        channelElement->setAttribute("compMakeup", channel.compMakeup);
    }

    // Save to file
    juce::File presetFile = getPresetFile(preset.name);
    root.writeTo(presetFile);
}

EQPresetManager::EQPreset EQPresetManager::loadPreset(const juce::String& name)
{
    EQPreset preset;
    juce::File presetFile = getPresetFile(name);

    if (!presetFile.existsAsFile())
        return preset;  // Return empty preset

        auto xml = juce::XmlDocument::parse(presetFile);
    if (xml == nullptr)
        return preset;

    // Check for both old and new format
    bool isNewFormat = xml->hasTagName("MixerPreset");
    bool isOldFormat = xml->hasTagName("EQPreset");

    if (!isNewFormat && !isOldFormat)
        return preset;

    preset.name = xml->getStringAttribute("name", name);
    preset.version = xml->getIntAttribute("version", 1);
    preset.masterVolume = static_cast<float>(xml->getDoubleAttribute("masterVolume", 1.0));
    preset.masterEQEnabled = xml->getBoolAttribute("masterEQEnabled", false);

    // Master EQ
    if (auto* masterEQ = xml->getChildByName("MasterEQ"))
    {
        // New format also stores enabled in root
        for (int i = 0; i < 8; ++i)
        {
            preset.masterEQGains[i] = static_cast<float>(
                masterEQ->getDoubleAttribute("band" + juce::String(i), 0.0));
        }
    }

    // Reverb
    if (auto* reverb = xml->getChildByName("Reverb"))
    {
        preset.reverbRoomSize = static_cast<float>(reverb->getDoubleAttribute("roomSize", 0.5));
        preset.reverbDamping = static_cast<float>(reverb->getDoubleAttribute("damping", 0.5));
        preset.reverbWetLevel = static_cast<float>(reverb->getDoubleAttribute("wetLevel", 0.33));
    }

    // Channels
    if (auto* channels = xml->getChildByName("Channels"))
    {
        for (auto* channelElement : channels->getChildIterator())
        {
            int index = channelElement->getIntAttribute("index", -1);
            if (index < 0 || index >= 7)
                continue;

            auto& channel = preset.channels[index];

            // Mixer settings
            channel.volume = static_cast<float>(channelElement->getDoubleAttribute("volume", 0.8));
            channel.pan = static_cast<float>(channelElement->getDoubleAttribute("pan", 0.0));
            channel.reverbSend = static_cast<float>(channelElement->getDoubleAttribute("reverbSend", 0.0));

            // SimpleEQ (3-band) - new format
            channel.eqLowGain = static_cast<float>(channelElement->getDoubleAttribute("eqLowGain", 0.0));
            channel.eqMidGain = static_cast<float>(channelElement->getDoubleAttribute("eqMidGain", 0.0));
            channel.eqHighGain = static_cast<float>(channelElement->getDoubleAttribute("eqHighGain", 0.0));

            // Try loading from old format if new values are zero (backward compatibility)
            if (preset.version < 3)
            {
                // Old format had lowShelfGain, etc. - convert to simple EQ
                float oldLow = static_cast<float>(channelElement->getDoubleAttribute("lowShelfGain", 0.0));
                float oldHigh = static_cast<float>(channelElement->getDoubleAttribute("highShelfGain", 0.0));
                float oldPeak1 = static_cast<float>(channelElement->getDoubleAttribute("peak1Gain", 0.0));

                if (channel.eqLowGain == 0.0f) channel.eqLowGain = oldLow;
                if (channel.eqHighGain == 0.0f) channel.eqHighGain = oldHigh;
                if (channel.eqMidGain == 0.0f) channel.eqMidGain = oldPeak1;
            }

            // Compressor settings
            channel.compEnabled = channelElement->getBoolAttribute("compEnabled", false);
            channel.compThreshold = static_cast<float>(channelElement->getDoubleAttribute("compThreshold", -20.0));
            channel.compRatio = static_cast<float>(channelElement->getDoubleAttribute("compRatio", 4.0));
            channel.compAttack = static_cast<float>(channelElement->getDoubleAttribute("compAttack", 10.0));
            channel.compRelease = static_cast<float>(channelElement->getDoubleAttribute("compRelease", 100.0));
            channel.compMakeup = static_cast<float>(channelElement->getDoubleAttribute("compMakeup", 0.0));
        }
    }

    return preset;
}

void EQPresetManager::deletePreset(const juce::String& name)
{
    juce::File presetFile = getPresetFile(name);
    if (presetFile.existsAsFile())
        presetFile.deleteFile();
}

bool EQPresetManager::presetExists(const juce::String& name) const
{
    juce::File presetFile = getPresetFile(name);
    return presetFile.existsAsFile();
}

juce::StringArray EQPresetManager::getPresetList() const
{
    juce::StringArray names;

    auto files = presetsDir.findChildFiles(juce::File::findFiles, false, "*.xml");

    for (const auto& file : files)
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr && (xml->hasTagName("MixerPreset") || xml->hasTagName("EQPreset")))
        {
            juce::String name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
            names.add(name);
        }
    }

    names.sort(true);
    return names;
}
