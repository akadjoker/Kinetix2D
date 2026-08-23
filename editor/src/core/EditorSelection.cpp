#include "EditorSelection.h"

#include <k2d/GameObject.h>
#include <k2d/Scene.h>

namespace k2d::editor
{

namespace
{
GameObject *findById(GameObject &object, uint64_t id)
{
    if (object.id() == id)
        return &object;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (GameObject *found = findById(*object.child(i), id))
            return found;
    return nullptr;
}
}

void EditorSelection::select(const GameObject *object)
{
    mObjectId = object ? object->id() : 0;
    mHasSelection = object != nullptr;
}

void EditorSelection::clear()
{
    mObjectId = 0;
    mHasSelection = false;
}

void EditorSelection::selectId(uint64_t objectId)
{
    mObjectId = objectId;
    mHasSelection = true;
}

GameObject *EditorSelection::resolve(Scene &scene) const
{
    return mHasSelection ? findById(scene.root(), mObjectId) : nullptr;
}

}
