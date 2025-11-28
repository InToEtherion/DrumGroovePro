#include "DrumMixerChannel.h"

DrumMixerChannel::DrumMixerChannel()
{
    updatePanGains();
}

void DrumMixerChannel::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    eq.prepareToPlay(sampleRate, maximumBlockSize);
    compressor.prepareToPlay(sampleRate, maximumBlockSize);
}

void DrumMixerChannel::processBlock(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& reverbSendBuffer)
{
    // Check mute
    if (mute)
    {
        buffer.clear();
        return;
    }

    // Signal chain: EQ → Compressor → Volume/Pan

    // Apply EQ
    eq.processBlock(buffer);

    // Apply Compressor
    compressor.processBlock(buffer);

    // Apply volume and pan
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    if (numChannels >= 1)
    {
        buffer.applyGain(0, 0, numSamples, volume * leftGain);

        if (numChannels >= 2)
        {
            buffer.applyGain(1, 0, numSamples, volume * rightGain);
        }
    }

    // Send to reverb (post-fader)
    if (reverbSend > 0.0f && reverbSendBuffer.getNumSamples() >= numSamples)
    {
        for (int ch = 0; ch < juce::jmin(numChannels, reverbSendBuffer.getNumChannels()); ++ch)
        {
            reverbSendBuffer.addFrom(ch, 0, buffer, ch, 0, numSamples, reverbSend);
        }
    }
}

void DrumMixerChannel::reset()
{
    eq.reset();
    compressor.reset();
}

void DrumMixerChannel::setVolume(float linearGain)
{
    volume = juce::jlimit(0.0f, 1.0f, linearGain);
}

void DrumMixerChannel::setPan(float newPan)
{
    pan = juce::jlimit(-1.0f, 1.0f, newPan);
    updatePanGains();
}

void DrumMixerChannel::setReverbSend(float send)
{
    reverbSend = juce::jlimit(0.0f, 1.0f, send);
}

void DrumMixerChannel::setSolo(bool shouldSolo)
{
    solo = shouldSolo;
}

void DrumMixerChannel::setMute(bool shouldMute)
{
    mute = shouldMute;
}

void DrumMixerChannel::updatePanGains()
{
    // Constant power panning
    const float halfPi = juce::MathConstants<float>::pi / 2.0f;
    leftGain = std::cos((pan + 1.0f) * halfPi / 2.0f);
    rightGain = std::sin((pan + 1.0f) * halfPi / 2.0f);
}
