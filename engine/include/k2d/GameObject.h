#pragma once

#include "k2d/Component.h"

#include <ct/detail/utils.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdint>
#include <mathc.h>
#include "Matrix2D.h"

namespace k2d
{

    class Scene;
    class RenderQueue;

    enum GameObjectFlags : uint32_t
    {
        GameObjectActive = 1 << 0,
        GameObjectVisible = 1 << 1,
        GameObjectDispose = 1 << 2,
        GameObjectLocked = 1 << 3
    };

    class GameObject final
    {
    public:
        ~GameObject();

        GameObject(const GameObject &) = delete;
        GameObject &operator=(const GameObject &) = delete;

        uint64_t id() const;
        Scene *scene() const;
        const ct::String &name() const;
        void setName(const ct::String &name);
        const ct::String &tag() const;
        void setTag(const ct::String &tag);

        bool active() const;
        void setActive(bool active);
        bool visible() const;
        void setVisible(bool visible);
        bool locked() const;
        void setLocked(bool locked);
        bool isActiveInHierarchy() const;
        bool isVisibleInHierarchy() const;
        bool isActiveAndVisibleInHierarchy() const;
        void dispose();
        bool disposed() const;

        GameObject *parent() const;
        GameObject *root();
        const GameObject *root() const;
        std::size_t childCount() const;
        GameObject *child(std::size_t index) const;
        std::size_t childIndex(const GameObject *child) const;
        GameObject *findChild(const char *name, bool recursive = true) const;

        bool addChild(GameObject *child);
        GameObject *removeChild(GameObject *child);
        bool deleteChild(GameObject *child);
        void deleteChildren();
        bool moveChildUp(GameObject *child);
        bool moveChildDown(GameObject *child);

        template <class T, class... Args> T *addComponent(Args &&...args)
        {
            T *component = new T(ct::detail::forward<Args>(args)...);
            if (!attachComponent(component))
            {
                delete component;
                return nullptr;
            }
            return component;
        }

        template <class T> T *getComponent() const
        {
            for (Component *c = mComponents[static_cast<uint8_t>(T::Type)]; c; c = c->mNextSibling)
                if (ComponentMatch<T>::test(c))
                    return static_cast<T *>(c);
            return nullptr;
        }

        template <class T> T *getComponent(uint32_t id) const
        {
            for (Component *c = mComponents[static_cast<uint8_t>(T::Type)]; c; c = c->mNextSibling)
                if (c->id() == id && ComponentMatch<T>::test(c))
                    return static_cast<T *>(c);
            return nullptr;
        }

        template <class T> T *getComponentAt(std::size_t index) const
        {
            std::size_t seen = 0;
            for (Component *c = mComponents[static_cast<uint8_t>(T::Type)]; c; c = c->mNextSibling)
            {
                if (!ComponentMatch<T>::test(c))
                    continue;
                if (seen == index)
                    return static_cast<T *>(c);
                ++seen;
            }
            return nullptr;
        }

        template <class T> std::size_t componentCount() const
        {
            std::size_t count = 0;
            for (Component *c = mComponents[static_cast<uint8_t>(T::Type)]; c; c = c->mNextSibling)
                if (ComponentMatch<T>::test(c))
                    ++count;
            return count;
        }

        template <class T> bool contains() const
        {
            return getComponent<T>() != nullptr;
        }

        template <class T> T *findComponent() const
        {
            return getComponent<T>();
        }

        template <class T> bool removeComponent()
        {
            return removeComponent(getComponent<T>());
        }

        bool removeComponent(Component *component);

        Component *rawComponent(ComponentType type, std::size_t index = 0) const;
        Component *uiComponent() const { return mUiComponent; }
        std::size_t rawComponentCount(ComponentType type) const;

        int zIndex() const;
        void setZIndex(int zIndex);

        const Math::Vec2 &position() const;
        float rotationDegrees() const;
        const Math::Vec2 &scale() const;
        void setPosition(const Math::Vec2 &position);
        void setRotationDegrees(float degrees);
        void setPositionAndRotation(const Math::Vec2 &position, float rotationDegrees);
        void setScale(const Math::Vec2 &scale);
        void translate(const Math::Vec2 &offset);
        void rotate(float degrees);
        void turn(float degrees,float speed);
        void moveTo(const Math::Vec2 &position, float rotationDegrees);
        void xadvance(float speed,float angle);
        void advance(float speed);
        const Matrix2D &localTransform() const;
        const Matrix2D &globalTransform() const;
        float globalRotationDegrees() const;
        Math::Vec2 globalPosition() const;
        Math::Vec2 right() const;
        Math::Vec2 up() const;

    private:
        friend class Scene;

        explicit GameObject(const char *name = "");

        bool addChildRaw(GameObject *child);
        GameObject *removeChildRaw(GameObject *child);
        void deleteChildrenRaw();

        bool attachComponent(Component *component);
        void unlinkComponent(Component *component);
        void deleteComponents();
        void updateComponents(float deltaTime);
        void lateUpdateComponents(float deltaTime);
        void renderComponents(RenderQueue &queue);
        void updateComponent(Component *component, float deltaTime);
        void lateUpdateComponent(Component *component, float deltaTime);
        void renderComponent(Component *component, RenderQueue &queue);
        void flushPendingComponentDeletes();
        bool isAncestorOf(const GameObject *object) const;
        void invalidateTransform();
        void invalidateChildren();
        void updateLocalTransform() const;
        void updateGlobalTransform() const;

        ct::String mName;
        ct::String mTag;
        uint64_t mId;
        uint32_t mFlags;
        Scene *mScene;
        GameObject *mParent;
        ct::Vector<GameObject *> mChildren;
        Component *mComponents[static_cast<uint8_t>(ComponentType::Count)];
        Component *mUiComponent;
        ct::Vector<Component *> mPendingComponentDeletes;
        uint32_t mComponentCallbackDepth;
        uint32_t mNextComponentId;

        Math::Vec2 mPosition;
        float mRotationDegrees;
        Math::Vec2 mScale;
        int mZIndex;
        mutable Matrix2D mLocalTransform;
        mutable Matrix2D mGlobalTransform;
        mutable bool mLocalDirty;
        mutable bool mGlobalDirty;
    };

}
