#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace LayoutUtils
{
    // Responsive scaling based on display resolution for Windows 11
    inline float getDisplayScale()
    {
        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (display)
        {
            float scale = display->scale;
            
            // Additional scaling based on resolution
            auto area = display->userArea;
            if (area.getWidth() >= 3440 && area.getHeight() >= 1440) // Ultra-wide
                return scale; // Use native DPI scaling for 3440x1440
            else if (area.getWidth() >= 2560) // QHD
                return scale * 1.1f;
            else
                return scale;
        }
        return 1.0f;
    }
    
    // Calculate proportional dimensions
    inline int scaleForDisplay(int baseValue)
    {
        return static_cast<int>(baseValue * getDisplayScale());
    }
    
    // Get optimal window size for current display
    inline juce::Rectangle<int> getOptimalWindowBounds()
    {
        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (display)
        {
            auto area = display->userArea;
            int width = 1300;
            int height = 900;
            
            // Scale based on display resolution
            if (area.getWidth() >= 3440 && area.getHeight() >= 1440)
            {
                // Ultra-wide - use specified 1300x900
                width = 1300;
                height = 900;
            }
            else if (area.getWidth() >= 2560)
            {
                // QHD
                width = 1400;
                height = 950;
            }
            else if (area.getWidth() < 1920)
            {
                // Lower resolution
                width = juce::jmin(1200, area.getWidth() - 100);
                height = juce::jmin(800, area.getHeight() - 100);
            }
            
            // Apply Windows DPI scaling
            float scale = display->scale;
            width = static_cast<int>(width * scale);
            height = static_cast<int>(height * scale);
            
            // Center on screen
            int x = area.getCentreX() - width / 2;
            int y = area.getCentreY() - height / 2;
            
            return juce::Rectangle<int>(x, y, width, height);
        }
        
        return juce::Rectangle<int>(0, 0, 1300, 900);
    }
    
    // Layout proportions for responsive design
    struct ResponsiveLayout
    {
        static constexpr float HEADER_RATIO = 0.15f;
        static constexpr float BROWSER_RATIO = 0.60f;
        static constexpr float TIMELINE_RATIO = 0.25f;
        static constexpr float LEFT_PANEL_RATIO = 0.23f;
        
        static int getHeaderHeight(int totalHeight)
        {
            return static_cast<int>(totalHeight * HEADER_RATIO);
        }
        
        static int getBrowserHeight(int totalHeight)
        {
            int headerHeight = getHeaderHeight(totalHeight);
            int timelineHeight = getTimelineHeight(totalHeight);
            return totalHeight - headerHeight - timelineHeight;
        }
        
        static int getTimelineHeight(int totalHeight)
        {
            return static_cast<int>(totalHeight * TIMELINE_RATIO);
        }
        
        static int getLeftPanelWidth(int totalWidth)
        {
            return juce::jmax(250, static_cast<int>(totalWidth * LEFT_PANEL_RATIO));
        }
    };
    
    // FlexBox helpers for consistent spacing
    inline juce::FlexBox createHorizontalBox(float gap = 10.0f)
    {
        juce::FlexBox box;
        box.flexDirection = juce::FlexBox::Direction::row;
        box.alignItems = juce::FlexBox::AlignItems::center;
        box.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        box.flexWrap = juce::FlexBox::Wrap::noWrap;
        box.alignContent = juce::FlexBox::AlignContent::stretch;
        return box;
    }
    
    inline juce::FlexBox createVerticalBox(float gap = 10.0f)
    {
        juce::FlexBox box;
        box.flexDirection = juce::FlexBox::Direction::column;
        box.alignItems = juce::FlexBox::AlignItems::stretch;
        box.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        box.flexWrap = juce::FlexBox::Wrap::noWrap;
        box.alignContent = juce::FlexBox::AlignContent::stretch;
        return box;
    }
    
    // Grid helpers
    inline juce::Grid createGrid(int columns, int rows, float gap = 10.0f)
    {
        juce::Grid grid;
        
        // Set up columns
        for (int i = 0; i < columns; ++i)
            grid.templateColumns.add(juce::Grid::TrackInfo(juce::Grid::Fr(1)));
        
        // Set up rows
        for (int i = 0; i < rows; ++i)
            grid.templateRows.add(juce::Grid::TrackInfo(juce::Grid::Fr(1)));
        
        grid.columnGap = juce::Grid::Px(gap);
        grid.rowGap = juce::Grid::Px(gap);
        
        return grid;
    }
}