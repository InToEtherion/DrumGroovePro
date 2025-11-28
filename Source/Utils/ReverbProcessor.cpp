#include "ReverbProcessor.h"

ReverbProcessor::ReverbProcessor()
{
    params.roomSize = 0.5f;
    params.damping = 0.5f;
    params.wetLevel = 0.33f;
    params.dryLevel = 0.4f;
    params.width = 1.0f;
    params.freezeMode = 0.0f;
    
    reverb.setParameters(params);
}

void ReverbProcessor::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    juce::ignoreUnused(maximumBlockSize);
    reverb.setSampleRate(sampleRate);
    reverb.reset();
}

void ReverbProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 1)
    {
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
    }
    else if (buffer.getNumChannels() >= 2)
    {
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), 
                            buffer.getNumSamples());
    }
}

void ReverbProcessor::reset()
{
    reverb.reset();
}

void ReverbProcessor::setRoomSize(float size)
{
    params.roomSize = juce::jlimit(0.0f, 1.0f, size);
    reverb.setParameters(params);
}

void ReverbProcessor::setDamping(float damping)
{
    params.damping = juce::jlimit(0.0f, 1.0f, damping);
    reverb.setParameters(params);
}

void ReverbProcessor::setWetLevel(float wet)
{
    params.wetLevel = juce::jlimit(0.0f, 1.0f, wet);
    reverb.setParameters(params);
}

void ReverbProcessor::setDryLevel(float dry)
{
    params.dryLevel = juce::jlimit(0.0f, 1.0f, dry);
    reverb.setParameters(params);
}

void ReverbProcessor::setWidth(float width)
{
    params.width = juce::jlimit(0.0f, 1.0f, width);
    reverb.setParameters(params);
}
