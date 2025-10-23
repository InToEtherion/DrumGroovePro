#include "AboutDialog.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"

#define CURRENT_VERSION "0.9.0"
#define GITHUB_RELEASES_API "https://api.github.com/repos/InToEtherion/DrumGroovePro/releases/latest"

// Semantic version comparison
static int compareVersions(const juce::String& v1, const juce::String& v2)
{
    auto parts1 = juce::StringArray::fromTokens(v1, ".", "");
    auto parts2 = juce::StringArray::fromTokens(v2, ".", "");
    
    // Ensure at least 3 parts (major.minor.patch)
    while (parts1.size() < 3) parts1.add("0");
    while (parts2.size() < 3) parts2.add("0");
    
    for (int i = 0; i < 3; ++i)
    {
        int num1 = parts1[i].getIntValue();
        int num2 = parts2[i].getIntValue();
        
        if (num1 > num2) return 1;   // v1 is newer
        if (num1 < num2) return -1;  // v2 is newer
    }
    
    return 0; // versions are equal
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
    juce::URL apiUrl(GITHUB_RELEASES_API);
    
    // Use WebInputStream to fetch the data
    std::unique_ptr<juce::InputStream> stream(apiUrl.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(5000)
        .withNumRedirectsToFollow(5)
        .withHttpRequestCmd("GET")
    ));
    
    bool updateAvailable = false;
    
    if (stream != nullptr)
    {
        juce::String response = stream->readEntireStreamAsString();
        
        // Parse JSON response
        auto jsonResult = juce::JSON::parse(response);
        
        if (jsonResult.isObject())
        {
            auto jsonObj = jsonResult.getDynamicObject();
            if (jsonObj != nullptr)
            {
                juce::String latestVersion = jsonObj->getProperty("tag_name").toString();
                
                // Remove 'v' prefix if present
                if (latestVersion.startsWith("v"))
                    latestVersion = latestVersion.substring(1);
                
                // Compare versions (only update if latest is NEWER)
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
    
    // Update UI on message thread
    juce::MessageManager::callAsync([this, updateAvailable]()
    {
        updateCheckComplete(updateAvailable);
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