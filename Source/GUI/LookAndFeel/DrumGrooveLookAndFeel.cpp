#include "DrumGrooveLookAndFeel.h"

DrumGrooveLookAndFeel& DrumGrooveLookAndFeel::getInstance()
{
    static DrumGrooveLookAndFeel instance;
    return instance;
}

DrumGrooveLookAndFeel::DrumGrooveLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, ColourPalette::mainBackground);
    setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    setColour(juce::TextButton::buttonOnColourId, ColourPalette::primaryBlue);
    setColour(juce::TextButton::textColourOffId, ColourPalette::primaryText);
    setColour(juce::TextButton::textColourOnId, ColourPalette::primaryText);
    setColour(juce::ComboBox::backgroundColourId, ColourPalette::inputBackground);
    setColour(juce::ComboBox::textColourId, ColourPalette::primaryText);
    setColour(juce::ComboBox::outlineColourId, ColourPalette::borderColour);
    setColour(juce::Label::textColourId, ColourPalette::primaryText);
    setColour(juce::ScrollBar::backgroundColourId, ColourPalette::secondaryBackground);
    setColour(juce::ScrollBar::thumbColourId, ColourPalette::borderColour);

    // IMPROVED: Set ToggleButton colours for better visibility
    setColour(juce::ToggleButton::textColourId, ColourPalette::primaryText);
    setColour(juce::ToggleButton::tickColourId, ColourPalette::primaryBlue);
    setColour(juce::ToggleButton::tickDisabledColourId, ColourPalette::disabledText);
}

void DrumGrooveLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(1, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont(getNormalFont().withHeight(14.0f));
}

void DrumGrooveLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool isMouseOverButton,
                                                 bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1);

    // Consistent corner radius for all control buttons
    float cornerSize = 6.0f;

    juce::Colour bg;
    if (!button.isEnabled())
        bg = ColourPalette::buttonDisabled;
    else if (isButtonDown)
        bg = ColourPalette::buttonPressed;
    else if (isMouseOverButton)
        bg = ColourPalette::buttonHover;
    else if (button.getToggleState())
        bg = ColourPalette::primaryBlue;
    else
        bg = backgroundColour;

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, cornerSize);

    if (button.isEnabled())
    {
        g.setColour(button.hasKeyboardFocus(true) ? ColourPalette::focusBorder : ColourPalette::borderColour);
        g.drawRoundedRectangle(bounds, cornerSize, button.hasKeyboardFocus(true) ? 2.0f : 1.0f);
    }
}

void DrumGrooveLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool, bool)
{
    auto font = getNormalFont();

    // IMPROVED: Better font sizing for different button types
    if (button.getButtonText() == "SOLO" || button.getButtonText() == "MUTE")
    {
        font = font.withHeight(11.0f).boldened(); // SMALLER but bold for Solo/Mute buttons
    }
    else if (button.getButtonText().contains("Sync"))  // Sync to Host button - SMALLER TEXT
    {
        font = font.withHeight(11.0f);  // REDUCED to 11.0f for Sync button
    }
    else if (button.getButtonText().contains("ADD FOLDER") ||
        button.getButtonText().contains("RESCAN") ||
        button.getButtonText().contains("ABOUT"))
    {
        font = font.withHeight(11.0f);  // Same font size for all other control buttons
    }
    else if (button.getButtonText().contains("PLAY") || button.getButtonText().contains("STOP"))
    {
        font = font.boldened();
    }

    g.setFont(font);
    g.setColour(button.isEnabled() ? ColourPalette::primaryText : ColourPalette::disabledText);

    // IMPROVED: Better text centering for Solo/Mute buttons
    int textMargin = 2;
    if (button.getButtonText() == "SOLO" || button.getButtonText() == "MUTE")
    {
        textMargin = 1; // Smaller margin for these buttons
    }

    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(textMargin, 0),
                     juce::Justification::centred, 1);
}

// IMPROVED: Custom ToggleButton drawing for Solo/Mute buttons
void DrumGrooveLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();

    // For SOLO and MUTE buttons, draw them as styled buttons
    if (button.getButtonText() == "SOLO" || button.getButtonText() == "MUTE")
    {
        // Draw background like a regular button
        float cornerSize = 4.0f;

        juce::Colour bg;
        if (!button.isEnabled())
            bg = ColourPalette::buttonDisabled;
        else if (shouldDrawButtonAsDown)
            bg = ColourPalette::buttonPressed;
        else if (shouldDrawButtonAsHighlighted)
            bg = ColourPalette::buttonHover;
        else if (button.getToggleState())
        {
            // Use specific colours for active state
            if (button.getButtonText() == "SOLO")
                bg = ColourPalette::warningOrange;
            else
                bg = ColourPalette::errorRed;
        }
        else
            bg = ColourPalette::buttonBackground;

        g.setColour(bg);
        g.fillRoundedRectangle(bounds.reduced(1), cornerSize);

        // Draw border
        g.setColour(button.hasKeyboardFocus(true) ? ColourPalette::focusBorder : ColourPalette::borderColour);
        g.drawRoundedRectangle(bounds.reduced(1), cornerSize, 1.0f);

        // Draw text
        g.setColour(button.isEnabled() ? ColourPalette::primaryText : ColourPalette::disabledText);
        g.setFont(getNormalFont().withHeight(11.0f).boldened());
        g.drawFittedText(button.getButtonText(), bounds.toNearestInt().reduced(2, 0),
                         juce::Justification::centred, 1);
    }
    else
    {
        // Default toggle button rendering for other buttons
        LookAndFeel_V4::drawToggleButton(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    }
}

void DrumGrooveLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                         bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    const float cornerSize = 4.0f;

    g.setColour(box.isEnabled() ? ColourPalette::inputBackground : ColourPalette::secondaryBackground);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(box.hasKeyboardFocus(true) ? ColourPalette::focusBorder : ColourPalette::borderColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, box.hasKeyboardFocus(true) ? 2.0f : 1.0f);

    juce::Path arrow;
    arrow.startNewSubPath(static_cast<float>(width) - 15.0f, static_cast<float>(height) * 0.45f);
    arrow.lineTo(static_cast<float>(width) - 10.0f, static_cast<float>(height) * 0.55f);
    arrow.lineTo(static_cast<float>(width) - 5.0f, static_cast<float>(height) * 0.45f);

    g.setColour(ColourPalette::primaryText);
    g.strokePath(arrow, juce::PathStrokeType(2.0f));
}

juce::Font DrumGrooveLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return getNormalFont().withHeight(15.0f);
}

juce::Font DrumGrooveLookAndFeel::getPopupMenuFont()
{
    return getNormalFont().withHeight(15.0f);
}

juce::Font DrumGrooveLookAndFeel::getLabelFont(juce::Label& /*label*/)
{
    return getNormalFont();
}

void DrumGrooveLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&,
                                          int x, int y, int width, int height,
                                          bool isScrollbarVertical, int thumbStartPosition,
                                          int thumbSize, bool isMouseOver, bool)
{
    g.setColour(ColourPalette::secondaryBackground);
    g.fillRect(x, y, width, height);

    if (thumbSize > 0)
    {
        juce::Rectangle<float> thumb;
        if (isScrollbarVertical)
            thumb = juce::Rectangle<float>(static_cast<float>(x) + 2.0f,
                                           static_cast<float>(thumbStartPosition) + 2.0f,
                                           static_cast<float>(width) - 4.0f,
                                           static_cast<float>(thumbSize) - 4.0f);
            else
                thumb = juce::Rectangle<float>(static_cast<float>(thumbStartPosition) + 2.0f,
                                               static_cast<float>(y) + 2.0f,
                                               static_cast<float>(thumbSize) - 4.0f,
                                               static_cast<float>(height) - 4.0f);

                g.setColour(isMouseOver ? ColourPalette::borderColour.brighter() : ColourPalette::borderColour);
            g.fillRoundedRectangle(thumb, 6.0f);
    }
}

void DrumGrooveLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float, float,
                                             const juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float trackHeight = 4.0f;
    const float thumbDiameter = 16.0f;

    juce::Rectangle<float> track(static_cast<float>(x),
                                 static_cast<float>(y) + (static_cast<float>(height) - trackHeight) * 0.5f,
                                 static_cast<float>(width), trackHeight);

    g.setColour(ColourPalette::inputBackground);
    g.fillRoundedRectangle(track, trackHeight * 0.5f);

    g.setColour(ColourPalette::primaryBlue);
    g.fillRoundedRectangle(track.withWidth(sliderPos - static_cast<float>(x)), trackHeight * 0.5f);

    g.setColour(slider.isMouseOverOrDragging() ? ColourPalette::hoverBlue : ColourPalette::primaryBlue);
    g.fillEllipse(sliderPos - thumbDiameter * 0.5f,
                  static_cast<float>(y) + (static_cast<float>(height) - thumbDiameter) * 0.5f,
                  thumbDiameter, thumbDiameter);
}

void DrumGrooveLookAndFeel::drawProgressBar(juce::Graphics& g, juce::ProgressBar&,
                                            int width, int height, double progress,
                                            const juce::String& textToShow)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    const float cornerSize = 3.0f;

    g.setColour(ColourPalette::inputBackground);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(ColourPalette::borderColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);

    if (progress > 0.0)
    {
        g.setColour(ColourPalette::successGreen);
        g.fillRoundedRectangle(bounds.reduced(2.0f).withWidth((static_cast<float>(width) - 4.0f) * static_cast<float>(progress)), cornerSize);
    }

    if (textToShow.isNotEmpty())
    {
        g.setColour(ColourPalette::primaryText);
        g.setFont(getSmallFont());
        g.drawText(textToShow, bounds, juce::Justification::centred);
    }
}

void DrumGrooveLookAndFeel::drawGroupComponentOutline(juce::Graphics& g, int width, int height,
                                                      const juce::String& text,
                                                      const juce::Justification&,
                                                      juce::GroupComponent&)
{
    const float cornerSize = 5.0f;
    auto bounds = juce::Rectangle<float>(0.0f, 10.0f, static_cast<float>(width), static_cast<float>(height) - 10.0f);

    g.setColour(ColourPalette::separator);
    g.drawRoundedRectangle(bounds, cornerSize, 2.0f);

    if (text.isNotEmpty())
    {
        auto font = getSubHeaderFont();
        // Use GlyphArrangement for JUCE 8+ string width calculation
        auto textWidth = juce::GlyphArrangement::getStringWidthInt(font, text) + 20;

        g.setColour(ColourPalette::panelBackground);
        g.fillRect(15.0f, 0.0f, static_cast<float>(textWidth), 20.0f);

        g.setColour(ColourPalette::primaryBlue);
        g.setFont(font);
        g.drawText(text, 20, 0, textWidth - 10, 20, juce::Justification::left);
    }
}
