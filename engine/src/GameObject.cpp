#include "k2d/GameObject.h"

#include "k2d/Scene.h"

#include <cmath>

namespace k2d
{

    static constexpr float EPSILON = 1e-6f;
    inline float clamp(float value, float min, float max)
    {
        return value < min ? min : (value > max ? max : value);
    }
    inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
    inline float lerpAngle(float a, float b, float t)
    {
        float delta = std::fmod(b - a + 180.0f, 360.0f) - 180.0f;
        return a + delta * t;
    }

    GameObject::GameObject(const char *name)
        : mName(name), mId(0), mFlags(GameObjectActive | GameObjectVisible), mScene(nullptr),
          mParent(nullptr), mComponents{}, mUiComponent(nullptr), mComponentCallbackDepth(0), mNextComponentId(0),
          mPosition(0.0f), mRotationDegrees(0.0f), mScale(1.0f, 1.0f), mZIndex(0),
          mLocalDirty(true), mGlobalDirty(true)
    {
    }

    GameObject::~GameObject()
    {
        deleteComponents();
        deleteChildrenRaw();
        if (mParent)
            mParent->removeChildRaw(this);
    }

    int GameObject::zIndex() const
    {
        return mZIndex;
    }

    void GameObject::setZIndex(int zIndex)
    {
        mZIndex = zIndex;
    }

    uint64_t GameObject::id() const
    {
        return mId;
    }

    Scene *GameObject::scene() const
    {
        return mScene;
    }

    const ct::String &GameObject::name() const
    {
        return mName;
    }

    void GameObject::setName(const ct::String &name)
    {
        mName = name;
        if (mScene)
            mScene->markTopologyChanged();
    }

    const ct::String &GameObject::tag() const
    {
        return mTag;
    }

    Component *GameObject::rawComponent(ComponentType type, std::size_t index) const
    {
        const uint8_t typeIndex = static_cast<uint8_t>(type);
        if (typeIndex >= static_cast<uint8_t>(ComponentType::Count))
            return nullptr;
        std::size_t seen = 0;
        for (Component *c = mComponents[typeIndex]; c; c = c->mNextSibling, ++seen)
            if (seen == index)
                return c;
        return nullptr;
    }

    std::size_t GameObject::rawComponentCount(ComponentType type) const
    {
        const uint8_t typeIndex = static_cast<uint8_t>(type);
        if (typeIndex >= static_cast<uint8_t>(ComponentType::Count))
            return 0;
        std::size_t count = 0;
        for (Component *c = mComponents[typeIndex]; c; c = c->mNextSibling)
            ++count;
        return count;
    }

    void GameObject::setTag(const ct::String &tag)
    {
        mTag = tag;
    }

    bool GameObject::active() const
    {
        return (mFlags & GameObjectActive) != 0;
    }

    bool GameObject::visible() const
    {
        return (mFlags & GameObjectVisible) != 0;
    }

    void GameObject::setActive(bool active)
    {
        active ? mFlags |= GameObjectActive : mFlags &= ~GameObjectActive;
    }

    void GameObject::setVisible(bool visible)
    {
        visible ? mFlags |= GameObjectVisible : mFlags &= ~GameObjectVisible;
    }

    bool GameObject::locked() const
    {
        return (mFlags & GameObjectLocked) != 0;
    }

    void GameObject::setLocked(bool locked)
    {
        locked ? mFlags |= GameObjectLocked : mFlags &= ~GameObjectLocked;
    }

    bool GameObject::isActiveInHierarchy() const
    {
        for (const GameObject *object = this; object; object = object->mParent)
            if (!object->active())
                return false;
        return true;
    }

    bool GameObject::isVisibleInHierarchy() const
    {
        for (const GameObject *object = this; object; object = object->mParent)
            if (!object->visible())
                return false;
        return true;
    }

    bool GameObject::isActiveAndVisibleInHierarchy() const
    {
        const uint32_t both = GameObjectActive | GameObjectVisible;
        for (const GameObject *object = this; object; object = object->mParent)
            if ((object->mFlags & both) != both)
                return false;
        return true;
    }

    void GameObject::dispose()
    {
        if (this != root())
        {
            mFlags |= GameObjectDispose;
            if (mScene)
                mScene->mHasDisposed = true;
        }
    }

    bool GameObject::disposed() const
    {
        return (mFlags & GameObjectDispose) != 0;
    }

    GameObject *GameObject::parent() const
    {
        return mParent;
    }

    GameObject *GameObject::root()
    {
        GameObject *object = this;
        while (object->mParent)
            object = object->mParent;
        return object;
    }

    const GameObject *GameObject::root() const
    {
        return const_cast<GameObject *>(this)->root();
    }

    std::size_t GameObject::childCount() const
    {
        return mChildren.size();
    }

    GameObject *GameObject::child(std::size_t index) const
    {
        return index < mChildren.size() ? mChildren[index] : nullptr;
    }

    std::size_t GameObject::childIndex(const GameObject *object) const
    {
        for (std::size_t i = 0; i < mChildren.size(); ++i)
            if (mChildren[i] == object)
                return i;
        return mChildren.size();
    }

    bool GameObject::moveChildUp(GameObject *child)
    {
        const std::size_t index = childIndex(child);
        if (index == 0 || index >= mChildren.size())
            return false;
        GameObject *previous = mChildren[index - 1];
        mChildren[index - 1] = mChildren[index];
        mChildren[index] = previous;
        return true;
    }

    bool GameObject::moveChildDown(GameObject *child)
    {
        const std::size_t index = childIndex(child);
        if (index >= mChildren.size() || index + 1 >= mChildren.size())
            return false;
        GameObject *next = mChildren[index + 1];
        mChildren[index + 1] = mChildren[index];
        mChildren[index] = next;
        return true;
    }

    GameObject *GameObject::findChild(const char *name, bool recursive) const
    {
        for (GameObject *object : mChildren)
        {
            if (object->name() == name)
                return object;
            if (recursive)
                if (GameObject *found = object->findChild(name, true))
                    return found;
        }
        return nullptr;
    }

    bool GameObject::addChildRaw(GameObject *object)
    {
        if (!object || object == this || object->mParent || object->isAncestorOf(this))
            return false;
        object->mParent = this;
        mChildren.push_back(object);
        object->mGlobalDirty = true;
        object->invalidateChildren();
        return true;
    }

    GameObject *GameObject::removeChildRaw(GameObject *object)
    {
        const std::size_t index = childIndex(object);
        if (!object || index == mChildren.size())
            return nullptr;
        mChildren.erase(mChildren.begin() + index);
        object->mParent = nullptr;
        object->mGlobalDirty = true;
        object->invalidateChildren();
        return object;
    }

    void GameObject::deleteChildrenRaw()
    {
        while (!mChildren.empty())
        {
            GameObject *object = mChildren.back();
            mChildren.pop_back();
            object->mParent = nullptr;
            delete object;
        }
    }

    bool GameObject::addChild(GameObject *object)
    {
        if (object && (mScene || object->mScene))
            return false;
        return addChildRaw(object);
    }

    GameObject *GameObject::removeChild(GameObject *object)
    {
        if (object && (mScene || object->mScene))
            return nullptr;
        return removeChildRaw(object);
    }

    bool GameObject::deleteChild(GameObject *object)
    {
        object = removeChild(object);
        if (!object)
            return false;
        delete object;
        return true;
    }

    void GameObject::deleteChildren()
    {
        if (mScene)
            return;
        deleteChildrenRaw();
    }

    bool GameObject::attachComponent(Component *component)
    {
        if (!component || component->mOwner)
            return false;
        const uint8_t index = static_cast<uint8_t>(component->type());
        if (index >= static_cast<uint8_t>(ComponentType::Count))
            return false;
        component->mOwner = this;
        component->mLocalId = mNextComponentId++;
        component->mNextSibling = nullptr;
        if (!mComponents[index])
            mComponents[index] = component;
        else
        {
            Component *tail = mComponents[index];
            while (tail->mNextSibling)
                tail = tail->mNextSibling;
            tail->mNextSibling = component;
        }
        if (!mUiComponent && component->uiControl())
            mUiComponent = component;
        if (mScene)
            mScene->registerComponent(component);
        component->attached();
        return true;
    }

    bool GameObject::removeComponent(Component *component)
    {
        if (!component || component->mOwner != this)
            return false;
        // Before anything else, and unconditionally: an object detached from
        // its scene still has to let a cached script handle go.
        Component::notifyRemoved(component);
        if (mScene)
            mScene->unregisterComponent(component);
        component->detached();
        component->mOwner = nullptr;
        // Splicing out of the chain is safe to do immediately: it only ever
        // rewrites the predecessor's mNextSibling (or the type's head), never
        // the removed component's own mNextSibling, so a still-running
        // updateComponents/lateUpdateComponents/renderComponents pass sitting
        // on a Component* it read before this call can still read that same
        // pointer's mNextSibling afterwards and continue correctly -- the
        // memory just isn't freed yet. Only the delete waits for
        // flushPendingComponentDeletes, so a component removing itself from
        // inside its own callback doesn't free the object that callback is
        // still running on.
        unlinkComponent(component);
        if (mComponentCallbackDepth > 0)
            mPendingComponentDeletes.push_back(component);
        else
            delete component;
        return true;
    }

    void GameObject::unlinkComponent(Component *component)
    {
        if (mUiComponent == component)
        {
            mUiComponent = nullptr;
            for (uint8_t i = 0; i < static_cast<uint8_t>(ComponentType::Count) && !mUiComponent; ++i)
                for (Component *c = mComponents[i]; c; c = c->mNextSibling)
                    if (c != component && c->uiControl())
                    {
                        mUiComponent = c;
                        break;
                    }
        }
        const uint8_t index = static_cast<uint8_t>(component->type());
        Component *&head = mComponents[index];
        if (head == component)
        {
            head = component->mNextSibling;
            return;
        }
        for (Component *c = head; c; c = c->mNextSibling)
        {
            if (c->mNextSibling == component)
            {
                c->mNextSibling = component->mNextSibling;
                return;
            }
        }
    }

    void GameObject::flushPendingComponentDeletes()
    {
        for (Component *component : mPendingComponentDeletes)
            delete component;
        mPendingComponentDeletes.clear();
    }

    void GameObject::deleteComponents()
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(ComponentType::Count); ++i)
            while (mComponents[i])
                removeComponent(mComponents[i]);
    }

    void GameObject::updateComponents(float deltaTime)
    {
        ++mComponentCallbackDepth;
        for (Component *head : mComponents)
        {
            for (Component *component = head; component; component = component->mNextSibling)
            {
                if (!component->active())
                    continue;
                if (!component->mStarted)
                {
                    component->mStarted = true;
                    component->onStart();
                }
                if ((component->mEvents & ComponentEventUpdate) != 0)
                    component->onUpdate(deltaTime);
            }
        }
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::lateUpdateComponents(float deltaTime)
    {
        ++mComponentCallbackDepth;
        for (Component *head : mComponents)
            for (Component *component = head; component; component = component->mNextSibling)
                if (component->active() && (component->mEvents & ComponentEventLateUpdate) != 0)
                    component->onLateUpdate(deltaTime);
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::renderComponents(RenderQueue &queue)
    {
        ++mComponentCallbackDepth;
        for (Component *head : mComponents)
            for (Component *component = head; component; component = component->mNextSibling)
                if (component->active() && (component->mEvents & ComponentEventRender) != 0)
                    component->onRender(queue);
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::updateComponent(Component *component, float deltaTime)
    {
        if (!component || component->mOwner != this)
            return;
        ++mComponentCallbackDepth;
        if (component->active())
        {
            if (!component->mStarted)
            {
                component->mStarted = true;
                component->onStart();
            }
            if (component->mOwner == this && (component->mEvents & ComponentEventUpdate) != 0)
                component->onUpdate(deltaTime);
        }
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::lateUpdateComponent(Component *component, float deltaTime)
    {
        if (!component || component->mOwner != this)
            return;
        ++mComponentCallbackDepth;
        if (component->active() && (component->mEvents & ComponentEventLateUpdate) != 0)
            component->onLateUpdate(deltaTime);
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::renderComponent(Component *component, RenderQueue &queue)
    {
        if (!component || component->mOwner != this)
            return;
        ++mComponentCallbackDepth;
        if (component->active() && (component->mEvents & ComponentEventRender) != 0)
            component->onRender(queue);
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    bool GameObject::isAncestorOf(const GameObject *object) const
    {
        while (object)
        {
            if (object == this)
                return true;
            object = object->mParent;
        }
        return false;
    }

    const Math::Vec2 &GameObject::position() const
    {
        return mPosition;
    }

    float GameObject::rotationDegrees() const
    {
        return mRotationDegrees;
    }

    const Math::Vec2 &GameObject::scale() const
    {
        return mScale;
    }

    void GameObject::setPosition(const Math::Vec2 &position)
    {
        mPosition = position;
        invalidateTransform();
    }

    void GameObject::setRotationDegrees(float degrees)
    {
        mRotationDegrees = degrees;
        invalidateTransform();
    }

    void GameObject::setPositionAndRotation(const Math::Vec2 &position, float rotationDegrees)
    {
        mPosition = position;
        mRotationDegrees = rotationDegrees;
        invalidateTransform();
    }

    void GameObject::setScale(const Math::Vec2 &scale)
    {
        mScale = scale;
        invalidateTransform();
    }

    void GameObject::translate(const Math::Vec2 &offset)
    {
        setPosition(mPosition + offset);
    }

    void GameObject::rotate(float degrees)
    {
        setRotationDegrees(mRotationDegrees + degrees);
    }

    void GameObject::turn(float degrees, float speed)
    {
        float targetRotation = lerpAngle(mRotationDegrees, mRotationDegrees + degrees,  clamp(speed, 0.0f, 1.0f));
        mRotationDegrees = targetRotation;
       
        invalidateTransform();
    }

    void GameObject::moveTo(const Math::Vec2& position, float rotationDegrees)
    {
        float deltaRotation = rotationDegrees - mRotationDegrees;
        float x = position.x - mPosition.x;
        float y = position.y - mPosition.y;
        float cosTheta = std::cos(deltaRotation * 0.01745329251f);
        float sinTheta = std::sin(deltaRotation * 0.01745329251f);
        float newX = cosTheta * x - sinTheta * y;
        float newY = sinTheta * x + cosTheta * y;
        mPosition.x += newX;
        mPosition.y += newY;
        mRotationDegrees = rotationDegrees;
        invalidateTransform();
    }

    void GameObject::xadvance(float speed, float angle)
    {
        float radians = angle * 0.01745329251f;
        float dx = std::cos(radians) * speed;
        float dy = std::sin(radians) * speed;
        translate(Math::Vec2(dx, dy));
    }

    void GameObject::advance(float speed)
    {
      
        float radians = mRotationDegrees * 0.01745329251f; // Convert degrees to radians
        float dx = std::cos(radians) * speed ;
        float dy = std::sin(radians) * speed ;
        translate(Math::Vec2(dx, dy));
    }

    const Matrix2D &GameObject::localTransform() const
    {
        updateLocalTransform();
        return mLocalTransform;
    }

    const Matrix2D &GameObject::globalTransform() const
    {
        updateGlobalTransform();
        return mGlobalTransform;
    }

    Math::Vec2 GameObject::globalPosition() const
    {
        updateGlobalTransform();
        return mGlobalTransform.Position();
    }

    Math::Vec2 GameObject::right() const
    {
        updateGlobalTransform();
        return Math::Vec2(mGlobalTransform.a, mGlobalTransform.b).Normalized();
    }

    Math::Vec2 GameObject::up() const
    {
        updateGlobalTransform();
        return Math::Vec2(mGlobalTransform.c, mGlobalTransform.d).Normalized();
    }

    void GameObject::invalidateTransform()
    {
        mLocalDirty = true;
        mGlobalDirty = true;
        invalidateChildren();
    }

    void GameObject::invalidateChildren()
    {
        for (GameObject *child : mChildren)
        {
            child->mGlobalDirty = true;
            child->invalidateChildren();
        }
    }

    void GameObject::updateLocalTransform() const
    {
        if (!mLocalDirty)
            return;
        mLocalTransform = Matrix2D::TRS(mPosition, mRotationDegrees, mScale);
        mLocalDirty = false;
    }

    void GameObject::updateGlobalTransform() const
    {
        if (!mGlobalDirty)
            return;
        updateLocalTransform();
        if (mParent)
        {
            mParent->updateGlobalTransform();
            mGlobalTransform = mParent->mGlobalTransform * mLocalTransform;
        }
        else
            mGlobalTransform = mLocalTransform;
        mGlobalDirty = false;
    }

}
