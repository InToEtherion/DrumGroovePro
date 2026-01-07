#include "CustomColourPicker.h"

CustomColourPicker::CustomColourPicker(const juce::Colour& initialColour)
: selectedColour(initialColour)
{
    // Palette label
    paletteLabel.setText("Quick Colors:", juce::dontSendNotification);
    paletteLabel.setFont(juce::Font(14.0f).boldened());
    paletteLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(paletteLabel);

    // Create 20 palette buttons (4 rows x 5 columns)
    auto paletteColours = getPaletteColours();
    for (int i = 0; i < paletteColours.size(); ++i)
    {
        auto* btn = new juce::TextButton();
        btn->setButtonText("");
        btn->setColour(juce::TextButton::buttonColourId, paletteColours[i]);
        btn->setColour(juce::TextButton::buttonOnColourId, paletteColours[i]);
        btn->onClick = [this, i]() { paletteButtonClicked(i); };
        paletteButtons.add(btn);
        addAndMakeVisible(btn);
    }

    // Custom color label
    customLabel.setText("Or choose any color:", juce::dontSendNotification);
    customLabel.setFont(juce::Font(14.0f).boldened());
    customLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(customLabel);

    // Full color selector
    colourSelector = std::make_unique<juce::ColourSelector>(
        juce::ColourSelector::showColourAtTop |
        juce::ColourSelector::showSliders |
        juce::ColourSelector::showColourspace);
    colourSelector->setCurrentColour(initialColour);
    colourSelector->addChangeListener(this);
    addAndMakeVisible(colourSelector.get());

    // OK button
    okButton.setButtonText("OK");
    okButton.setColour(juce::TextButton::buttonColourId, ColourPalette::primaryBlue);
    okButton.addListener(this);
    addAndMakeVisible(okButton);

    // Cancel button
    cancelButton.setButtonText("Cancel");
    cancelButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonBackground);
    cancelButton.addListener(this);
    addAndMakeVisible(cancelButton);

    setSize(420, 550);
}

CustomColourPicker::~CustomColourPicker()
{
    if (colourSelector)
        colourSelector->removeChangeListener(this);
}

void CustomColourPicker::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);

    // Draw border around palette area
    auto paletteBounds = juce::Rectangle<int>(10, 35, 400, 120);
    g.setColour(ColourPalette::borderColour);
    g.drawRect(paletteBounds, 1);
}

void CustomColourPicker::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Palette label
    paletteLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);

    // Palette buttons (4 rows x 5 columns)
    auto paletteBounds = bounds.removeFromTop(120);
    paletteBounds.reduce(5, 5);

    int buttonSize = 70;
    int spacing = 8;

    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 5; ++col)
        {
            int index = row * 5 + col;
            if (index < paletteButtons.size())
            {
                int x = paletteBounds.getX() + col * (buttonSize + spacing);
                int y = paletteBounds.getY() + row * (25 + spacing);
                paletteButtons[index]->setBounds(x, y, buttonSize, 25);
            }
        }
    }

    bounds.removeFromTop(10);

    // Custom color label
    customLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);

    // Color selector
    auto selectorBounds = bounds.removeFromTop(300);
    colourSelector->setBounds(selectorBounds);

    bounds.removeFromTop(10);

    // Buttons at bottom
    auto buttonArea = bounds.removeFromTop(30);
    int buttonWidth = 100;
    cancelButton.setBounds(buttonArea.removeFromRight(buttonWidth));
    buttonArea.removeFromRight(10);
    okButton.setBounds(buttonArea.removeFromRight(buttonWidth));
}

void CustomColourPicker::buttonClicked(juce::Button* button)
{
    if (button == &okButton)
    {
        selectedColour = colourSelector->getCurrentColour();
        okClicked = true;

        if (onColourChanged)
            onColourChanged(selectedColour);
    }
    else if (button == &cancelButton)
    {
        okClicked = false;
    }

    // Close the callout box
    if (auto* parent = getParentComponent())
    {
        if (auto* callout = dynamic_cast<juce::CallOutBox*>(parent))
        {
            callout->dismiss();
        }
    }
}

void CustomColourPicker::paletteButtonClicked(int buttonIndex)
{
    auto paletteColours = getPaletteColours();
    if (buttonIndex >= 0 && buttonIndex < paletteColours.size())
    {
        selectedColour = paletteColours[buttonIndex];
        colourSelector->setCurrentColour(selectedColour);
    }
}

void CustomColourPicker::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // Update selected color as user adjusts the selector
    // (but don't commit until OK is clicked)
    selectedColour = colourSelector->getCurrentColour();
}

juce::Array<juce::Colour> CustomColourPicker::getPaletteColours()
{
    return {
        // Row 1 - Blues and Cyans
        juce::Colour(0xff0084ff),  // Primary Blue
        juce::Colour(0xff00a0ff),  // Hover Blue
        juce::Colour(0xff00e5ff),  // Cyan Accent
        juce::Colour(0xff0066cc),  // Deep Blue
        juce::Colour(0xff00ccff),  // Light Cyan

        // Row 2 - Greens and Yellows
        juce::Colour(0xff4caf50),  // Success Green
        juce::Colour(0xff66bb6a),  // Light Green
        juce::Colour(0xff2e7d32),  // Dark Green
        juce::Colour(0xffffd54f),  // Yellow (Playhead)
        juce::Colour(0xffffeb3b),  // Bright Yellow

        // Row 3 - Oranges and Reds
        juce::Colour(0xffff9800),  // Warning Orange
        juce::Colour(0xffffb74d),  // Light Orange
        juce::Colour(0xfff44336),  // Error Red
        juce::Colour(0xffef5350),  // Light Red
        juce::Colour(0xffc62828),  // Dark Red

        // Row 4 - Purples and Pinks
        juce::Colour(0xff9c27b0),  // Purple
        juce::Colour(0xffba68c8),  // Light Purple
        juce::Colour(0xffe91e63),  // Pink
        juce::Colour(0xfff06292),  // Light Pink
        juce::Colour(0xff880e4f)   // Dark Pink
    };
}
