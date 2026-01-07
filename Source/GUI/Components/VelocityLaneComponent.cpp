#include "VelocityLaneComponent.h"
#include "DrumGridView.h"

VelocityLaneComponent::VelocityLaneComponent(EditableMidiClip& midiClip, DrumGridView& grid,
                                             DrumLibraryManager& libMgr)
: clip(midiClip), gridView(grid), drumLibraryManager(libMgr)
{
    setSize(800, LANE_HEIGHT);
}

VelocityLaneComponent::~VelocityLaneComponent()
{
}

void VelocityLaneComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(ColourPalette::mainBackground);

    // Draw horizontal reference lines for velocity levels
    g.setColour(ColourPalette::timelineGrid.withAlpha(0.5f));
    for (int vel = 0; vel <= 127; vel += 32)
    {
        int y = yFromVelocity(vel);
        g.drawHorizontalLine(y, 0.0f, (float)getWidth());

        // Draw velocity labels on the left
        if (vel > 0)  // Don't draw label at 0
        {
            g.setColour(ColourPalette::secondaryText.withAlpha(0.6f));
            g.setFont(9.0f);
            g.drawText(juce::String(vel), 2, y - 10, 30, 12, juce::Justification::centredLeft);
        }
    }

    // Draw velocity bars for each note
    const auto& notes = clip.getNotes();

    for (const auto& note : notes)
    {
        auto barBounds = getBarBounds(note);

        // Skip notes that are off-screen
        if (barBounds.getRight() < 0 || barBounds.getX() > getWidth())
            continue;

        // Get color based on drum part
        juce::Colour barColour = getColourForDrumPart(note, note.isSelected);

        // Draw the velocity bar
        g.setColour(barColour.withAlpha(0.8f));
        g.fillRect(barBounds);

        // Draw outline
        g.setColour(barColour.brighter(0.2f));
        g.drawRect(barBounds, 1.0f);

        // Draw velocity value on top of bar if selected
        if (note.isSelected && barBounds.getHeight() > 15.0f)
        {
            g.setColour(juce::Colours::white);
            g.setFont(10.0f);
            g.drawText(juce::String(note.velocity),
                       (int)barBounds.getX(), (int)barBounds.getY() - 14,
                       (int)barBounds.getWidth(), 12,
                       juce::Justification::centred);
        }
    }

    // Draw zero line at bottom
    g.setColour(ColourPalette::timelineGrid);
    int zeroY = yFromVelocity(0);
    g.drawHorizontalLine(zeroY, 0.0f, (float)getWidth());
}

void VelocityLaneComponent::resized()
{
}

juce::Rectangle<float> VelocityLaneComponent::getBarBounds(const MidiNote& note) const
{
    static constexpr int LABEL_WIDTH = 120;

    // Get all values from grid for exact match
    double scrollOffsetX = gridView.getScrollOffsetX();
    float zoomFactor = gridView.getZoomLevel();
    float pixelsPerQN = gridView.getPixelsPerQuarterNote();

    // Match DrumGridView::timeToX() exactly - LEFT-aligned
    float x = LABEL_WIDTH + (float)((note.startTime - scrollOffsetX) * pixelsPerQN * zoomFactor);

    // 12px width, LEFT-aligned (not centered)
    float width = 12.0f;

    // Velocity height
    int bottomY = yFromVelocity(0);
    int topY = yFromVelocity(note.velocity);
    float height = (float)(bottomY - topY);

    return juce::Rectangle<float>(x, (float)topY, width, height);
}

MidiNote* VelocityLaneComponent::getNoteAtPosition(int x, int y)
{
    auto& notes = clip.getNotes();

    // Search in reverse to get topmost note if overlapping
    for (int i = (int)notes.size() - 1; i >= 0; --i)
    {
        // Get non-const reference through const_cast (we need to modify notes)
        MidiNote& note = const_cast<MidiNote&>(notes[i]);
        auto bounds = getBarBounds(note);

        if (bounds.contains((float)x, (float)y))
            return &note;
    }

    return nullptr;
}

int VelocityLaneComponent::velocityFromY(int y) const
{
    // Velocity increases from bottom to top
    float ratio = 1.0f - (y / (float)getHeight());
    int velocity = (int)(ratio * MAX_VELOCITY);
    return juce::jlimit(MIN_VELOCITY, MAX_VELOCITY, velocity);
}

int VelocityLaneComponent::yFromVelocity(int velocity) const
{
    // Velocity increases from bottom to top
    float ratio = velocity / (float)MAX_VELOCITY;
    return (int)((1.0f - ratio) * getHeight());
}

juce::Colour VelocityLaneComponent::getColourForDrumPart(const MidiNote& note, bool selected) const
{
    if (selected)
        return juce::Colour(0xFF00AAFF);

    DrumLibrary sourceLib = clip.getSourceLibrary();
    return MidiDissector::getColourForNote(note.noteNumber, sourceLib, &drumLibraryManager);
}

void VelocityLaneComponent::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isLeftButtonDown())
    {
        draggedNote = getNoteAtPosition(event.x, event.y);

        if (draggedNote)
        {
            dragStartVelocity = draggedNote->velocity;
            isDragging = true;

            // If clicking on unselected note, select only this note
            if (!draggedNote->isSelected)
            {
                clip.clearSelection();
                draggedNote->isSelected = true;
                if (onNeedsRefresh)
                    onNeedsRefresh();
            }
        }
    }
}

void VelocityLaneComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedNote && isDragging && event.mods.isLeftButtonDown())
    {
        int newVelocity = velocityFromY(event.y);

        // Calculate delta from start
        int delta = newVelocity - dragStartVelocity;

        // Apply delta to all selected notes
        auto& notes = clip.getNotes();
        for (auto& note : notes)
        {
            // Get non-const reference
            MidiNote& mutableNote = const_cast<MidiNote&>(note);

            if (mutableNote.isSelected)
            {
                int targetVelocity = mutableNote.velocity + delta;
                targetVelocity = juce::jlimit(MIN_VELOCITY, MAX_VELOCITY, targetVelocity);

                if (mutableNote.velocity != targetVelocity)
                {
                    clip.setNoteVelocity(mutableNote.id, targetVelocity);
                }
            }
        }

        // Update drag start for next iteration
        dragStartVelocity = newVelocity;

        if (onVelocityChanged)
            onVelocityChanged();

        repaint();
        if (onNeedsRefresh)
            onNeedsRefresh();
    }
}

void VelocityLaneComponent::mouseUp(const juce::MouseEvent& event)
{
    draggedNote = nullptr;
    isDragging = false;
}
