#pragma once

#include <JuceHeader.h>
#include "EditableMidiClip.h"

// Base command class
class MidiNoteCommand
{
public:
    virtual ~MidiNoteCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

// Add note command
class AddNoteCommand : public MidiNoteCommand
{
public:
    AddNoteCommand(EditableMidiClip& clip, const MidiNote& note)
    : clip(clip), note(note) {}

    void execute() override
    {
        clip.addNote(note);
    }

    void undo() override
    {
        clip.removeNote(note.id);
    }

private:
    EditableMidiClip& clip;
    MidiNote note;
};

// Remove note command
class RemoveNoteCommand : public MidiNoteCommand
{
public:
    RemoveNoteCommand(EditableMidiClip& clip, const MidiNote& note)
    : clip(clip), note(note) {}

    void execute() override
    {
        clip.removeNote(note.id);
    }

    void undo() override
    {
        clip.addNote(note);
    }

private:
    EditableMidiClip& clip;
    MidiNote note;
};

// Move note command
class MoveNoteCommand : public MidiNoteCommand
{
public:
    MoveNoteCommand(EditableMidiClip& clip, const juce::String& noteId,
                    double oldStartTime, int oldNoteNumber,
                    double newStartTime, int newNoteNumber)
    : clip(clip), noteId(noteId),
    oldStartTime(oldStartTime), oldNoteNumber(oldNoteNumber),
    newStartTime(newStartTime), newNoteNumber(newNoteNumber) {}

    void execute() override
    {
        clip.moveNote(noteId, newStartTime, newNoteNumber);
    }

    void undo() override
    {
        clip.moveNote(noteId, oldStartTime, oldNoteNumber);
    }

private:
    EditableMidiClip& clip;
    juce::String noteId;
    double oldStartTime, newStartTime;
    int oldNoteNumber, newNoteNumber;
};

// Change velocity command
class ChangeVelocityCommand : public MidiNoteCommand
{
public:
    ChangeVelocityCommand(EditableMidiClip& clip, const juce::String& noteId,
                          int oldVelocity, int newVelocity)
    : clip(clip), noteId(noteId),
    oldVelocity(oldVelocity), newVelocity(newVelocity) {}

    void execute() override
    {
        clip.setNoteVelocity(noteId, newVelocity);
    }

    void undo() override
    {
        clip.setNoteVelocity(noteId, oldVelocity);
    }

private:
    EditableMidiClip& clip;
    juce::String noteId;
    int oldVelocity, newVelocity;
};

// Batch command for quantize operations
class BatchMoveCommand : public MidiNoteCommand
{
public:
    struct NoteMove
    {
        juce::String noteId;
        double oldStartTime;
        int oldNoteNumber;
        double newStartTime;
        int newNoteNumber;
    };

    BatchMoveCommand(EditableMidiClip& clip, const std::vector<NoteMove>& moves)
    : clip(clip), moves(moves) {}

    void execute() override
    {
        for (const auto& move : moves)
            clip.moveNote(move.noteId, move.newStartTime, move.newNoteNumber);
    }

    void undo() override
    {
        for (const auto& move : moves)
            clip.moveNote(move.noteId, move.oldStartTime, move.oldNoteNumber);
    }

private:
    EditableMidiClip& clip;
    std::vector<NoteMove> moves;
};

// Undo manager for MIDI editor
class MidiEditorUndoManager
{
public:
    MidiEditorUndoManager(int maxLevels = 50) : maxUndoLevels(maxLevels) {}

    void perform(std::unique_ptr<MidiNoteCommand> command)
    {
        command->execute();

        undoStack.push_back(std::move(command));
        if (undoStack.size() > maxUndoLevels)
            undoStack.erase(undoStack.begin());

        redoStack.clear();
    }

    bool undo()
    {
        if (undoStack.empty())
            return false;

        auto cmd = std::move(undoStack.back());
        undoStack.pop_back();

        cmd->undo();

        redoStack.push_back(std::move(cmd));
        return true;
    }

    bool redo()
    {
        if (redoStack.empty())
            return false;

        auto cmd = std::move(redoStack.back());
        redoStack.pop_back();

        cmd->execute();

        undoStack.push_back(std::move(cmd));
        return true;
    }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    void clearUndoHistory()
    {
        undoStack.clear();
        redoStack.clear();
    }

private:
    std::vector<std::unique_ptr<MidiNoteCommand>> undoStack;
    std::vector<std::unique_ptr<MidiNoteCommand>> redoStack;
    size_t maxUndoLevels;
};
