#pragma once

#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <ct/vector.hpp>
#include <mathc.h>

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

    struct ParticlePrefab
    {

        Math::Vec2 direction = Math::Vec2(0.0f, -1.0f);
        float spreadDegrees = 0.0f;
        float speedMin = 0.0f;
        float speedMax = 0.0f;

        float lifeMin = 1.0f;
        float lifeMax = 1.0f;

        float sizeMin = 1.0f;
        float sizeMax = 1.0f;
        float endSize = 0.0f; 

        float rotationMin = 0.0f;
        float rotationMax = 0.0f;
        float angularVelocityMin = 0.0f;
        float angularVelocityMax = 0.0f;

        float drag = 0.0f; 

        bool faceDirection = false;
        float faceDirectionOffsetDegrees = 0.0f;

        float fadeIn = 0.0f;  
        float fadeOut = 0.0f; 

        Color colorStart = Color(1.0f);
        Color colorEnd = Color(1.0f);

        Math::Vec4 atlasBounds = Math::Vec4(0.0f);
    };

    class BatchRenderer;
    class RenderQueue;
    class Texture;

    struct Particle
    {
        Math::Vec2 position;
        Math::Vec2 velocity;
        Color colorStart;
        Color colorEnd;
        Color color;
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
        Math::Vec4 atlasBounds;
    };

    class ParticleSystem
    {
    public:
        explicit ParticleSystem(size_t capacity = 256);

        void SetTexture(Texture *texture);
        Texture *GetTexture() const { return mTexture; }
        void SetGravity(const Math::Vec2 &gravity) { mGravity = gravity; }
        const Math::Vec2 &Gravity() const { return mGravity; }
        void SetCapacity(size_t capacity);

        void SetMode(ParticleMode mode) { mMode = mode; }
        ParticleMode GetMode() const { return mMode; }
        void SetPrefab(const ParticlePrefab &prefab) { mPrefab = prefab; }
        const ParticlePrefab &GetPrefab() const { return mPrefab; }
        void SetEmitterPosition(const Math::Vec2 &position) { mEmitterPosition = position; }
        const Math::Vec2 &EmitterPosition() const { return mEmitterPosition; }
        void SetEmitterShape(ParticleEmitterShape shape) { mEmitterShape = shape; }
        ParticleEmitterShape GetEmitterShape() const { return mEmitterShape; }
        void SetEmitterRadius(float radius) { mEmitterSize = Math::Vec2(radius > 0.0f ? radius : 0.0f); }
        void SetEmitterSize(const Math::Vec2 &size) { mEmitterSize = size; }
        const Math::Vec2 &EmitterSize() const { return mEmitterSize; }
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

        bool Emit(const Math::Vec2 &position, const Math::Vec2 &velocity,
                  float lifetime, float size, const Color &color = Color(1.0f),
                  float rotation = 0.0f, float angularVelocity = 0.0f);
        bool Emit(const Math::Vec2 &position, const Math::Vec2 &velocity,
                  float lifetime, float size, const Color &color,
                  float rotation, float angularVelocity,
                  const Math::Vec4 &atlasBounds);

        bool Emit(const Math::Vec2 &position, const ParticlePrefab &prefab);
        void Update(float deltaTime);

        void Draw(BatchRenderer &batch) const;

        void Submit(RenderQueue &queue, int zIndex = 0, bool ySort = false,
                    BlendMode blendMode = BLEND_MIX) const;

        size_t ActiveCount() const { return mParticles.size(); }
        size_t Capacity() const { return mCapacity; }
        const Particle &Get(size_t index) const { return mParticles[index]; }

    private:
        Texture *mTexture;
        Math::Vec2 mGravity;
        size_t mCapacity;
        ct::Vector<Particle> mParticles;
        ParticleMode mMode;
        ParticlePrefab mPrefab;
        Math::Vec2 mEmitterPosition;
        float mEmissionRate;
        float mEmissionAccumulator;
        size_t mOneShotCount;
        bool mPlaying;
        bool mFinished;
        ParticleEmitterShape mEmitterShape;
        Math::Vec2 mEmitterSize;
        unsigned int mRandomState;

        float Random01();
        float RandomRange(float minValue, float maxValue);
        Math::Vec2 EmitPosition();
    };

}