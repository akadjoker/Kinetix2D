#pragma once

#include <kx/debugdraw.h>
#include <kx/world.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace kx
{
class TileMapCollider;
}

namespace k2d
{

class GameObject;
class RigidBody2D;
class Collider2D;
class CanvasRenderer;
class TileMapComponent;

struct CollisionInfo
{
    GameObject* self = nullptr;
    GameObject* other = nullptr;
    Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 travel = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 remainder = Math::Vec2(0.0f, 0.0f);
    float fraction = 1.0f;
    bool sensor = false;
    bool began = false;
    bool hit = false;
};

class PhysicsWorld2D
{
  public:
    friend class RigidBody2D;

    using CollisionCallback = void (*)(const CollisionInfo& info, void* user);

    explicit PhysicsWorld2D(const Math::Vec2& gravity = Math::Vec2(0.0f, 980.0f));
    ~PhysicsWorld2D();

    PhysicsWorld2D(const PhysicsWorld2D&) = delete;
    PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;

    void build(GameObject& root);
    void clear();
    void step(float deltaTime);

    void attach(GameObject& object);
    void detach(RigidBody2D& rigidBody);
    bool markDirty(RigidBody2D& rigidBody);

    static void SetActive(PhysicsWorld2D* world);
    static PhysicsWorld2D* Active();

    void setGravity(const Math::Vec2& gravity)
    {
        mWorld.SetGravity(gravity);
    }
    Math::Vec2 gravity() const
    {
        return mWorld.Gravity();
    }

    void setFixedTimeStep(float seconds)
    {
        mFixedStep = seconds > 0.0f ? seconds : 0.0f;
    }
    float fixedTimeStep() const
    {
        return mFixedStep;
    }

    void setCollisionCallback(CollisionCallback callback, void* user);

    // Renders the current simulation as an overlay. Call after Scene::render so it stays on top.
    void debugDraw(CanvasRenderer& canvas, unsigned flags = kx::DebugDrawShapes);

    GameObject* raycast(const Math::Vec2& origin, const Math::Vec2& direction, float distance,
                        Math::Vec2* outPoint = nullptr, Math::Vec2* outNormal = nullptr,
                        const GameObject* ignore = nullptr);
    GameObject* objectAtPoint(const Math::Vec2& point);
    void overlapCircle(const Math::Vec2& center, float radius, ct::Vector<GameObject*>& out);

    std::size_t bodyCount() const
    {
        return mWorld.BodyCount();
    }
    std::size_t contactCount() const
    {
        return mWorld.ContactCount();
    }
    kx::World& world()
    {
        return mWorld;
    }

    static GameObject* objectFor(const kx::Body* body);

  private:
    void collect(GameObject& object);
    void createTileMapCollider(GameObject& object, TileMapComponent& tileMap);
    void createBody(GameObject& object, RigidBody2D& rigidBody);
    void attachColliders(GameObject& object, RigidBody2D& rigidBody);
    void drainPending();
    void rebuildChanged();
    void rebuildBody(RigidBody2D& rigidBody);
    bool needsRebuild(const RigidBody2D& rigidBody) const;
    void pushTransforms();
    void pullTransforms();
    static void onContact(const kx::ContactEvent& event, void* context);

    kx::World mWorld;
    ct::Vector<RigidBody2D*> mBodies;
    ct::Vector<RigidBody2D*> mPending;
    ct::Vector<kx::TileMapCollider*> mTileMaps;
    float mFixedStep;
    float mAccumulator;
    CollisionCallback mCallback;
    void* mCallbackUser;
};

} // namespace k2d
