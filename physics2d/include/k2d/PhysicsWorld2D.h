#pragma once

#include <kx/world.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{

    class GameObject;
    class RigidBody2D;
    class Collider2D;

    struct CollisionInfo
    {
        GameObject *self = nullptr;
        GameObject *other = nullptr;
        Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
        Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
        bool sensor = false;
        bool began = false;
    };

    class PhysicsWorld2D
    {
    public:
        using CollisionCallback = void (*)(const CollisionInfo &info, void *user);

        explicit PhysicsWorld2D(const Math::Vec2 &gravity = Math::Vec2(0.0f, 980.0f));
        ~PhysicsWorld2D();

        PhysicsWorld2D(const PhysicsWorld2D &) = delete;
        PhysicsWorld2D &operator=(const PhysicsWorld2D &) = delete;

        void build(GameObject &root);
        void clear();
        void step(float deltaTime);

        void setGravity(const Math::Vec2 &gravity) { mWorld.SetGravity(gravity); }
        Math::Vec2 gravity() const { return mWorld.Gravity(); }

        void setFixedTimeStep(float seconds) { mFixedStep = seconds > 0.0f ? seconds : 0.0f; }
        float fixedTimeStep() const { return mFixedStep; }

        void setCollisionCallback(CollisionCallback callback, void *user);

        GameObject *raycast(const Math::Vec2 &origin, const Math::Vec2 &direction, float distance,
                            Math::Vec2 *outPoint = nullptr, Math::Vec2 *outNormal = nullptr,
                            const GameObject *ignore = nullptr);
        GameObject *objectAtPoint(const Math::Vec2 &point);
        void overlapCircle(const Math::Vec2 &center, float radius, ct::Vector<GameObject *> &out);

        std::size_t bodyCount() const { return mWorld.BodyCount(); }
        std::size_t contactCount() const { return mWorld.ContactCount(); }
        kx::World &world() { return mWorld; }

        static GameObject *objectFor(const kx::Body *body);

    private:
        void collect(GameObject &object);
        void createBody(GameObject &object, RigidBody2D &rigidBody);
        void attachColliders(GameObject &object, RigidBody2D &rigidBody);
        void pushTransforms();
        void pullTransforms();
        static void onContact(const kx::ContactEvent &event, void *context);

        kx::World mWorld;
        ct::Vector<RigidBody2D *> mBodies;
        float mFixedStep;
        float mAccumulator;
        CollisionCallback mCallback;
        void *mCallbackUser;
    };

}
