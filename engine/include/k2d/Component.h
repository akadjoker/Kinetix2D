#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>

namespace k2d
{

class GameObject;
class RenderQueue;
class UiControl;

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
    AudioPlayer,
    RigidBody,
    Collider,
    Joint,
    CircleShape,
    RectShape,
    CapsuleShape,
    UiCanvas,
    UiPanel,
    UiLabel,
    UiButton,
    UiCheckBox,
    UiSlider,
    NavigationRegion,
    NavigationAgent,
    MotionTween,
    MotionStreak,
    CharacterBody,
    Skeleton,
    Bone,
    ParallaxLayer,
    Steering,
    Formation,
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
    static bool test(const Component*)
    {
        return true;
    }
};

class Component
{
  public:
    // Told when a component leaves its object, just before it is deleted (or
    // queued for deletion). The scripting layer caches one script handle per
    // component address and those handles are persistent, so without this the
    // cache keeps a dead pointer and the next component allocated at the same
    // address inherits the handle. The engine cannot call into the scripting
    // library - it is the one that depends on the engine - so the hook is
    // registered from up there.
    using RemovedCallback = void (*)(Component* component, void* user);
    static void SetRemovedCallback(RemovedCallback callback, void* user);

    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    GameObject* owner() const;
    ComponentType type() const;
    uint32_t id() const;
    bool active() const;
    void setActive(bool active);
    virtual UiControl* uiControl()
    {
        return nullptr;
    }

  protected:
    explicit Component(ComponentType type, uint8_t events = ComponentEventNone);

    virtual void onAwake();
    virtual void onStart();
    virtual void onEnable();
    virtual void onDisable();
    virtual void onUpdate(float deltaTime);
    virtual void onLateUpdate(float deltaTime);
    virtual void onRender(RenderQueue& queue);
    virtual void onDestroy();

  private:
    friend class GameObject;
    friend class Scene;

    static void notifyRemoved(Component* component);

    void attached();
    void detached();

    GameObject* mOwner;
    Component* mNextSibling;
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

} // namespace k2d
