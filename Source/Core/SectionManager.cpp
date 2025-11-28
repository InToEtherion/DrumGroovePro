#include "SectionManager.h"

SectionManager::SectionManager()
{
    // Start with empty section
}

SectionManager::~SectionManager()
{
}

void SectionManager::ensureDefaultSection()
{
    // Note: Called internally, lock should already be held by caller
    if (sections.empty())
    {
        Section defaultSection(nextSectionId++, 1, 4, 4, 4, 0.0);
        sections.push_back(defaultSection);
    }
}

void SectionManager::addSection(const Section& section)
{
    {
        juce::ScopedLock lock(sectionLock);
        sections.push_back(section);
        recalculateStartBars();
    }
    sendChangeMessage();
}

void SectionManager::addSectionAfter(int afterSectionIndex)
{
    {
        juce::ScopedLock lock(sectionLock);

        // Handle empty sections - add first section
        if (sections.empty())
        {
            Section newSection(
                nextSectionId++,
                1,      // Start at bar 1
                4,      // Default 4/4
                4,
                4,      // 4 bars
                0.0     // Use global BPM
            );
            sections.push_back(newSection);
            recalculateStartBars();
        }
        else if (afterSectionIndex >= 0 && afterSectionIndex < static_cast<int>(sections.size()))
        {
            const auto& prevSection = sections[static_cast<size_t>(afterSectionIndex)];
            int newStartBar = prevSection.getEndBar();

            Section newSection(
                nextSectionId++,
                newStartBar,
                prevSection.numerator,
                prevSection.denominator,
                prevSection.numBars,
                prevSection.bpm
            );

            sections.insert(sections.begin() + afterSectionIndex + 1, newSection);
            recalculateStartBars();
        }
        else
        {
            return; // Invalid index, don't send change message
        }
    }
    sendChangeMessage();
}

int SectionManager::getSectionIndex(const Section* section) const
{
    if (!section)
        return -1;

    juce::ScopedLock lock(sectionLock);
    for (int i = 0; i < static_cast<int>(sections.size()); ++i)
    {
        if (&sections[static_cast<size_t>(i)] == section)
            return i;
    }
    return -1;
}

void SectionManager::removeSection(int sectionIndex)
{
    {
        juce::ScopedLock lock(sectionLock);

        if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
            return;

        // Don't allow removing the last section
        if (sections.size() <= 1)
            return;

        sections.erase(sections.begin() + sectionIndex);
        recalculateStartBars();
    }
    sendChangeMessage();
}

void SectionManager::updateSection(int sectionIndex, const Section& newSection)
{
    {
        juce::ScopedLock lock(sectionLock);

        if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
            return;

        sections[static_cast<size_t>(sectionIndex)] = newSection;
        recalculateStartBars();
    }
    sendChangeMessage();
}

void SectionManager::clearSections()
{
    {
        juce::ScopedLock lock(sectionLock);
        sections.clear();
        nextSectionId = 1;
    }
    sendChangeMessage();
}

void SectionManager::recalculateStartBars()
{
    // Note: Called internally, lock should already be held by caller
    int currentBar = 1;
    for (auto& section : sections)
    {
        section.startBar = currentBar;
        currentBar += section.numBars;
    }
}

int SectionManager::getNumSections() const
{
    juce::ScopedLock lock(sectionLock);
    return static_cast<int>(sections.size());
}

const Section* SectionManager::getSection(int index) const
{
    juce::ScopedLock lock(sectionLock);
    if (index >= 0 && index < static_cast<int>(sections.size()))
        return &sections[static_cast<size_t>(index)];
    return nullptr;
}

Section* SectionManager::getSection(int index)
{
    juce::ScopedLock lock(sectionLock);
    if (index >= 0 && index < static_cast<int>(sections.size()))
        return &sections[static_cast<size_t>(index)];
    return nullptr;
}

const Section* SectionManager::getSectionAtBar(int barNumber) const
{
    juce::ScopedLock lock(sectionLock);
    for (const auto& section : sections)
    {
        if (section.containsBar(barNumber))
            return &section;
    }
    return nullptr;
}

const Section* SectionManager::getSectionAtTime(double timeInSeconds, double bpm) const
{
    juce::ScopedLock lock(sectionLock);
    double currentTime = 0.0;

    for (const auto& section : sections)
    {
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : bpm;
        double sectionDuration = section.getDurationInSeconds(effectiveBPM);

        if (timeInSeconds >= currentTime && timeInSeconds < currentTime + sectionDuration)
            return &section;

        currentTime += sectionDuration;
    }

    return nullptr;
}

double SectionManager::barToTime(int barNumber, int beat) const
{
    juce::ScopedLock lock(sectionLock);
    double time = 0.0;
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);

    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto& section = sections[i];

        if (barNumber < section.startBar)
            break;

        if (barNumber < section.getEndBar())
        {
            // Bar is in this section
            int barsIntoSection = barNumber - section.startBar;
            double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;

            // Time = (bars * beats_per_bar + additional_beats) * (60 / BPM)
            double totalBeats = (barsIntoSection * section.numerator) + beat;
            time += (totalBeats * 60.0) / effectiveBPM;
            break;
        }
        else
        {
            // Add full section duration
            double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
            time += section.getDurationInSeconds(effectiveBPM);
        }
    }

    return time;
}

int SectionManager::timeToBar(double timeInSeconds) const
{
    juce::ScopedLock lock(sectionLock);
    double currentTime = 0.0;
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);

    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto& section = sections[i];
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        double sectionDuration = section.getDurationInSeconds(effectiveBPM);

        if (timeInSeconds < currentTime + sectionDuration)
        {
            // Time is in this section
            double timeIntoSection = timeInSeconds - currentTime;
            double beatsIntoSection = (timeIntoSection * effectiveBPM) / 60.0;
            int barsIntoSection = static_cast<int>(beatsIntoSection / section.numerator);
            return section.startBar + barsIntoSection;
        }

        currentTime += sectionDuration;
    }

    // Time is past all sections - return last bar
    if (!sections.empty())
        return sections.back().getEndBar() - 1;

    return 1;
}

double SectionManager::getEffectivePlaybackBPM(int sectionIndex) const
{
    juce::ScopedLock lock(sectionLock);
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);

    if (sectionIndex >= 0 && sectionIndex < static_cast<int>(sections.size()))
    {
        const auto& section = sections[static_cast<size_t>(sectionIndex)];
        return (section.playbackBPM > 0.0) ? section.playbackBPM : currentGlobalBPM;
    }
    return currentGlobalBPM;
}

double SectionManager::timeToBarPrecise(double timeInSeconds) const
{
    juce::ScopedLock lock(sectionLock);
    double currentTime = 0.0;
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);

    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto& section = sections[i];
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        double sectionDuration = section.getDurationInSeconds(effectiveBPM);

        if (timeInSeconds < currentTime + sectionDuration)
        {
            // Time is in this section
            double timeIntoSection = timeInSeconds - currentTime;
            double beatsIntoSection = (timeIntoSection * effectiveBPM) / 60.0;
            double barsIntoSection = beatsIntoSection / section.numerator;
            return section.startBar + barsIntoSection;
        }

        currentTime += sectionDuration;
    }

    // Time is past all sections - return end position
    if (!sections.empty())
        return static_cast<double>(sections.back().getEndBar());

    return 1.0;
}

double SectionManager::getSectionStartTime(int sectionIndex) const
{
    juce::ScopedLock lock(sectionLock);

    if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
        return 0.0;

    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);
    double time = 0.0;

    for (int i = 0; i < sectionIndex; ++i)
    {
        const auto& section = sections[static_cast<size_t>(i)];
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        time += section.getDurationInSeconds(effectiveBPM);
    }

    return time;
}

double SectionManager::getSectionEndTime(int sectionIndex) const
{
    juce::ScopedLock lock(sectionLock);

    if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
        return 0.0;

    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);
    double time = 0.0;

    // Calculate start time
    for (int i = 0; i < sectionIndex; ++i)
    {
        const auto& section = sections[static_cast<size_t>(i)];
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        time += section.getDurationInSeconds(effectiveBPM);
    }

    // Add this section's duration
    const auto& section = sections[static_cast<size_t>(sectionIndex)];
    double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
    time += section.getDurationInSeconds(effectiveBPM);

    return time;
}

double SectionManager::getSectionDuration(int sectionIndex) const
{
    juce::ScopedLock lock(sectionLock);

    if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
        return 0.0;

    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);
    const auto& section = sections[static_cast<size_t>(sectionIndex)];
    double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
    return section.getDurationInSeconds(effectiveBPM);
}

double SectionManager::getTotalDuration() const
{
    juce::ScopedLock lock(sectionLock);
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);
    double total = 0.0;

    for (const auto& section : sections)
    {
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        total += section.getDurationInSeconds(effectiveBPM);
    }
    return total;
}

int SectionManager::getTotalBars() const
{
    juce::ScopedLock lock(sectionLock);

    if (sections.empty())
        return 0;

    return sections.back().getEndBar() - 1;
}

double SectionManager::getEffectiveBPM(int sectionIndex) const
{
    juce::ScopedLock lock(sectionLock);
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);

    if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
        return currentGlobalBPM;

    double sectionBPM = sections[static_cast<size_t>(sectionIndex)].bpm;
    return (sectionBPM > 0.0) ? sectionBPM : currentGlobalBPM;
}

double SectionManager::getBPMAtTime(double timeInSeconds) const
{
    juce::ScopedLock lock(sectionLock);
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);
    double currentTime = 0.0;

    for (const auto& section : sections)
    {
        double effectiveBPM = (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        double sectionDuration = section.getDurationInSeconds(effectiveBPM);

        if (timeInSeconds >= currentTime && timeInSeconds < currentTime + sectionDuration)
        {
            return effectiveBPM;
        }

        currentTime += sectionDuration;
    }

    return currentGlobalBPM;
}

double SectionManager::getBPMAtBar(int barNumber) const
{
    juce::ScopedLock lock(sectionLock);
    double currentGlobalBPM = globalBPM.load(std::memory_order_relaxed);

    for (const auto& section : sections)
    {
        if (section.containsBar(barNumber))
        {
            return (section.bpm > 0.0) ? section.bpm : currentGlobalBPM;
        }
    }

    return currentGlobalBPM;
}

juce::ValueTree SectionManager::saveState() const
{
    juce::ScopedLock lock(sectionLock);

    juce::ValueTree state("Sections");
    state.setProperty("globalBPM", globalBPM.load(std::memory_order_relaxed), nullptr);
    state.setProperty("nextSectionId", nextSectionId, nullptr);

    for (const auto& section : sections)
    {
        state.appendChild(section.toValueTree(), nullptr);
    }

    return state;
}

void SectionManager::restoreState(const juce::ValueTree& state)
{
    if (!state.hasType("Sections"))
        return;

    {
        juce::ScopedLock lock(sectionLock);

        sections.clear();

        globalBPM.store(state.getProperty("globalBPM", 120.0), std::memory_order_relaxed);
        nextSectionId = state.getProperty("nextSectionId", 1);

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto sectionTree = state.getChild(i);
            if (sectionTree.hasType("Section"))
            {
                sections.push_back(Section::fromValueTree(sectionTree));
            }
        }
    }

    sendChangeMessage();
}
