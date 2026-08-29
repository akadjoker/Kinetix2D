#include "k2d/Joint2D.h"

#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"

#include <limits>

namespace k2d
{

namespace
{
constexpr uint32_t kUnresolved = (std::numeric_limits<uint32_t>::max)();
}

Joint2D::Joint2D()
    : Component(Type, ComponentEventNone), mBodyA(nullptr), mBodyB(nullptr), mCollideConnected(false),
      mResolvedVersion(kUnresolved)
{
}

Joint2D::~Joint2D()
{
}

void Joint2D::setTargetName(const char *name)
{
    mTargetName = name ? name : "";
    mBodyB = nullptr;
    mResolvedVersion = kUnresolved;
}

void Joint2D::resolve()
{
    GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mResolvedVersion == scene->topologyVersion())
        return;

    mBodyA = object->getComponent<RigidBody2D>();
    mBodyB = nullptr;
    if (!mTargetName.empty())
    {
        GameObject *target = scene->find(mTargetName.c_str());
        mBodyB = target ? target->getComponent<RigidBody2D>() : nullptr;
    }

    if (mBodyA && mBodyB)
    {
        // Only lock the version in once connected: an unresolved joint (its
        // target not registered yet, or - for GearJoint2D - its referenced
        // joint not resolved in this same pass) keeps retrying every call
        // instead of getting stuck unconnected until the next topology change.
        mResolvedVersion = scene->topologyVersion();
        onConnected();
    }
}

}
