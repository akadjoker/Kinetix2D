#pragma once

#include <ct/json.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdint>

namespace k2d::editor
{

struct SceneCommand
{
    ct::String label;
    ct::Json before;
    ct::Json after;
    uint64_t selectionBefore = 0;
    uint64_t selectionAfter = 0;
    bool hadSelectionBefore = false;
    bool hadSelectionAfter = false;
};

class CommandStack
{
public:
    void push(SceneCommand command);
    const SceneCommand *undo();
    const SceneCommand *redo();
    void clear();

    bool canUndo() const { return mCursor > 0; }
    bool canRedo() const { return mCursor < mCommands.size(); }
    const char *undoName() const;
    const char *redoName() const;

private:
    ct::Vector<SceneCommand> mCommands;
    size_t mCursor = 0;
};

}
