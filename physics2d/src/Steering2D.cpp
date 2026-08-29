#include "k2d/Steering2D.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"

#include <limits>

namespace k2d
{

namespace
{
constexpr uint32_t kUnresolved = (std::numeric_limits<uint32_t>::max)();
}

Steering2D::Steering2D()
    : Component(Type, ComponentEventNone), mTargetPosition(0.0f, 0.0f), mTargetObject(nullptr),
      mResolvedVersion(kUnresolved), mWeight(1.0f)
{
}

Steering2D::~Steering2D()
{
}

void Steering2D::setWeight(float weight)
{
    mWeight = weight;
}

void Steering2D::setTargetName(const char *name)
{
    mTargetName = name ? name : "";
    mTargetObject = nullptr;
    mResolvedVersion = kUnresolved;
}

void Steering2D::resolve() const
{
    const GameObject *object = owner();
    const Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mTargetName.empty() || mResolvedVersion == scene->topologyVersion())
        return;

    mTargetObject = scene->find(mTargetName.c_str());
    mResolvedVersion = scene->topologyVersion();
}

bool Steering2D::target(Math::Vec2 &out) const
{
    if (mTargetName.empty())
    {
        out = mTargetPosition;
        return true;
    }

    resolve();
    const GameObject *object = owner();
    const Scene *scene = object ? object->scene() : nullptr;
    if (!mTargetObject || !scene || mResolvedVersion != scene->topologyVersion())
        return false;
    out = mTargetObject->globalPosition();
    return true;
}

}
