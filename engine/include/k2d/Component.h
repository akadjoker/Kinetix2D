#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>

namespace k2d
{

    class GameObject;
    class RenderQueue;

    enum class ComponentType : uint8_t
    {
        Sprite,
        Script,
        Camera,
        TileMap,
    SpriteBatch,
    Polygon2D,
    Animation,
    Light,
        Occluder,
        LinePath,
        NinePatch,
        Particle,
        RigidBody,
        Collider,
        CircleShape,
        RectShape,
        Count
    };

    enum ComponentEventFlags : uint8_t
    {
        ComponentEventNone = 0,
        ComponentEventUpdate = 1 << 0,
        ComponentEventLateUpdate = 1 << 1,
        ComponentEventRender = 1 << 2
    };

    class Component;

    template <class T> struct ComponentMatch
    {
        static bool test(const Component *)
        {
            return true;
        }
    };

    class Component
    {
    public:
        virtual ~Component() = default;

        Component(const Component &) = delete;
        Component &operator=(const Component &) = delete;

        GameObject *owner() const;
        ComponentType type() const;
        uint32_t id() const;
        bool active() const;
        void setActive(bool active);

    protected:
        explicit Component(ComponentType type, uint8_t events = ComponentEventNone);

        virtual void onAwake();
        virtual void onStart();
        virtual void onEnable();
        virtual void onDisable();
        virtual void onUpdate(float deltaTime);
        virtual void onLateUpdate(float deltaTime);
        virtual void onRender(RenderQueue &queue);
        virtual void onDestroy();

    private:
        friend class GameObject;
        friend class Scene;

        void attached();
        void detached();

        GameObject *mOwner;
        Component *mNextSibling;
        ComponentType mType;
        uint32_t mLocalId;
        uint8_t mEvents;
        bool mActive;
        bool mStarted;
        static constexpr std::size_t InvalidSceneListIndex = (std::numeric_limits<std::size_t>::max)();
        std::size_t mSceneAllIndex;
        std::size_t mSceneLateIndex;
        std::size_t mSceneRenderIndex;
    };

}
