#pragma once

#include <JuceHeader.h>
#include "EditableMidiClip.h"
#include "ColourPalette.h"
#include "MidiDissector.h"

// Forward declaration
class DrumGridView;

class VelocityLaneComponent : public juce::Component
{
public:
    VelocityLaneComponent(EditableMidiClip& clip, DrumGridView& gridView,
                          DrumLibraryManager& libMgr);
    ~VelocityLaneComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Synchronization with main grid
    void setZoomFactor(float zoom) { zoomFactor = zoom; repaint(); }
    void refresh() { repaint(); }

    // Get dimensions
    static constexpr int LANE_HEIGHT = 100;  // Height of velocity lane

    // Callbacks
    std::function<void()> onVelocityChanged;
    std::function<void()> onNeedsRefresh;

private:
    EditableMidiClip& clip;
    DrumLibraryManager& drumLibraryManager;
    DrumGridView& gridView;
    float zoomFactor = 1.0f;

    static constexpr int MIN_VELOCITY = 1;
    static constexpr int MAX_VELOCITY = 127;

    // Track dragging state
    MidiNote* draggedNote = nullptr;
    int dragStartVelocity = 0;
    bool isDragging = false;

    // Helper methods
    juce::Rectangle<float> getBarBounds(const MidiNote& note) const;
    MidiNote* getNoteAtPosition(int x, int y);
    int velocityFromY(int y) const;
    int yFromVelocity(int velocity) const;
    juce::Colour getColourForDrumPart(const MidiNote& note, bool selected) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelocityLaneComponent)
};
