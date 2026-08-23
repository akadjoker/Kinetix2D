#pragma once

#include "k2d/Component.h"

#include <ct/detail/utils.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdint>
#include <glm/glm.hpp>
#include "Matrix2D.h"

namespace k2d
{

    class Scene;
    class RenderQueue;

    enum GameObjectFlags : uint32_t
    {
        GameObjectActive = 1 << 0,
        GameObjectVisible = 1 << 1,
        GameObjectDispose = 1 << 2
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

        template <class T, class... Args> T *addComponent(Args &&...args)
        {
            if (getComponent<T>())
                return nullptr;
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
            return static_cast<T *>(mComponents[static_cast<uint8_t>(T::Type)]);
        }

        template <class T> bool contains() const
        {
            const Component *component = mComponents[static_cast<uint8_t>(T::Type)];
            return component != nullptr && ComponentMatch<T>::test(component);
        }

        template <class T> T *findComponent() const
        {
            Component *component = mComponents[static_cast<uint8_t>(T::Type)];
            return (component && ComponentMatch<T>::test(component)) ? static_cast<T *>(component)
                                                                       : nullptr;
        }

        template <class T> bool removeComponent()
        {
            return removeComponent(T::Type);
        }

        Component *rawComponent(ComponentType type) const;

        int zIndex() const;
        void setZIndex(int zIndex);

        const glm::vec2 &position() const;
        float rotationDegrees() const;
        const glm::vec2 &scale() const;
        void setPosition(const glm::vec2 &position);
        void setRotationDegrees(float degrees);
        void setScale(const glm::vec2 &scale);
        void translate(const glm::vec2 &offset);
        void rotate(float degrees);
        const Matrix2D &localTransform() const;
        const Matrix2D &globalTransform() const;
        glm::vec2 globalPosition() const;
        glm::vec2 right() const;
        glm::vec2 up() const;

    private:
        friend class Scene;

        explicit GameObject(const char *name = "");

        bool addChildRaw(GameObject *child);
        GameObject *removeChildRaw(GameObject *child);
        void deleteChildrenRaw();

        bool attachComponent(Component *component);
        bool removeComponent(ComponentType type);
        void deleteComponents();
        void updateComponents(float deltaTime);
        void lateUpdateComponents(float deltaTime);
        void renderComponents(RenderQueue &queue);
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
        ct::Vector<Component *> mPendingComponentDeletes;
        uint32_t mComponentCallbackDepth;

        glm::vec2 mPosition;
        float mRotationDegrees;
        glm::vec2 mScale;
        int mZIndex;
        mutable Matrix2D mLocalTransform;
        mutable Matrix2D mGlobalTransform;
        mutable bool mLocalDirty;
        mutable bool mGlobalDirty;
    };

}