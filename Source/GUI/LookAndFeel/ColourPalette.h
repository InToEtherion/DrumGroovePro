#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ColourPalette
{
    // Main backgrounds
    const juce::Colour mainBackground      = juce::Colour(0xff1a1a1a);  // Dark background
    const juce::Colour panelBackground     = juce::Colour(0xff252525);  // Panel background
    const juce::Colour secondaryBackground = juce::Colour(0xff2d2d2d);  // Secondary panels
    
    // Text colors
    const juce::Colour primaryText         = juce::Colour(0xffe8e8e8);  // Primary white text
    const juce::Colour secondaryText       = juce::Colour(0xffb8b8b8);  // Secondary gray text
    const juce::Colour mutedText          = juce::Colour(0xff808080);  // Muted gray text
    const juce::Colour lightGrayText      = juce::Colour(0xffa0a0a0);  // Light gray text
    const juce::Colour disabledText       = juce::Colour(0xff606060);  // Disabled text
    
    // Accent colors
    const juce::Colour primaryBlue        = juce::Colour(0xff0084ff);  // Primary blue accent
    const juce::Colour hoverBlue          = juce::Colour(0xff00a0ff);  // Hover blue
    const juce::Colour cyanAccent         = juce::Colour(0xff00e5ff);  // Cyan for "Pro"
    const juce::Colour successGreen       = juce::Colour(0xff4caf50);  // Success green
    const juce::Colour warningOrange      = juce::Colour(0xffff9800);  // Warning orange
    const juce::Colour errorRed           = juce::Colour(0xfff44336);  // Error red
    
    // UI elements
    const juce::Colour borderColour       = juce::Colour(0xff404040);  // Border gray
    const juce::Colour focusBorder        = juce::Colour(0xff0084ff);  // Focus border blue
    const juce::Colour separator          = juce::Colour(0xff383838);  // Separator line
    const juce::Colour inputBackground    = juce::Colour(0xff1f1f1f);  // Input field background
    
    // Button states
    const juce::Colour buttonBackground   = juce::Colour(0xff353535);  // Button default
    const juce::Colour buttonHover        = juce::Colour(0xff454545);  // Button hover
    const juce::Colour buttonPressed      = juce::Colour(0xff505050);  // Button pressed
    const juce::Colour buttonDisabled     = juce::Colour(0xff2a2a2a);  // Button disabled
    
    // Timeline specific colors
    const juce::Colour yellowPlayhead     = juce::Colour(0xffffd54f);  // Playhead yellow
    const juce::Colour timelineGrid       = juce::Colour(0xff2a2a2a);  // Grid lines
    const juce::Colour selectedRegion     = juce::Colour(0xff0084ff);  // Selection region
}