#include "AboutDialog.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"
#include <utility>  // For std::pair

#define CURRENT_VERSION "1.0.0"
#define GITHUB_RELEASES_API "https://api.github.com/repos/InToEtherion/DrumGroovePro/releases"

// Semantic version comparison
static int compareVersions(const juce::String& v1, const juce::String& v2)
{
    auto extractVersionParts = [](const juce::String& version) -> std::pair<juce::StringArray, int>
    {
        juce::String upperVersion = version.toUpperCase();
        int rcNumber = -1;  // -1 = final release
        juce::String baseVersion = version;

        int rcPos = upperVersion.indexOf("RC");
        if (rcPos >= 0)
        {
            juce::String rcPart = version.substring(rcPos + 2);
            rcNumber = rcPart.getIntValue();
            if (rcNumber == 0) rcNumber = 1;
            baseVersion = version.substring(0, rcPos);
        }

        auto parts = juce::StringArray::fromTokens(baseVersion, ".", "");
        while (parts.size() < 3) parts.add("0");

        return {parts, rcNumber};
    };

    auto [parts1, rc1] = extractVersionParts(v1);
    auto [parts2, rc2] = extractVersionParts(v2);

    for (int i = 0; i < 3; ++i)
    {
        int num1 = parts1[i].getIntValue();
        int num2 = parts2[i].getIntValue();

        if (num1 > num2) return 1;
        if (num1 < num2) return -1;
    }

    if (rc1 == rc2) return 0;
    if (rc1 == -1) return 1;  // Final > RC
    if (rc2 == -1) return -1;

    if (rc1 > rc2) return 1;
    return -1;
}

AboutDialog::AboutDialog()
    : DialogWindow("About DrumGroovePro", ColourPalette::panelBackground, true)
{
    setContentOwned(new AboutContent(), true);
    setSize(550, 580);
    setResizable(false, false);
    setUsingNativeTitleBar(true);
    centreWithSize(getWidth(), getHeight());
}

AboutDialog::~AboutDialog() = default;

void AboutDialog::closeButtonPressed()
{
    setVisible(false);
}

//==============================================================================
AboutContent::AboutContent()
    : juce::Thread("UpdateChecker")
{
    auto& lnf = DrumGrooveLookAndFeel::getInstance();

    versionLabel.setText("Version " CURRENT_VERSION, juce::dontSendNotification);
    versionLabel.setFont(lnf.getSubHeaderFont().withHeight(16.0f));
    versionLabel.setJustificationType(juce::Justification::centred);
    versionLabel.setColour(juce::Label::textColourId, ColourPalette::successGreen);
    addAndMakeVisible(versionLabel);

    authorLabel.setText("By InToEtherion", juce::dontSendNotification);
    authorLabel.setFont(lnf.getHeaderFont().withHeight(20.0f));
    authorLabel.setJustificationType(juce::Justification::centred);
    authorLabel.setColour(juce::Label::textColourId, ColourPalette::primaryText);
    addAndMakeVisible(authorLabel);

    descriptionLabel.setText("A VST3 plugin for browsing and playing MIDI drum grooves.\n"
                             "Features intuitive navigation, BPM sync, and support for multiple drum library formats.",
                             juce::dontSendNotification);
    descriptionLabel.setFont(lnf.getNormalFont().withHeight(14.0f));
    descriptionLabel.setJustificationType(juce::Justification::centred);
    descriptionLabel.setColour(juce::Label::textColourId, ColourPalette::secondaryText);
    addAndMakeVisible(descriptionLabel);

    licenseLabel.setText("Licensed under GPL v3", juce::dontSendNotification);
    licenseLabel.setFont(lnf.getNormalFont().withHeight(14.0f));
    licenseLabel.setJustificationType(juce::Justification::centred);
    licenseLabel.setColour(juce::Label::textColourId, ColourPalette::warningOrange);
    addAndMakeVisible(licenseLabel);

    githubButton.setButtonText("GitHub Repository");
    githubButton.addListener(this);
    addAndMakeVisible(githubButton);

    // Updated: Changed from "View License" to "Check for Update"
    updateCheckButton.setButtonText("Check for Update");
    updateCheckButton.addListener(this);
    addAndMakeVisible(updateCheckButton);

    // Updated: Changed button text and made background black
    coffeeButton.setButtonText("Buy Me a Coffee");
    coffeeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    coffeeButton.addListener(this);
    addAndMakeVisible(coffeeButton);

    supportLabel.setText("If you enjoy DrumGroovePro, please consider supporting its development!",
                        juce::dontSendNotification);
    supportLabel.setFont(lnf.getNormalFont().italicised().withHeight(15.0f));
    supportLabel.setJustificationType(juce::Justification::centred);
    supportLabel.setColour(juce::Label::textColourId, ColourPalette::mutedText);
    addAndMakeVisible(supportLabel);

    // Try to load logo from Resources folder - multiple fallback paths
    juce::File logoFile;
    juce::Array<juce::File> searchPaths;
    
    // Add multiple search paths
    searchPaths.add(juce::File::getCurrentWorkingDirectory().getChildFile("Resources/logo/logo.png"));
    searchPaths.add(juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory().getChildFile("Resources/logo/logo.png"));
    searchPaths.add(juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory().getParentDirectory().getChildFile("Resources/logo/logo.png"));
    
    // For VST3 bundle structure
    searchPaths.add(juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory().getParentDirectory().getParentDirectory()
                    .getChildFile("Resources/logo/logo.png"));

    // Try each path
    for (const auto& path : searchPaths)
    {
        #ifdef DEBUG
        DBG("Trying logo path: " + path.getFullPathName());
        #endif
        
        if (path.existsAsFile())
        {
            logoFile = path;
            #ifdef DEBUG
            DBG("Found logo at: " + path.getFullPathName());
            #endif
            break;
        }
    }

    if (logoFile.existsAsFile())
    {
        auto image = juce::ImageCache::getFromFile(logoFile);
        if (image.isValid())
        {
            logoImage.setImage(image, juce::RectanglePlacement::centred);
            #ifdef DEBUG
            DBG("Logo loaded successfully: " + juce::String(image.getWidth()) + "x" + juce::String(image.getHeight()));
            #endif
        }
        else
        {
            #ifdef DEBUG
            DBG("Failed to load logo image from file");
            #endif
        }
    }
    else
    {
        #ifdef DEBUG
        DBG("No logo file found in any search paths");
        #endif
    }
    addAndMakeVisible(logoImage);

    closeButton.setButtonText("Close");
    closeButton.addListener(this);
    addAndMakeVisible(closeButton);
}

AboutContent::~AboutContent()
{
    // Stop thread if running
    stopThread(1000);
}

void AboutContent::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::panelBackground);

    // Draw title with cyan "Pro"
    auto titleBounds = juce::Rectangle<int>(0, 10, getWidth(), 40);
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    auto font = lnf.getTitleFont();
    g.setFont(font);

    auto drumGrooveWidth = juce::GlyphArrangement::getStringWidthInt(font, "DrumGroove");
    auto proWidth = juce::GlyphArrangement::getStringWidthInt(font, "Pro");
    auto totalWidth = drumGrooveWidth + proWidth;
    auto startX = (getWidth() - totalWidth) / 2;

    g.setColour(ColourPalette::primaryText);
    g.drawText("DrumGroove", startX, 10, drumGrooveWidth, 40,
               juce::Justification::left);

    g.setColour(ColourPalette::cyanAccent);
    g.drawText("Pro", startX + drumGrooveWidth, 10,
               proWidth, 40, juce::Justification::left);

    // Draw separators
    g.setColour(ColourPalette::separator);
    g.drawLine(50.0f, 120.0f, getWidth() - 50.0f, 120.0f, 1.0f);
    g.drawLine(50.0f, 240.0f, getWidth() - 50.0f, 240.0f, 1.0f);
    
    #ifdef DEBUG
    if (!logoImage.getImage().isValid())
    {
        int logoY = getHeight() - 32 - 15 - 100 - 15;
        auto logoBounds = juce::Rectangle<int>(0, logoY, getWidth(), 100).withSizeKeepingCentre(100, 100);
        g.setColour(ColourPalette::borderColour);
        g.drawRect(logoBounds, 1);
        g.setFont(lnf.getSmallFont());
        g.drawText("Logo Missing", logoBounds, juce::Justification::centred);
    }
    #endif
}

void AboutContent::resized()
{
    auto bounds = getLocalBounds();

    // Title space
    bounds.removeFromTop(50);

    // Content with adjusted spacing for bigger fonts
    versionLabel.setBounds(bounds.removeFromTop(28));
    authorLabel.setBounds(bounds.removeFromTop(32));

    bounds.removeFromTop(12);
    descriptionLabel.setBounds(bounds.removeFromTop(75));
    licenseLabel.setBounds(bounds.removeFromTop(28));

    bounds.removeFromTop(12);

    // Buttons
    auto buttonRow = bounds.removeFromTop(32);
    auto buttonArea = buttonRow.withSizeKeepingCentre(350, 32);
    githubButton.setBounds(buttonArea.removeFromLeft(170));
    buttonArea.removeFromLeft(10);
    updateCheckButton.setBounds(buttonArea); // Updated button name

    bounds.removeFromTop(12);
    coffeeButton.setBounds(bounds.removeFromTop(32).withSizeKeepingCentre(180, 32)); // Wider for new text

    bounds.removeFromTop(12);
    supportLabel.setBounds(bounds.removeFromTop(40));

    // Logo
    bounds.removeFromTop(15);
    logoImage.setBounds(bounds.removeFromTop(100).withSizeKeepingCentre(100, 100));

    // Close button
    bounds.removeFromTop(15);
    closeButton.setBounds(bounds.removeFromTop(32).withSizeKeepingCentre(100, 32));
}

void AboutContent::buttonClicked(juce::Button* button)
{
    if (button == &githubButton)
    {
        juce::URL("https://github.com/InToEtherion/DrumGroovePro").launchInDefaultBrowser();
    }
    else if (button == &updateCheckButton)
    {
        if (!isCheckingForUpdates)
        {
            checkForUpdates();
        }
    }
    else if (button == &coffeeButton)
    {
        // Updated URL
        juce::URL("https://coff.ee/intoetherion").launchInDefaultBrowser();
    }
    else if (button == &closeButton)
    {
        if (auto* dialog = findParentComponentOfClass<AboutDialog>())
            dialog->closeButtonPressed();
    }
}

void AboutContent::checkForUpdates()
{
    if (isThreadRunning())
        return;
        
    isCheckingForUpdates = true;
    updateCheckButton.setButtonText("Checking...");
    updateCheckButton.setEnabled(false);
    
    startThread();
}

void AboutContent::run()
{
    bool updateAvailable = false;
    
    try
    {
        juce::URL apiUrl(GITHUB_RELEASES_API);
        
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                          .withConnectionTimeoutMs(10000)
                          .withNumRedirectsToFollow(5)
                          .withExtraHeaders("User-Agent: DrumGroovePro/1.0");
        
        std::unique_ptr<juce::InputStream> stream(apiUrl.createInputStream(options));
        
        if (stream != nullptr)
        {
            juce::String response = stream->readEntireStreamAsString();
            
            if (response.isNotEmpty())
            {
                auto jsonResult = juce::JSON::parse(response);
                
                // API now returns an ARRAY of releases, not a single object
                if (jsonResult.isArray())
                {
                    auto* jsonArray = jsonResult.getArray();
                    if (jsonArray != nullptr && jsonArray->size() > 0)
                    {
                        // Get the first release (most recent)
                        auto firstRelease = (*jsonArray)[0];
                        if (firstRelease.isObject())
                        {
                            auto* releaseObj = firstRelease.getDynamicObject();
                            if (releaseObj != nullptr && releaseObj->hasProperty("tag_name"))
                            {
                                juce::String latestVersion = releaseObj->getProperty("tag_name").toString();
                                
                                // Remove 'v' prefix if present (v0.9.1 -> 0.9.1)
                                if (latestVersion.startsWith("v") || latestVersion.startsWith("V"))
                                    latestVersion = latestVersion.substring(1);
                                
                                if (latestVersion.isNotEmpty())
                                {
                                    int comparison = compareVersions(latestVersion, CURRENT_VERSION);
                                    if (comparison > 0)  // latestVersion is newer
                                    {
                                        updateAvailable = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
        // Network error - silently fail
    }
    
    // Update UI on message thread with WeakReference
    juce::WeakReference<juce::Component> weakThis(this);
    juce::MessageManager::callAsync([weakThis, updateAvailable]()
    {
        if (weakThis != nullptr)
        {
            if (auto* content = dynamic_cast<AboutContent*>(weakThis.get()))
                content->updateCheckComplete(updateAvailable);
        }
    });
}

void AboutContent::updateCheckComplete(bool updateAvailable)
{
    isCheckingForUpdates = false;
    updateCheckButton.setEnabled(true);
    
    if (updateAvailable)
    {
        updateCheckButton.setButtonText("Update Available");
        updateCheckButton.setColour(juce::TextButton::buttonColourId, ColourPalette::successGreen);
    }
    else
    {
        updateCheckButton.setButtonText("No Update");
        updateCheckButton.setColour(juce::TextButton::buttonColourId, ColourPalette::panelBackground);
    }
}
