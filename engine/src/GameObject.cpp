#include "k2d/GameObject.h"

#include "k2d/Scene.h"

#include <cmath>

namespace k2d
{

    GameObject::GameObject(const char *name)
        : mName(name), mId(0), mFlags(GameObjectActive | GameObjectVisible), mScene(nullptr),
          mParent(nullptr), mComponents{}, mComponentCallbackDepth(0), mPosition(0.0f),
          mRotationDegrees(0.0f), mScale(1.0f, 1.0f), mZIndex(0),
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
    }

    const ct::String &GameObject::tag() const
    {
        return mTag;
    }

    Component *GameObject::rawComponent(ComponentType type) const
    {
        const uint8_t index = static_cast<uint8_t>(type);
        return index < static_cast<uint8_t>(ComponentType::Count) ? mComponents[index] : nullptr;
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
            mFlags |= GameObjectDispose;
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
        if (index >= static_cast<uint8_t>(ComponentType::Count) || mComponents[index])
            return false;
        component->mOwner = this;
        mComponents[index] = component;
        if (mScene && (component->mEvents & ComponentEventLateUpdate) != 0)
            ++mScene->mLateUpdateCount;
        component->attached();
        return true;
    }

    bool GameObject::removeComponent(ComponentType type)
    {
        const uint8_t index = static_cast<uint8_t>(type);
        if (index >= static_cast<uint8_t>(ComponentType::Count) || !mComponents[index])
            return false;
        Component *component = mComponents[index];
        if (mScene && (component->mEvents & ComponentEventLateUpdate) != 0)
            --mScene->mLateUpdateCount;
        component->detached();
        component->mOwner = nullptr;
        mComponents[index] = nullptr;
        if (mComponentCallbackDepth > 0)
            mPendingComponentDeletes.push_back(component);
        else
            delete component;
        return true;
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
            if (mComponents[i])
                removeComponent(static_cast<ComponentType>(i));
    }

    void GameObject::updateComponents(float deltaTime)
    {
        ++mComponentCallbackDepth;
        for (Component *component : mComponents)
        {
            if (!component || !component->active())
                continue;
            if (!component->mStarted)
            {
                component->mStarted = true;
                component->onStart();
            }
            if ((component->mEvents & ComponentEventUpdate) != 0)
                component->onUpdate(deltaTime);
        }
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::lateUpdateComponents(float deltaTime)
    {
        ++mComponentCallbackDepth;
        for (Component *component : mComponents)
            if (component && component->active() &&
                (component->mEvents & ComponentEventLateUpdate) != 0)
                component->onLateUpdate(deltaTime);
        if (--mComponentCallbackDepth == 0)
            flushPendingComponentDeletes();
    }

    void GameObject::renderComponents(RenderQueue &queue)
    {
        ++mComponentCallbackDepth;
        for (Component *component : mComponents)
            if (component && component->active() &&
                (component->mEvents & ComponentEventRender) != 0)
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

    const glm::vec2 &GameObject::position() const
    {
        return mPosition;
    }

    float GameObject::rotationDegrees() const
    {
        return mRotationDegrees;
    }

    const glm::vec2 &GameObject::scale() const
    {
        return mScale;
    }

    void GameObject::setPosition(const glm::vec2 &position)
    {
        mPosition = position;
        invalidateTransform();
    }

    void GameObject::setRotationDegrees(float degrees)
    {
        mRotationDegrees = degrees;
        invalidateTransform();
    }

    void GameObject::setScale(const glm::vec2 &scale)
    {
        mScale = scale;
        invalidateTransform();
    }

    void GameObject::translate(const glm::vec2 &offset)
    {
        setPosition(mPosition + offset);
    }

    void GameObject::rotate(float degrees)
    {
        setRotationDegrees(mRotationDegrees + degrees);
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

    glm::vec2 GameObject::globalPosition() const
    {
        updateGlobalTransform();
        return mGlobalTransform.Position();
    }

    glm::vec2 GameObject::right() const
    {
        updateGlobalTransform();
        return glm::normalize(glm::vec2(mGlobalTransform.a, mGlobalTransform.b));
    }

    glm::vec2 GameObject::up() const
    {
        updateGlobalTransform();
        return glm::normalize(glm::vec2(mGlobalTransform.c, mGlobalTransform.d));
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
