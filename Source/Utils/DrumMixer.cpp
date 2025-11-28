#include "DrumMixer.h"
#include "SimpleEQ.h"

DrumMixer::DrumMixer()
{
}

void DrumMixer::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    for (auto& channel : channels)
    {
        channel.prepareToPlay(sampleRate, maximumBlockSize);
    }

    masterReverb.prepareToPlay(sampleRate, maximumBlockSize);

    reverbSendBuffer.setSize(2, maximumBlockSize);
    tempBuffer.setSize(2, maximumBlockSize);

    // Initialize 8-band master EQ filters
    eqSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(maximumBlockSize);
    spec.numChannels = 2;

    for (int i = 0; i < 8; ++i)
    {
        masterEQFilters[i].prepare(spec);
        updateMasterEQFilter(i);
    }
}

void DrumMixer::processPerPartBuffers(std::array<juce::AudioBuffer<float>, NumParts>& partBuffers,
                                      juce::AudioBuffer<float>& outputBuffer)
{
    // Clear output and reverb send
    outputBuffer.clear();
    reverbSendBuffer.clear();

    // Check if any channels are soloed
    bool anySolo = isAnySoloed();

    // Process each drum part through its own channel
    for (int i = 0; i < NumParts; ++i)
    {
        auto& partBuffer = partBuffers[i];
        auto& channel = channels[i];

        // Skip if this part has no audio
        if (partBuffer.getNumSamples() == 0)
            continue;

        // Apply solo logic: if any channel is soloed, skip non-soloed channels
        if (anySolo && !channel.isSoloed())
        {
            continue;
        }

        // Skip muted channels
        if (channel.isMuted())
        {
            continue;
        }

        // Copy to temp buffer for processing (so we don't modify the original)
        tempBuffer.makeCopyOf(partBuffer);

        // Process this part through its channel (EQ, volume, pan, reverb send)
        channel.processBlock(tempBuffer, reverbSendBuffer);

        // Add processed part to main output
        for (int ch = 0; ch < juce::jmin(outputBuffer.getNumChannels(), tempBuffer.getNumChannels()); ++ch)
        {
            outputBuffer.addFrom(ch, 0, tempBuffer, ch, 0, outputBuffer.getNumSamples());
        }
    }

    // Process master reverb if enabled (auto-enable when any send > 0)
    bool hasReverbSend = false;
    for (int i = 0; i < NumParts; ++i)
    {
        if (channels[i].getReverbSend() > 0.001f)
        {
            hasReverbSend = true;
            break;
        }
    }

    if (hasReverbSend)
    {
        masterReverb.processBlock(reverbSendBuffer);

        // Mix reverb into main output
        for (int ch = 0; ch < juce::jmin(outputBuffer.getNumChannels(), reverbSendBuffer.getNumChannels()); ++ch)
        {
            outputBuffer.addFrom(ch, 0, reverbSendBuffer, ch, 0, outputBuffer.getNumSamples());
        }
    }

    // Apply 8-band master EQ if enabled
    if (masterEQEnabled)
    {
        juce::dsp::AudioBlock<float> block(outputBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        for (int i = 0; i < 8; ++i)
        {
            masterEQFilters[i].process(context);
        }
    }

    // Apply master volume (allows boost up to 4x/400%)
    outputBuffer.applyGain(masterVolume);
}

void DrumMixer::processBlock(juce::AudioBuffer<float>& buffer)
{
    // Legacy single-buffer processing (for backwards compatibility)
    // Note: This doesn't provide true per-part processing since all audio is already mixed

    reverbSendBuffer.clear();

    // Check if any channels are soloed
    bool anySolo = isAnySoloed();

    // Process each channel (but they all process the same mixed buffer)
    for (auto& channel : channels)
    {
        if (anySolo && !channel.isSoloed())
            continue;

        if (channel.isMuted())
            continue;

        channel.processBlock(buffer, reverbSendBuffer);
    }

    // Process master reverb if enabled
    if (reverbEnabled)
    {
        masterReverb.processBlock(reverbSendBuffer);

        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), reverbSendBuffer.getNumChannels()); ++ch)
        {
            buffer.addFrom(ch, 0, reverbSendBuffer, ch, 0, buffer.getNumSamples());
        }
    }

    // Apply 8-band master EQ if enabled
    if (masterEQEnabled)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        for (int i = 0; i < 8; ++i)
        {
            masterEQFilters[i].process(context);
        }
    }

    // Apply master volume
    buffer.applyGain(masterVolume);
}

void DrumMixer::reset()
{
    for (auto& channel : channels)
    {
        channel.reset();
    }

    masterReverb.reset();
    reverbSendBuffer.clear();
    tempBuffer.clear();

    // Reset master EQ filters
    for (auto& filter : masterEQFilters)
    {
        filter.reset();
    }
}

void DrumMixer::setMasterVolume(float linearGain)
{
    // Allow gain up to 4.0 (400%) for boosting quiet samples
    masterVolume = juce::jlimit(0.0f, 4.0f, linearGain);
}

bool DrumMixer::isAnySoloed() const
{
    for (const auto& channel : channels)
    {
        if (channel.isSoloed())
            return true;
    }
    return false;
}

void DrumMixer::setMasterEQGain(int band, float gainDb)
{
    if (band >= 0 && band < 8)
    {
        masterEQGains[band] = juce::jlimit(-12.0f, 12.0f, gainDb);
        updateMasterEQFilter(band);
    }
}

float DrumMixer::getMasterEQGain(int band) const
{
    if (band >= 0 && band < 8)
        return masterEQGains[band];
    return 0.0f;
}

void DrumMixer::updateMasterEQFilter(int band)
{
    if (band < 0 || band >= 8 || eqSampleRate <= 0)
        return;

    float freq = masterEQFreqs[band];
    float gain = masterEQGains[band];
    float q = 1.0f;  // Moderate Q for graphic EQ

    // Use peak filter for all bands (graphic EQ style)
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        eqSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));

    *masterEQFilters[band].state = *coeffs;
}

// ==========================================================================
// CORRECTED: getDrumPartForNote matching salamanderDrumkit SFZ mapping
// ==========================================================================
DrumMixer::DrumPart DrumMixer::getDrumPartForNote(int midiNote)
{
    // Kick1: 36 (bass drum 1 / primary kick)
    if (midiNote == 36)
        return Kick1;

    // Kick2: 35 (bass drum 2 / acoustic bass drum)
    if (midiNote == 35)
        return Kick2;

    // Snare: 37-41 (all snare variations in salamanderDrumkit)
    // 37=Snare2 OFF, 38=Snare2/rim, 39=Snare OFF, 40=Snare1, 41=Snare Stick
    if (midiNote >= 37 && midiNote <= 41)
        return Snare;

    // Hi-Hat: 42, 44, 46 (closed, pedal, open)
    if (midiNote == 42 || midiNote == 44 || midiNote == 46)
        return HiHat;

    // Toms: 43, 45 (only actual toms in salamanderDrumkit)
    // 43=Lo Tom (Floor Tom), 45=Hi Tom
    if (midiNote == 43 || midiNote == 45)
        return Toms;

    // Rides: 48-53 (all ride variations in salamanderDrumkit)
    // 48=Ride2 bow, 49=Ride2 bell, 50=Ride2 crash, 51=Ride2 choke
    // 52=Ride1 bow, 53=Ride1 bell
    if (midiNote >= 48 && midiNote <= 53)
        return Rides;

    // Crashes/Chinas/Splashes/Effects: 54-64 and Cowbell (47)
    // 47=Cowbell (grouped with effects for mixing purposes)
    // 54=crash1 choke, 55=crash1, 56=crash2 choke, 57=crash2
    // 58=china1 choke, 59=china1, 60=china2, 61=china2 choke
    // 62=crash3, 63=splash, 64=bellchime
    if (midiNote == 47 || (midiNote >= 54 && midiNote <= 64))
        return CrashChina;

    // Default to CrashChina for any unmapped notes (usually high cymbals)
    return CrashChina;
}

juce::String DrumMixer::getPartName(DrumPart part)
{
    switch (part)
    {
        case Kick1:      return "Kick 1";
        case Kick2:      return "Kick 2";
        case Snare:      return "Snare";
        case HiHat:      return "Hi-Hat";
        case Toms:       return "Toms";
        case CrashChina: return "Crash";
        case Rides:      return "Rides";
        default:         return "Unknown";
    }
}

juce::String DrumMixer::getPartName(int partIndex)
{
    if (partIndex >= 0 && partIndex < NumParts)
        return getPartName(static_cast<DrumPart>(partIndex));
    return "Unknown";
}

juce::ValueTree DrumMixer::saveState() const
{
    juce::ValueTree state("DrumMixer");

    state.setProperty("masterVolume", masterVolume, nullptr);
    state.setProperty("reverbEnabled", reverbEnabled, nullptr);
    state.setProperty("masterEQEnabled", masterEQEnabled, nullptr);
    state.setProperty("currentPresetName", currentPresetName, nullptr);

    // Save 8-band master EQ gains
    for (int i = 0; i < 8; ++i)
    {
        state.setProperty("masterEQBand" + juce::String(i), masterEQGains[i], nullptr);
    }

    // Save reverb settings
    juce::ValueTree reverbState("Reverb");
    reverbState.setProperty("roomSize", masterReverb.getRoomSize(), nullptr);
    reverbState.setProperty("damping", masterReverb.getDamping(), nullptr);
    reverbState.setProperty("wetLevel", masterReverb.getWetLevel(), nullptr);
    reverbState.setProperty("width", masterReverb.getWidth(), nullptr);
    state.appendChild(reverbState, nullptr);

    // Save each channel
    for (int i = 0; i < NumParts; ++i)
    {
        const auto& channel = channels[i];
        juce::ValueTree channelState("Channel");
        channelState.setProperty("index", i, nullptr);
        channelState.setProperty("volume", channel.getVolume(), nullptr);
        channelState.setProperty("pan", channel.getPan(), nullptr);
        channelState.setProperty("reverbSend", channel.getReverbSend(), nullptr);
        channelState.setProperty("solo", channel.isSoloed(), nullptr);
        channelState.setProperty("mute", channel.isMuted(), nullptr);

        // Save SimpleEQ settings (3-band)
        const auto& eq = channel.getEQ();
        channelState.setProperty("eqLowGain", eq.getGain(SimpleEQ::Low), nullptr);
        channelState.setProperty("eqMidGain", eq.getGain(SimpleEQ::Mid), nullptr);
        channelState.setProperty("eqHighGain", eq.getGain(SimpleEQ::High), nullptr);

        // Save Compressor settings
        const auto& comp = channel.getCompressor();
        channelState.setProperty("compEnabled", comp.isEnabled(), nullptr);
        channelState.setProperty("compThreshold", comp.getThreshold(), nullptr);
        channelState.setProperty("compRatio", comp.getRatio(), nullptr);
        channelState.setProperty("compAttack", comp.getAttack(), nullptr);
        channelState.setProperty("compRelease", comp.getRelease(), nullptr);
        channelState.setProperty("compMakeup", comp.getMakeupGain(), nullptr);

        state.appendChild(channelState, nullptr);
    }

    return state;
}

void DrumMixer::restoreState(const juce::ValueTree& state)
{
    if (!state.isValid() || !state.hasType("DrumMixer"))
        return;

    masterVolume = static_cast<float>(state.getProperty("masterVolume", 1.0f));
    reverbEnabled = static_cast<bool>(state.getProperty("reverbEnabled", false));  // Default OFF
    masterEQEnabled = static_cast<bool>(state.getProperty("masterEQEnabled", false));
    currentPresetName = state.getProperty("currentPresetName", "").toString();

    // Restore 8-band master EQ gains
    for (int i = 0; i < 8; ++i)
    {
        masterEQGains[i] = static_cast<float>(state.getProperty("masterEQBand" + juce::String(i), 0.0f));
        updateMasterEQFilter(i);  // Update filter coefficients
    }

    // Restore reverb settings
    auto reverbState = state.getChildWithName("Reverb");
    if (reverbState.isValid())
    {
        masterReverb.setRoomSize(static_cast<float>(reverbState.getProperty("roomSize", 0.5f)));
        masterReverb.setDamping(static_cast<float>(reverbState.getProperty("damping", 0.5f)));
        masterReverb.setWetLevel(static_cast<float>(reverbState.getProperty("wetLevel", 0.33f)));
        masterReverb.setWidth(static_cast<float>(reverbState.getProperty("width", 1.0f)));
    }

    // Restore each channel
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto channelState = state.getChild(i);
        if (!channelState.hasType("Channel"))
            continue;

        int index = static_cast<int>(channelState.getProperty("index", -1));
        if (index < 0 || index >= NumParts)
            continue;

        auto& channel = channels[index];
        channel.setVolume(static_cast<float>(channelState.getProperty("volume", 0.8f)));
        channel.setPan(static_cast<float>(channelState.getProperty("pan", 0.0f)));
        channel.setReverbSend(static_cast<float>(channelState.getProperty("reverbSend", 0.0f)));
        channel.setSolo(static_cast<bool>(channelState.getProperty("solo", false)));
        channel.setMute(static_cast<bool>(channelState.getProperty("mute", false)));

        // Restore SimpleEQ settings (3-band)
        auto& eq = channel.getEQ();
        eq.setGain(SimpleEQ::Low, static_cast<float>(channelState.getProperty("eqLowGain", 0.0f)));
        eq.setGain(SimpleEQ::Mid, static_cast<float>(channelState.getProperty("eqMidGain", 0.0f)));
        eq.setGain(SimpleEQ::High, static_cast<float>(channelState.getProperty("eqHighGain", 0.0f)));

        // Restore Compressor settings
        auto& comp = channel.getCompressor();
        comp.setEnabled(static_cast<bool>(channelState.getProperty("compEnabled", false)));
        comp.setThreshold(static_cast<float>(channelState.getProperty("compThreshold", -20.0f)));
        comp.setRatio(static_cast<float>(channelState.getProperty("compRatio", 4.0f)));
        comp.setAttack(static_cast<float>(channelState.getProperty("compAttack", 10.0f)));
        comp.setRelease(static_cast<float>(channelState.getProperty("compRelease", 100.0f)));
        comp.setMakeupGain(static_cast<float>(channelState.getProperty("compMakeup", 0.0f)));
    }
}
