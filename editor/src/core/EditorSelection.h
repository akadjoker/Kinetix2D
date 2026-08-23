#pragma once

#include <ct/vector.hpp>

#include <cstdint>

namespace k2d
{
class GameObject;
class Scene;
}

namespace k2d::editor
{

class EditorSelection
{
public:
    void select(const GameObject *object);
    void selectId(uint64_t objectId);
    void toggle(const GameObject *object);
    void clear();

    uint64_t objectId() const { return mIds.empty() ? 0 : mIds.back(); }
    bool hasSelection() const { return !mIds.empty(); }
    GameObject *resolve(Scene &scene) const;

    const ct::Vector<uint64_t> &ids() const { return mIds; }
    bool isSelected(uint64_t id) const;
    std::size_t count() const { return mIds.size(); }

private:
    ct::Vector<uint64_t> mIds;
};

}
