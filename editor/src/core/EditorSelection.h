#pragma once

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
    void clear();
    uint64_t objectId() const { return mObjectId; }
    bool hasSelection() const { return mHasSelection; }
    GameObject *resolve(Scene &scene) const;

private:
    uint64_t mObjectId = 0;
    bool mHasSelection = false;
};

}
