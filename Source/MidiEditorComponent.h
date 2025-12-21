#pragma once

#include <JuceHeader.h>
#include "EditableMidiClip.h"
#include "DrumGridView.h"
#include "MidiEditorToolbar.h"
#include "VelocityLaneComponent.h"
#include "DrumLibraryManager.h"
#include "MidiProcessor.h"
#include "MidiNoteCommands.h"

// Forward declaration
class DrumGrooveProcessor;

class MidiEditorComponent : public juce::DocumentWindow
{
public:
    MidiEditorComponent(DrumLibraryManager& libManager, MidiProcessor& midiProc, DrumGrooveProcessor& proc);
    ~MidiEditorComponent() override;

    // Open existing MIDI file for editing
    bool openMidiFile(const juce::File& file, DrumLibrary sourceLib);

    // Create new MIDI clip
    void createNewClip(DrumLibrary sourceLib, double bpm, int bars);

    // Close button
    void closeButtonPressed() override;

    // Save/Load
    bool saveClip();
    bool saveClipAs();

    // Callbacks
    std::function<void(const juce::File&)> onClipSaved;
    std::function<void()> onEditorClosed;

private:
    class EditorContent : public juce::Component,
    public juce::Timer
    {
    public:
        EditorContent(EditableMidiClip& clip, DrumLibraryManager& libManager, MidiProcessor& midiProc, DrumGrooveProcessor& proc);
        ~EditorContent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        bool keyPressed(const juce::KeyPress& key) override;

        bool hasUnsavedChanges() const { return unsavedChanges; }
        void setUnsavedChanges(bool changed) { unsavedChanges = changed; }

        MidiEditorToolbar toolbar;
        DrumGridView gridView;
        VelocityLaneComponent velocityLane;

        // Playback control
        void startPlayback();
        void stopPlayback();
        void quantizeNotes();

        MidiEditorUndoManager& getUndoManager() { return undoManager; }

        // Loop control
        void setLoopEnabled(bool enabled);
        bool isLoopEnabled() const { return loopEnabled; }
        void setLoopRegion(double startQN, double endQN);

        // Velocity lane control
        void setVelocityLaneVisible(bool visible);
        bool isVelocityLaneVisible() const { return velocityLaneVisible; }

        // Read-only mode
        void setReadOnly(bool readOnly);

        bool previewNotesEnabled = false;
        void playPreviewNote(int noteNumber, int velocity);

    private:
        bool loopEnabled = false;
        double loopStartQN = 0.0;
        double loopEndQN = 4.0;
        bool velocityLaneVisible = false;

        EditableMidiClip& clip;
        MidiProcessor& midiProcessor;
        DrumGrooveProcessor& processor;
        MidiEditorUndoManager undoManager;
        bool unsavedChanges = false;
        bool isPlaying = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorContent)
    };

    DrumLibraryManager& drumLibraryManager;
    MidiProcessor& midiProcessor;
    DrumGrooveProcessor& processor;
    std::unique_ptr<EditableMidiClip> clip;
    std::unique_ptr<EditorContent> content;

    juce::File currentFile;
    juce::File originalFile;

    void setupCallbacks();
    void updateTitle();
    bool promptToSaveChanges();
    bool isLibraryFile(const juce::File& file) const;
    bool isReadOnlyFile(const juce::File& file) const;

    bool isReadOnly = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditorComponent)
};
