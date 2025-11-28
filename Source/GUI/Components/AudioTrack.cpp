#include "AudioTrack.h"
#include "../LookAndFeel/ColourPalette.h"

// Static shared format manager for all audio tracks
juce::AudioFormatManager& AudioTrack::getSharedFormatManager()
{
    static juce::AudioFormatManager formatManager;
    static bool initialized = false;

    if (!initialized)
    {
        formatManager.registerBasicFormats();
        initialized = true;
    }

    return formatManager;
}

AudioTrack::AudioTrack(const juce::File& audioFile)
: file(audioFile)
{
    loadAudioFile();
}

AudioTrack::~AudioTrack()
{
}

void AudioTrack::loadAudioFile()
{
    if (!file.existsAsFile())
        return;

    auto& formatManager = getSharedFormatManager();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
        return;

    sampleRate = reader->sampleRate;
    fullDuration = reader->lengthInSamples / sampleRate;
    visibleDuration = fullDuration; // Initially show full audio

    audioBuffer.setSize(static_cast<int>(reader->numChannels),
                        static_cast<int>(reader->lengthInSamples));
    reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    generateWaveform();
}

void AudioTrack::generateWaveform()
{
    waveformPath.clear();

    if (audioBuffer.getNumSamples() == 0)
        return;

    const int totalSamples = audioBuffer.getNumSamples();
    const float* channelData = audioBuffer.getReadPointer(0);

    // Calculate the sample range we want to display (the trimmed portion)
    int startSample = static_cast<int>(offsetSeconds * sampleRate);
    int endSample = static_cast<int>((offsetSeconds + visibleDuration) * sampleRate);

    startSample = juce::jlimit(0, totalSamples, startSample);
    endSample = juce::jlimit(startSample, totalSamples, endSample);

    int numSamplesToShow = endSample - startSample;

    if (numSamplesToShow <= 0)
        return;

    // Calculate how wide the waveform should be
    float waveformWidth = visibleDuration * pixelsPerSecond;

    // Calculate samples per pixel for the visible portion only
    int samplesPerPixel = juce::jmax(1, numSamplesToShow / static_cast<int>(waveformWidth));

    // Find max amplitude in the visible range for scaling
    float maxVal = 0.0f;
    for (int i = startSample; i < endSample; i += samplesPerPixel)
    {
        float sample = std::abs(channelData[i]);
        if (sample > maxVal)
            maxVal = sample;
    }

    if (maxVal < 0.001f)
        maxVal = 0.001f;

    // Draw the waveform for the visible portion
    waveformPath.startNewSubPath(0.0f, getHeight() * 0.5f);

    for (int i = startSample; i < endSample; i += samplesPerPixel)
    {
        float sample = channelData[i];

        // Calculate x position based on how far through the visible portion we are
        float progress = static_cast<float>(i - startSample) / static_cast<float>(numSamplesToShow);
        float x = progress * waveformWidth;

        // Calculate y position
        float y = getHeight() * 0.5f * (1.0f - sample / maxVal);

        waveformPath.lineTo(x, y);
    }
}

AudioTrack::DragMode AudioTrack::getMouseDragMode(const juce::MouseEvent& event)
{
    int mouseX = event.x;
    int width = getWidth();

    // Check left edge
    if (mouseX <= EDGE_GRAB_DISTANCE)
        return DragMode::ResizeLeft;

    // Check right edge
    if (mouseX >= width - EDGE_GRAB_DISTANCE)
        return DragMode::ResizeRight;

    // Middle = move
    return DragMode::Move;
}

void AudioTrack::mouseMove(const juce::MouseEvent& event)
{
    DragMode mode = getMouseDragMode(event);

    // Change cursor based on what would happen if we clicked
    if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AudioTrack::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isLeftButtonDown())
    {
        dragMode = getMouseDragMode(event);
        dragStartPosition = startPositionSeconds;
        dragStartOffset = offsetSeconds;
        dragStartDuration = visibleDuration;
        dragStartX = event.x;
    }
}

void AudioTrack::mouseDrag(const juce::MouseEvent& event)
{
    if (dragMode == DragMode::None)
        return;

    int deltaX = event.x - dragStartX;
    double deltaSeconds = deltaX / pixelsPerSecond;

    if (dragMode == DragMode::Move)
    {
        // Move the entire clip
        startPositionSeconds = juce::jmax(0.0, dragStartPosition + deltaSeconds);
    }
    else if (dragMode == DragMode::ResizeLeft)
    {
        // Trim from the left (change offset and start position)
        double newOffset = juce::jlimit(0.0, fullDuration - 0.1, dragStartOffset + deltaSeconds);
        double offsetChange = newOffset - offsetSeconds;

        offsetSeconds = newOffset;
        startPositionSeconds = dragStartPosition + deltaSeconds;
        visibleDuration = juce::jmax(0.1, dragStartDuration - deltaSeconds);

        // Make sure we don't go past the end
        if (offsetSeconds + visibleDuration > fullDuration)
            visibleDuration = fullDuration - offsetSeconds;

        generateWaveform();
    }
    else if (dragMode == DragMode::ResizeRight)
    {
        // Trim from the right (change duration)
        visibleDuration = juce::jlimit(0.1, fullDuration - offsetSeconds, dragStartDuration + deltaSeconds);
        generateWaveform();
    }

    // Update parent to reposition/resize this component
    if (auto* parent = getParentComponent())
        parent->resized();
}

void AudioTrack::mouseUp(const juce::MouseEvent& event)
{
    dragMode = DragMode::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AudioTrack::prepareToPlay(double sr)
{
    sampleRate = sr;
}

void AudioTrack::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill,
                                   double playheadPosition, double speed)
{
    if (muted || audioBuffer.getNumSamples() == 0)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    // Check if playhead is within this audio clip's visible time range
    double clipStart = startPositionSeconds;
    double clipEnd = startPositionSeconds + visibleDuration;

    if (playheadPosition < clipStart || playheadPosition >= clipEnd)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    const int numChannels = juce::jmin(bufferToFill.buffer->getNumChannels(), audioBuffer.getNumChannels());
    const int numSamples = bufferToFill.numSamples;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* outputBuffer = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);
        const auto* inputBuffer = audioBuffer.getReadPointer(channel % audioBuffer.getNumChannels());

        for (int i = 0; i < numSamples; ++i)
        {
            double timePos = playheadPosition + (i / sampleRate) * speed;

            // Convert timeline position to position within this audio clip
            double clipPosition = timePos - startPositionSeconds;

            // Add the offset to get the actual position in the audio file
            double filePosition = clipPosition + offsetSeconds;
            int samplePos = static_cast<int>(filePosition * sampleRate);

            if (samplePos >= 0 && samplePos < audioBuffer.getNumSamples())
                outputBuffer[i] = inputBuffer[samplePos] * gain;
            else
                outputBuffer[i] = 0.0f;
        }
    }
}

void AudioTrack::setPixelsPerSecond(float pps)
{
    pixelsPerSecond = pps;
    generateWaveform();

    // Update width based on VISIBLE duration and pixels per second
    setSize(static_cast<int>(visibleDuration * pixelsPerSecond), getHeight());
    repaint();
}

void AudioTrack::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);

    // Draw waveform
    g.setColour(ColourPalette::primaryBlue.withAlpha(0.6f));
    g.strokePath(waveformPath, juce::PathStrokeType(1.0f));

    // Draw border
    g.setColour(ColourPalette::borderColour);
    g.drawRect(getLocalBounds(), 1);

    // Highlight edges when hovering
    if (dragMode == DragMode::ResizeLeft || dragMode == DragMode::ResizeRight)
    {
        g.setColour(ColourPalette::primaryBlue.withAlpha(0.3f));
        if (dragMode == DragMode::ResizeLeft)
            g.fillRect(0, 0, EDGE_GRAB_DISTANCE, getHeight());
        else
            g.fillRect(getWidth() - EDGE_GRAB_DISTANCE, 0, EDGE_GRAB_DISTANCE, getHeight());
    }
}

void AudioTrack::resized()
{
    generateWaveform();
}
