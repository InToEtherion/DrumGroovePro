#include "MultiTrackContainer.h"
#include "Track.h"
#include "TrackHeader.h"
#include "TimelineControls.h"
#include "../../Utils/TimelineUtils.h"
#include "../LookAndFeel/ColourPalette.h"
#include "../LookAndFeel/DrumGrooveLookAndFeel.h"

//==============================================================================
// TimelineContent implementation
//==============================================================================
TimelineContent::TimelineContent(DrumGrooveProcessor& p)
    : processor(p)
{
}

void TimelineContent::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::secondaryBackground);
    
    // Draw selection region if active
    if (container && container->hasSelection())
    {
        double selectionStart = container->getSelectionStart();
        double selectionEnd = container->getSelectionEnd();
        
        float startX = static_cast<float>(selectionStart * zoomLevel);
        float endX = static_cast<float>(selectionEnd * zoomLevel);
        
        if (endX > startX)
        {
            g.setColour(juce::Colours::yellow.withAlpha(0.2f));
            g.fillRect(startX, 0.0f, endX - startX, static_cast<float>(getHeight()));
            
            // Draw selection borders
            g.setColour(juce::Colours::yellow.withAlpha(0.6f));
            g.drawVerticalLine(juce::roundToInt(startX), 0.0f, static_cast<float>(getHeight()));
            g.drawVerticalLine(juce::roundToInt(endX), 0.0f, static_cast<float>(getHeight()));
        }
    }
}

void TimelineContent::resized()
{
    if (!tracks)
        return;
    
    int yPos = 0;  // Tracks start at top since ruler is now separate
    int contentWidth = getWidth();
    
    for (auto& track : *tracks)
    {
        if (track)
        {
            // Tracks now use full width (no header offset needed)
            track->setBounds(0, yPos, contentWidth, MultiTrackContainer::TRACK_HEIGHT);
            yPos += MultiTrackContainer::TRACK_HEIGHT;
        }
    }
}

void TimelineContent::updateSize(double maxTime, float zoomLevel)
{
    // Prevent infinite update loops
    if (isUpdating)
        return;
    
    juce::ScopedValueSetter<bool> guard(isUpdating, true);
    
    this->zoomLevel = zoomLevel;
    
    double timelineSeconds = juce::jmax(MultiTrackContainer::MIN_TIMELINE_WIDTH_SECONDS,
                                       maxTime + MultiTrackContainer::BUFFER_TIME);
    int timelineWidth = static_cast<int>(timelineSeconds * zoomLevel);
    
    // Calculate height based on actual number of tracks
    int numTracks = tracks ? static_cast<int>(tracks->size()) : 3;
    int timelineHeight = (MultiTrackContainer::TRACK_HEIGHT * numTracks);
    
    // Only update if size actually changed
    if (getWidth() != timelineWidth || getHeight() != timelineHeight)
    {
        // Set the size - this will automatically call resized()
        setSize(timelineWidth, timelineHeight);
    }
}

void TimelineContent::setTracks(std::vector<std::unique_ptr<Track>>* trackList)
{
    tracks = trackList;
    resized();
}

//==============================================================================
// FixedHeaderColumn implementation
//==============================================================================
FixedHeaderColumn::FixedHeaderColumn(DrumGrooveProcessor& p)
    : processor(p)
{
}

void FixedHeaderColumn::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);
    
    // Draw right border
    g.setColour(ColourPalette::separator);
    g.drawLine(static_cast<float>(getWidth() - 1), 0.0f,
               static_cast<float>(getWidth() - 1), static_cast<float>(getHeight()), 2.0f);
}

void FixedHeaderColumn::resized()
{
    if (!headers)
        return;
    
    // Headers start at 0 since the viewport already accounts for the ruler offset
    int yPos = 0;
    
    for (auto& header : *headers)
    {
        if (header)
        {
            header->setBounds(0, yPos, getWidth(), MultiTrackContainer::TRACK_HEIGHT);
            yPos += MultiTrackContainer::TRACK_HEIGHT;
        }
    }
}

void FixedHeaderColumn::setHeaders(std::vector<std::unique_ptr<TrackHeader>>* headerList)
{
    headers = headerList;
    
    // Add headers as children
    if (headers)
    {
        for (auto& header : *headers)
        {
            if (header)
                addAndMakeVisible(header.get());
        }
    }
    
    // Update size based on number of headers
    updateSize();
    resized();
}

void FixedHeaderColumn::updateSize()
{
    if (!headers)
        return;
    
    int numHeaders = static_cast<int>(headers->size());
    // Ensure height matches exactly with TimelineContent
    int totalHeight = (numHeaders * MultiTrackContainer::TRACK_HEIGHT);
    
    // Don't add extra space - headers should align perfectly with tracks
    
    setSize(MultiTrackContainer::TRACK_HEADER_WIDTH, totalHeight);
    
    // Force immediate layout update
    resized();
}

//==============================================================================
// FixedRulerRow implementation
//==============================================================================
FixedRulerRow::FixedRulerRow(DrumGrooveProcessor& p)
    : processor(p)
{
}

void FixedRulerRow::paint(juce::Graphics& g)
{
    // Draw ruler background
    g.setColour(juce::Colour(0xff454545));
    g.fillRect(getLocalBounds());
    
    // Draw selected region if active
    if (isDraggingRegion || (regionEndTime > regionStartTime))
    {
        float startX = static_cast<float>(regionStartTime * zoomLevel - viewportX);
        float endX = static_cast<float>(regionEndTime * zoomLevel - viewportX);
        
        if (endX > startX)
        {
            g.setColour(juce::Colours::yellow.withAlpha(0.3f));
            g.fillRect(startX, 0.0f, endX - startX, static_cast<float>(getHeight()));
            
            // Draw region borders
            g.setColour(juce::Colours::yellow);
            g.drawVerticalLine(juce::roundToInt(startX), 0.0f, static_cast<float>(getHeight()));
            g.drawVerticalLine(juce::roundToInt(endX), 0.0f, static_cast<float>(getHeight()));
        }
    }
    
    // Draw bottom border
    g.setColour(juce::Colour(0xff3c3c3c));
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1),
               static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1));
    
    drawRuler(g);
}

void FixedRulerRow::drawRuler(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff969696));
    auto& lnf = DrumGrooveLookAndFeel::getInstance();
    g.setFont(lnf.getSmallFont());
    
    // Calculate grid intervals based on zoom level
    double mainGridStep;
    double subGridStep;
    bool showSubGrid = false;
    
    if (zoomLevel <= 50.0f)
    {
        // Very zoomed out (10%-50%): Show every 5 seconds to avoid clutter
        mainGridStep = 5.0;     // Show labels every 5 seconds (0, 5, 10, 15...)
        subGridStep = 0.0;      // No sub-grid
        showSubGrid = false;
    }
    else if (zoomLevel < 200.0f)
    {
        // Medium zoom (50%-200%): Show whole seconds with optional tick marks at 0.5s
        mainGridStep = 1.0;     // Show labels every 1 second
        subGridStep = 0.5;      // Small tick marks at 0.5s
        showSubGrid = true;
    }
    else
    {
        // Zoomed in (200%+): Show 500ms intervals for precision
        mainGridStep = 0.5;     // Show labels every 0.5 seconds
        subGridStep = 0.0;      // No sub-grid needed
        showSubGrid = false;
    }
    
    double maxTime = static_cast<double>(contentWidth) / zoomLevel;
    
    // Draw sub-grid tick marks (only below 200% zoom)
    if (showSubGrid)
    {
        g.setColour(juce::Colour(0xff707070));  // Dimmer color for sub-grid
        for (double time = subGridStep; time <= maxTime; time += mainGridStep)
        {
            float x = static_cast<float>(time * zoomLevel) - viewportX;
            
            if (x >= 0 && x <= getWidth())
            {
                // Draw small tick mark (no label)
                g.drawLine(x, static_cast<float>(getHeight() - 5),
                          x, static_cast<float>(getHeight()));
            }
        }
    }
    
    // Draw main grid with labels
    g.setColour(juce::Colour(0xff969696));
    
    juce::String lastTimeText = "";  // Track last label to avoid duplicates
    
    for (double time = 0.0; time <= maxTime; time += mainGridStep)
    {
        float x = static_cast<float>(time * zoomLevel) - viewportX;
        
        if (x >= -30 && x <= getWidth() + 30)  // Draw slightly outside visible area
        {
            // Draw main tick mark
            g.drawLine(x, static_cast<float>(getHeight() - 10),
                      x, static_cast<float>(getHeight()));
            
            // Format time text based on grid step
            juce::String timeText;
            
            if (mainGridStep >= 1.0)
            {
                // Whole seconds: show as MM:SS
                int minutes = static_cast<int>(time) / 60;
                int seconds = static_cast<int>(time) % 60;
                timeText = juce::String::formatted("%d:%02d", minutes, seconds);
            }
            else
            {
                // Sub-second intervals: show as MM:SS.5
                int wholeSec = static_cast<int>(time);
                int minutes = wholeSec / 60;
                int seconds = wholeSec % 60;
                double fracSec = time - wholeSec;
                
                if (fracSec > 0.01)  // If it's a .5 mark
                    timeText = juce::String::formatted("%d:%02d.5", minutes, seconds);
                else
                    timeText = juce::String::formatted("%d:%02d", minutes, seconds);
            }
            
            // Only draw label if it's different from the last one and visible
            if (timeText != lastTimeText && x >= -10 && x <= getWidth())
            {
                g.drawText(timeText, 
                          static_cast<int>(x - 30), 
                          0, 
                          60, 
                          getHeight() - 10, 
                          juce::Justification::centred);
                
                lastTimeText = timeText;
            }
        }
    }
}

void FixedRulerRow::setZoomLevel(float newZoomLevel)
{
    if (zoomLevel != newZoomLevel)
    {
        zoomLevel = newZoomLevel;
        repaint();
    }
}

void FixedRulerRow::setViewportX(int x)
{
    if (viewportX != x)
    {
        viewportX = x;
        repaint();
    }
}

void FixedRulerRow::setContentWidth(int width)
{
    if (contentWidth != width)
    {
        contentWidth = width;
        repaint();
    }
}

void FixedRulerRow::mouseDown(const juce::MouseEvent& e)
{
    if (!container) return;
    
    if (e.mods.isLeftButtonDown())
    {
        // Left-click starts region selection
        isDraggingRegion = true;
        double clickX = e.position.x + viewportX;
        regionStartTime = clickX / zoomLevel;
        regionEndTime = regionStartTime;
        
        // Immediately set selection in container
        container->setSelectionStart(regionStartTime);
        container->setSelectionEnd(regionStartTime);
        
        DBG("Ruler mouseDown - Starting selection at: " + juce::String(regionStartTime, 3) + "s");
        
        repaint();
    }
}

void FixedRulerRow::mouseDrag(const juce::MouseEvent& e)
{
    if (!container) return;
    
    if (isDraggingRegion && e.mods.isLeftButtonDown())
    {
        // Update region end time
        double dragX = e.position.x + viewportX;
        regionEndTime = dragX / zoomLevel;
        
        // Ensure start < end for display
        double displayStart = juce::jmin(regionStartTime, regionEndTime);
        double displayEnd = juce::jmax(regionStartTime, regionEndTime);
        
        // Update container selection
        container->setSelectionStart(displayStart);
        container->setSelectionEnd(displayEnd);
        
        DBG("Ruler mouseDrag - Selection: " + juce::String(displayStart, 3) + "s to " + juce::String(displayEnd, 3) + "s");
        
        repaint();
    }
}

void FixedRulerRow::mouseUp([[maybe_unused]] const juce::MouseEvent& e)
{
    if (!container) return;
    
    if (isDraggingRegion)
    {
        isDraggingRegion = false;
        
        // Ensure start < end
        double finalStart = juce::jmin(regionStartTime, regionEndTime);
        double finalEnd = juce::jmax(regionStartTime, regionEndTime);
        
        // Set final selection in container
        container->setSelectionStart(finalStart);
        container->setSelectionEnd(finalEnd);
        
        // Notify timeline controls about the selection change
        container->sendChangeMessage();
        
        DBG("Ruler mouseUp - Final selection: " + juce::String(finalStart, 3) + "s to " + juce::String(finalEnd, 3) + "s");
        
        repaint();
    }
}

void FixedRulerRow::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!container) return;
    
    if (e.mods.isLeftButtonDown())
    {
        // Double left-click moves playhead to position
        double clickX = e.position.x + viewportX;
        double timePosition = clickX / zoomLevel;
        
        DBG("Ruler doubleClick - Moving playhead to: " + juce::String(timePosition, 3) + "s");
        
        container->setPlayheadPosition(timePosition);
        
        repaint();
    }
}

//==============================================================================
// MultiTrackContainer implementation
//==============================================================================
MultiTrackContainer::MultiTrackContainer(DrumGrooveProcessor& p)
    : processor(p)
{
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    
    loopEnabled = false;
    selectionValid = false;
    isSettingSelection = false;
    selectionStart = 0.0;
    selectionEnd = 0.0;
    selectionDragStart = 0.0;
    
    playing = false;
    playheadPosition = 0.0;
    lastPlaybackTime = 0.0;
    autoScrollEnabled = true;
    
    zoomLevel = 100.0f;
    gridInterval = 0.5;
    
    selectedTrackIndex = -1;
    clipboardIsCut = false;
    
    globalGhostClip = nullptr;
    originalGhostDuration = 0.0;
    currentTargetTrack = -1;
    
	// Setup fixed ruler row
	fixedRulerRow = std::make_unique<FixedRulerRow>(processor);
	addAndMakeVisible(fixedRulerRow.get());
	
	fixedRulerRow->setContainer(this);

	// Setup header viewport (left column, non-scrolling horizontally)
	headerViewport.setScrollBarsShown(false, false);  // Headers don't need scrollbars (synced with main viewport)
	fixedHeaderColumn = std::make_unique<FixedHeaderColumn>(processor);
	headerViewport.setViewedComponent(fixedHeaderColumn.get(), false);
	addAndMakeVisible(headerViewport);

	// Setup main timeline viewport
	timelineContent = std::make_unique<TimelineContent>(processor);
	timelineContent->setTracks(&tracks);
	timelineContent->setContainer(this);

	viewport.setViewedComponent(timelineContent.get(), false);

	// DISABLE viewport's automatic scrollbars completely
	viewport.setScrollBarsShown(false, false);
	viewport.setScrollBarThickness(14);

	addAndMakeVisible(viewport);

	// Setup MANUAL vertical scrollbar (we control it completely)
	manualVerticalScrollbar.setAutoHide(false);
	manualVerticalScrollbar.setColour(juce::ScrollBar::backgroundColourId, juce::Colour(0xff2a2a2a));
	manualVerticalScrollbar.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff4a4a4a));
	manualVerticalScrollbar.addListener(this);
	manualVerticalScrollbar.setVisible(false);  // Hidden initially
	addAndMakeVisible(manualVerticalScrollbar);

	// Setup overlay horizontal scrollbar
	overlayHorizontalScrollbar.setAutoHide(false);
	overlayHorizontalScrollbar.setColour(juce::ScrollBar::backgroundColourId, juce::Colour(0xff2a2a2a).withAlpha(0.8f));
	overlayHorizontalScrollbar.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff4a4a4a));
	overlayHorizontalScrollbar.addListener(this);
	addAndMakeVisible(overlayHorizontalScrollbar);
    
    // Create 3 initial tracks WITH their headers
    for (int i = 0; i < 3; ++i)
    {
        addTrack();
    }
    
    // Set headers in fixed column
    fixedHeaderColumn->setHeaders(&trackHeaders);
    
    // **FIX: Set tracks pointer in TimelineContent so it knows where the tracks are**
    timelineContent->setTracks(&tracks);
    
    updateGridInterval();
    updateTimelineSize();
    
    startTimer(16);
    
    // Restore saved GUI state after everything is initialized
    processor.restoreCompleteGuiState();
}

MultiTrackContainer::~MultiTrackContainer()
{
    // Save complete GUI state before closing
    processor.saveCompleteGuiState();
    
    // Remove scroll listeners
    manualVerticalScrollbar.removeListener(this);
    overlayHorizontalScrollbar.removeListener(this);
    
    stopTimer();
    removeKeyListener(this);
}

void MultiTrackContainer::paint(juce::Graphics& g)
{
    g.fillAll(ColourPalette::mainBackground);
    
    // Draw top-left corner (above header viewport)
    g.setColour(juce::Colour(0xff454545));
    g.fillRect(0, 0, TRACK_HEADER_WIDTH, RULER_HEIGHT);
    
    drawGrid(g);
}

void MultiTrackContainer::paintOverChildren(juce::Graphics& g)
{
    drawSelectionRegion(g);
    drawPlayhead(g);
    drawGlobalGhostClip(g);
}

void MultiTrackContainer::resized()
{
    // Prevent infinite resize loops
    if (isUpdatingLayout)
        return;
    
    juce::ScopedValueSetter<bool> guard(isUpdatingLayout, true);
    
    auto bounds = getLocalBounds();
    
    // Reserve top area for ruler
    auto rulerArea = bounds.removeFromTop(RULER_HEIGHT);
    rulerArea.removeFromLeft(TRACK_HEADER_WIDTH);
    
    // Left column for header viewport
    auto leftColumn = bounds.removeFromLeft(TRACK_HEADER_WIDTH);
    headerViewport.setBounds(leftColumn);
    
    // Main viewport bounds
    viewport.setBounds(bounds);
    
    // Fixed ruler row
    fixedRulerRow->setBounds(rulerArea);
    
    // Position overlay horizontal scrollbar at bottom
    auto scrollbarBounds = bounds;
    scrollbarBounds.setY(scrollbarBounds.getBottom() - 14);
    scrollbarBounds.setHeight(14);
    
    // If manual vertical scrollbar is visible, make room for it
    if (needsManualVerticalScrollbar)
    {
        scrollbarBounds = scrollbarBounds.withTrimmedRight(14);
    }
    
    overlayHorizontalScrollbar.setBounds(scrollbarBounds);
    
    // Position manual vertical scrollbar on right edge
    if (needsManualVerticalScrollbar)
    {
        auto vScrollBounds = bounds;
        vScrollBounds.setX(vScrollBounds.getRight() - 14);
        vScrollBounds.setWidth(14);
        vScrollBounds.setHeight(vScrollBounds.getHeight() - 14);  // Leave room for horizontal scrollbar
        manualVerticalScrollbar.setBounds(vScrollBounds);
    }
    
    // Update scrollbar visibility
    updateScrollbarVisibility();
}

void MultiTrackContainer::updateScrollbarVisibility()
{
    // ✅ REMOVED: isUpdatingLayout guard - this method needs to run even during layout updates
    // The scrollbar visibility logic doesn't trigger layout changes, so it's safe
    
    if (!timelineContent)
        return;
    
    const int contentHeight = timelineContent->getHeight();
    const int contentWidth = timelineContent->getWidth();
    const int viewportHeight = viewport.getHeight();
    const int viewportWidth = viewport.getWidth();
    const int numTracks = static_cast<int>(tracks.size());
    
    // Determine if we need vertical scrollbar (more than 3 tracks)
    bool needsVertical = (numTracks > 3);
    
    // Only update if changed
    if (needsVertical != needsManualVerticalScrollbar)
    {
        needsManualVerticalScrollbar = needsVertical;
        
        if (needsManualVerticalScrollbar)
        {
            manualVerticalScrollbar.setRangeLimits(0.0, contentHeight);
            manualVerticalScrollbar.setCurrentRange(viewport.getViewPositionY(), viewportHeight - 14, juce::dontSendNotification);
            manualVerticalScrollbar.setVisible(true);
            manualVerticalScrollbar.toFront(false);
        }
        else
        {
            manualVerticalScrollbar.setVisible(false);
        }
    }
    else if (needsManualVerticalScrollbar)
    {
        manualVerticalScrollbar.setRangeLimits(0.0, contentHeight);
        manualVerticalScrollbar.setCurrentRange(viewport.getViewPositionY(), viewportHeight - 14, juce::dontSendNotification);
    }
    
    overlayHorizontalScrollbar.setRangeLimits(0.0, contentWidth);
    overlayHorizontalScrollbar.setCurrentRange(viewport.getViewPositionX(), viewportWidth, juce::dontSendNotification);
    
    headerViewport.setViewPosition(0, viewport.getViewPositionY());
}


void MultiTrackContainer::updateTimelineSize()
{
    if (!timelineContent || isUpdatingLayout)
        return;
    
    juce::ScopedValueSetter<bool> guard(isUpdatingLayout, true);
        
    double maxTime = getMaxTime();
    timelineContent->updateSize(maxTime, zoomLevel);
    
    // Get actual content dimensions
    int contentWidth = timelineContent->getWidth();
    int contentHeight = timelineContent->getHeight();
    
    // CRITICAL: Explicitly set bounds to ensure viewport sees the size
    timelineContent->setBounds(0, 0, contentWidth, contentHeight);
    
    // Update ruler with new content width
    if (fixedRulerRow)
    {
        fixedRulerRow->setContentWidth(contentWidth);
        fixedRulerRow->setViewportX(viewport.getViewPositionX());
    }
    
    // Update scrollbar visibility
    updateScrollbarVisibility();
}

double MultiTrackContainer::getTimelineWidthInSeconds() const
{
    if (!timelineContent)
        return MIN_TIMELINE_WIDTH_SECONDS;
        
    int contentWidth = timelineContent->getWidth();
    return static_cast<double>(contentWidth) / zoomLevel;
}

void MultiTrackContainer::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (e.mods.isRightButtonDown())
    {
        showRightClickMenu(e.getPosition());
        return;
    }

    // Check if click is in ruler area (for selection)
    if (e.y < RULER_HEIGHT && e.x >= TRACK_HEADER_WIDTH)
    {
        isSettingSelection = true;
        // Convert mouse X to time, accounting for fixed header and viewport scroll
        double mouseX = static_cast<double>(e.x - TRACK_HEADER_WIDTH + viewport.getViewPositionX());
        selectionDragStart = mouseX / zoomLevel;
        selectionStart = selectionDragStart;
        selectionEnd = selectionDragStart;
        selectionValid = false;
        sendChangeMessage();
        repaint();
    }
}

void MultiTrackContainer::mouseDrag(const juce::MouseEvent& e)
{
    if (isSettingSelection)
    {
        double mouseX = static_cast<double>(e.x - TRACK_HEADER_WIDTH + viewport.getViewPositionX());
        double currentTime = mouseX / zoomLevel;
        
        if (currentTime < selectionDragStart)
        {
            selectionStart = currentTime;
            selectionEnd = selectionDragStart;
        }
        else
        {
            selectionStart = selectionDragStart;
            selectionEnd = currentTime;
        }
        
        selectionValid = true;
        sendChangeMessage();
        repaint();
    }
    else if (e.mods.isCtrlDown() && e.mods.isAltDown()) // Ctrl+Alt drag for external DAW export
    {
        // Begin external drag of selected clips
        beginDragOfSelectedClips(e);
    }
}

void MultiTrackContainer::mouseUp(const juce::MouseEvent&)
{
    if (isSettingSelection)
    {
        isSettingSelection = false;
        
        if (selectionEnd - selectionStart < 0.001)
        {
            clearSelection();
        }
        
        repaint();
    }
}

void MultiTrackContainer::mouseMove(const juce::MouseEvent&)
{
}

void MultiTrackContainer::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.y < RULER_HEIGHT && e.x >= TRACK_HEADER_WIDTH)
    {
        double mouseX = static_cast<double>(e.x - TRACK_HEADER_WIDTH + viewport.getViewPositionX());
        double newTime = mouseX / zoomLevel;
        setPlayheadPosition(newTime);
        repaint();
    }
}

void MultiTrackContainer::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        float zoomFactor = 1.0f + (wheel.deltaY * 2.0f);
        setZoom(zoomLevel * zoomFactor);
    }
    else
    {
        int currentX = viewport.getViewPositionX();
        int deltaX = static_cast<int>(-wheel.deltaX * 50);
        viewport.setViewPosition(juce::jmax(0, currentX + deltaX), viewport.getViewPositionY());
    }
}

bool MultiTrackContainer::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // CRITICAL: Always consume key events to prevent them from reaching the host DAW
    bool handled = false;
    
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (playing)
            pause();
        else
            play();
        handled = true;
    }
    else if (key.getKeyCode() == juce::KeyPress::deleteKey ||
             key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        deleteSelectedClips();
        handled = true;
    }
    else if (key.isKeyCode('A') && key.getModifiers().isCtrlDown())
    {
        selectAllClips();
        handled = true;
    }
    else if (key.isKeyCode('C') && key.getModifiers().isCtrlDown())
    {
        copySelectedClips();
        handled = true;
    }
    else if (key.isKeyCode('X') && key.getModifiers().isCtrlDown())
    {
        cutSelectedClips();
        handled = true;
    }
    else if (key.isKeyCode('V') && key.getModifiers().isCtrlDown())
    {
        pasteClips();
        handled = true;
    }
    else if (key.getModifiers().isShiftDown() || 
             key.getModifiers().isCtrlDown() || 
             key.getModifiers().isAltDown())
    {
        // Consume all modifier key combinations to prevent DAW interference
        handled = true;
    }

    // Always return true to consume the key event and prevent DAW interference
    return true;
}

bool MultiTrackContainer::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isString();
}

void MultiTrackContainer::itemDragEnter(const SourceDetails& details)
{
    if (!details.description.isString())
        return;

    juce::String description = details.description.toString();
    juce::StringArray parts = juce::StringArray::fromTokens(description, "|", "");

    if (parts.size() >= 2)
    {
        globalGhostClip = std::make_unique<MidiClip>();
        globalGhostClip->name = parts[0];
        globalGhostClip->colour = juce::Colours::yellow.withAlpha(0.6f);
        globalGhostClip->duration = 4.0;
        originalGhostDuration = 4.0;
        currentTargetTrack = -1;
    }
}

void MultiTrackContainer::itemDragMove(const SourceDetails& details)
{
    if (!globalGhostClip)
        return;

    // Find which track we're over
    int targetTrack = -1;
    int yPosInFixed = details.localPosition.getY() - RULER_HEIGHT;
    
    if (yPosInFixed >= 0)
    {
        targetTrack = yPosInFixed / TRACK_HEIGHT;
        if (targetTrack >= static_cast<int>(tracks.size()))
            targetTrack = -1;
    }

    if (targetTrack != currentTargetTrack)
    {
        currentTargetTrack = targetTrack;
        
        if (currentTargetTrack >= 0 && currentTargetTrack < static_cast<int>(tracks.size()))
        {
            double targetBPM = tracks[currentTargetTrack]->getTrackBPM();
            globalGhostClip->duration = originalGhostDuration * (120.0 / targetBPM);
        }
    }

    // Calculate position in timeline
    double mouseX = static_cast<double>(details.localPosition.getX() - TRACK_HEADER_WIDTH + viewport.getViewPositionX());
    double mouseTime = mouseX / zoomLevel;
    double halfDuration = globalGhostClip->duration * 0.5;
    globalGhostClip->startTime = snapToGrid(mouseTime - halfDuration);

    repaint();
}

void MultiTrackContainer::itemDragExit(const SourceDetails&)
{
    globalGhostClip.reset();
    currentTargetTrack = -1;
    repaint();
}

void MultiTrackContainer::itemDropped(const SourceDetails& details)
{
    // Forward to appropriate track
    int targetTrack = -1;
    int yPosInFixed = details.localPosition.getY() - RULER_HEIGHT;
    
    if (yPosInFixed >= 0)
    {
        targetTrack = yPosInFixed / TRACK_HEIGHT;
        if (targetTrack >= 0 && targetTrack < static_cast<int>(tracks.size()))
        {
            // Create modified details with adjusted coordinates
            auto modifiedDetails = details;
            tracks[targetTrack]->itemDropped(modifiedDetails);
        }
    }
    
    globalGhostClip.reset();
    currentTargetTrack = -1;
    updateTimelineSize();
    repaint();
}

void MultiTrackContainer::play()
{
    playing = true;
    lastPlaybackTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    // Clear all clips first to remove any deleted clips
    processor.midiProcessor.clearAllClips();
    
    // Check if ANY track is soloed
    bool anySoloed = false;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        if (tracks[i]->isSoloed())
        {
            anySoloed = true;
            break;
        }
    }
    
    // Add clips based on mute/solo state
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto& track = tracks[i];
        
        // Proper solo/mute logic
        bool shouldPlay = false;
        
        if (anySoloed)
        {
            // If ANY track is soloed, ONLY play soloed tracks (unless they're also muted)
            shouldPlay = track->isSoloed() && !track->isMuted();
        }
        else
        {
            // If NO tracks are soloed, play all tracks that aren't muted
            shouldPlay = !track->isMuted();
        }
        
        if (shouldPlay)
        {
            double trackBPM = track->getTrackBPM();
            int trackNumber = static_cast<int>(i) + 1;
            
            for (const auto& clip : track->getClips())
            {
                processor.midiProcessor.addMidiClip(clip->file, clip->startTime, 
                                                   DrumLibrary::Unknown, 
                                                   clip->referenceBPM, 
                                                   trackBPM, 
                                                   trackNumber);
            }
        }
    }
    
       // Set up loop if enabled and selection exists
       if (loopEnabled && selectionValid)
       {
           processor.midiProcessor.setLoopEnabled(true);
           processor.midiProcessor.setLoopRange(selectionStart, selectionEnd);
           
           // Start playback from selection start if playhead is outside selection
           if (playheadPosition < selectionStart || playheadPosition > selectionEnd)
           {
               playheadPosition = selectionStart;
               processor.midiProcessor.setPlayheadPosition(selectionStart);
           }
       }
       else
       {
           processor.midiProcessor.setLoopEnabled(false);
       }
    processor.midiProcessor.play();
    repaint();
}

void MultiTrackContainer::pause()
{
    playing = false;
    processor.midiProcessor.pause();
    repaint();
}

void MultiTrackContainer::stop()
{
    playing = false;
    playheadPosition = 0.0;
    processor.midiProcessor.stop();
    repaint();
}

void MultiTrackContainer::setPlayheadPosition(double timeInSeconds)
{
    playheadPosition = juce::jmax(0.0, timeInSeconds);
    processor.midiProcessor.setPlayheadPosition(playheadPosition);
    repaint();
}

void MultiTrackContainer::setLoopStart(double timeInSeconds)
{
    // Update loop start time in timeline controls
    if (timelineControls)
    {
        timelineControls->setLoopStartTime(timeInSeconds);
    }
}

void MultiTrackContainer::setLoopEnd(double timeInSeconds)
{
    // Update loop end time in timeline controls
    if (timelineControls)
    {
        timelineControls->setLoopEndTime(timeInSeconds);
    }
}

void MultiTrackContainer::setZoom(float pixelsPerSecond)
{
    int viewportX = viewport.getViewPositionX();
    int viewportWidth = viewport.getWidth();
    int centerX = viewportX + (viewportWidth / 2);
    
    double centerTime = static_cast<double>(centerX) / zoomLevel;

    zoomLevel = juce::jlimit(10.0f, 500.0f, pixelsPerSecond);
    
    updateGridInterval();
    
    if (timelineContent)
        timelineContent->setZoomLevel(zoomLevel);
    
    if (fixedRulerRow)
        fixedRulerRow->setZoomLevel(zoomLevel);
    
    updateTimelineSize();
    
    int newCenterX = static_cast<int>(centerTime * zoomLevel);
    int newViewportX = newCenterX - (viewportWidth / 2);
    viewport.setViewPosition(juce::jmax(0, newViewportX), viewport.getViewPositionY());

    repaint();
    viewport.repaint();
}


void MultiTrackContainer::updateGridInterval()
{
    if (zoomLevel < 50.0f)
        gridInterval = 1.0;
    else if (zoomLevel < 100.0f)
        gridInterval = 0.5;
    else if (zoomLevel < 150.0f)
        gridInterval = 0.1;
    else
        gridInterval = 0.05;
}

void MultiTrackContainer::handleSoloChange(int soloedTrackIndex)
{
    // Unsolo all other tracks
    for (int i = 0; i < static_cast<int>(trackHeaders.size()); ++i)
    {
        if (i != soloedTrackIndex)
        {
            trackHeaders[i]->setSoloed(false);
        }
    }
    
    // Update MIDI processor to reflect solo/mute states
    updateTrackPlaybackStates();
    
    // Repaint all track headers to show visual feedback
    for (auto& header : trackHeaders)
    {
        header->repaint();
    }
}

void MultiTrackContainer::updateTrackPlaybackStates()
{
    // Determine which tracks should play based on solo/mute states
    bool anySolo = false;
    for (const auto& header : trackHeaders)
    {
        if (header->isSoloed())
        {
            anySolo = true;
            break;
        }
    }
    
    // Update each track's playback state
    for (int i = 0; i < static_cast<int>(trackHeaders.size()); ++i)
    {
        bool shouldPlay = true;
        
        if (anySolo)
        {
            // If any track is soloed, only soloed tracks play
            shouldPlay = trackHeaders[i]->isSoloed();
        }
        else
        {
            // If no tracks are soloed, muted tracks don't play
            shouldPlay = !trackHeaders[i]->isMuted();
        }
        
        // Update track processor (implementation depends on your architecture)
        // This would typically update the MIDI processor to enable/disable track output
    }
}

double MultiTrackContainer::getTrackBPM(int trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < static_cast<int>(trackHeaders.size()))
    {
        return trackHeaders[trackIndex]->getTrackBPM();
    }
    return 120.0; // Default BPM
}

void MultiTrackContainer::setScrollPosition(int horizontalPos, int verticalPos)
{
    // Set both viewport positions
    viewport.setViewPosition(horizontalPos, verticalPos);
    headerViewport.setViewPosition(horizontalPos, headerViewport.getViewPositionY());
    
    // Update ruler position if it exists
    if (fixedRulerRow)
    {
        fixedRulerRow->setViewportX(horizontalPos);
    }
}

juce::Point<int> MultiTrackContainer::getScrollPosition() const
{
    return juce::Point<int>(viewport.getViewPositionX(), viewport.getViewPositionY());
}

void MultiTrackContainer::fitToContent()
{
    double maxTime = getMaxTime();
    if (maxTime > 0)
    {
        zoomLevel = static_cast<float>(getWidth() - TRACK_HEADER_WIDTH) / static_cast<float>(maxTime);
        updateTimelineSize();
        viewport.setViewPosition(0, 0);
        repaint();
    }
}

void MultiTrackContainer::setSelectionStart(double time)
{
    selectionStart = juce::jmax(0.0, time);
    selectionValid = true;
    sendChangeMessage();
    repaint();
}

void MultiTrackContainer::setSelectionEnd(double time)
{
    selectionEnd = juce::jmax(selectionStart + 0.001, time);
    selectionValid = true;
    sendChangeMessage();
    repaint();
}

void MultiTrackContainer::clearSelection()
{
    selectionValid = false;
    isSettingSelection = false;
    sendChangeMessage();
    repaint();
}

void MultiTrackContainer::toggleLoop()
{
    loopEnabled = !loopEnabled;
    
    if (loopEnabled && selectionValid)
    {
        processor.midiProcessor.setLoopEnabled(true);
        processor.midiProcessor.setLoopRange(selectionStart, selectionEnd);
    }
    else
    {
        processor.midiProcessor.setLoopEnabled(false);
    }
    
    repaint();
}

juce::String MultiTrackContainer::formatTime(double seconds) const
{
    return TimelineUtils::formatTime(seconds);
}

double MultiTrackContainer::parseTime(const juce::String& timeStr) const
{
    return TimelineUtils::parseTime(timeStr);
}

//==============================================================================
// Track and clip access for TimelineManager
std::vector<const MidiClip*> MultiTrackContainer::getTrackClips(int trackIndex) const
{
    std::vector<const MidiClip*> result;
    if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size()))
    {
        auto& clips = tracks[trackIndex]->getClips();
        for (const auto& clip : clips)
        {
            if (clip)
                result.push_back(clip.get());
        }
    }
    return result;
}

std::vector<const MidiClip*> MultiTrackContainer::getSelectedClips(int trackIndex) const
{
    std::vector<const MidiClip*> result;
    if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size()))
    {
        auto& clips = tracks[trackIndex]->getClips();
        for (const auto& clip : clips)
        {
            if (clip && clip->isSelected)
                result.push_back(clip.get());
        }
    }
    return result;
}

double MultiTrackContainer::getMasterBPM() const
{
    // Return the BPM of the first track as master BPM
    if (!tracks.empty())
        return tracks[0]->getTrackBPM();
    return 120.0;
}

double MultiTrackContainer::pixelsToTime(float pixels) const
   {
    return pixels / zoomLevel;
}

float MultiTrackContainer::timeToPixels(double time) const
{
    return static_cast<float>(time * zoomLevel);
}

double MultiTrackContainer::visualPixelsToTime(float pixels) const
{
    double scaleFactor = getVisualScaleFactor();
    return pixels / (zoomLevel * scaleFactor);
}

float MultiTrackContainer::visualTimeToPixels(double time) const
{
    double scaleFactor = getVisualScaleFactor();
    return static_cast<float>(time * zoomLevel * scaleFactor);
}

double MultiTrackContainer::snapToGrid(double time) const
{
    return juce::roundToInt(time / gridInterval) * gridInterval;
}

Track* MultiTrackContainer::getTrack(int index) const
{
    if (index >= 0 && index < static_cast<int>(tracks.size()))
        return tracks[index].get();
    return nullptr;
}

void MultiTrackContainer::selectAllClips()
{
    for (auto& track : tracks)
        track->selectAll();
}

void MultiTrackContainer::deselectAllClips()
{
    for (auto& track : tracks)
        track->deselectAll();
}

void MultiTrackContainer::deleteSelectedClips()
{
    for (auto& track : tracks)
        track->removeSelectedClips();
    
    updateTimelineSize();
    repaint();
}

void MultiTrackContainer::clearAllTracks()
{
    for (auto& track : tracks)
        track->clearAllClips();
     
    updateTimelineSize();
    repaint();
}

void MultiTrackContainer::copySelectedClips()
{
    clipboardClips.clear();
    clipboardIsCut = false;
    
    // Collect all selected clips from all tracks
    for (auto& track : tracks)
    {
        auto selectedClips = track->getSelectedClips();
        for (auto* clip : selectedClips)
        {
            clipboardClips.push_back(*clip);
        }
    }
}

void MultiTrackContainer::cutSelectedClips()
{
    copySelectedClips();
    clipboardIsCut = true;
    
    // Remove selected clips from all tracks
    deleteSelectedClips();
}



void MultiTrackContainer::pasteClips()
{
    if (clipboardClips.empty())
        return;
    
    // Find the currently selected track, or use track 0 if none selected
    int targetTrackIndex = selectedTrackIndex >= 0 ? selectedTrackIndex : 0;
    
    if (targetTrackIndex >= 0 && targetTrackIndex < static_cast<int>(tracks.size()))
    {
        Track* targetTrack = tracks[targetTrackIndex].get();
        double targetBPM = targetTrack->getTrackBPM();
        
        // Paste clips to the selected track with BPM adjustment
        for (const auto& clip : clipboardClips)
        {
            MidiClip newClip = targetTrack->createClipForTrack(clip, targetBPM);
            targetTrack->addClip(newClip);
        }
        
        // If this was a cut operation, clear the clipboard
        if (clipboardIsCut)
        {
            clipboardClips.clear();
            clipboardIsCut = false;
        }
        
        targetTrack->repaint();
        updateTimelineSize();
    }
}

void MultiTrackContainer::addTrack()
{
    int trackNumber = static_cast<int>(tracks.size()) + 1;
    
    // Create track (without internal header)
    auto track = std::make_unique<Track>(processor, *this, trackNumber);
    track->onClipSelected = [this](const juce::File& file) {
        if (onClipSelected)
            onClipSelected(file);
    };
    
    // Create separate header
    auto header = std::make_unique<TrackHeader>(processor, *this, trackNumber);
    
    // Add to vectors FIRST
    tracks.push_back(std::move(track));
    trackHeaders.push_back(std::move(header));
    
    // Add track to timeline content
    if (timelineContent)
        timelineContent->addAndMakeVisible(tracks.back().get());
    
    // Update fixed header column with new headers
    if (fixedHeaderColumn)
    {
        fixedHeaderColumn->setHeaders(&trackHeaders);
        fixedHeaderColumn->updateSize();
    }
    
    // Update timeline size to accommodate new track
    updateTimelineSize();
    
    // Force complete layout update
    resized();
    
    // ✅ CRITICAL FIX: Update scrollbar visibility AFTER layout is complete
    // This must be called here, after isUpdatingLayout has been reset
    updateScrollbarVisibility();
    
    // Ensure both viewports are synchronized
    headerViewport.setViewPosition(0, viewport.getViewPositionY());
    
    repaint();
    
    DBG("addTrack() completed - Total tracks: " + juce::String(tracks.size()));
}

double MultiTrackContainer::getCurrentBPM() const
{
    if (!tracks.empty())
        return tracks[0]->getTrackBPM();
    return 120.0;
}

double MultiTrackContainer::getVisualScaleFactor() const
{
    return TimelineUtils::getVisualScaleFactor(getCurrentBPM());
}

void MultiTrackContainer::onTrackBPMChanged()
{
    repaint();
}

void MultiTrackContainer::selectTrack(int trackIndex, bool multiSelect, bool toggleMode)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int>(trackHeaders.size()))
    {
        DBG("selectTrack: Invalid track index " + juce::String(trackIndex) + 
            ", total headers: " + juce::String(trackHeaders.size()));
        return;
    }
    
    DBG("Selecting track " + juce::String(trackIndex) + 
        ", multiSelect: " + (multiSelect ? "true" : "false") +
        ", toggleMode: " + (toggleMode ? "true" : "false"));
    
    if (multiSelect)
    {
        // Multi-select mode (Shift held)
        if (selectedTrackIndices.find(trackIndex) != selectedTrackIndices.end())
        {
            // Track is already selected, deselect it
            selectedTrackIndices.erase(trackIndex);
            if (trackHeaders[trackIndex])
                trackHeaders[trackIndex]->setSelected(false);
            DBG("Track " + juce::String(trackIndex) + " deselected from multi-selection");
        }
        else
        {
            // Track is not selected, add it to selection
            selectedTrackIndices.insert(trackIndex);
            if (trackHeaders[trackIndex])
                trackHeaders[trackIndex]->setSelected(true);
            DBG("Track " + juce::String(trackIndex) + " added to multi-selection");
        }
        
        // Update single selection index to the last selected track
        if (!selectedTrackIndices.empty())
            selectedTrackIndex = *selectedTrackIndices.rbegin();
        else
            selectedTrackIndex = -1;
    }
    else if (toggleMode)
    {
        // Toggle mode (clicking same track again)
        if (selectedTrackIndices.find(trackIndex) != selectedTrackIndices.end())
        {
            // Track is selected, deselect it
            clearTrackSelection();
            DBG("Track " + juce::String(trackIndex) + " toggled off (deselected)");
        }
        else
        {
            // Track is not selected, select it (single selection)
            clearTrackSelection();
            selectedTrackIndices.insert(trackIndex);
            selectedTrackIndex = trackIndex;
            if (trackHeaders[trackIndex])
                trackHeaders[trackIndex]->setSelected(true);
            DBG("Track " + juce::String(trackIndex) + " toggled on (selected)");
        }
    }
    else
    {
        // Normal single selection mode
        clearTrackSelection();
        selectedTrackIndices.insert(trackIndex);
        selectedTrackIndex = trackIndex;
        if (trackHeaders[trackIndex])
            trackHeaders[trackIndex]->setSelected(true);
        DBG("Track " + juce::String(trackIndex) + " selected (single selection)");
    }
    
    // Force immediate repaint of all headers
    for (auto& header : trackHeaders)
    {
        if (header)
            header->repaint();
    }
    
    // Also repaint the header viewport
    headerViewport.repaint();
    
    DBG("Selection complete. Total selected tracks: " + juce::String(selectedTrackIndices.size()));
}

void MultiTrackContainer::clearTrackSelection()
{
    // Deselect all tracks
    for (auto& header : trackHeaders)
    {
        if (header)
            header->setSelected(false);
    }
    
    selectedTrackIndices.clear();
    selectedTrackIndex = -1;
    
    DBG("All tracks deselected");
}

void MultiTrackContainer::removeTrack(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size()))
        return;
    
    // For first 3 tracks, just clear them instead of removing
    if (trackIndex < 3)
    {
        if (tracks[trackIndex])
            tracks[trackIndex]->clearAllClips();
        return;
    }
    
    // Remove track and header
    tracks.erase(tracks.begin() + trackIndex);
    trackHeaders.erase(trackHeaders.begin() + trackIndex);
    
    // Update track numbers for remaining tracks
    for (size_t i = trackIndex; i < tracks.size(); ++i)
    {
        // Track numbers are 1-based, so i+1
        // We would need to update the track number, but Track class doesn't expose a setter
        // For now, the track numbers will be off after deletion
    }
    
    // Update fixed header column
    if (fixedHeaderColumn)
    {
        fixedHeaderColumn->setHeaders(&trackHeaders);
        fixedHeaderColumn->resized();
    }
    
    // Clear selection if we removed the selected track
    if (selectedTrackIndex == trackIndex)
        selectedTrackIndex = -1;
    else if (selectedTrackIndex > trackIndex)
        selectedTrackIndex--;
    
    // Update timeline size
    updateTimelineSize();
    
    // Force layout update
    resized();
    repaint();
}

void MultiTrackContainer::showRightClickMenu(const juce::Point<int>&)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Clear All Tracks", true);
    menu.addSeparator();
    
    // File operations
    juce::PopupMenu fileMenu;
    fileMenu.addItem(10, "Save Timeline State...");
    fileMenu.addItem(11, "Load Timeline State...");
    fileMenu.addSeparator();
    fileMenu.addItem(12, "Export as Single MIDI...");
    fileMenu.addItem(13, "Export as Separate MIDIs...");
    
    menu.addSubMenu("File", fileMenu);
    menu.addSeparator();
    menu.addItem(2, "Select All", true);
    menu.addItem(3, "Copy Selected", true);
    menu.addItem(4, "Paste", true);
    menu.addItem(5, "Delete Selected", true);

    auto result = menu.show();

    switch (result)
    {
        case 1:
            clearAllTracks();
            break;
        case 2:
            selectAllClips();
            break;
        case 3:
            copySelectedClips();
            break;
        case 4:
            pasteClips();
            break;
        case 5:
            deleteSelectedClips();
            break;
        case 10:
            saveTimelineState();
            break;
        case 11:
            loadTimelineState();
            break;
        case 12:
            exportTimelineAsMidi();
            break;
        case 13:
            exportTimelineAsSeparateMidis();
            break;
    }
}

void MultiTrackContainer::drawGrid(juce::Graphics& g)
{
    double visualGridStep = gridInterval;
    double subGridStep = gridInterval / 2.0;
    
    // Get visible time range in the viewport
    double startTime = static_cast<double>(viewport.getViewPositionX()) / zoomLevel;
    double endTime = static_cast<double>(viewport.getViewPositionX() + viewport.getWidth()) / zoomLevel;

    // Draw sub-grid
    if (zoomLevel > 150.0f)
    {
        g.setColour(ColourPalette::timelineGrid.withAlpha(0.3f));
        for (double time = startTime; time < endTime; time += subGridStep)
        {
            if (std::fmod(time, visualGridStep) < 0.001)
                continue;
                
            float x = timeToPixels(time) - viewport.getViewPositionX() + TRACK_HEADER_WIDTH;
            
            if (x >= static_cast<float>(TRACK_HEADER_WIDTH) && x <= static_cast<float>(getWidth()))
            {
                g.drawVerticalLine(static_cast<int>(x), static_cast<float>(RULER_HEIGHT), 
                                 static_cast<float>(getHeight()));
            }
        }
    }

    // Draw main grid
    g.setColour(ColourPalette::timelineGrid);
    for (double time = startTime; time < endTime; time += visualGridStep)
    {
        float x = timeToPixels(time) - viewport.getViewPositionX() + TRACK_HEADER_WIDTH;
        
        if (x >= static_cast<float>(TRACK_HEADER_WIDTH) && x <= static_cast<float>(getWidth()))
        {
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(RULER_HEIGHT), 
                             static_cast<float>(getHeight()));
        }
    }
}

void MultiTrackContainer::drawPlayhead(juce::Graphics& g)
{
    float x = timeToPixels(playheadPosition) - viewport.getViewPositionX() + TRACK_HEADER_WIDTH;

    if (x >= TRACK_HEADER_WIDTH && x <= getWidth())
    {
        g.setColour(ColourPalette::yellowPlayhead);
        g.drawLine(x, static_cast<float>(RULER_HEIGHT), x, static_cast<float>(getHeight()), 2.0f);

        juce::Path triangle;
        triangle.addTriangle(x - 6.0f, static_cast<float>(RULER_HEIGHT), 
                           x + 6.0f, static_cast<float>(RULER_HEIGHT), 
                           x, static_cast<float>(RULER_HEIGHT + 10));
        g.fillPath(triangle);
    }
}

void MultiTrackContainer::drawSelectionRegion(juce::Graphics& g)
{
    if (!selectionValid && !isSettingSelection)
        return;

    float startX = timeToPixels(selectionStart) - viewport.getViewPositionX() + TRACK_HEADER_WIDTH;
    float endX = timeToPixels(selectionEnd) - viewport.getViewPositionX() + TRACK_HEADER_WIDTH;

    if (endX > startX)
    {
        startX = juce::jmax(static_cast<float>(TRACK_HEADER_WIDTH), startX);
        endX = juce::jmin(static_cast<float>(getWidth()), endX);

        float alpha = loopEnabled ? 0.25f : 0.15f;
        g.setColour(ColourPalette::primaryBlue.withAlpha(alpha));
        g.fillRect(startX, static_cast<float>(RULER_HEIGHT), endX - startX, static_cast<float>(getHeight() - RULER_HEIGHT));

        g.setColour(ColourPalette::primaryBlue.withAlpha(0.5f));
        g.drawLine(startX, static_cast<float>(RULER_HEIGHT), startX, static_cast<float>(getHeight()), 2.0f);
        g.drawLine(endX, static_cast<float>(RULER_HEIGHT), endX, static_cast<float>(getHeight()), 2.0f);
    }
}

void MultiTrackContainer::drawGlobalGhostClip(juce::Graphics& g)
{
    if (!globalGhostClip || currentTargetTrack < 0)
        return;

    // Use the TARGET track's BPM for ghost clip width calculation
    // This ensures the ghost clip preview shows the correct size for the track it will be dropped on
    double targetTrackBPM = 120.0;
    if (currentTargetTrack >= 0 && currentTargetTrack < static_cast<int>(tracks.size()))
    {
        targetTrackBPM = tracks[currentTargetTrack]->getTrackBPM();
    }
    
    // Calculate scale factor based on target track's BPM
    double targetScaleFactor = TimelineUtils::getVisualScaleFactor(targetTrackBPM);

    // Calculate position and size
    float x = timeToPixels(globalGhostClip->startTime) - viewport.getViewPositionX() + TRACK_HEADER_WIDTH;
    
    // Use target track's scale factor for accurate width preview
    float width = static_cast<float>(globalGhostClip->duration * zoomLevel * targetScaleFactor);
    
    float y = static_cast<float>(RULER_HEIGHT + (currentTargetTrack * TRACK_HEIGHT) + 10);
    float height = static_cast<float>(TRACK_HEIGHT - 20);

    auto clipBounds = juce::Rectangle<float>(x, y, width, height);

    // Draw ghost clip with transparency
    g.setColour(globalGhostClip->colour);
    g.fillRoundedRectangle(clipBounds, 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(clipBounds, 4.0f, 2.0f);
}

void MultiTrackContainer::timerCallback()
{
    if (playing)
    {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double deltaTime = currentTime - lastPlaybackTime;
        lastPlaybackTime = currentTime;

        playheadPosition += deltaTime;

        // âœ… CRITICAL FIX: Use setPlayheadPosition instead of syncPlayheadPosition
        processor.midiProcessor.setPlayheadPosition(playheadPosition);

        if (loopEnabled && selectionValid && playheadPosition >= selectionEnd)
        {
            playheadPosition = selectionStart;
            lastPlaybackTime = currentTime;
            processor.midiProcessor.setPlayheadPosition(selectionStart);
        }

        double maxTime = getMaxTime();
        if (!loopEnabled && maxTime > 0 && playheadPosition >= maxTime)
        {
            stop();
            return;
        }

        repaint();
    }
}

void MultiTrackContainer::updateAutoScroll()
{
    float playheadX = timeToPixels(playheadPosition);
    int viewportX = viewport.getViewPositionX();
    int viewportWidth = viewport.getWidth();

    if (playheadX > (viewportX + viewportWidth * 0.9f))
    {
        int newX = static_cast<int>(playheadX - viewportWidth * 0.5f);
        viewport.setViewPosition(juce::jmax(0, newX), viewport.getViewPositionY());
    }
    else if (playheadX < (viewportX + viewportWidth * 0.1f) && viewportX > 0)
    {
        int newX = static_cast<int>(playheadX - viewportWidth * 0.5f);
        viewport.setViewPosition(juce::jmax(0, newX), viewport.getViewPositionY());
    }
}

double MultiTrackContainer::getMaxTime() const
{
    double maxTime = 0.0;
    for (const auto& track : tracks)
    {
        for (const auto& clip : track->getClips())
        {
            maxTime = juce::jmax(maxTime, clip->startTime + clip->duration);
        }
    }
    return maxTime;
}

bool MultiTrackContainer::isTrackMuted(int trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < static_cast<int>(trackHeaders.size()))
        return trackHeaders[trackIndex]->isMuted();
    return false;
}

bool MultiTrackContainer::isTrackSoloed(int trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < static_cast<int>(trackHeaders.size()))
        return trackHeaders[trackIndex]->isSoloed();
    return false;
}

//==========================================================================
// STATE PERSISTENCE - PRODUCTION REQUIREMENT
//==========================================================================

juce::ValueTree MultiTrackContainer::saveGuiState() const
{
    juce::ValueTree state("GuiState");
    
    // Window & layout
    state.setProperty("width", getWidth(), nullptr);
    state.setProperty("height", getHeight(), nullptr);
    state.setProperty("zoom", zoomLevel, nullptr);
    state.setProperty("scrollX", viewport.getViewPositionX(), nullptr);
    state.setProperty("scrollY", viewport.getViewPositionY(), nullptr);
    
    // Tracks
    juce::ValueTree tracksTree("Tracks");
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
    {
        juce::ValueTree track("Track");
        track.setProperty("index", i, nullptr);
        track.setProperty("bpm", getTrackBPM(i), nullptr);
        track.setProperty("solo", isTrackSoloed(i), nullptr);
        track.setProperty("mute", isTrackMuted(i), nullptr);
        
        // Save clips for this track
        juce::ValueTree clipsTree("Clips");
        const auto& trackClips = tracks[i]->getClips();
        for (const auto& clipPtr : trackClips)
        {
            if (clipPtr)
            {
                juce::ValueTree clip("Clip");
                clip.setProperty("startTime", clipPtr->startTime, nullptr);
                clip.setProperty("duration", clipPtr->duration, nullptr);
                clip.setProperty("file", clipPtr->file.getFullPathName(), nullptr);
                   clip.setProperty("originalBPM", clipPtr->originalBPM, nullptr);
                   clip.setProperty("referenceBPM", clipPtr->referenceBPM, nullptr);
                   clip.setProperty("id", clipPtr->id, nullptr);
                clip.setProperty("colour", clipPtr->colour.toString(), nullptr);
                clip.setProperty("name", clipPtr->name, nullptr);
                clipsTree.appendChild(clip, nullptr);
            }
        }
        track.appendChild(clipsTree, nullptr);
        
        tracksTree.appendChild(track, nullptr);
    }
    state.appendChild(tracksTree, nullptr);
    
    return state;
}

void MultiTrackContainer::restoreGuiState(const juce::ValueTree& state)
{
    if (!state.isValid()) return;
    
    // Rebuild tracks to match saved count FIRST (before setting size)
    auto tracksTree = state.getChildWithName("Tracks");
    const int wantedTracks = tracksTree.getNumChildren();
    
    while (tracks.size() < wantedTracks) addTrack();
    while (tracks.size() > wantedTracks)
       {
           if (!tracks.empty())
               removeTrack(static_cast<int>(tracks.size() - 1));
       }
    
    // Restore each track's state and clips
    int idx = 0;
    for (auto trackNode : tracksTree)
    {
        if (idx < static_cast<int>(trackHeaders.size()))
        {
            trackHeaders[idx]->setTrackBPM(trackNode.getProperty("bpm", 120.0));
            trackHeaders[idx]->setSoloed(trackNode.getProperty("solo", false));
            trackHeaders[idx]->setMuted(trackNode.getProperty("mute", false));
        }
        
        // Restore clips
        if (idx < static_cast<int>(tracks.size()))
        {
            auto clipsTree = trackNode.getChildWithName("Clips");
            tracks[idx]->clearAllClips();
            for (auto clipNode : clipsTree)
            {
                MidiClip mc;
                mc.startTime = clipNode.getProperty("startTime", 0.0);
                mc.duration = clipNode.getProperty("duration", 1.0);
                mc.file = juce::File(clipNode.getProperty("file", "").toString());
                   mc.originalBPM = clipNode.getProperty("originalBPM", 120.0);
                   mc.referenceBPM = clipNode.getProperty("referenceBPM", 120.0);
                   mc.id = clipNode.getProperty("id", juce::Uuid().toString()).toString();
                mc.colour = juce::Colour::fromString(clipNode.getProperty("colour", "ff000000").toString());
                mc.name = clipNode.getProperty("name", "").toString();
                tracks[idx]->addClip(mc);
            }
        }
        ++idx;
    }
    
    // Restore zoom level BEFORE updating timeline size
    float savedZoom = state.getProperty("zoom", 100.0f);
    zoomLevel = savedZoom;
    
    // Update timeline with restored zoom
    updateTimelineSize();
    
    // Restore scroll positions
    const int scrollX = state.getProperty("scrollX", 0);
    const int scrollY = state.getProperty("scrollY", 0);
    
    // Use timer to ensure scroll is applied after layout is complete
    juce::Timer::callAfterDelay(50, [this, scrollX, scrollY]()
    {
        viewport.setViewPosition(scrollX, scrollY);
    });
    
    // Force complete repaint
    resized();
    repaint();
}

//==============================================================================
// File menu operations
void MultiTrackContainer::saveTimelineState()
{
    timelineManager->saveTimelineState();
}

void MultiTrackContainer::loadTimelineState()
{
    timelineManager->loadTimelineState();
}

void MultiTrackContainer::exportTimelineAsMidi()
{
    timelineManager->exportTimelineAsMidi();
}

void MultiTrackContainer::exportTimelineAsSeparateMidis()
{
    timelineManager->exportTimelineAsSeparateMidis();
}

void MultiTrackContainer::beginDragOfSelectedClips(const juce::MouseEvent& e)
{
    timelineManager->beginDragOfSelectedClips(e);
}

void MultiTrackContainer::exportSelectedClipsForDragDrop(juce::DragAndDropContainer& dragContainer)
{
    // Get all selected clips
    std::vector<MidiClip> selectedClips;
    for (auto& track : tracks)
    {
        auto trackClips = track->getSelectedClips();
        for (auto* clip : trackClips)
        {
            selectedClips.push_back(*clip);
        }
    }
    
    if (selectedClips.empty())
        return;
    
    // Create temporary MIDI file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File tempFile = tempDir.getNonexistentChildFile("drum_groove", ".mid");
    
    // Export selected clips to MIDI file
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);
    
    for (const auto& clip : selectedClips)
    {
        if (clip.file.existsAsFile())
        {
            juce::MidiFile clipMidi;
            juce::FileInputStream stream(clip.file);
            if (clipMidi.readFrom(stream))
            {
                // Add clip MIDI to main file with time offset
                for (int trackIndex = 0; trackIndex < clipMidi.getNumTracks(); ++trackIndex)
                {
                    auto* track = clipMidi.getTrack(trackIndex);
                    if (trackIndex >= midiFile.getNumTracks())
                    {
                        midiFile.addTrack(*track);
                    }
                    else
                    {
                        juce::MidiMessageSequence* mainTrack = const_cast<juce::MidiMessageSequence*>(midiFile.getTrack(trackIndex));
                        for (int i = 0; i < track->getNumEvents(); ++i)
                        {
                            auto* event = track->getEventPointer(i);
                            double newTime = event->message.getTimeStamp() + (clip.startTime * 960 * 2);
                            mainTrack->addEvent(event->message, newTime);
                        }
                    }
                }
            }
        }
    }
    
    // Write MIDI file
    juce::FileOutputStream outputStream(tempFile);
    if (outputStream.openedOk())
    {
        midiFile.writeTo(outputStream);
        outputStream.flush();
        
        // Create drag description
        juce::StringArray filePaths;
        filePaths.add(tempFile.getFullPathName());
        
        // Start drag operation with proper image parameter
        dragContainer.startDragging(juce::var(filePaths), this, juce::ScaledImage(), true);
    }
}

juce::String MultiTrackContainer::getTrackName(int trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < static_cast<int>(trackHeaders.size()))
    {
        if (trackHeaders[trackIndex])
            return trackHeaders[trackIndex]->getTrackName();
    }
    return "Track " + juce::String(trackIndex + 1);
}

//==============================================================================
// ScrollBar synchronization
void MultiTrackContainer::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    // Handle manual vertical scrollbar
    if (scrollBarThatHasMoved == &manualVerticalScrollbar)
    {
        viewport.setViewPosition(viewport.getViewPositionX(), static_cast<int>(newRangeStart));
        headerViewport.setViewPosition(0, static_cast<int>(newRangeStart));
        return;
    }
    
    // Handle overlay horizontal scrollbar
    if (scrollBarThatHasMoved == &overlayHorizontalScrollbar)
    {
        viewport.setViewPosition(static_cast<int>(newRangeStart), viewport.getViewPositionY());
        
        if (fixedRulerRow)
        {
            fixedRulerRow->setViewportX(static_cast<int>(newRangeStart));
        }
        return;
    }
    
    // Update scrollbars when viewport scrolls programmatically
    if (timelineContent)
    {
        overlayHorizontalScrollbar.setCurrentRange(
            viewport.getViewPositionX(), 
            viewport.getWidth(), 
            juce::dontSendNotification
        );
        
        if (needsManualVerticalScrollbar)
        {
            manualVerticalScrollbar.setCurrentRange(
                viewport.getViewPositionY(),
                viewport.getHeight() - 14,
                juce::dontSendNotification
            );
        }
    }
}