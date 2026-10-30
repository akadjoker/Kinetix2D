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
    mIds.clear();
    if (object)
        mIds.push_back(object->id());
    mComponentId = 0;
}

void EditorSelection::selectId(uint64_t objectId)
{
    mIds.clear();
    mIds.push_back(objectId);
    mComponentId = 0;
}

void EditorSelection::toggle(const GameObject *object)
{
    if (!object)
        return;
    const uint64_t id = object->id();
    for (size_t i = 0; i < mIds.size(); ++i)
    {
        if (mIds[i] == id)
        {
            for (size_t j = i; j + 1 < mIds.size(); ++j)
                mIds[j] = mIds[j + 1];
            mIds.pop_back();
            return;
        }
    }
    mIds.push_back(id);
}

void EditorSelection::clear()
{
    mIds.clear();
    mComponentId = 0;
}

bool EditorSelection::isSelected(uint64_t id) const
{
    for (size_t i = 0; i < mIds.size(); ++i)
        if (mIds[i] == id)
            return true;
    return false;
}

GameObject *EditorSelection::resolve(Scene &scene) const
{
    return hasSelection() ? findById(scene.root(), objectId()) : nullptr;
}

}
