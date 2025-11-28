#pragma once

#include <JuceHeader.h>

/**
 * Represents a musical section in the timeline with time signature and bar count
 */
struct Section
{
    int id = 0;
    int startBar = 1;
    int numerator = 4;
    int denominator = 4;
    int numBars = 4;
    double bpm = 0.0;              
    double playbackBPM = 0.0;      
    juce::String name;
    juce::Colour colour = juce::Colour(0x40ffffff);                 // Visual color for section
    
    /**
     * Default constructor - creates 4/4 section with 4 bars
     */
    Section()
    {
        colour = juce::Colour(0x40ffffff); // Semi-transparent white
    }
    
    /**
     * Constructor with parameters
     */
    Section(int sectionId, int startBarNum, int timeSigNum, int timeSigDenom, int barCount, double sectionBPM = 0.0)
        : id(sectionId)
        , startBar(startBarNum)
        , numerator(timeSigNum)
        , denominator(timeSigDenom)
        , numBars(barCount)
        , bpm(sectionBPM)
    {
        colour = juce::Colour(0x40ffffff);
    }
    
    /**
     * Get time signature as string (e.g., "4/4", "3/4", "6/8")
     */
    juce::String getTimeSignatureString() const
    {
        return juce::String(numerator) + "/" + juce::String(denominator);
    }
    
    /**
     * Get end bar (exclusive)
     */
    int getEndBar() const
    {
        return startBar + numBars;
    }
    
    /**
     * Check if a bar number is within this section
     */
    bool containsBar(int barNumber) const
    {
        return barNumber >= startBar && barNumber < getEndBar();
    }
    
    /**
     * Get duration in beats
     */
    double getDurationInBeats() const
    {
        return numBars * numerator;
    }
    
    /**
     * Get duration in seconds at given BPM
     */
    double getDurationInSeconds(double effectiveBPM) const
    {
        if (effectiveBPM <= 0.0) effectiveBPM = 120.0;
        
        // Duration = (number of bars * beats per bar) * (60 seconds / BPM)
        double totalBeats = numBars * numerator;
        return (totalBeats * 60.0) / effectiveBPM;
    }
    
    /**
     * Serialize to ValueTree
     */
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree tree("Section");
        tree.setProperty("id", id, nullptr);
        tree.setProperty("startBar", startBar, nullptr);
        tree.setProperty("numerator", numerator, nullptr);
        tree.setProperty("denominator", denominator, nullptr);
        tree.setProperty("numBars", numBars, nullptr);
        tree.setProperty("bpm", bpm, nullptr);
        tree.setProperty("playbackBPM", playbackBPM, nullptr);
        tree.setProperty("name", name, nullptr);
        tree.setProperty("colour", colour.toString(), nullptr);
        return tree;
    }
    
    /**
     * Deserialize from ValueTree
     */
    static Section fromValueTree(const juce::ValueTree& tree)
    {
        Section section;
        section.id = tree.getProperty("id", 0);
        section.startBar = tree.getProperty("startBar", 1);
        section.numerator = tree.getProperty("numerator", 4);
        section.denominator = tree.getProperty("denominator", 4);
        section.numBars = tree.getProperty("numBars", 4);
        section.bpm = tree.getProperty("bpm", 0.0);
        section.playbackBPM = tree.getProperty("playbackBPM", 0.0); 
        section.name = tree.getProperty("name", "");
        section.colour = juce::Colour::fromString(tree.getProperty("colour", "0x40ffffff").toString());
        return section;
    }
};
