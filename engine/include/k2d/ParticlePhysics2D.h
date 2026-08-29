#pragma once

// Cheap point/circle projectiles for VFX-driven physics: explosion debris,
// sparks, blood drops. Not RigidBody2D: no inertia, no joints, no sleeping -
// just position/velocity integrated in bulk and swept against the world with
// Scene::raycast(), which reuses whatever broadphase the scene already has.
// On a hit a callback reports the point, normal and incoming velocity, so the
// caller can spawn a decal, a shard, whatever. Each spawn also carries its own
// sprite and the Scene submits the live pool into the render queue, so one call
// gives a burst that both collides and draws. Not to be confused with
// ParticleSystem, which is the visual emitter and does not collide.

#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <ct/vector.hpp>
#include <mathc.h>

#include <cstddef>
#include <cstdint>

namespace k2d
{

    class Scene;
    class GameObject;
    class RenderQueue;
    class Texture;

    enum class ParticleResponse : unsigned char
    {
        Kill,
        Bounce,
        Stick
    };

    struct ParticleHit2D
    {
        uint32_t particleId = 0;
        uint32_t userTag = 0;
        Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
        Math::Vec2 normal = Math::Vec2(0.0f, -1.0f);
        Math::Vec2 incomingVelocity = Math::Vec2(0.0f, 0.0f);
        GameObject *other = nullptr;
    };

    struct ParticleSpawn2D
    {
        Math::Vec2 position = Math::Vec2(0.0f, 0.0f);
        Math::Vec2 velocity = Math::Vec2(0.0f, 0.0f);
        float radius = 1.0f;
        float life = 1.0f;
        float restitution = 0.3f;
        float drag = 0.1f;
        uint32_t userTag = 0;
        ParticleResponse response = ParticleResponse::Kill;

        Texture *texture = nullptr;
        Math::Vec2 size = Math::Vec2(4.0f, 4.0f);
        Color colorStart = Color(1.0f);
        Color colorEnd = Color(1.0f);
        int zIndex = 0;
    };

    struct PhysicsParticle2D
    {
        Math::Vec2 position = Math::Vec2(0.0f, 0.0f);
        Math::Vec2 velocity = Math::Vec2(0.0f, 0.0f);
        float radius = 1.0f;
        float life = 1.0f;
        float restitution = 0.3f;
        float drag = 0.1f;
        float initialLife = 1.0f;
        uint32_t userTag = 0;
        uint32_t id = 0;
        ParticleResponse response = ParticleResponse::Kill;
        bool settled = false;

        Texture *texture = nullptr;
        Math::Vec2 size = Math::Vec2(4.0f, 4.0f);
        Color colorStart = Color(1.0f);
        Color colorEnd = Color(1.0f);
        int zIndex = 0;
    };

    // Pooled and index-stable only within a step: dead particles are
    // swap-removed at the end of step(), so an id handed to the hit callback is
    // valid for that step alone. Copy out whatever the callback needs.
    class ParticlePhysics2D
    {
    public:
        using HitCallback = void (*)(const ParticleHit2D &hit, void *user);

        static const uint32_t InvalidId = 0;

        explicit ParticlePhysics2D(Scene &scene);

        void setGravity(const Math::Vec2 &gravity) { mGravity = gravity; }
        const Math::Vec2 &gravity() const { return mGravity; }

        void setCategoryMask(uint16_t mask) { mCategoryMask = mask; }
        uint16_t categoryMask() const { return mCategoryMask; }

        // Runaway guard: emit() past this returns InvalidId and spawns nothing.
        void setMaxParticles(std::size_t maxParticles);
        std::size_t maxParticles() const { return mMaxParticles; }

        void setHitCallback(HitCallback callback, void *user);
        void setSeed(uint32_t seed) { mRandomState = seed; }

        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }

        uint32_t emit(const ParticleSpawn2D &spawn);

        void explode(const Math::Vec2 &center, std::size_t count, float speedMin, float speedMax,
                     float radiusMin, float radiusMax, float life, uint32_t userTag = 0,
                     ParticleResponse response = ParticleResponse::Bounce, Texture *texture = nullptr,
                     const Math::Vec2 &size = Math::Vec2(4.0f, 4.0f), const Color &colorStart = Color(1.0f),
                     const Color &colorEnd = Color(1.0f), int zIndex = 0);

        void step(float deltaTime);

        // Batched by texture and z index; particles without a texture are
        // simulated but never drawn.
        void submit(RenderQueue &queue) const;

        void clear() { mParticles.clear(); }

        std::size_t count() const { return mParticles.size(); }
        const PhysicsParticle2D &at(std::size_t index) const { return mParticles[index]; }
        // Start colour lerped towards the end colour by how much life is spent.
        Color colorAt(std::size_t index) const;

    private:
        float random01();
        float randomRange(float minValue, float maxValue);
        Math::Vec2 randomDirection();

        Scene &mScene;
        ct::Vector<PhysicsParticle2D> mParticles;
        Math::Vec2 mGravity;
        uint16_t mCategoryMask;
        std::size_t mMaxParticles;
        HitCallback mHitCallback;
        void *mHitUser;
        uint32_t mNextId;
        uint32_t mRandomState;
        BlendMode mBlendMode;
        mutable ct::Vector<unsigned char> mSubmitted;
    };

}
