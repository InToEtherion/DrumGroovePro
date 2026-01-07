#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "ColourPalette.h"

/**
 * Custom color picker with a predefined 20-color palette at the top
 * and a full color selector below for any custom color.
 */
class CustomColourPicker : public juce::Component,
public juce::Button::Listener,
    public juce::ChangeListener
    {
    public:
        CustomColourPicker(const juce::Colour& initialColour);
        ~CustomColourPicker() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void buttonClicked(juce::Button* button) override;
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;

        juce::Colour getSelectedColour() const { return selectedColour; }
        bool wasOkClicked() const { return okClicked; }

        std::function<void(const juce::Colour&)> onColourChanged;

        // Get the predefined palette colors
        static juce::Array<juce::Colour> getPaletteColours();

    private:
        juce::Colour selectedColour;
        bool okClicked = false;

        juce::Label paletteLabel;
        juce::Label customLabel;

        // Palette buttons (20 colors in 4 rows x 5 columns)
        juce::OwnedArray<juce::TextButton> paletteButtons;

        // Full color selector for custom colors
        std::unique_ptr<juce::ColourSelector> colourSelector;

        // OK and Cancel buttons
        juce::TextButton okButton;
        juce::TextButton cancelButton;

        void paletteButtonClicked(int buttonIndex);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomColourPicker)
    };
