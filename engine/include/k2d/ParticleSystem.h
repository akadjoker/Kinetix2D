#pragma once

#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    enum class ParticleMode
    {
        OneShot,
        Persistent,
        Loop
    };

    enum class ParticleEmitterShape
    {
        Point,
        Circle,
        Rectangle
    };

    // Everything a particle needs is a *range* (Min==Max degenerates to a
    // fixed value) so one prefab can drive a whole believable effect instead
    // of every particle coming out an identical clone. Modeled after
    // zenengine's ParticleEmitter2D (direction+spread cone, speed/rotation
    // ranges, drag, face-direction) with one deliberate change: the color
    // ramp keys are normalized life fractions [0,1], not absolute seconds --
    // that's what lets lifeMin/lifeMax vary per particle and still have the
    // ramp complete exactly at death every time.
    struct ParticlePrefab
    {
        // Particles fire within +/-spreadDegrees/2 of `direction`, at a
        // random speed in [speedMin, speedMax]. spreadDegrees=0 (default) is
        // a dead-straight line, same as the old fixed-velocity behaviour.
        glm::vec2 direction = glm::vec2(0.0f, -1.0f);
        float spreadDegrees = 0.0f;
        float speedMin = 0.0f;
        float speedMax = 0.0f;

        float lifeMin = 1.0f;
        float lifeMax = 1.0f;

        float sizeMin = 1.0f;
        float sizeMax = 1.0f;
        float endSize = 0.0f; // 0 keeps size constant (no shrink/grow over life)

        float rotationMin = 0.0f;
        float rotationMax = 0.0f;
        float angularVelocityMin = 0.0f;
        float angularVelocityMax = 0.0f;

        float drag = 0.0f; // velocity *= (1 - drag*dt) each frame; 0 = no damping

        // Overrides rotation with the velocity's own heading every frame
        // (rain, sparks, debris -- anything that should visibly point the
        // way it's moving) instead of angularVelocity-driven spin.
        bool faceDirection = false;
        float faceDirectionOffsetDegrees = 0.0f;

        float fadeIn = 0.0f;  // seconds; extra alpha multiplier on top of the ramp
        float fadeOut = 0.0f; // seconds

        // Two-key RGBA ramp sampled at age/lifetime in [0,1].
        glm::vec4 colorStart = glm::vec4(1.0f);
        glm::vec4 colorEnd = glm::vec4(1.0f);

        // Pixel bounds in the atlas: x, y, width, height.
        // Zero width/height means the complete texture.
        glm::vec4 atlasBounds = glm::vec4(0.0f);
    };

    class BatchRenderer;
    class RenderQueue;
    class Texture;

    struct Particle
    {
        glm::vec2 position;
        glm::vec2 velocity;
        glm::vec4 colorStart;
        glm::vec4 colorEnd;
        glm::vec4 color; // colorStart/colorEnd sampled at the current age
        float age;
        float lifetime;
        float size;
        float startSize;
        float endSize;
        float fadeIn;
        float fadeOut;
        float rotation;
        float angularVelocity;
        float drag;
        bool faceDirection;
        float faceDirectionOffsetDegrees;
        glm::vec4 atlasBounds;
    };

    class ParticleSystem
    {
    public:
        explicit ParticleSystem(size_t capacity = 256);

        void SetTexture(Texture *texture);
        Texture *GetTexture() const { return mTexture; }
        void SetGravity(const glm::vec2 &gravity) { mGravity = gravity; }
        const glm::vec2 &Gravity() const { return mGravity; }
        void SetCapacity(size_t capacity);

        void SetMode(ParticleMode mode) { mMode = mode; }
        ParticleMode GetMode() const { return mMode; }
        void SetPrefab(const ParticlePrefab &prefab) { mPrefab = prefab; }
        const ParticlePrefab &GetPrefab() const { return mPrefab; }
        void SetEmitterPosition(const glm::vec2 &position) { mEmitterPosition = position; }
        const glm::vec2 &EmitterPosition() const { return mEmitterPosition; }
        void SetEmitterShape(ParticleEmitterShape shape) { mEmitterShape = shape; }
        ParticleEmitterShape GetEmitterShape() const { return mEmitterShape; }
        void SetEmitterRadius(float radius) { mEmitterSize = glm::vec2(radius > 0.0f ? radius : 0.0f); }
        void SetEmitterSize(const glm::vec2 &size) { mEmitterSize = size; }
        const glm::vec2 &EmitterSize() const { return mEmitterSize; }
        void SetEmissionRate(float particlesPerSecond);
        float EmissionRate() const { return mEmissionRate; }
        void SetOneShotCount(size_t count) { mOneShotCount = count; }
        size_t OneShotCount() const { return mOneShotCount; }
        void Start();
        void Stop() { mPlaying = false; }
        void Reset();
        void Restart() { Reset(); }
        bool IsPlaying() const { return mPlaying; }
        bool IsFinished() const { return mFinished; }

        // Direct/manual emission: flat (unranged) values, no prefab involved.
        // Used for one-off scripted particles (e.g. a firework burst) where
        // the caller already computed the exact velocity per particle.
        bool Emit(const glm::vec2 &position, const glm::vec2 &velocity,
                  float lifetime, float size, const glm::vec4 &color = glm::vec4(1.0f),
                  float rotation = 0.0f, float angularVelocity = 0.0f);
        bool Emit(const glm::vec2 &position, const glm::vec2 &velocity,
                  float lifetime, float size, const glm::vec4 &color,
                  float rotation, float angularVelocity,
                  const glm::vec4 &atlasBounds);
        // Prefab emission: every ranged field is rolled independently, plus
        // spread-cone direction, drag and face-direction.
        bool Emit(const glm::vec2 &position, const ParticlePrefab &prefab);
        void Update(float deltaTime);
        // Unlit/debug path. Draws immediately through the low-level batcher.
        void Draw(BatchRenderer &batch) const;
        // Lit canvas path. Adds particles to the render queue so CanvasRenderer
        // applies point/directional lights and the existing shadow atlas.
        void Submit(RenderQueue &queue, int zIndex = 0, bool ySort = false,
                    BlendMode blendMode = BLEND_MIX) const;

        size_t ActiveCount() const { return mParticles.size(); }
        size_t Capacity() const { return mCapacity; }
        const Particle &Get(size_t index) const { return mParticles[index]; }

    private:
        Texture *mTexture;
        glm::vec2 mGravity;
        size_t mCapacity;
        ct::Vector<Particle> mParticles;
        ParticleMode mMode;
        ParticlePrefab mPrefab;
        glm::vec2 mEmitterPosition;
        float mEmissionRate;
        float mEmissionAccumulator;
        size_t mOneShotCount;
        bool mPlaying;
        bool mFinished;
        ParticleEmitterShape mEmitterShape;
        glm::vec2 mEmitterSize;
        unsigned int mRandomState;

        float Random01();
        float RandomRange(float minValue, float maxValue);
        glm::vec2 EmitPosition();
    };

}
