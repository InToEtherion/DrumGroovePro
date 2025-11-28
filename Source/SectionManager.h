#pragma once

#include <JuceHeader.h>
#include "Section.h"
#include <vector>
#include <memory>

/**
 * Manages musical sections in the timeline
 * Provides conversion between bar-based and time-based coordinates
 */
class SectionManager : public juce::ChangeBroadcaster
{
public:
    SectionManager();
    ~SectionManager();

    // Section management (call from message thread only)
    void addSection(const Section& section);
    void addSectionAfter(int afterSectionIndex);
    void removeSection(int sectionIndex);
    void updateSection(int sectionIndex, const Section& newSection);
    void clearSections();

    // Section access
    int getNumSections() const;
    const Section* getSection(int index) const;
    Section* getSection(int index);
    const Section* getSectionAtBar(int barNumber) const;
    const Section* getSectionAtTime(double timeInSeconds, double globalBPM) const;

    // Global BPM - thread safe
    void setGlobalBPM(double bpm) { globalBPM.store(bpm, std::memory_order_relaxed); }
    double getGlobalBPM() const { return globalBPM.load(std::memory_order_relaxed); }

    // Time/Bar conversions
    double barToTime(int barNumber, int beat = 0) const;
    int timeToBar(double timeInSeconds) const;
    double timeToBarPrecise(double timeInSeconds) const;  // Returns fractional bar position

    // Section time calculations
    double getSectionStartTime(int sectionIndex) const;
    double getSectionEndTime(int sectionIndex) const;
    double getSectionDuration(int sectionIndex) const;
    double getTotalDuration() const;  // Total timeline duration in seconds
    int getTotalBars() const;         // Total bars across all sections

    // BPM for specific section or time
    double getEffectiveBPM(int sectionIndex) const;
    double getBPMAtTime(double timeInSeconds) const;
    double getBPMAtBar(int barNumber) const;

    // State persistence
    juce::ValueTree saveState() const;
    void restoreState(const juce::ValueTree& state);

    int getSectionIndex(const Section* section) const;

    double getEffectivePlaybackBPM(int sectionIndex) const;

private:
    mutable juce::CriticalSection sectionLock;
    std::vector<Section> sections;
    std::atomic<double> globalBPM { 120.0 };
    int nextSectionId = 1;

    void ensureDefaultSection();
    void recalculateStartBars();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SectionManager)
};
