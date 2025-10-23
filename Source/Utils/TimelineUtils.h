#pragma once

#include <juce_core/juce_core.h>

namespace TimelineUtils
{
    // BPM scaling calculations
    constexpr double BASE_BPM = 120.0; // Reference BPM for 1.0 scale
    
    /**
     * Calculate visual scaling factor based on track BPM
     * @param trackBPM Current track BPM
     * @return Scale factor (1.0 = normal, <1.0 = shorter clips, >1.0 = longer clips)
     */
    inline double getVisualScaleFactor(double trackBPM)
    {
        return BASE_BPM / juce::jmax(1.0, trackBPM);
    }
    
    /**
     * Convert time to visual pixels with BPM scaling
     * @param time Time in seconds
     * @param viewStartTime Start time of view
     * @param zoomLevel Zoom level in pixels per second
     * @param trackBPM Track BPM for scaling
     * @param offsetX X offset (e.g., track header width)
     * @return Pixel position
     */
    inline float timeToVisualPixels(double time, double viewStartTime, float zoomLevel, 
                                   double trackBPM, float offsetX = 0.0f)
    {
        double scaleFactor = getVisualScaleFactor(trackBPM);
        return offsetX + static_cast<float>((time - viewStartTime) * zoomLevel * scaleFactor);
    }
    
    /**
     * Convert visual pixels to time with BPM scaling
     * @param pixels Pixel position
     * @param viewStartTime Start time of view
     * @param zoomLevel Zoom level in pixels per second
     * @param trackBPM Track BPM for scaling
     * @param offsetX X offset (e.g., track header width)
     * @return Time in seconds
     */
    inline double visualPixelsToTime(float pixels, double viewStartTime, float zoomLevel,
                                    double trackBPM, float offsetX = 0.0f)
    {
        double scaleFactor = getVisualScaleFactor(trackBPM);
        return viewStartTime + ((pixels - offsetX) / (zoomLevel * scaleFactor));
    }
    
    /**
     * Calculate actual playback time from visual time
     * Since visual scaling affects appearance but not playback timing
     * @param visualTime Time position in visual space
     * @return Actual playback time (unchanged)
     */
    inline double visualTimeToPlaybackTime(double visualTime)
    {
        // Visual scaling doesn't affect playback timing
        return visualTime;
    }
    
    /**
     * Calculate visual duration from actual duration
     * @param actualDuration Duration in seconds
     * @param trackBPM Track BPM for scaling
     * @return Visual duration for display
     */
    inline double actualDurationToVisualDuration(double actualDuration, double trackBPM)
    {
        return actualDuration * getVisualScaleFactor(trackBPM);
    }
    
    /**
     * Format time with high precision
     * @param seconds Time in seconds
     * @return Formatted string (HH:MM:SS:mmm)
     */
    inline juce::String formatTime(double seconds)
    {
        int hours = static_cast<int>(seconds) / 3600;
        int mins = (static_cast<int>(seconds) % 3600) / 60;
        int secs = static_cast<int>(seconds) % 60;
        int millis = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);
        
        return juce::String::formatted("%02d:%02d:%02d:%03d", hours, mins, secs, millis);
    }
    
    /**
     * Parse time string to seconds
     * @param timeStr Time string in format HH:MM:SS:mmm
     * @return Time in seconds
     */
    inline double parseTime(const juce::String& timeStr)
    {
        auto parts = juce::StringArray::fromTokens(timeStr, ":", "");
        if (parts.size() >= 4)
        {
            int hours = parts[0].getIntValue();
            int mins = parts[1].getIntValue();
            int secs = parts[2].getIntValue();
            int millis = parts[3].getIntValue();
            
            return hours * 3600.0 + mins * 60.0 + secs + millis / 1000.0;
        }
        return 0.0;
    }
    
    /**
     * Check if time string is valid format
     * @param timeStr Time string to validate
     * @return True if valid format
     */
    inline bool isValidTimeFormat(const juce::String& timeStr)
    {
        auto parts = juce::StringArray::fromTokens(timeStr, ":", "");
        return parts.size() == 4 && 
               parts[0].containsOnly("0123456789") &&
               parts[1].containsOnly("0123456789") &&
               parts[2].containsOnly("0123456789") &&
               parts[3].containsOnly("0123456789");
    }
    
    /**
     * Snap time to grid
     * @param time Time to snap
     * @param gridInterval Grid interval in seconds
     * @return Snapped time
     */
    inline double snapToGrid(double time, double gridInterval)
    {
        return std::round(time / gridInterval) * gridInterval;
    }
    
    /**
     * Calculate optimal grid interval based on zoom level - PRECISION OPTIMIZED
     * @param zoomLevel Current zoom level
     * @return Optimal grid interval in seconds
     */
    inline double calculateOptimalGridInterval(float zoomLevel)
    {
        // FIXED: Professional-grade precision intervals for drum editing
        if (zoomLevel < 15)        return 10.0;     // Very zoomed out: 10 seconds
        else if (zoomLevel < 30)   return 5.0;      // 5 seconds
        else if (zoomLevel < 60)   return 2.0;      // 2 seconds  
        else if (zoomLevel < 100)  return 1.0;      // 1 second
        else if (zoomLevel < 150)  return 0.5;      // 500ms
        else if (zoomLevel < 200)  return 0.25;     // 250ms
        else if (zoomLevel < 280)  return 0.1;      // 100ms ← TARGET FOR DRUM EDITING
        else if (zoomLevel < 350)  return 0.05;     // 50ms
        else if (zoomLevel < 420)  return 0.025;    // 25ms
        else                       return 0.01;     // 10ms - ultra precise
    }
    
    /**
     * Convenience method for setting common grid intervals
     * @param interval String representation of interval ("1s", "100ms", etc.)
     * @return Grid interval in seconds
     */
    inline double parseGridInterval(const juce::String& interval)
    {
        if (interval == "1s")       return 1.0;
        else if (interval == "500ms") return 0.5;
        else if (interval == "250ms") return 0.25;
        else if (interval == "100ms") return 0.1;    // TARGET FOR DRUMS
        else if (interval == "50ms")  return 0.05;
        else if (interval == "25ms")  return 0.025;
        else if (interval == "10ms")  return 0.01;
        else return 1.0; // Default
    }
}