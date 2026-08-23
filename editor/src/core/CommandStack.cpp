#include "CommandStack.h"

namespace k2d::editor
{

void CommandStack::push(SceneCommand command)
{
    if (mCursor < mCommands.size())
        mCommands.erase(mCommands.begin() + mCursor, mCommands.end());
    mCommands.push_back(ct::detail::move(command));
    mCursor = mCommands.size();
}

const SceneCommand *CommandStack::undo()
{
    if (!canUndo())
        return nullptr;
    --mCursor;
    return &mCommands[mCursor];
}

const SceneCommand *CommandStack::redo()
{
    if (!canRedo())
        return nullptr;
    const SceneCommand *command = &mCommands[mCursor];
    ++mCursor;
    return command;
}

void CommandStack::clear()
{
    mCommands.clear();
    mCursor = 0;
}

const char *CommandStack::undoName() const
{
    return canUndo() ? mCommands[mCursor - 1].label.c_str() : "";
}

const char *CommandStack::redoName() const
{
    return canRedo() ? mCommands[mCursor].label.c_str() : "";
}

}
