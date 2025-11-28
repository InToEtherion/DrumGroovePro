#pragma once

#include <JuceHeader.h>

class AudioTrack : public juce::Component
{
public:
    AudioTrack(const juce::File& audioFile);
    ~AudioTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse interaction for dragging and resizing
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Audio file info
    juce::String getFileName() const { return file.getFileNameWithoutExtension(); }
    juce::String getFilePath() const { return file.getFullPathName(); }
    double getDurationSeconds() const { return visibleDuration; }
    double getFullDurationSeconds() const { return fullDuration; }
    bool isLoaded() const { return audioBuffer.getNumSamples() > 0; }

    // Position in timeline
    void setStartPosition(double seconds) { startPositionSeconds = seconds; repaint(); }
    double getStartPosition() const { return startPositionSeconds; }
    double getEndPosition() const { return startPositionSeconds + visibleDuration; }

    // Trimming
    double getOffsetSeconds() const { return offsetSeconds; }
    void setOffsetSeconds(double offset) { offsetSeconds = juce::jlimit(0.0, fullDuration, offset); }

    // Playback control
    void prepareToPlay(double sampleRate);
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill,
                           double playheadPosition, double speed);
    void setVolume(float volume) { gain = volume; }
    float getVolume() const { return gain; }
    void setMuted(bool shouldMute) { muted = shouldMute; }
    bool isMuted() const { return muted; }

    // Visual
    void setPixelsPerSecond(float pps);

    // Shared format manager for all audio tracks
    static juce::AudioFormatManager& getSharedFormatManager();

private:
    juce::File file;
    juce::AudioBuffer<float> audioBuffer;
    double sampleRate { 44100.0 };
    double fullDuration { 0.0 };        // Total duration of audio file
    double visibleDuration { 0.0 };     // Visible/playable duration (can be trimmed)
    double offsetSeconds { 0.0 };       // Offset from start of audio file (for left trim)
    double startPositionSeconds { 0.0 }; // Where in timeline this audio starts
    float gain { 0.8f };
    bool muted { false };

    // Drag handling
    enum class DragMode { None, Move, ResizeLeft, ResizeRight };
    DragMode dragMode { DragMode::None };
    double dragStartPosition { 0.0 };
    double dragStartOffset { 0.0 };
    double dragStartDuration { 0.0 };
    int dragStartX { 0 };

    static constexpr int EDGE_GRAB_DISTANCE = 8; // pixels from edge to trigger resize

    // Waveform display
    juce::Path waveformPath;
    float pixelsPerSecond { 100.0f };

    void loadAudioFile();
    void generateWaveform();
    DragMode getMouseDragMode(const juce::MouseEvent& event);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrack)
};
