#pragma once

#include <cstdint>

namespace k2d
{

    class GameObject;
    class BatchRenderer;

    enum class ComponentType : uint8_t
    {
        Sprite,
        Script,
        Camera,
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
        virtual void onRender(BatchRenderer &batch);
        virtual void onDestroy();

    private:
        friend class GameObject;
        friend class Scene;

        void attached();
        void detached();

        GameObject *mOwner;
        ComponentType mType;
        uint8_t mEvents;
        bool mActive;
        bool mStarted;
    };

}
