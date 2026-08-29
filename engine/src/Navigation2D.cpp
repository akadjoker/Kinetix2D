#include "k2d/Navigation2D.h"

#include "k2d/GameObject.h"
#include "k2d/NavigationRegion2D.h"
#include "k2d/Scene.h"

namespace k2d
{
namespace
{
ct::Vector<NavigationRegion2D*>& regions()
{
    static ct::Vector<NavigationRegion2D*> value;
    return value;
}
} // namespace

void Navigation2D::Register(NavigationRegion2D* region)
{
    if (!region)
        return;
    for (NavigationRegion2D* known : regions())
        if (known == region)
            return;
    regions().push_back(region);
}

void Navigation2D::Unregister(NavigationRegion2D* region)
{
    for (size_t i = 0; i < regions().size(); ++i)
        if (regions()[i] == region)
        {
            regions().erase(regions().begin() + i);
            return;
        }
}

bool Navigation2D::GetPath(const Scene& scene, const Math::Vec2& from, const Math::Vec2& to,
                           ct::Vector<Math::Vec2>& outPath)
{
    outPath.clear();
    for (NavigationRegion2D* region : regions())
    {
        GameObject* object = region ? region->owner() : nullptr;
        if (!object || object->scene() != &scene || !object->isActiveInHierarchy())
            continue;
        if (region->getPath(from, to, outPath))
            return true;
    }
    return false;
}

bool Navigation2D::Contains(const Scene& scene, const Math::Vec2& point)
{
    for (NavigationRegion2D* region : regions())
    {
        GameObject* object = region ? region->owner() : nullptr;
        if (!object || object->scene() != &scene || !object->isActiveInHierarchy())
            continue;
        if (region->containsPoint(point))
            return true;
    }
    return false;
}
} // namespace k2d
