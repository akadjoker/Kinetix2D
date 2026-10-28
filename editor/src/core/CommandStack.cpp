#include "CommandStack.h"

#include <k2d/Compress.h>

namespace k2d::editor
{

bool CommandStack::pack(const ct::Json& state, State& out)
{
    const ct::String text = state.dump();
    out.size = text.size();
    return Compress(text.c_str(), text.size(), out.data);
}

bool CommandStack::unpack(std::size_t index, ct::Json& state) const
{
    const State& stored = mStates[index];
    ct::Vector<unsigned char> text;
    if (!Decompress(stored.data.data(), stored.data.size(), stored.size, text))
        return false;
    text.push_back(0);
    state = ct::Json::parse(reinterpret_cast<const char*>(text.data()));
    return true;
}

void CommandStack::reset(const ct::Json& state)
{
    clear();
    State first;
    if (pack(state, first))
        mStates.push_back(ct::detail::move(first));
}

void CommandStack::push(const char* label, const ct::Json& state)
{
    if (mStates.empty())
        return;
    State next;
    next.label = label ? label : "";
    if (!pack(state, next))
        return;
    if (mCursor + 1 < mStates.size())
        mStates.erase(mStates.begin() + static_cast<std::ptrdiff_t>(mCursor + 1), mStates.end());
    mStates.push_back(ct::detail::move(next));
    if (mStates.size() > kLimit)
        mStates.erase(mStates.begin());
    mCursor = mStates.size() - 1;
}

bool CommandStack::undo(ct::Json& state)
{
    if (!canUndo() || !unpack(mCursor - 1, state))
        return false;
    --mCursor;
    return true;
}

bool CommandStack::redo(ct::Json& state)
{
    if (!canRedo() || !unpack(mCursor + 1, state))
        return false;
    ++mCursor;
    return true;
}

void CommandStack::clear()
{
    mStates.clear();
    mCursor = 0;
}

const char* CommandStack::undoName() const
{
    return canUndo() ? mStates[mCursor].label.c_str() : "";
}

const char* CommandStack::redoName() const
{
    return canRedo() ? mStates[mCursor + 1].label.c_str() : "";
}

std::size_t CommandStack::bytes() const
{
    std::size_t total = 0;
    for (const State& state : mStates)
        total += state.data.size();
    return total;
}

} // namespace k2d::editor
