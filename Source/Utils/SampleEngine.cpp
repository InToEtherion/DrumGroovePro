#include "SampleEngine.h"
#include <cmath>

SampleEngine::SampleEngine()
{
    // Register audio formats for loading WAV files
    formatManager.registerBasicFormats();
}

SampleEngine::~SampleEngine()
{
    unloadSamples();
}

SampleEngine::LibraryFormat SampleEngine::detectLibraryFormat(const juce::File& libraryDir) const
{
    // Check for DrumGizmo format (has KitName.xml matching directory name)
    juce::String kitName = libraryDir.getFileName();
    juce::File kitXml = libraryDir.getChildFile(kitName + ".xml");
    juce::File midimapXml = libraryDir.getChildFile("Midimap.xml");

    if (kitXml.existsAsFile() && midimapXml.existsAsFile())
    {
        DBG("Detected DrumGizmo format (found " + kitName + ".xml and Midimap.xml)");
        return LibraryFormat::DrumGizmo;
    }

    // Check for SFZ format (has ALL.sfz file)
    juce::File sfzFile = libraryDir.getChildFile("ALL.sfz");
    if (sfzFile.existsAsFile())
    {
        DBG("Detected SFZ format (found ALL.sfz)");
        return LibraryFormat::SFZ;
    }

    return LibraryFormat::Unknown;
}

bool SampleEngine::loadLibraryByName(const juce::String& libraryName)
{
    DBG("=== SampleEngine::loadLibraryByName ===");
    DBG("Library name: " + libraryName);

    juce::File baseDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("DrumGroovePro").getChildFile("Samples");

    if (!baseDir.exists())
    {
        DBG("ERROR: Samples base directory not found: " + baseDir.getFullPathName());
        return false;
    }

    // Find the directory matching the library name
    juce::File libraryDir = baseDir.getChildFile(libraryName);

    if (!libraryDir.exists() || !libraryDir.isDirectory())
    {
        DBG("ERROR: Library directory not found: " + libraryDir.getFullPathName());
        return false;
    }

    // Detect format
    LibraryFormat format = detectLibraryFormat(libraryDir);

    bool success = false;

    switch (format)
    {
        case LibraryFormat::SFZ:
            success = loadSFZLibrary(libraryDir);
            break;

        case LibraryFormat::DrumGizmo:
            success = loadDrumGizmoLibrary(libraryDir);
            break;

        default:
            DBG("ERROR: Unknown library format for: " + libraryName);
            return false;
    }

    if (success)
    {
        currentLibraryName = libraryName;
        currentFormat = format;
    }

    return success;
}

bool SampleEngine::loadSFZLibrary(const juce::File& libraryDir)
{
    DBG("Loading SFZ library from: " + libraryDir.getFullPathName());
    return loadSamplesFromDirectory(libraryDir);
}

bool SampleEngine::loadSamplesFromDirectory(const juce::File& samplesDirectory)
{
    DBG("=== SampleEngine::loadSamplesFromDirectory ===");
    DBG("Directory: " + samplesDirectory.getFullPathName());

    if (!samplesDirectory.exists() || !samplesDirectory.isDirectory())
    {
        DBG("ERROR: Samples directory not found");
        return false;
    }

    // Unload any existing samples
    unloadSamples();

    samplesBaseDirectory = samplesDirectory;

    // Find and parse the ALL.sfz file
    auto sfzFile = samplesDirectory.getChildFile("ALL.sfz");
    if (!sfzFile.existsAsFile())
    {
        DBG("ERROR: ALL.sfz not found");
        return false;
    }

    // Parse the SFZ file
    if (!sfzParser.parseFile(sfzFile))
    {
        DBG("ERROR: Failed to parse SFZ file");
        return false;
    }

    DBG("SFZ parsed successfully:");
    DBG("  Total regions: " + juce::String(sfzParser.getRegionCount()));
    DBG("  Unique notes: " + juce::String(sfzParser.getUniqueNoteCount()));

    // Load all unique samples referenced in the SFZ
    int loadedCount = 0;
    int failedCount = 0;

    for (const auto& region : sfzParser.getAllRegions())
    {
        if (region.samplePath.isEmpty())
            continue;

        // Check if already loaded
        if (loadedSamples.find(region.samplePath) != loadedSamples.end())
        {
            loadedSamples[region.samplePath].referenceCount++;
            continue;
        }

        // Load the sample
        if (loadSample(region.samplePath))
        {
            loadedCount++;
        }
        else
        {
            failedCount++;
        }
    }

    DBG("Sample loading complete:");
    DBG("  Loaded: " + juce::String(loadedCount));
    DBG("  Failed: " + juce::String(failedCount));

    samplesLoaded = (loadedCount > 0);
    return samplesLoaded;
}

bool SampleEngine::loadDrumGizmoLibrary(const juce::File& libraryDir)
{
    DBG("=== SampleEngine::loadDrumGizmoLibrary ===");
    DBG("Directory: " + libraryDir.getFullPathName());

    // Unload any existing samples
    unloadSamples();

    samplesBaseDirectory = libraryDir;

    // Parse the DrumGizmo kit
    if (!drumGizmoParser.parseKit(libraryDir))
    {
        DBG("ERROR: Failed to parse DrumGizmo kit");
        return false;
    }

    const auto& kit = drumGizmoParser.getKit();

    DBG("DrumGizmo kit parsed successfully:");
    DBG("  Kit name: " + kit.name);
    DBG("  Instruments: " + juce::String(kit.instruments.size()));

    // Load all samples from all instruments
    int loadedCount = 0;
    int failedCount = 0;

    for (const auto& [name, instrument] : kit.instruments)
    {
        for (const auto& sample : instrument.samples)
        {
            if (sample.filePath.isEmpty())
                continue;

            // Check if already loaded
            if (loadedSamples.find(sample.filePath) != loadedSamples.end())
            {
                loadedSamples[sample.filePath].referenceCount++;
                continue;
            }

            // Load the sample (DrumGizmo WAVs should already be stereo after conversion)
            if (loadSample(sample.filePath))
            {
                loadedCount++;
            }
            else
            {
                failedCount++;
            }
        }
    }

    DBG("DrumGizmo sample loading complete:");
    DBG("  Loaded: " + juce::String(loadedCount));
    DBG("  Failed: " + juce::String(failedCount));

    samplesLoaded = (loadedCount > 0);
    return samplesLoaded;
}

bool SampleEngine::loadSample(const juce::String& relativePath)
{
    // Build full path
    auto sampleFile = samplesBaseDirectory.getChildFile(relativePath);

    if (!sampleFile.existsAsFile())
    {
        DBG("Sample file not found: " + sampleFile.getFullPathName());
        return false;
    }

    // Create reader for the WAV file
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(sampleFile));

    if (reader == nullptr)
    {
        DBG("Failed to create reader for: " + sampleFile.getFullPathName());
        return false;
    }

    // Create buffer and load the sample
    LoadedSample sample;
    sample.filePath = sampleFile.getFullPathName();
    sample.referenceCount = 1;

    int numChannels = static_cast<int>(reader->numChannels);
    int numSamples = static_cast<int>(reader->lengthInSamples);

    sample.buffer.setSize(numChannels, numSamples);
    reader->read(&sample.buffer, 0, numSamples, 0, true, true);

    // Store in map
    loadedSamples[relativePath] = std::move(sample);

    return true;
}

const juce::AudioBuffer<float>* SampleEngine::getSampleBuffer(const juce::String& relativePath)
{
    auto it = loadedSamples.find(relativePath);
    if (it != loadedSamples.end())
        return &it->second.buffer;

    return nullptr;
}

void SampleEngine::unloadSamples()
{
    stopAllVoices();
    loadedSamples.clear();
    samplesLoaded = false;
    currentLibraryName.clear();
    currentFormat = LibraryFormat::Unknown;
    lastKickWas35 = false;  // Reset alternation state
    lastRoundRobinIndex.clear();  // Reset round robin state
    pendingNotes.clear();  // Clear any pending humanized notes
    DBG("All samples unloaded");
}

void SampleEngine::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = maximumExpectedSamplesPerBlock;
    stopAllVoices();
    pendingNotes.clear();  // Clear any pending humanized notes

    // Initialize per-part buffers
    for (auto& buffer : partBuffers)
    {
        buffer.setSize(2, maximumExpectedSamplesPerBlock);
    }
}

int SampleEngine::getActualNoteToPlay(int requestedNote)
{
    // If kick alternation is disabled, just return the requested note
    if (!kickAlternationEnabled)
        return requestedNote;

    // Only apply alternation to kick notes (35 and 36)
    if (requestedNote != 35 && requestedNote != 36)
        return requestedNote;

    // Alternate between 35 and 36
    int actualNote;
    if (lastKickWas35)
    {
        actualNote = 36;
        lastKickWas35 = false;
    }
    else
    {
        actualNote = 35;
        lastKickWas35 = true;
    }

    DBG("Kick Alternation: requested " + juce::String(requestedNote) +
    " -> playing " + juce::String(actualNote));

    return actualNote;
}

int SampleEngine::getDrumPartForNote(int midiNote)
{
    // Route note 36 to Kick1, note 35 to Kick2
    // All other notes use the standard DrumMixer mapping
    if (midiNote == 36)
        return static_cast<int>(DrumMixer::Kick1);
    if (midiNote == 35)
        return static_cast<int>(DrumMixer::Kick2);

    // For non-kick notes, use the standard mapping
    return static_cast<int>(DrumMixer::getDrumPartForNote(midiNote));
}

// =========================================================================
// HUMANIZATION HELPERS
// =========================================================================

int SampleEngine::applyVelocityHumanization(int velocity)
{
    if (velocityHumanization <= 0.0f)
        return velocity;

    // Calculate max variation based on humanization percentage
    float maxVariation = (velocityHumanization / 100.0f) * static_cast<float>(MAX_VELOCITY_VARIATION);

    // Generate random variation in range [-maxVariation, +maxVariation]
    float variation = (humanizationRandom.nextFloat() * 2.0f - 1.0f) * maxVariation;

    // Apply variation and clamp to valid MIDI velocity range
    int humanizedVelocity = velocity + static_cast<int>(std::round(variation));
    humanizedVelocity = juce::jlimit(1, 127, humanizedVelocity);

    if (humanizedVelocity != velocity)
    {
        DBG("Velocity Humanization: " + juce::String(velocity) + " -> " + juce::String(humanizedVelocity) +
        " (variation: " + juce::String(variation, 1) + ")");
    }

    return humanizedVelocity;
}

const SFZParser::Region* SampleEngine::getRegionWithRoundRobin(int midiNote, int velocity)
{
    // Get all matching regions for this note/velocity
    auto matchingRegions = sfzParser.getRegionsForNote(midiNote, velocity);

    if (matchingRegions.empty())
        return nullptr;

    if (matchingRegions.size() == 1)
        return matchingRegions[0];

    // Round robin amount determines how we select:
    // 0% = always first sample
    // 100% = full cycling/random through all samples

    if (roundRobinAmount <= 0.0f)
    {
        // No round robin - always use first matching region
        return matchingRegions[0];
    }

    // Get or initialize the round robin index for this note
    size_t& lastIndex = lastRoundRobinIndex[midiNote];

    // CRITICAL FIX: Ensure lastIndex is valid for current matchingRegions size
    // The number of matching regions can vary based on velocity, so we must
    // clamp the stored index to prevent out-of-bounds access
    if (lastIndex >= matchingRegions.size())
    {
        lastIndex = 0;
    }

    if (roundRobinAmount >= 100.0f)
    {
        // Full round robin - cycle through all samples
        lastIndex = (lastIndex + 1) % matchingRegions.size();
        return matchingRegions[lastIndex];
    }
    else
    {
        // Partial round robin - probabilistic selection
        // Higher roundRobinAmount = more likely to pick a different sample
        float probabilityToChange = roundRobinAmount / 100.0f;

        if (humanizationRandom.nextFloat() < probabilityToChange)
        {
            // Select a different sample randomly
            size_t newIndex;
            if (matchingRegions.size() == 2)
            {
                // Toggle between two samples
                newIndex = (lastIndex + 1) % 2;
            }
            else
            {
                // Pick random different sample
                do {
                    newIndex = static_cast<size_t>(humanizationRandom.nextInt(static_cast<int>(matchingRegions.size())));
                } while (newIndex == lastIndex && matchingRegions.size() > 1);
            }
            lastIndex = newIndex;
        }

        return matchingRegions[lastIndex];
    }
}

const DrumGizmoParser::Sample* SampleEngine::getSampleWithRoundRobin(int midiNote, int velocity)
{
    // Get the instrument for this note
    const DrumGizmoParser::Instrument* instrument = drumGizmoParser.getInstrumentForNote(midiNote);
    if (instrument == nullptr || instrument->samples.empty())
        return nullptr;

    // Find all samples that match the velocity range
    std::vector<const DrumGizmoParser::Sample*> matchingSamples;
    int clampedVelocity = juce::jlimit(1, 127, velocity);

    for (const auto& sample : instrument->samples)
    {
        if (clampedVelocity >= sample.loVel && clampedVelocity <= sample.hiVel)
        {
            matchingSamples.push_back(&sample);
        }
    }

    if (matchingSamples.empty())
    {
        // Fallback to closest velocity layer
        return drumGizmoParser.getSampleForNoteAndVelocity(midiNote, velocity);
    }

    if (matchingSamples.size() == 1)
        return matchingSamples[0];

    // Apply round robin logic
    if (roundRobinAmount <= 0.0f)
    {
        return matchingSamples[0];
    }

    size_t& lastIndex = lastRoundRobinIndex[midiNote];

    // CRITICAL FIX: Ensure lastIndex is valid for current matchingSamples size
    // The number of matching samples can vary based on velocity, so we must
    // clamp the stored index to prevent out-of-bounds access
    if (lastIndex >= matchingSamples.size())
    {
        lastIndex = 0;
    }

    if (roundRobinAmount >= 100.0f)
    {
        // Full round robin
        lastIndex = (lastIndex + 1) % matchingSamples.size();
        return matchingSamples[lastIndex];
    }
    else
    {
        // Partial round robin
        float probabilityToChange = roundRobinAmount / 100.0f;

        if (humanizationRandom.nextFloat() < probabilityToChange)
        {
            size_t newIndex;
            if (matchingSamples.size() == 2)
            {
                newIndex = (lastIndex + 1) % 2;
            }
            else
            {
                do {
                    newIndex = static_cast<size_t>(humanizationRandom.nextInt(static_cast<int>(matchingSamples.size())));
                } while (newIndex == lastIndex && matchingSamples.size() > 1);
            }
            lastIndex = newIndex;
        }

        return matchingSamples[lastIndex];
    }
}

void SampleEngine::processBlockToPartBuffers(std::array<juce::AudioBuffer<float>, NUM_DRUM_PARTS>& outPartBuffers,
                                             juce::MidiBuffer& midiMessages)
{
    if (!samplesLoaded)
        return;

    const int blockSize = outPartBuffers[0].getNumSamples();

    // Clear all part buffers
    for (auto& buffer : outPartBuffers)
    {
        buffer.clear();
    }

    // ============================================================================
    // PHASE 1: Process pending notes from previous blocks (timing humanization)
    // ============================================================================
    for (auto it = pendingNotes.begin(); it != pendingNotes.end(); )
    {
        it->samplesRemaining -= blockSize;

        if (it->samplesRemaining <= 0)
        {
            // This note should trigger now (the remaining samples is the offset into this block)
            int triggerOffset = blockSize + it->samplesRemaining;  // samplesRemaining is negative
            triggerOffset = juce::jlimit(0, blockSize - 1, triggerOffset);

            DBG("Pending Note Trigger: note=" + juce::String(it->midiNote) +
            " vel=" + juce::String(it->velocity) +
            " @sample=" + juce::String(triggerOffset));

            // Trigger the note (velocity already humanized, timing already applied)
            handleNoteOnDirect(it->midiNote, it->velocity, triggerOffset);

            it = pendingNotes.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // ============================================================================
    // PHASE 2: Process incoming MIDI events with sample-accurate timing
    // Each MIDI event has a sample offset (samplePosition) that tells us exactly
    // when in this block the note should trigger
    // ============================================================================
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        int samplePosition = metadata.samplePosition;

        // Clamp sample position to valid range
        samplePosition = juce::jlimit(0, blockSize - 1, samplePosition);

        if (message.isNoteOn())
        {
            DBG("MIDI Note ON: " + juce::String(message.getNoteNumber()) +
            " vel: " + juce::String(message.getVelocity()) +
            " @sample: " + juce::String(samplePosition));

            // Apply humanization and handle the note
            handleNoteOn(message.getNoteNumber(), message.getVelocity(), samplePosition);
        }
        else if (message.isNoteOff())
        {
            handleNoteOff(message.getNoteNumber());
        }
    }

    // ============================================================================
    // PHASE 3: Process all active voices, routing to appropriate part buffer
    // Each voice knows its start sample offset and will output correctly
    // ============================================================================
    for (auto& voice : voices)
    {
        if (voice.isActive())
        {
            int partIndex = voice.getDrumPart();
            if (partIndex >= 0 && partIndex < NUM_DRUM_PARTS)
            {
                // CRITICAL: Pass block size so voice can handle sample-accurate start
                voice.processBlock(outPartBuffers[partIndex], blockSize);
            }
        }
    }

    // Apply master gain to each part buffer
    if (masterGain != 1.0f)
    {
        for (auto& buffer : outPartBuffers)
        {
            buffer.applyGain(masterGain);
        }
    }
}

void SampleEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Legacy single-buffer processing
    // Route to per-part buffers first, then mix down

    if (!samplesLoaded)
        return;

    const int numSamples = buffer.getNumSamples();

    // Ensure part buffers are correct size
    for (auto& partBuffer : partBuffers)
    {
        if (partBuffer.getNumSamples() != numSamples)
        {
            partBuffer.setSize(2, numSamples, false, false, true);
        }
    }

    // Process to per-part buffers
    processBlockToPartBuffers(partBuffers, midiMessages);

    // Mix all parts into single output buffer
    buffer.clear();
    for (const auto& partBuffer : partBuffers)
    {
        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), partBuffer.getNumChannels()); ++ch)
        {
            buffer.addFrom(ch, 0, partBuffer, ch, 0, numSamples);
        }
    }
}

void SampleEngine::handleNoteOn(int midiNote, int velocity, int sampleOffset)
{
    if (velocity == 0)
    {
        handleNoteOff(midiNote);
        return;
    }

    // =========================================================================
    // HUMANIZATION: Apply velocity variation
    // =========================================================================
    int humanizedVelocity = applyVelocityHumanization(velocity);

    // =========================================================================
    // TIMING HUMANIZATION: Calculate delay and either trigger now or queue
    // =========================================================================
    if (timingHumanization > 0.0f)
    {
        // Calculate timing variation in samples
        double maxVariationMs = (timingHumanization / 100.0) * MAX_TIMING_VARIATION_MS;
        double maxVariationSamples = (maxVariationMs / 1000.0) * currentSampleRate;

        // Generate random variation in range [0, +maxVariation] (delay only, no advance)
        // This is simpler and more musically useful - notes can be late but not early
        double variation = humanizationRandom.nextFloat() * maxVariationSamples;

        int totalDelaySamples = sampleOffset + static_cast<int>(std::round(variation));

        if (totalDelaySamples < currentBlockSize)
        {
            // Can trigger within this block
            int triggerOffset = juce::jlimit(0, currentBlockSize - 1, totalDelaySamples);

            if (variation > 1.0)
            {
                DBG("Timing Humanization (immediate): delay=" + juce::String(variation, 1) +
                " samples (" + juce::String((variation / currentSampleRate) * 1000.0, 2) + " ms)");
            }

            handleNoteOnDirect(midiNote, humanizedVelocity, triggerOffset);
        }
        else
        {
            // Queue for future block
            PendingNote pending;
            pending.midiNote = midiNote;
            pending.velocity = humanizedVelocity;
            pending.samplesRemaining = totalDelaySamples;

            pendingNotes.push_back(pending);

            DBG("Timing Humanization (queued): note=" + juce::String(midiNote) +
            " delay=" + juce::String(totalDelaySamples) +
            " samples (" + juce::String((static_cast<double>(totalDelaySamples) / currentSampleRate) * 1000.0, 2) + " ms)");
        }
    }
    else
    {
        // No timing humanization - trigger immediately at original offset
        handleNoteOnDirect(midiNote, humanizedVelocity, sampleOffset);
    }
}

void SampleEngine::handleNoteOnDirect(int midiNote, int velocity, int sampleOffset)
{
    // Apply kick alternation if enabled
    int actualNote = getActualNoteToPlay(midiNote);

    // Determine which drum part this note belongs to (based on actual note being played)
    int drumPart = getDrumPartForNote(actualNote);

    // =========================================================================
    // POLYPHONIC PLAYBACK: Do NOT stop existing voices playing the same note
    // Professional samplers allow multiple instances of the same drum to overlap
    // This creates natural decay layering for fast repeated hits (metal drumming)
    // =========================================================================

    // Format-specific sample lookup with ROUND ROBIN support
    const juce::AudioBuffer<float>* sampleBuffer = nullptr;
    float volume = 0.0f;
    juce::String samplePath;

    if (currentFormat == LibraryFormat::SFZ)
    {
        // SFZ format: use round robin to get matching region for the ACTUAL note
        const auto* region = getRegionWithRoundRobin(actualNote, velocity);

        // FALLBACK: If no region found for alternated note, try original note
        if (region == nullptr && actualNote != midiNote)
        {
            DBG("SFZ: No region for alternated note " + juce::String(actualNote) +
            ", falling back to original note " + juce::String(midiNote));
            actualNote = midiNote;
            drumPart = getDrumPartForNote(actualNote);
            region = getRegionWithRoundRobin(actualNote, velocity);
        }

        if (region == nullptr)
        {
            DBG("SFZ: No region found for note " + juce::String(actualNote) + " vel " + juce::String(velocity));
            return;
        }

        samplePath = region->samplePath;
        volume = region->volume;

        DBG("SFZ: Note " + juce::String(actualNote) +
        " vel " + juce::String(velocity) +
        " part " + juce::String(drumPart) +
        " @offset " + juce::String(sampleOffset) +
        " -> Sample: " + samplePath +
        " (vel range: " + juce::String(region->velocityLow) + "-" + juce::String(region->velocityHigh) +
        ", vol: " + juce::String(volume, 1) + "dB)");
    }
    else if (currentFormat == LibraryFormat::DrumGizmo)
    {
        // DrumGizmo format: use round robin to get matching sample
        const auto* sample = getSampleWithRoundRobin(actualNote, velocity);

        // FALLBACK: If no sample found for alternated note, try original note
        if (sample == nullptr && actualNote != midiNote)
        {
            DBG("DrumGizmo: No sample for alternated note " + juce::String(actualNote) +
            ", falling back to original note " + juce::String(midiNote));
            actualNote = midiNote;
            drumPart = getDrumPartForNote(actualNote);
            sample = getSampleWithRoundRobin(actualNote, velocity);
        }

        if (sample == nullptr)
        {
            DBG("DrumGizmo: No sample found for note " + juce::String(actualNote) + " vel " + juce::String(velocity));
            return;
        }

        samplePath = sample->filePath;
        volume = 0.0f;  // DrumGizmo doesn't have volume in the sample definition, use 0dB

        DBG("DrumGizmo: Note " + juce::String(actualNote) +
        " vel " + juce::String(velocity) +
        " part " + juce::String(drumPart) +
        " @offset " + juce::String(sampleOffset) +
        " -> Sample: " + samplePath);
    }
    else
    {
        DBG("ERROR: Unknown library format");
        return;
    }

    // Get the sample buffer
    sampleBuffer = getSampleBuffer(samplePath);

    if (sampleBuffer == nullptr)
    {
        DBG("Sample buffer not found for: " + samplePath);
        return;
    }

    // Find a free voice (or steal the oldest one with crossfade)
    auto* voice = findFreeVoice();

    if (voice == nullptr)
    {
        DBG("No free voices available (should not happen - findFreeVoice always returns something)");
        return;
    }

    // CRITICAL: Trigger the voice with sample-accurate offset
    voice->trigger(sampleBuffer, volume, actualNote, velocity, drumPart, sampleOffset);
}

void SampleEngine::handleNoteOff(int midiNote)
{
    // For drums, note-off usually doesn't matter (one-shot samples)
    juce::ignoreUnused(midiNote);
}

void SampleEngine::stopAllVoices()
{
    for (auto& voice : voices)
    {
        voice.stop();
    }
}

SampleVoice* SampleEngine::findFreeVoice()
{
    // First, find an inactive voice
    for (auto& voice : voices)
    {
        if (!voice.isActive())
        {
            return &voice;
        }
    }

    // All voices busy - steal the oldest one with crossfade
    DBG("WARNING: All voices busy, stealing oldest with crossfade");

    SampleVoice* oldestVoice = findOldestVoice();

    if (oldestVoice != nullptr)
    {
        // Start crossfade on the oldest voice
        oldestVoice->startFadeOut(VOICE_STEAL_FADE_SAMPLES);

        // Find another voice that's not fading (or the least important one)
        // If all are fading, just use the oldest one anyway
        for (auto& voice : voices)
        {
            if (!voice.isActive())
            {
                return &voice;
            }
        }

        // Still no free voice - force stop the oldest
        oldestVoice->stop();
        return oldestVoice;
    }

    // Fallback - should never reach here
    voices[0].stop();
    return &voices[0];
}

SampleVoice* SampleEngine::findOldestVoice()
{
    SampleVoice* oldestVoice = nullptr;
    juce::int64 oldestTime = std::numeric_limits<juce::int64>::max();

    for (auto& voice : voices)
    {
        if (voice.isActive() && !voice.isFadingOut())
        {
            juce::int64 triggerTime = voice.getTriggerTime();
            if (triggerTime < oldestTime)
            {
                oldestTime = triggerTime;
                oldestVoice = &voice;
            }
        }
    }

    // If all active voices are fading, find any active voice
    if (oldestVoice == nullptr)
    {
        for (auto& voice : voices)
        {
            if (voice.isActive())
            {
                juce::int64 triggerTime = voice.getTriggerTime();
                if (triggerTime < oldestTime)
                {
                    oldestTime = triggerTime;
                    oldestVoice = &voice;
                }
            }
        }
    }

    return oldestVoice;
}

void SampleEngine::releaseResources()
{
    stopAllVoices();
}

int SampleEngine::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices)
    {
        if (voice.isActive())
            count++;
    }
    return count;
}

juce::String SampleEngine::getStatusText() const
{
    if (!samplesLoaded)
        return "No samples loaded";

    juce::String formatStr;
    switch (currentFormat)
    {
        case LibraryFormat::SFZ: formatStr = "SFZ"; break;
        case LibraryFormat::DrumGizmo: formatStr = "DrumGizmo"; break;
        default: formatStr = "Unknown"; break;
    }

    juce::String status = currentLibraryName + " (" + formatStr + ") - " +
    juce::String(getLoadedSampleCount()) + " samples, " +
    juce::String(getActiveVoiceCount()) + "/" + juce::String(MAX_VOICES) + " voices";

    if (kickAlternationEnabled)
        status += " [Kick Alt]";

    return status;
}
