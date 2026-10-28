#pragma once

#include <ct/json.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstddef>

namespace k2d::editor
{

// The scene after each edit, compressed, with the edit's name: undo steps one
// state back, redo one forward. Past kLimit states the oldest is let go.
class CommandStack
{
public:
    static constexpr std::size_t kLimit = 200;

    // The state the history starts from: a scene opened or made.
    void reset(const ct::Json& state);
    // An edit made, and the scene as it is after it.
    void push(const char* label, const ct::Json& state);
    // The state to show after a step back or forward; false when there is none.
    bool undo(ct::Json& state);
    bool redo(ct::Json& state);
    void clear();
    bool canUndo() const { return mCursor > 0; }
    bool canRedo() const { return mCursor + 1 < mStates.size(); }
    const char* undoName() const;
    const char* redoName() const;
    // The bytes the history holds, compressed.
    std::size_t bytes() const;

private:
    struct State
    {
        ct::String label;
        ct::Vector<unsigned char> data;
        std::size_t size = 0;
    };

    static bool pack(const ct::Json& state, State& out);
    bool unpack(std::size_t index, ct::Json& state) const;

    ct::Vector<State> mStates;
    std::size_t mCursor = 0;
};

} // namespace k2d::editor
