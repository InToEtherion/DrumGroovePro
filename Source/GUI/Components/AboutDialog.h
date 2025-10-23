#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class AboutDialog : public juce::DialogWindow
{
public:
    AboutDialog();
    ~AboutDialog() override;
    void closeButtonPressed() override;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutDialog)
};

class AboutContent : public juce::Component, 
                     public juce::Button::Listener,
                     public juce::Thread
{
public:
    AboutContent();
    ~AboutContent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void run() override; // Thread function for update check
    
private:
    void checkForUpdates();
    void updateCheckComplete(bool updateAvailable);
    
    juce::Label versionLabel, authorLabel, descriptionLabel, licenseLabel, supportLabel;
    juce::TextButton githubButton, updateCheckButton, coffeeButton, closeButton;
    juce::ImageComponent logoImage;
    
    bool isCheckingForUpdates = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutContent)
};