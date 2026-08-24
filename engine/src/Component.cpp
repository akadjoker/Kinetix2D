#include "k2d/Component.h"

namespace k2d
{

    Component::Component(ComponentType type, uint8_t events)
        : mOwner(nullptr), mNextSibling(nullptr), mType(type), mLocalId(0), mEvents(events),
          mActive(true), mStarted(false), mSceneAllIndex(InvalidSceneListIndex),
          mSceneLateIndex(InvalidSceneListIndex), mSceneRenderIndex(InvalidSceneListIndex)
    {
    }

    GameObject *Component::owner() const
    {
        return mOwner;
    }

    ComponentType Component::type() const
    {
        return mType;
    }

    uint32_t Component::id() const
    {
        return mLocalId;
    }

    bool Component::active() const
    {
        return mActive;
    }

    void Component::setActive(bool active)
    {
        if (mActive == active)
            return;
        mActive = active;
        if (mOwner)
            active ? onEnable() : onDisable();
    }

    void Component::attached()
    {
        onAwake();
        if (mActive)
            onEnable();
    }

    void Component::detached()
    {
        if (mActive)
            onDisable();
        onDestroy();
    }

    void Component::onAwake()
    {
    }

    void Component::onStart()
    {
    }

    void Component::onEnable()
    {
    }

    void Component::onDisable()
    {
    }

    void Component::onUpdate(float)
    {
    }

    void Component::onLateUpdate(float)
    {
    }

    void Component::onRender(RenderQueue &)
    {
    }

    void Component::onDestroy()
    {
    }

}
