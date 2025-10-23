#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ColourPalette.h"

class DrumGrooveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static DrumGrooveLookAndFeel& getInstance();

    DrumGrooveLookAndFeel();
    ~DrumGrooveLookAndFeel() override = default;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;
                              void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                                  bool isMouseOverButton, bool isButtonDown) override;
                                                  void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
                                                                        void drawComboBox(juce::Graphics& g, int width, int height,
                                                                                          bool isButtonDown, int buttonX, int buttonY,
                                                                                          int buttonW, int buttonH, juce::ComboBox& box) override;
                                                                                          juce::Font getComboBoxFont(juce::ComboBox&) override;
                                                                                          juce::Font getLabelFont(juce::Label& label) override;
                                                                                          void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                                                                                                             int x, int y, int width, int height,
                                                                                                             bool isScrollbarVertical, int thumbStartPosition,
                                                                                                             int thumbSize, bool isMouseOver, bool isMouseDown) override;
                                                                                                             void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                                                                                                                   float sliderPos, float minSliderPos, float maxSliderPos,
                                                                                                                                   const juce::Slider::SliderStyle style, juce::Slider& slider) override;
                                                                                                                                   void drawProgressBar(juce::Graphics& g, juce::ProgressBar& progressBar,
                                                                                                                                                        int width, int height, double progress,
                                                                                                                                                        const juce::String& textToShow) override;
                                                                                                                                                        void drawGroupComponentOutline(juce::Graphics& g, int width, int height,
                                                                                                                                                                                       const juce::String& text,
                                                                                                                                                                                       const juce::Justification& position,
                                                                                                                                                                                       juce::GroupComponent& group) override;

                                                                                                                                                                                       // Font utilities - CORRECTED for JUCE 8
                                                                                                                                                                                       juce::Font getTitleFont() const {
                                                                                                                                                                                           return juce::Font(juce::FontOptions("Segoe UI", 32.0f, juce::Font::bold));
                                                                                                                                                                                       }
                                                                                                                                                                                       juce::Font getHeaderFont() const {
                                                                                                                                                                                           return juce::Font(juce::FontOptions("Segoe UI", 24.0f, juce::Font::bold));
                                                                                                                                                                                       }
                                                                                                                                                                                       juce::Font getSubHeaderFont() const {
                                                                                                                                                                                           return juce::Font(juce::FontOptions("Segoe UI", 14.0f, juce::Font::bold));
                                                                                                                                                                                       }
                                                                                                                                                                                       juce::Font getNormalFont() const {
                                                                                                                                                                                           return juce::Font(juce::FontOptions("Segoe UI", 12.0f, juce::Font::plain));
                                                                                                                                                                                       }
                                                                                                                                                                                       juce::Font getSmallFont() const {
                                                                                                                                                                                           return juce::Font(juce::FontOptions("Segoe UI", 11.0f, juce::Font::plain));
                                                                                                                                                                                       }
                                                                                                                                                                                       juce::Font getMonospaceFont() const {
                                                                                                                                                                                           return juce::Font(juce::FontOptions("Consolas", 14.0f, juce::Font::bold));
                                                                                                                                                                                       }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGrooveLookAndFeel)
};
