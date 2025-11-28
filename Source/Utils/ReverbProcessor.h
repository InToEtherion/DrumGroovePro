#pragma once

#include <JuceHeader.h>

/**
 * Master reverb processor
 * Shared by all drum parts via send levels
 */
class ReverbProcessor
{
public:
    ReverbProcessor();
    ~ReverbProcessor() = default;
    
    void prepareToPlay(double sampleRate, int maximumBlockSize);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void reset();
    
    // Reverb parameters
    void setRoomSize(float size);      // 0.0 to 1.0
    void setDamping(float damping);    // 0.0 to 1.0
    void setWetLevel(float wet);       // 0.0 to 1.0
    void setDryLevel(float dry);       // 0.0 to 1.0
    void setWidth(float width);        // 0.0 to 1.0
    
    float getRoomSize() const { return params.roomSize; }
    float getDamping() const { return params.damping; }
    float getWetLevel() const { return params.wetLevel; }
    float getDryLevel() const { return params.dryLevel; }
    float getWidth() const { return params.width; }
    
private:
    juce::Reverb reverb;
    juce::Reverb::Parameters params;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbProcessor)
};
